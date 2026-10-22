"""Foreground JSONL boundary for Runtime Orchestrator V1."""
import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

bruce, exporter, worker, timed_worker, _compiler = map(os.path.abspath, sys.argv[1:])
limits = ["--max-queued-runs", "1", "--max-active-runs", "1",
          "--max-reserved-parallel-nodes", "4", "--max-reserved-started-workers", "4",
          "--max-reserved-payload-bytes", "4096"]

def reply(process, payload):
    process.stdin.write(json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n")
    process.stdin.flush()
    line = process.stdout.readline()
    assert line, process.stderr.read()
    return json.loads(line)

def forbidden(value):
    rendered = json.dumps(value)
    for text in ("/tmp/", "pid", "pgid", "http", "credential", "frame", "signature", "telemetry"):
        assert text not in rendered.lower(), (text, rendered)

# All malformed public input is contained in one redacted reply and does not
# cause a path open. The missing fifth host limit is rejected at argv parsing.
for source in ("not-json", "{\"action\":\"status\"}", "{\"action\":\"status\",\"action\":\"status\"}", "{\"action\":\"unknown\"}", "{" + "x" * 70000 + "}"):
    process = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process.stdin.write(source + "\n"); process.stdin.close()
    output = process.stdout.readline(); process.wait(timeout=10)
    assert process.returncode == 0
    parsed = json.loads(output)
    assert parsed["ok"] is False and parsed["error"] in ("INVALID_COMMAND", "UNKNOWN_ACTION")
    forbidden(parsed)

missing = subprocess.run([bruce, "runtime-orchestrator-v1", "serve", *limits[:-2]], text=True,
                         capture_output=True, timeout=10)
assert missing.returncode == 2 and json.loads(missing.stderr) == {"error": "InvalidRuntimeOrchestratorArguments"}

# Streaming framing is bounded even when a single read carries an oversized
# line and a following command. The oversized command gets one reply; its
# remaining bytes are discarded through the delimiter; the final unterminated
# command is still processed at EOF.
process = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
stdout, stderr = process.communicate("x" * 70000 + "\n{\"action\":\"unknown\"}", timeout=10)
assert process.returncode == 0, stderr
assert [json.loads(line) for line in stdout.splitlines()] == [
    {"contract": "runtime-orchestrator-v1", "ok": False, "error": "INVALID_COMMAND"},
    {"contract": "runtime-orchestrator-v1", "ok": False, "error": "UNKNOWN_ACTION"},
]
# Strict canonical JSON rejects whitespace and alternate serializations.
process = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
stdout, stderr = process.communicate('{"action": "unknown"}\n', timeout=10)
assert process.returncode == 0 and json.loads(stdout) == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "INVALID_COMMAND"}

