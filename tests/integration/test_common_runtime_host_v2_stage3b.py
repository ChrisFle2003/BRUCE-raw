import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


def spec_bytes(max_started=4, max_receipt=65536, large=False, failing=False):
    payload = ("41" * 10000) if large else "41"
    root_b = "callback=literal-v1\nrequest-hex=" + ("42" * 10000 if large else "42")
    return (f"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
            f"dag-id=stage3b\nmax-started-nodes={max_started}\n"
            f"max-total-receipt-bytes={max_receipt}\n\n"
            f"node=a\ndepends=\ncallback=literal-v1\nrequest-hex={payload}\n\n"
            f"node=b\ndepends=\n{root_b}\n\n"
            f"node=c\ndepends=a,b\ncallback=join-v1\nrequest-hex=43\n").encode()


def read_events(bundle):
    path = Path(str(bundle) + ".run/control/run-log.jsonl")
    return [json.loads(line) for line in path.read_text().splitlines()]


class CommonRuntimeHostV2Stage3BTests(unittest.TestCase):
    def execute(self, spec, workers=2, *extra):
        directory = tempfile.TemporaryDirectory(prefix="bruce-host-v2-stage3b-")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        spec_path = root / "workflow.spec"
        bundle = root / "bundle"
        spec_path.write_bytes(spec)
        process = subprocess.run(
            [BIN, "dag-scheduler-v5", "execute", str(spec_path), "--output",
             str(bundle), "--workers", str(workers), *extra],
            capture_output=True, timeout=30)
        return process, json.loads(bundle.read_text()), read_events(bundle)

    def assert_common_report(self, process, report, workers):
        self.assertEqual(report["contract"], "dag-scheduler-v5-cli-v1")
        self.assertEqual(report["action"], "execute")
        self.assertEqual(report["workers"], workers)
        self.assertEqual(len(report["receipts"]), 3)
        self.assertNotIn(b"payload_bytes", process.stdout)

    def test_two_roots_join_and_parallel_worker_limit(self):
        process, report, events = self.execute(spec_bytes(), 2)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assert_common_report(process, report, 2)
        self.assertEqual(report["status"], "READY")
        self.assertTrue(all(receipt["status"] == "READY" for receipt in report["receipts"]))
        started = [event for event in events if event["event"] == "ATTEMPT_STARTED"]
        first_exit = next(index for index, event in enumerate(events)
                          if event["event"] == "PROCESS_EXITED")
        self.assertEqual(len([event for event in events[:first_exit]
                              if event["event"] == "ATTEMPT_STARTED"]), 2)
        self.assertEqual(started[0]["attempt_id"], "a-1-1")
        self.assertEqual(started[1]["attempt_id"], "a-2-1")
        self.assertEqual(report["receipts"][2]["node_id"], "c")
        self.assertEqual(report["receipts"][2]["attempts"][0]["accepted"], True)
        self.assertEqual([event["seq"] for event in events], list(range(1, len(events) + 1)))
        self.assertEqual(events[-1]["event"], "RUN_FINISHED")

    def test_workers_one_is_deterministic_and_max_started_is_separate(self):
        process, report, events = self.execute(spec_bytes(), 1)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(report["status"], "READY")
        self.assertEqual([event["attempt_id"] for event in events
                          if event["event"] == "ATTEMPT_STARTED"],
                         ["a-1-1", "a-2-1", "a-3-1"])
        process, report, _ = self.execute(spec_bytes(max_started=2), 2)
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertEqual(report["status"], "FAILED")
        self.assertEqual(report["receipts"][2]["result_code"], "START_BUDGET_EXCEEDED")
        self.assertIsNone(report["receipts"][2]["reference"])

    def test_failed_root_blocks_join_and_keeps_metadata_only_report(self):
        process, report, events = self.execute(spec_bytes(max_receipt=1), 2)
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertEqual(report["status"], "FAILED")
        self.assertTrue(any(receipt["failure_code"] == "OUTPUT_LIMIT_EXCEEDED"
                            for receipt in report["receipts"]))
        self.assertEqual(report["receipts"][2]["status"], "BLOCKED")
        self.assertIsNone(report["receipts"][2]["reference"])
        self.assertNotIn(b"41" * 20, process.stdout)
        self.assertEqual(events[-1]["event"], "RUN_FINISHED")

    def test_external_cancel_stops_multiple_workers_without_acceptance(self):
        for _ in range(20):
            with tempfile.TemporaryDirectory(prefix="bruce-host-v2-stage3b-cancel-") as directory:
                root = Path(directory)
                spec_path = root / "workflow.spec"
                bundle = root / "bundle"
                spec_path.write_bytes(spec_bytes(large=True))
                process = subprocess.Popen(
                    [BIN, "dag-scheduler-v5", "execute", str(spec_path), "--output",
                     str(bundle), "--workers", "2"], stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE)
                stopped = False
                try:
                    log = Path(str(bundle) + ".run/control/run-log.jsonl")
                    children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
                    deadline = time.monotonic() + 10
                    worker_pids = []
                    while time.monotonic() < deadline:
                        if process.poll() is not None:
                            break
                        if log.exists() and log.read_text().count('"event":"ATTEMPT_STARTED"') >= 2:
                            if children.exists():
                                worker_pids = [int(value) for value in children.read_text().split()]
                            break
                        time.sleep(0.001)
                    if process.poll() is not None or not log.exists():
                        if process.poll() is not None:
                            process.communicate(timeout=5)
                        continue
                    os.kill(process.pid, signal.SIGSTOP)
                    stopped = True
                    deadline = time.monotonic() + 2
                    while time.monotonic() < deadline and worker_pids:
                        if all((not Path(f"/proc/{pid}/status").exists()) or
                               "State:\tZ" in Path(f"/proc/{pid}/status").read_text()
                               for pid in worker_pids):
                            break
                        time.sleep(0.001)
                    cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                            capture_output=True, timeout=10)
                    self.assertEqual(cancel.returncode, 0, cancel.stderr)
                    os.kill(process.pid, signal.SIGCONT)
                    stopped = False
                    stdout, stderr = process.communicate(timeout=20)
                    report = json.loads(bundle.read_text())
                    events = read_events(bundle)
                    self.assertEqual(process.returncode, 1, stderr)
                    self.assertEqual(report["status"], "CANCELLED")
                    self.assertEqual([report["receipts"][index]["attempt_id"] for index in (0, 1)],
                                     ["a-1-1", "a-2-1"])
                    self.assertTrue(all(report["receipts"][index]["status"] == "CANCELLED"
                                        for index in (0, 1)))
                    self.assertTrue(any(event["event"] == "CANCEL_REQUESTED" for event in events))
                    self.assertFalse(any(event["event"] == "ARTIFACT_ACCEPTED" for event in events))
                    self.assertTrue(all(receipt["reference"] is None for receipt in report["receipts"]))
                    self.assertNotIn(b"payload_bytes", stdout)
                    return
                finally:
                    if stopped and process.poll() is None:
                        os.kill(process.pid, signal.SIGCONT)
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)
        self.fail("could not pause a host with two workers started")


if __name__ == "__main__":
    unittest.main()
