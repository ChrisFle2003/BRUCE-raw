"""V6 exposes supervised runs while keeping replay offline and sidecar-free."""
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile


bruce, exporter, worker, timed_worker, _compiler = map(os.path.abspath, sys.argv[1:])


def invoke(command, arguments, code=0):
    result = subprocess.run([bruce, command, *arguments], text=True, capture_output=True,
                            timeout=15)
    assert result.returncode == code, (result.returncode, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix="bruce-dag-v6-cli-") as temporary:
    root = pathlib.Path(temporary)
    subprocess.run([exporter, worker, temporary, "export"], check=True)
    spec = root / "dag.json"
    store, record, report = root / "store", root / "dag.record", root / "report"
    spec.write_text(json.dumps({"schema_version": 2, "nodes": [{
        "id": "a", "package": str(root / "output.brucemod"), "trust_store": str(root / "trust.txt"),
        "input": "3/4 hoch 2", "predecessors": [], "output_manifest": "a-out"
    }]}))

    def args(action="run", selected_spec=spec, selected_store=store, selected_record=record, selected_report=report):
        base = [action, str(selected_spec), "--store", str(selected_store), "--record", str(selected_record)]
        return [*base, "--report", str(selected_report)] if action == "run" else base

    def node(identifier, parents=(), package_root=root, text="3/4 hoch 2", use_combined=False):
        value = {"id": identifier,
                 "package": str(package_root / ("combined.brucemod" if parents or use_combined else "output.brucemod")),
                 "trust_store": str(package_root / "trust.txt"), "input": text,
                 "predecessors": list(parents), "output_manifest": identifier + "-out"}
        if parents:
            value["input_manifest"] = identifier + "-input"
        return value

    def controlled_run(tag, nodes, options, code=2):
        case_spec = root / (tag + ".json")
        case_store, case_record, case_report = root / (tag + "-store"), root / (tag + ".record"), root / (tag + "-report")
        case_spec.write_text(json.dumps({"schema_version": 2, "nodes": nodes}))
        process = invoke("logic-mod-artifact-dag-v6", ["run", str(case_spec), "--store", str(case_store),
                                                         "--record", str(case_record), "--report", str(case_report), *options], code)
        assert process.stdout, (process.stderr, tag, options)
        return json.loads(process.stdout), case_store, case_record, case_report

    # The dispatcher and mandatory report boundary are public and strict.
    unknown = invoke("logic-mod-artifact-dag-v6-unknown", args(), 2)
    assert json.loads(unknown.stderr)["error"].startswith("UnknownCommand:")
    missing = invoke("logic-mod-artifact-dag-v6", args()[:-2], 2)
    assert json.loads(missing.stderr) == {"error": "InvalidLogicModDagArguments"}
    duplicate = invoke("logic-mod-artifact-dag-v6", [*args(), "--report", str(root / "again")], 2)
    assert json.loads(duplicate.stderr) == {"error": "InvalidLogicModDagArguments"}
    invalid_number = invoke("logic-mod-artifact-dag-v6", [*args(), "--max-parallel-nodes", "01"], 2)
    assert json.loads(invalid_number.stderr) == {"error": "InvalidLogicModDagArguments"}

    # A true V6 run writes canonical records plus only its private sidecar.
    result = invoke("logic-mod-artifact-dag-v6", args())
    response = json.loads(result.stdout)
    assert response["contract"] == "logic-mod-artifact-dag-v6"
    assert response["action"] == "run" and response["accepted"] is True
    assert "report" not in response and "peak" not in response and "reason" not in response
    log = report.with_name(report.name + ".run") / "control" / "supervision-v6.jsonl"
    assert log.is_file() and stat.S_IMODE(log.stat().st_mode) == 0o600
    assert stat.S_IMODE(log.parent.stat().st_mode) == 0o700
    events = [json.loads(line) for line in log.read_text().splitlines()]
    assert [event["event"] for event in events] == ["ATTEMPT_STARTED", "REAPED", "ATTEMPT_FINISHED"]
    assert all(not any(forbidden in json.dumps(event) for forbidden in ("/", "pid", "pgid", "payload", "signature"))
               for event in events)

    # V4 admission controls are accepted by V6 and leave canonical DAG bytes
    # unchanged. A distinct report root prevents one diagnostic lifecycle from
    # being treated as another run's history.
    controls_record, controls_report = root / "controls.record", root / "controls-report"
    controls = ["--max-parallel-nodes", "1", "--max-total-wall-ms", "60000",
                "--max-total-payload-bytes", "0", "--max-started-workers", "1",
                "--cancel-after-ms", "60000"]
    controlled = invoke("logic-mod-artifact-dag-v6", [*args(selected_record=controls_record,
                                                               selected_report=controls_report), *controls])
    assert json.loads(controlled.stdout)["accepted"] is True
    assert controls_record.read_bytes() == record.read_bytes()

    # Controls are behavioral, not parser decoration. A whole ready wave is
    # refused before its first worker when the worker-start budget cannot admit
    # the wave; sequential admission accepts precisely the first root.
    whole, whole_store, whole_record, _ = controlled_run(
        "whole-wave", [node("a"), node("b")],
        ["--max-parallel-nodes", "2", "--max-started-workers", "1"])
    assert whole["status"] == "DAG_GLOBAL_BUDGET_EXHAUSTED" and whole["nodes"] == []
    assert not whole_store.exists() and not whole_record.exists()
    sequential, sequential_store, sequential_record, _ = controlled_run(
        "sequential-wave", [node("a"), node("b")],
        ["--max-parallel-nodes", "1", "--max-started-workers", "1"])
    assert sequential["status"] == "DAG_GLOBAL_BUDGET_EXHAUSTED"
    assert [entry["id"] for entry in sequential["nodes"]] == ["a"] and not sequential_record.exists()
    assert (sequential_store / "manifests" / "a-out.manifest").is_file()
    assert not (sequential_store / "manifests" / "b-out.manifest").exists()

    # Root waves have no internal payload. The zero-byte aggregate budget lets
    # that root complete but blocks its dependent child before input artifact
    # publication.
    payload, payload_store, payload_record, _ = controlled_run(
        "payload-zero", [node("a"), node("c", ("a",))], ["--max-total-payload-bytes", "0"])
    assert payload["status"] == "DAG_GLOBAL_BUDGET_EXHAUSTED"
    assert [entry["id"] for entry in payload["nodes"]] == ["a"] and not payload_record.exists()
    assert not (payload_store / "manifests" / "c-input.manifest").exists()

    # The static combined-worker fixture holds execution for 220 ms. A 100 ms host cancellation
    # occurs after the fast root has admitted its child but before that child ends,
    # so this proves the actual child cancellation lifecycle rather than a
    # pre-admission budget rejection. It rejects the otherwise valid result artifact.
    timed = root / "timed"; timed.mkdir()
    subprocess.run([exporter, timed_worker, str(timed), "export"], check=True)
    cancelled, cancelled_store, cancelled_record, _ = controlled_run(
        "cancel", [node("a"), node("c", ("a",), package_root=timed)], ["--cancel-after-ms", "100"])
    assert cancelled["status"] == "DAG_HOST_CANCELLED" and cancelled["nodes"]
    assert not cancelled_record.exists() and not (cancelled_store / "manifests" / "c-out.manifest").exists()
    cancelled_log = cancelled_store.parent / "cancel-report.run" / "control" / "supervision-v6.jsonl"
    child_events = [json.loads(line) for line in cancelled_log.read_text().splitlines()
                    if json.loads(line)["node_id"] == "c"]
    assert [event["event"] for event in child_events] == ["ATTEMPT_STARTED", "CANCEL_SENT", "REAPED", "ATTEMPT_FINISHED"]
    assert [event.get("terminal_reason") for event in child_events[2:]] == ["HOST_CANCELLED", "HOST_CANCELLED"]

    # The V5 remote endpoint remains run-only and is validated before the
    # report root, Store or record can be touched. The core fixture uses an
    # in-memory transport; the real V6 TLS redirect workflow was run
    # independently. This sandbox denies AF_INET for that loopback workflow.
    remote_record, remote_report = root / "remote.record", root / "remote-report"
    bad_remote = invoke("logic-mod-artifact-dag-v6", [*args(selected_record=remote_record,
                                                               selected_report=remote_report),
                                                        "--remote-restore", "http://localhost:443"], 2)
    assert json.loads(bad_remote.stderr) == {"error": "LogicModDagFailed"}
    assert not remote_record.exists() and not (root / "remote-report.run").exists()

    # Replay has the six fixed V2 words only: no report access or sidecar writes.
    before = log.read_bytes()
    replay = invoke("logic-mod-artifact-dag-v6", args("replay"))
    assert json.loads(replay.stdout)["action"] == "replay"
    assert log.read_bytes() == before
    fifo = root / "never-opened"; os.mkfifo(fifo, 0o600)
    rejected = invoke("logic-mod-artifact-dag-v6", ["replay", str(fifo), "--store", str(root / "x"),
                                                      "--record", str(root / "missing"), "--report", str(root / "bad")], 2)
    assert json.loads(rejected.stderr) == {"error": "InvalidLogicModDagArguments"}
    assert not (root / "bad.run").exists()

    # Unsafe report topology is rejected before a result record can publish.
    unsafe = root / "unsafe"
    (root / "unsafe.run").symlink_to(root / "target")
    bad_record = root / "unsafe.record"
    unsafe_run = invoke("logic-mod-artifact-dag-v6", args(selected_record=bad_record)[:-2] + ["--report", str(unsafe)], 2)
    assert json.loads(unsafe_run.stderr) == {"error": "LogicModDagFailed"}
    assert not bad_record.exists()

    help_result = subprocess.run([bruce, "--help"], text=True, capture_output=True, timeout=10)
    assert help_result.returncode == 0
    assert "logic-mod-artifact-dag-v6 run" in help_result.stdout
    assert "logic-mod-artifact-dag-v6 replay SPEC_JSON --store LOCAL_STORE --record DAG_RECORD" in help_result.stdout