with tempfile.TemporaryDirectory(prefix="bruce-orchestrator-v1-") as temporary:
    root = pathlib.Path(temporary)
    subprocess.run([exporter, timed_worker, temporary, "export"], check=True)
    report = root / "report"; report.mkdir(mode=0o700)
    spec = root / "dag.json"
    # The child uses the V5 timed fixture (500 ms for this input), keeping the
    # first submitted job active while the queue-full hostile-path request is
    # parsed. It proves that queue capacity is checked before opening paths.
    spec.write_text(json.dumps({"schema_version": 2, "nodes": [
        {"id": "a", "package": str(root / "output.brucemod"), "trust_store": str(root / "trust.txt"),
         "input": "3/4 hoch 2", "predecessors": [], "output_manifest": "a-out"},
        {"id": "c", "package": str(root / "combined.brucemod"), "trust_store": str(root / "trust.txt"),
         "input": "1/2 hoch 2", "predecessors": ["a"], "input_manifest": "c-input", "output_manifest": "c-out"}
    ]}))
    def submit(extra=None):
        request = {"action": "submit", "spec": str(spec), "store": str(root / "store"), "record": str(root / "run.record"), "report": str(report),
                   "max_parallel_nodes": 1, "max_total_wall_ms": 60000, "max_total_payload_bytes": 4096,
                   "max_started_workers": 2, "reservation_max_parallel_nodes": 1,
                   "reservation_max_total_wall_ms": 60000, "reservation_max_total_payload_bytes": 4096,
                   "reservation_max_started_workers": 2}
        if extra: request.update(extra)
        return request

    process = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # Parser-before-access: duplicate/unknown fields and reservation mismatch remain redacted.
    bad = reply(process, {"action": "submit", "spec": str(root / "never-open"), "store": "x", "report": "x",
                          "max_parallel_nodes": 1, "max_total_wall_ms": 1, "max_total_payload_bytes": 0,
                          "max_started_workers": 1, "reservation_max_parallel_nodes": 2,
                          "reservation_max_total_wall_ms": 1, "reservation_max_total_payload_bytes": 0,
                          "reservation_max_started_workers": 1})
    assert bad == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "INVALID_COMMAND"}
    forbidden(bad)
    missing_record = submit(); del missing_record["record"]
    assert reply(process, missing_record) == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "INVALID_COMMAND"}
    accepted = reply(process, submit())
    assert accepted["ok"] is True and accepted["action"] == "submit", accepted
    run_id = accepted["status"]["run_id"]
    assert run_id.startswith("orv1-") and accepted["status"]["reservation"]["max_total_payload_bytes"] == 4096
    assert {"submit_seq", "start_seq", "finish_seq", "v6_accepted", "v6_status", "dag_id", "sidecar_ref"} <= set(accepted["status"])
    assert accepted["status"]["sidecar_ref"] == "sidecar-v1-" + run_id
    forbidden(accepted)
    queued = reply(process, submit())
    assert queued["ok"] is True and queued["status"]["state"] == "QUEUED", queued
    hostile = submit({"spec": str(root / "never-open"), "store": str(root / "never-store"), "report": str(root / "never-report")})
    full = reply(process, hostile)
    assert full == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "QUEUE_FULL"}, full
    assert not (root / "never-store").exists() and not (root / "never-report").exists()
    forbidden(full)
    state = reply(process, {"action": "status", "run_id": run_id})
    assert state["ok"] is True and state["action"] == "status" and state["status"]["run_id"] == run_id
    forbidden(state)
    # Admission diagnostics only reads the accepted in-memory host status. It
    # accepts no plan/path field and does not relay the native status or V6
    # completion metadata.
    admission = reply(process, {"action": "admission-status", "run_id": run_id})
    assert admission["ok"] is True and admission["action"] == "admission-status"
    diagnostic = admission["admission_status"]
    assert diagnostic["schema"] == "admission-diagnostics-v1"
    assert diagnostic["source"] == "runtime-orchestrator-v1"
    assert diagnostic["state"] in ("ADMISSION_RESERVED", "ACTIVE")
    assert diagnostic["reason"] == "NONE"
    assert set(diagnostic) == {"schema", "source", "state", "reason", "enqueue_seq", "submit_seq", "start_seq", "finish_seq", "reservation"}
    assert "run_id" not in diagnostic and "v6_status" not in diagnostic and "dag_id" not in diagnostic
    forbidden(admission)
    unknown_admission = reply(process, {"action": "admission-status", "run_id": "orv1-999"})
    assert unknown_admission == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "RUN_NOT_FOUND"}
    no_io = reply(process, {"action": "admission-status", "run_id": "unsafe/path"})
    assert no_io == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "INVALID_COMMAND"}
    assert not (root / "unsafe").exists()
    # This is an admission handshake, not a timing assumption: cancel only
    # after V6 has recorded that the timed dependent worker actually started.
    sidecar = pathlib.Path(str(report) + ".run") / "control" / "supervision-v6.jsonl"
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if sidecar.is_file():
            events = [json.loads(line) for line in sidecar.read_text().splitlines()]
            if any(event.get("node_id") == "c" and event.get("event") == "ATTEMPT_STARTED" for event in events):
                break
        time.sleep(0.01)
    else:
        raise AssertionError("timed child was not admitted")
    # Cancellation is by opaque id only. The controller, never this CLI,
    # owns V6 draining and resource release after this acknowledgement.
    cancelled = reply(process, {"action": "cancel", "run_id": run_id})
    assert cancelled["ok"] is True and cancelled["action"] == "cancel"
    forbidden(cancelled)
    # The queued peer must not start after the active cancellation; cancelling
    # it also lets EOF prove that no canonical record is published for either.
    queued_cancel = reply(process, {"action": "cancel", "run_id": queued["status"]["run_id"]})
    assert queued_cancel["ok"] is True and queued_cancel["status"]["state"] == "CANCELLED"
    unknown = reply(process, {"action": "cancel", "run_id": "orv1-999"})
    assert unknown == {"contract": "runtime-orchestrator-v1", "ok": False, "error": "RUN_NOT_FOUND"}
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        events = [json.loads(line) for line in sidecar.read_text().splitlines()]
        child_events = [event for event in events if event.get("node_id") == "c"]
        if child_events and child_events[-1].get("event") == "ATTEMPT_FINISHED":
            break
        time.sleep(0.01)
    else:
        raise AssertionError("cancelled V6 child did not finish")
    assert [event["event"] for event in child_events] == ["ATTEMPT_STARTED", "CANCEL_SENT", "REAPED", "ATTEMPT_FINISHED"]
    assert [event.get("terminal_reason") for event in child_events[2:]] == ["HOST_CANCELLED", "HOST_CANCELLED"]
    # ATTEMPT_FINISHED is written before V6 returns to the controller.  Poll
    # public status until the controller has joined and published its terminal
    # result; this deliberately tolerates a delayed sidecar fsync.
    terminal = None
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        observed = reply(process, {"action": "status", "run_id": run_id})
        if observed["status"]["state"] == "CANCELLED":
            terminal = observed
            break
        time.sleep(0.01)
    assert terminal is not None, "controller did not publish terminal cancel"
    assert terminal["status"]["reason"] == "CANCELLED_ACTIVE"
    # The controller has no sidecar reader or repair path.  Hostile
    # replacement after its terminal result must not alter it, recreate the
    # file, or launch a worker.
    poison = root / "sidecar-poison"
    poison.write_text("not-a-v6-sidecar\n")
    sidecar.unlink()
    sidecar.symlink_to(poison)
    after_tamper = reply(process, {"action": "status", "run_id": run_id})
    assert after_tamper["status"]["state"] == "CANCELLED"
    assert after_tamper["status"]["reason"] == "CANCELLED_ACTIVE"
    assert sidecar.is_symlink() and sidecar.read_text() == "not-a-v6-sidecar\n"
    forbidden(terminal); forbidden(after_tamper)
    assert not (root / "store" / "manifests" / "c-out.manifest").exists()
    assert not (root / "run.record").exists()
    process.stdin.close()
    process.wait(timeout=20)
    assert process.returncode == 0, process.stderr.read()

    # A foreground host has no restart protocol.  Termination during a V6
    # attempt is deliberately tested only at the existing V6 host/child
    # boundary: the next host cannot reconstruct the opaque in-memory job.
    report2 = root / "report-host-exit"; report2.mkdir(mode=0o700)
    exiting = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    exiting_reply = reply(exiting, submit({"report": str(report2), "record": str(root / "host-exit.record")}))
    exiting_id = exiting_reply["status"]["run_id"]
    exit_sidecar = pathlib.Path(str(report2) + ".run") / "control" / "supervision-v6.jsonl"
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if exit_sidecar.is_file() and any(json.loads(line).get("event") == "ATTEMPT_STARTED"
                                          for line in exit_sidecar.read_text().splitlines()):
            break
        time.sleep(0.01)
    else:
        raise AssertionError("host-exit child was not admitted")
    exiting.send_signal(signal.SIGTERM)
    exiting.wait(timeout=10)
    assert exiting.returncode == -signal.SIGTERM
    # A new in-memory host neither imports the old queue nor repairs/replays
    # old sidecars or records.  It knows no run id from the terminated host.
    fresh = subprocess.Popen([bruce, "runtime-orchestrator-v1", "serve", *limits], text=True,
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    missing_after_exit = reply(fresh, {"action": "status", "run_id": exiting_id})
    assert missing_after_exit["ok"] is False
    forbidden(missing_after_exit)
    fresh.stdin.close(); fresh.wait(timeout=10)

# No daemon/direct worker spelling becomes a valid host surface.
unknown = subprocess.run([bruce, "runtime-orchestrator-v1", "worker"], text=True, capture_output=True, timeout=10)
assert unknown.returncode == 2 and json.loads(unknown.stderr) == {"error": "InvalidRuntimeOrchestratorArguments"}
