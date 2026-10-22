"""Exercise the four existing control families without adding a new CLI route.

This is local consistency evidence for the read-only compatibility descriptor.
The descriptor itself is intentionally exercised through the focused C++ matrix
binding executable passed as the optional second argument.
"""

import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest


PARSER = argparse.ArgumentParser()
PARSER.add_argument("binary", type=Path)
PARSER.add_argument("matrix_binary", type=Path, nargs="?")
OPTIONS = PARSER.parse_args()
BIN = OPTIONS.binary.resolve()
MATRIX_BINARY = OPTIONS.matrix_binary.resolve() if OPTIONS.matrix_binary else None
import sys
sys.argv = [sys.argv[0]]


class CommonRuntimeCompatibilityWorkflows(unittest.TestCase):
    def run_cli(self, *args, code=0):
        process = subprocess.run(
            [str(BIN), *map(str, args)],
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(process.returncode, code, process.stderr)
        return json.loads(process.stdout if code == 0 else process.stderr)

    def test_real_control_families_preserve_legacy_boundaries(self):
        with tempfile.TemporaryDirectory(prefix="bruce-common-runtime-v1-") as directory:
            root = Path(directory)
            train = root / "train.csv"
            holdout = root / "holdout.csv"
            rules = root / "rules.txt"
            source = root / "source.bruce"
            train.write_text("x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n")
            holdout.write_text("x,y\n5,7/6\n")
            rules.write_text(
                "BRUCE_RULES_V1\n"
                "snapshot accepted\n"
                "rule first positive value ge 0 0 1 manual -\n"
                "rule second positive value ge 0 0 1 manual -\n"
            )

            # Session family: a controlled run publishes exactly the same
            # bundle as the legacy invocation, while its zero budget retains
            # the existing structured application failure envelope.
            research = [
                "research", "--strategy", "portfolio-rational", "--max-degree", "2",
                "--train", train, "--holdout", holdout, "--verification", "replay",
            ]
            plain = self.run_cli(*research, "--output", source)
            controlled_source = root / "controlled-source.bruce"
            controlled = self.run_cli(
                *research, "--max-session-checkpoints", "1000000",
                "--output", controlled_source,
            )
            self.assertEqual(controlled["result"], plain["result"])
            self.assertEqual(source.read_bytes(), controlled_source.read_bytes())
            self.assertTrue(controlled["execution"]["replay_verified"])
            failed_source = root / "failed-source.bruce"
            session_failure = self.run_cli(
                *research, "--max-session-checkpoints", "0", "--output", failed_source,
                code=2,
            )
            self.assertEqual(
                session_failure["error"],
                "ExecutionSessionFailed at EXECUTING: "
                "SessionCheckpointBudgetExceeded: phase=EXECUTING checkpoints=0 limit=0",
            )
            self.assertEqual(session_failure["execution"]["contract"],
                             "application-execution-failure-v2")
            self.assertFalse(failed_source.exists())

            # Gate family: the gate has no publication option.  Its controlled
            # result remains byte-for-byte equal to the legacy result and the
            # existing cancellation option is accepted without changing it.
            gate_plain = self.run_cli("gate-run", source)
            gate_controlled = self.run_cli(
                "gate-run", source, "--max-session-checkpoints", "1000000",
                "--cancellation", "checkpoints",
            )
            self.assertEqual(gate_controlled["result"], gate_plain)
            self.assertEqual(gate_controlled["usage"]["model"], "session-checkpoints-v1")
            gate_zero = self.run_cli("gate-run", source,
                                     "--max-session-checkpoints", "0", code=2)
            self.assertEqual(
                gate_zero,
                {"error": "SessionCheckpointBudgetExceeded: phase=REPLAYING "
                          "checkpoints=0 limit=0"},
            )
            self.assertEqual(
                self.run_cli("gate-run", source, "--max-transfer-checkpoints", "1", code=2),
                {"error": "UnknownOption: --max-transfer-checkpoints"},
            )

            # Transfer family: the plan and applied bundle keep their legacy
            # bytes under the bounded transfer wrapper.  A one-check plan is a
            # real partial result and therefore remains INCOMPLETE.
            plan_args = [
                "compatibility-plan", "--from-run", source, "--rules", rules, "--at", "0",
            ]
            plain_plan_path = root / "plain.plan"
            plain_plan = self.run_cli(*plan_args, "--output", plain_plan_path)
            bounded_plan_path = root / "bounded.plan"
            bounded_plan = self.run_cli(
                *plan_args, "--max-transfer-checkpoints", "1000000",
                "--transfer-cancellation", "checkpoints", "--output", bounded_plan_path,
            )
            self.assertEqual(bounded_plan["result"], plain_plan)
            self.assertEqual(plain_plan_path.read_bytes(), bounded_plan_path.read_bytes())
            self.assertEqual(bounded_plan["usage"]["model"], "transfer-checkpoints-v1")
            transfer_zero_path = root / "transfer-zero.plan"
            transfer_zero = self.run_cli(
                *plan_args, "--max-transfer-checkpoints", "0",
                "--output", transfer_zero_path, code=2,
            )
            self.assertEqual(
                transfer_zero,
                {"error": "TransferCheckpointBudgetExceeded: checkpoints=0 limit=0"},
            )
            self.assertFalse(transfer_zero_path.exists())

            # The transfer path supports a real signal cancellation mode. A
            # FIFO keeps the workflow at its input boundary until the handler
            # is installed, making the assertion deterministic and local.
            transfer_fifo = root / "transfer-input.fifo"
            os.mkfifo(transfer_fifo)
            cancelled_path = root / "cancelled.plan"
            cancelled = subprocess.Popen(
                [
                    str(BIN), "compatibility-plan", "--from-run", transfer_fifo,
                    "--rules", rules, "--at", "0", "--transfer-cancellation",
                    "checkpoints", "--output", cancelled_path,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            writer = None
            try:
                deadline = time.monotonic() + 5
                while writer is None:
                    try:
                        writer = os.open(transfer_fifo, os.O_WRONLY | os.O_NONBLOCK)
                    except OSError:
                        if time.monotonic() > deadline:
                            raise
                        time.sleep(0.01)
                cancelled.send_signal(signal.SIGTERM)
                os.set_blocking(writer, True)
                payload = source.read_bytes()
                while payload:
                    payload = payload[os.write(writer, payload):]
                os.close(writer)
                writer = None
                stdout, stderr = cancelled.communicate(timeout=10)
                self.assertEqual(cancelled.returncode, 2, stderr.decode())
                self.assertEqual(stdout, b"")
                self.assertEqual(json.loads(stderr), {"error": "TransferCancelled"})
                self.assertFalse(cancelled_path.exists())
            finally:
                if writer is not None:
                    os.close(writer)
                if cancelled.poll() is None:
                    cancelled.kill()
                    cancelled.wait()

            limited_plan_path = root / "limited.plan"
            self.run_cli(*plan_args, "--max-checks", "1", "--output", limited_plan_path)
            incomplete = self.run_cli(
                "compatibility-apply", limited_plan_path, "--from-run", source,
                "--rules", rules,
            )
            self.assertEqual(incomplete["result"]["status"], "INCOMPLETE")
            self.assertEqual(incomplete["result"]["stop_reason"], "OperationBudgetExceeded")

            # Management family: build a real experience snapshot and inspect
            # it through the read-only path.  The bounded wrapper does not
            # change the result or mutate the snapshot.
            snapshot = root / "experience.snapshot"
            self.run_cli("experience", "add", "--from-run", source, "--output", snapshot)
            snapshot_bytes = snapshot.read_bytes()
            management_plain = self.run_cli("experience", "inspect", snapshot)
            management_controlled = self.run_cli(
                "experience", "inspect", snapshot,
                "--max-management-checkpoints", "1000000",
                "--management-cancellation", "none",
            )
            self.assertEqual(management_controlled["result"], management_plain)
            self.assertEqual(management_controlled["usage"]["model"],
                             "management-checkpoints-v1")
            management_zero = self.run_cli(
                "experience", "inspect", snapshot,
                "--max-management-checkpoints", "0", code=2,
            )
            self.assertEqual(
                management_zero,
                {"error": "ManagementCheckpointBudgetExceeded: checkpoints=0 limit=0"},
            )
            self.assertEqual(
                self.run_cli(
                    "experience", "inspect", snapshot,
                    "--max-transfer-checkpoints", "1", code=2,
                ),
                {"error": "Usage: bruce experience inspect SNAPSHOT"},
            )
            self.assertEqual(snapshot.read_bytes(), snapshot_bytes)

            # The descriptor is intentionally not exposed as a new CLI.  The
            # focused C++ binding test remains the evidence for exact family,
            # unit, publication-boundary and six mutation rejections.
            if MATRIX_BINARY is not None:
                descriptor = subprocess.run(
                    [str(MATRIX_BINARY)], capture_output=True, text=True, timeout=30,
                )
                self.assertEqual(descriptor.returncode, 0, descriptor.stderr)
                self.assertEqual(
                    descriptor.stdout,
                    "Four control family bindings and six mutations verified\n",
                )


if __name__ == "__main__":
    unittest.main()
