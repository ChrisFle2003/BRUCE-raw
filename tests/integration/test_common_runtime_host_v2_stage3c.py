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


def spec_bytes(nodes, receipt_bytes=4096):
    lines = ["BRUCE_DAG_SCHEDULER_SPEC_V3", "dag-id=stage3c",
             f"max-started-nodes={len(nodes)}",
             f"max-total-receipt-bytes={receipt_bytes}", ""]
    for name, dependencies in nodes:
        callback = "join-v1" if "," in dependencies else ("append-v1" if dependencies else "literal-v1")
        lines += [f"node={name}", f"depends={dependencies}",
                  f"callback={callback}", "request-hex=41", ""]
    return "\n".join(lines).encode()


def read_events(bundle):
    return [json.loads(line) for line in
            Path(str(bundle) + ".run/control/run-log.jsonl").read_text().splitlines()]


class CommonRuntimeHostV2Stage3CTests(unittest.TestCase):
    def prepare(self, nodes, retry=2, workers=1, receipt_bytes=4096, extra=()):
        directory = tempfile.TemporaryDirectory(prefix="bruce-host-v2-stage3c-")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        spec = root / "workflow.spec"
        bundle = root / "bundle"
        spec.write_bytes(spec_bytes(nodes, receipt_bytes))
        command = [BIN, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle),
                   "--workers", str(workers), "--retry", str(retry), *extra]
        return command, bundle

    def execute(self, nodes, **options):
        command, bundle = self.prepare(nodes, **options)
        process = subprocess.run(command, capture_output=True, timeout=20)
        self.assertTrue(bundle.exists(), process.stderr)
        return process, json.loads(bundle.read_text()), read_events(bundle), bundle

    def assert_inspector(self, bundle, report):
        inspected = subprocess.run([BIN, "dag-scheduler-v5", "inspect", str(bundle)],
                                   capture_output=True, timeout=15)
        self.assertEqual(inspected.returncode, 0, inspected.stderr)
        self.assertEqual(inspected.stderr, b"")
        metadata = json.loads(inspected.stdout)
        self.assertEqual(metadata["status"], "VALID")
        self.assertEqual(metadata["run_status"], report["status"])
        self.assertTrue(metadata["run_finished"])
        histories = [attempt for receipt in report.get("receipts", [report])
                     for attempt in receipt["attempts"]]
        self.assertEqual(len(metadata["attempts"]), len(histories))
        inspected_attempts = {attempt["attempt_id"]: attempt for attempt in metadata["attempts"]}
        for attempt in histories:
            observed = inspected_attempts[attempt["attempt_id"]]
            self.assertEqual(observed["accepted"], attempt["accepted"])
            self.assertEqual(observed["artifact_verified"], attempt["accepted"])

    def assert_log(self, events, status):
        self.assertEqual([event["seq"] for event in events], list(range(1, len(events) + 1)))
        self.assertEqual(events[-1]["event"], "RUN_FINISHED")
        self.assertEqual(events[-1]["status"], status)
        # The shared host publishes starts as RUNNING, also on retry routes.
        self.assertTrue(all(event["status"] == "RUNNING" for event in events
                            if event["event"] == "ATTEMPT_STARTED"))

    def test_single_retry_success_keeps_history_and_private_attempt_directories(self):
        process, report, events, bundle = self.execute([("crash_once", "")])
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(report["status"], "READY")
        self.assertEqual(report["reference"], "artifact/crash_once")
        self.assertEqual([attempt["attempt_id"] for attempt in report["attempts"]],
                         ["a-1", "a-1-2"])
        self.assertEqual([attempt["failure_code"] for attempt in report["attempts"]],
                         ["PROCESS_CRASHED", "READY"])
        self.assertEqual([attempt["accepted"] for attempt in report["attempts"]], [False, True])
        for attempt in ("a-1", "a-1-2"):
            self.assertTrue(Path(str(bundle) + f".run/attempts/{attempt}/output").is_dir())
        self.assertEqual([event["attempt_id"] for event in events
                          if event["event"] == "ARTIFACT_ACCEPTED"], ["a-1-2"])
        self.assertNotIn(b"payload_bytes", process.stdout)
        self.assert_log(events, "READY")
        self.assert_inspector(bundle, report)

    def test_multi_retry_success_preserves_dependencies_and_node_start_budget(self):
        for workers in (1, 2):
            with self.subTest(workers=workers):
                process, report, events, bundle = self.execute(
                    [("crash_once", ""), ("protocol_once", ""),
                     ("z", "crash_once,protocol_once")], workers=workers)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertEqual(report["status"], "READY")
                receipts = report["receipts"]
                self.assertEqual([len(item["attempts"]) for item in receipts], [2, 2, 1])
                self.assertEqual(receipts[1]["attempts"][0]["failure_code"], "PROTOCOL_ERROR")
                z_start = next(index for index, event in enumerate(events)
                               if event["event"] == "ATTEMPT_STARTED" and
                               event["attempt_id"] == "a-3-1")
                self.assertEqual(len([event for event in events[:z_start]
                                      if event["event"] == "ARTIFACT_ACCEPTED"]), 2)
                self.assertTrue(all(item["status"] == "READY" for item in receipts))
                self.assert_log(events, "READY")
                self.assert_inspector(bundle, report)

    def test_retry_exhaustion_blocks_dependents_and_finishes_failed(self):
        for nodes in ([('always_crash', '')],
                      [('always_crash', ''), ('z', 'always_crash')]):
            with self.subTest(nodes=nodes):
                process, report, events, bundle = self.execute(nodes)
                self.assertEqual(process.returncode, 1, process.stderr)
                self.assertEqual(report["status"], "FAILED")
                receipt = report.get("receipts", [report])[0]
                self.assertEqual(len(receipt["attempts"]), 3)
                self.assertTrue(all(item["failure_code"] == "PROCESS_CRASHED"
                                    for item in receipt["attempts"]))
                self.assertFalse(any(event["event"] == "ARTIFACT_ACCEPTED" for event in events))
                if len(nodes) > 1:
                    self.assertEqual(report["receipts"][1]["status"], "BLOCKED")
                self.assert_log(events, "FAILED")
                self.assert_inspector(bundle, report)

    def test_nonretryable_failure_and_canonical_output_limit_are_final(self):
        for nodes, receipt_bytes in (([("failed", "")], 4096),
                                     ([("crash_once", "")], 1),
                                     ([("crash_once", ""), ("z", "crash_once")], 1)):
            with self.subTest(nodes=nodes, receipt_bytes=receipt_bytes):
                process, report, events, bundle = self.execute(nodes, receipt_bytes=receipt_bytes)
                self.assertEqual(process.returncode, 1, process.stderr)
                receipt = report.get("receipts", [report])[0]
                self.assertEqual(len(receipt["attempts"]), 1 if nodes[0][0] == "failed" else 2)
                self.assertEqual(receipt["failure_code"],
                                 "FAILED" if nodes[0][0] == "failed" else "OUTPUT_LIMIT_EXCEEDED")
                self.assertFalse(any(event["event"] == "ARTIFACT_ACCEPTED" for event in events))
                self.assert_log(events, "FAILED")
                self.assert_inspector(bundle, report)

    def test_retry_zero_does_not_launch_second_attempt(self):
        for nodes in ([('crash_once', '')], [('crash_once', ''), ('z', 'crash_once')]):
            process, report, events, _ = self.execute(nodes, retry=0)
            self.assertEqual(process.returncode, 1, process.stderr)
            self.assertEqual(len(report.get("receipts", [report])[0]["attempts"]), 1)
            self.assertEqual(sum(event["event"] == "ATTEMPT_STARTED" for event in events), 1)
            self.assert_log(events, "FAILED")

    def cancelled(self, node, attempt, *, pause_for_late_result=False, multi=False):
        nodes = [(node, "")]
        if multi:
            nodes.append(("z", node))
        command, bundle = self.prepare(nodes)
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stopped = False
        try:
            marker = Path(str(bundle) + f".run/attempts/{attempt}/output/fixture-started")
            deadline = time.monotonic() + 10
            while not marker.exists():
                self.assertIsNone(process.poll(), "host finished before controlled worker start")
                self.assertLess(time.monotonic(), deadline, "worker start timeout")
                time.sleep(0.001)
            if pause_for_late_result:
                os.kill(process.pid, signal.SIGSTOP)
                stopped = True
                host_status = Path(f"/proc/{process.pid}/status")
                while "State:\tT" not in host_status.read_text():
                    self.assertLess(time.monotonic(), deadline, "host stop timeout")
                    time.sleep(0.001)
                children = Path(f"/proc/{process.pid}/task/{process.pid}/children")
                pids = [int(value) for value in children.read_text().split()]
                while any(Path(f"/proc/{pid}/status").exists() and
                          "State:\tZ" not in Path(f"/proc/{pid}/status").read_text()
                          for pid in pids):
                    self.assertLess(time.monotonic(), deadline, "worker exit timeout")
                    time.sleep(0.001)
            cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                    capture_output=True, timeout=10)
            self.assertEqual(cancel.returncode, 0, cancel.stderr)
            if stopped:
                os.kill(process.pid, signal.SIGCONT)
                stopped = False
            _, stderr = process.communicate(timeout=20)
            self.assertEqual(process.returncode, 1, stderr)
            report, events = json.loads(bundle.read_text()), read_events(bundle)
            self.assertEqual(report["status"], "CANCELLED")
            self.assertFalse(any(event["event"] == "ARTIFACT_ACCEPTED" for event in events))
            receipt = report.get("receipts", [report])[0]
            self.assertEqual(receipt["status"], "CANCELLED")
            self.assertIsNone(receipt["reference"])
            self.assertTrue(all(not item["accepted"] for item in receipt["attempts"]))
            self.assertEqual(sum(event["event"] == "ATTEMPT_STARTED" for event in events),
                             1 if node == "cancel_before_retry" else 2)
            self.assert_log(events, "CANCELLED")
            self.assert_inspector(bundle, report)
        finally:
            if stopped and process.poll() is None:
                os.kill(process.pid, signal.SIGCONT)
            if process.poll() is None:
                process.kill()
                process.communicate(timeout=5)

    def test_cancel_before_retry_prevents_second_attempt(self):
        self.cancelled("cancel_before_retry", "a-1", pause_for_late_result=True)

    def test_cancel_during_retry_stops_future_attempts(self):
        self.cancelled("slow_retry", "a-1-2")

    def test_late_ready_from_retry_is_never_accepted(self):
        self.cancelled("slow_retry", "a-1-2", pause_for_late_result=True)
        self.cancelled("slow_retry", "a-1-2", pause_for_late_result=True, multi=True)

    def test_cancel_between_retries_keeps_attempt_identity_and_history(self):
        for multi in (False, True):
            observed = False
            for _ in range(30):
                nodes = [("cancel_before_retry", "")]
                if multi:
                    nodes.append(("z", "cancel_before_retry"))
                command, bundle = self.prepare(nodes)
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                stopped = False
                try:
                    deadline = time.monotonic() + 10
                    log = Path(str(bundle) + ".run/control/run-log.jsonl")
                    while not log.exists() or '"event":"RESULT_RECEIVED"' not in log.read_text():
                        self.assertIsNone(process.poll())
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.0001)
                    os.kill(process.pid, signal.SIGSTOP)
                    stopped = True
                    while "State:\tT" not in Path(f"/proc/{process.pid}/status").read_text():
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.0001)
                    if log.read_text().count('"event":"ATTEMPT_STARTED"') != 1:
                        continue
                    cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                            capture_output=True, timeout=10)
                    self.assertEqual(cancel.returncode, 0, cancel.stderr)
                    os.kill(process.pid, signal.SIGCONT)
                    stopped = False
                    _, stderr = process.communicate(timeout=20)
                    self.assertEqual(process.returncode, 1, stderr)
                    report = json.loads(bundle.read_text())
                    receipt = report.get("receipts", [report])[0]
                    self.assertEqual(report["status"], "CANCELLED")
                    self.assertEqual(receipt["status"], "CANCELLED")
                    self.assertEqual([attempt["attempt_id"] for attempt in receipt["attempts"]],
                                     ["a-1-1" if multi else "a-1"])
                    if multi:
                        self.assertEqual(receipt["attempt_id"], "a-1-1")
                    events = read_events(bundle)
                    self.assertEqual(sum(event["event"] == "ATTEMPT_STARTED" for event in events), 1)
                    self.assert_inspector(bundle, report)
                    observed = True
                    break
                finally:
                    if stopped and process.poll() is None:
                        os.kill(process.pid, signal.SIGCONT)
                    if process.poll() is None:
                        process.kill()
                        process.communicate(timeout=5)
            self.assertTrue(observed, "could not pause host between retry attempts")

    def test_cancel_already_projected_pending_ready_and_failure(self):
        for pending_node in ("crash_once", "z_ready"):
            command, bundle = self.prepare([("a_slow", ""), (pending_node, "")], workers=2)
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + 10
                log = Path(str(bundle) + ".run/control/run-log.jsonl")
                while not log.exists() or not any(event["event"] == "RESULT_RECEIVED" and
                                                 event.get("attempt_id") == "a-2-1"
                                                 for event in read_events(bundle)):
                    self.assertIsNone(process.poll())
                    self.assertLess(time.monotonic(), deadline)
                    time.sleep(0.001)
                cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                        capture_output=True, timeout=10)
                self.assertEqual(cancel.returncode, 0, cancel.stderr)
                _, stderr = process.communicate(timeout=20)
                self.assertEqual(process.returncode, 1, stderr)
                report, events = json.loads(bundle.read_text()), read_events(bundle)
                self.assertEqual(report["status"], "CANCELLED")
                self.assertTrue(all(receipt["status"] == "CANCELLED" for receipt in report["receipts"]))
                self.assertEqual(sum(event["event"] == "ATTEMPT_STARTED" for event in events), 2)
                self.assertFalse(any(event["event"] == "ARTIFACT_ACCEPTED" for event in events))
                self.assert_inspector(bundle, report)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)


if __name__ == "__main__":
    unittest.main()
