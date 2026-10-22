import hashlib
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
ACCEPTANCE_FIXTURE = str(Path(sys.argv.pop(1)).resolve())
SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=stage3a-single\n"
    b"max-started-nodes=1\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=4142\n"
)
CANCEL_SPEC = SPEC.replace(b"max-total-receipt-bytes=4096",
                           b"max-total-receipt-bytes=65536").replace(
                               b"request-hex=4142", b"request-hex=" + b"41" * 30000)
EVENTS_READY = ["RUN_CREATED", "ATTEMPT_STARTED", "STATUS", "PROCESS_EXITED",
                "RESULT_RECEIVED", "ARTIFACT_ACCEPTED", "RUN_FINISHED"]


def events_at(bundle):
    log = Path(str(bundle) + ".run/control/run-log.jsonl")
    return [json.loads(line) for line in log.read_text().splitlines()]


class CommonRuntimeHostV2Stage3Tests(unittest.TestCase):
    def assert_log(self, events, expected, terminal):
        self.assertEqual([event["event"] for event in events], expected)
        self.assertEqual([event["seq"] for event in events],
                         list(range(1, len(events) + 1)))
        self.assertEqual(events[-1]["status"], terminal)
        self.assertEqual(events[1]["status"], "RUNNING")
        self.assertEqual({event["attempt_id"] for event in events
                          if "attempt_id" in event}, {"a-1"})

    def run_execute(self, *options):
        directory = tempfile.TemporaryDirectory(prefix="bruce-host-v2-stage3-")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        spec = root / "workflow.spec"
        bundle = root / "bundle"
        spec.write_bytes(SPEC)
        result = subprocess.run([BIN, "dag-scheduler-v5", "execute", str(spec),
                                 "--output", str(bundle), *options],
                                capture_output=True, timeout=20)
        return result, json.loads(bundle.read_text()), events_at(bundle)

    def test_single_node_host_v2_preserves_v5_contract(self):
        process, report, events = self.run_execute()
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(set(report), {
            "contract", "action", "spec_digest", "dag_id", "node_id", "adapter_id",
            "request_digest", "retry", "workers", "budget", "status", "failure_code",
            "result_code", "reference", "payload_digest", "payload_size", "usage", "attempts",
        })
        self.assertEqual(report["contract"], "dag-scheduler-v5-cli-v1")
        self.assertEqual(report["action"], "execute")
        self.assertEqual(report["status"], "READY")
        self.assertEqual(report["reference"], "artifact/a")
        self.assertEqual(report["payload_digest"], hashlib.sha256(b"AB").hexdigest())
        self.assertEqual(report["payload_size"], 2)
        self.assertEqual(report["attempts"][0]["attempt_id"], "a-1")
        self.assertTrue(report["attempts"][0]["accepted"])
        self.assertNotIn(b"AB", process.stdout)
        self.assertNotIn(b"payload_bytes", process.stdout)
        self.assert_log(events, EVENTS_READY, "READY")
        accepted = events[-2]
        self.assertEqual(accepted["reference"], "attempts/a-1/output/result.bin")
        self.assertEqual(accepted["digest"], report["payload_digest"])
        self.assertEqual(accepted["size_bytes"], report["payload_size"])

    def test_single_node_timeout_has_no_artifact_acceptance(self):
        process, report, events = self.run_execute("--max-wall-ms", "1")
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertEqual(report["failure_code"], "TIMEOUT")
        self.assertIsNone(report["reference"])
        self.assertIsNone(report["payload_digest"])
        self.assertEqual(report["payload_size"], 0)
        self.assertEqual(report["attempts"][0]["failure_code"], "TIMEOUT")
        self.assertFalse(report["attempts"][0]["accepted"])
        self.assert_log(events, [event for event in EVENTS_READY
                                 if event != "ARTIFACT_ACCEPTED"], "FAILED")

    def test_external_cancel_at_observable_acceptance_boundary(self):
        with tempfile.TemporaryDirectory(prefix="bruce-host-v2-boundary-") as directory:
            root = Path(directory) / "run"
            process = subprocess.Popen(
                [ACCEPTANCE_FIXTURE, "boundary", str(root)], stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                self.assertEqual(process.stdout.readline(), "ACCEPTANCE_BOUNDARY_V1\n")
                cancel = subprocess.run(
                    [ACCEPTANCE_FIXTURE, "cancel", str(root)], capture_output=True,
                    text=True, timeout=10)
                self.assertEqual(cancel.returncode, 0, cancel.stderr)
                self.assertEqual(cancel.stdout, "REQUESTED\n")
                stdout, stderr = process.communicate("continue\n", timeout=10)
                self.assertEqual(process.returncode, 1, stderr)
                self.assertEqual(json.loads(stdout), {
                    "disposition": "CANCEL",
                    "host_exit_code": 1,
                    "append_cancel_requested": True,
                    "append_artifact_accepted": False,
                    "has_reference": False,
                    "has_digest": False,
                    "payload_size": 0,
                })
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)

    @staticmethod
    def cancel_stress_case(index):
        with tempfile.TemporaryDirectory(
                prefix=f"bruce-host-v2-cancel-stress-{index}-") as directory:
            root = Path(directory)
            spec = root / "workflow.spec"
            bundle = root / "bundle"
            spec.write_bytes(CANCEL_SPEC)
            process = subprocess.Popen(
                [BIN, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                log = Path(str(bundle) + ".run/control/run-log.jsonl")
                deadline = time.monotonic() + 10
                finished_before_cancel = False
                while time.monotonic() < deadline:
                    if (log.exists() and
                            '"event":"ATTEMPT_STARTED"' in log.read_text()):
                        break
                    if process.poll() is not None:
                        finished_before_cancel = True
                        break
                    time.sleep(0.0005)
                else:
                    raise AssertionError("worker start timeout")
                cancel_status = "ALREADY_FINISHED" if finished_before_cancel else None
                cancel_error = b""
                while time.monotonic() < deadline and process.poll() is None:
                    cancel = subprocess.run(
                        [BIN, "dag-scheduler-v5", "cancel", str(bundle)], capture_output=True,
                        timeout=10)
                    if cancel.returncode == 0:
                        cancel_status = json.loads(cancel.stdout)["status"]
                        break
                    cancel_error = cancel.stderr
                    time.sleep(0.0005)
                if cancel_status is None:
                    if process.poll() is None:
                        raise AssertionError(f"cancel failed: {cancel_error!r}")
                    cancel_status = "ALREADY_FINISHED"
                stdout, stderr = process.communicate(timeout=20)
                report = json.loads(bundle.read_text())
                events = events_at(bundle)
                names = [event["event"] for event in events]
                if [event["seq"] for event in events] != list(range(1, len(events) + 1)):
                    raise AssertionError("non-contiguous event sequence")
                if names[-1] != "RUN_FINISHED":
                    raise AssertionError("missing terminal event")
                if report["status"] == "CANCELLED":
                    if process.returncode != 1 or report["failure_code"] != "CANCELLED":
                        raise AssertionError(f"invalid cancelled result: {stderr!r}")
                    if (report["reference"] is not None or
                            report["payload_digest"] is not None or
                            report["payload_size"] != 0 or
                            report["attempts"][0]["accepted"] or
                            "ARTIFACT_ACCEPTED" in names):
                        raise AssertionError("cancelled result retained an accepted artifact")
                elif report["status"] == "READY":
                    # The cancel command can observe an unfinished journal and
                    # still publish after the successful final probe.  That is
                    # the explicitly documented late-marker outcome.
                    if (process.returncode != 0 or not report["attempts"][0]["accepted"] or
                            names.count("ARTIFACT_ACCEPTED") != 1):
                        raise AssertionError("late-marker READY was not durably accepted")
                else:
                    raise AssertionError(f"unexpected terminal status {report['status']}")
                if cancel_status == "ALREADY_FINISHED" and report["status"] != "READY":
                    raise AssertionError("finished cancellation target was not READY")
                if b"payload_bytes" in stdout:
                    raise AssertionError("worker payload framing leaked to stdout")
                return report["status"]
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)

    def test_real_process_cancel_stress_is_semantically_coherent(self):
        cases = int(os.environ.get("BRUCE_CANCEL_STRESS_CASES", "1"))
        concurrency = int(os.environ.get("BRUCE_CANCEL_STRESS_CONCURRENCY", "1"))
        self.assertGreaterEqual(cases, 1)
        self.assertGreaterEqual(concurrency, 1)
        started = time.monotonic()
        with ThreadPoolExecutor(max_workers=concurrency) as executor:
            statuses = list(executor.map(self.cancel_stress_case, range(cases)))
        elapsed = time.monotonic() - started
        print(f"cancel-stress cases={cases} concurrency={concurrency} "
              f"cancelled={statuses.count('CANCELLED')} ready={statuses.count('READY')} "
              f"elapsed_seconds={elapsed:.3f}")


if __name__ == "__main__":
    unittest.main()
