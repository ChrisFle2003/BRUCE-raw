#!/usr/bin/env python3
"""Verify public controller-boundary compatibility; no production writes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def failure_reply(process: subprocess.CompletedProcess[str], label: str,
                  expected_keys: set[str]) -> dict:
    if process.stdout:
        raise RuntimeError(f"{label}: unexpected stdout on failure: {process.stdout}")
    reply = json.loads(process.stderr)
    if set(reply) != expected_keys:
        raise RuntimeError(f"{label}: unexpected keys on failure: {sorted(reply)}")
    return reply


def verify(binary: Path) -> dict:
    records = []
    checks = 0

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        if not condition:
            raise RuntimeError(message)
        checks += 1

    with tempfile.TemporaryDirectory(prefix="bruce-control-contract-matrix-") as directory:
        root = Path(directory)

        def call(label: str, argv: list[object], exit_code: int = 0,
                 failure_keys: set[str] | None = None) -> dict:
            args = [str(value) for value in argv]
            process = subprocess.run(
                [str(binary), *args], cwd=root, capture_output=True, text=True, timeout=60)
            require(process.returncode == exit_code,
                    f"{label}: exit {process.returncode}: {process.stderr}")
            if exit_code == 0:
                reply = json.loads(process.stdout)
            else:
                if failure_keys is None:
                    raise RuntimeError(f"{label}: missing failure envelope contract")
                reply = failure_reply(process, label, failure_keys)
            records.append({"label": label, "argv": args, "exit_code": exit_code, "reply": reply})
            return reply

        def absent(name: str) -> None:
            require(not (root / name).exists(), f"unexpected publication: {name}")
            require(not list(root.glob(name + ".tmp.*")), f"temporary export residue: {name}")

        inputs = {
            "train.csv": "x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n",
            "holdout.csv": "x,y\n5,7/6\n",
            "rules.txt": "BRUCE_RULES_V1\nsnapshot accepted\n"
                         "rule first positive value ge 0 0 1 manual -\n"
                         "rule second positive value ge 0 0 1 manual -\n",
        }
        for name, content in inputs.items():
            (root / name).write_text(content)

        version = call("version", ["--version"])
        research = ["research", "--strategy", "portfolio-rational", "--max-degree", 2,
                    "--train", "train.csv", "--holdout", "holdout.csv", "--verification", "replay"]
        session = call("session", research + ["--max-session-checkpoints", 1_000_000,
                                                "--output", "source.bruce"])
        require(session["execution"]["replay_verified"], "research session was not replay verified")
        require(call("source-replay", ["replay", "source.bruce"]) == session["result"],
                "research replay mismatch")

        session_failure = call("session-zero-budget", research + ["--max-session-checkpoints", 0,
                                                                     "--output", "stopped-session"], 2,
                               {"error", "execution"})
        require(session_failure["error"].startswith("ExecutionSessionFailed at EXECUTING:"),
                "session failure cause wrapper changed")
        require(session_failure["execution"]["contract"] == "application-execution-failure-v2",
                f"session checkpoint failure envelope changed: {session_failure}")
        require(session_failure["execution"]["state"] == "FAILED" and
                session_failure["execution"]["failed_at"] == "EXECUTING" and
                session_failure["execution"]["phases"] == ["CREATED", "EXECUTING", "FAILED"],
                "session failure state changed")
        require(session_failure["execution"]["cause"] ==
                "SessionCheckpointBudgetExceeded: phase=EXECUTING checkpoints=0 limit=0",
                "session checkpoint cause changed")
        require(session_failure["execution"]["session_budget"] == {
                    "model": "session-checkpoints-v1", "max_checkpoints": 0,
                    "checkpoints": 0,
                    "by_phase": {"EXECUTING": 0, "SERIALIZING": 0, "REPLAYING": 0}},
                "session checkpoint usage changed")
        absent("stopped-session")

        gate_failure = call("gate-zero-budget", ["gate-run", "source.bruce",
                                                   "--max-session-checkpoints", 0], 2, {"error"})
        require(gate_failure["error"].startswith("SessionCheckpointBudgetExceeded:"),
                "gate zero-budget error changed")

        plan_args = ["compatibility-plan", "--from-run", "source.bruce", "--rules", "rules.txt", "--at", 0]
        transfer_failure = call("transfer-zero-budget", plan_args + ["--max-transfer-checkpoints", 0,
                                                                        "--output", "stopped-plan"], 2, {"error"})
        require(transfer_failure["error"].startswith("TransferCheckpointBudgetExceeded:"),
                "transfer zero-budget error changed")
        absent("stopped-plan")

        call("limited-plan", plan_args + ["--max-checks", 1, "--output", "limited.plan"])
        partial = call("limited-apply", ["compatibility-apply", "limited.plan", "--from-run", "source.bruce",
                                          "--rules", "rules.txt", "--max-transfer-checkpoints", 1_000_000,
                                          "--output", "partial.bruce"])
        require(partial["result"]["result"]["status"] == "INCOMPLETE",
                "native bounded result became a control failure")
        require(call("partial-replay", ["replay", "partial.bruce"]) == partial["result"]["result"],
                "partial result replay mismatch")

        return {
            "contract": "bruce-control-contract-matrix-v1",
            "accepted": True,
            "checks": checks,
            "version": version,
            "source_sha256": hashlib.sha256((root / "source.bruce").read_bytes()).hexdigest(),
            "calls": len(records),
        }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    options = parser.parse_args()
    print(json.dumps(verify(options.binary.resolve())))
