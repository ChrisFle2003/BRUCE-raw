import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


def spec_for(nodes, max_started=None):
    if max_started is None:
        max_started = len(nodes)
    parts = [
        "BRUCE_DAG_SCHEDULER_SPEC_V3",
        "dag-id=abort-workflow",
        f"max-started-nodes={max_started}",
        "max-total-receipt-bytes=4096",
        "",
    ]
    for node, depends, callback, request_hex in nodes:
        parts.extend([
            f"node={node}",
            f"depends={depends}",
            f"callback={callback}",
            f"request-hex={request_hex}",
            "",
        ])
    return ("\n".join(parts)).encode()


SINGLE = spec_for([("a", "", "literal-v1", "414141")])
QUEUED = spec_for([
    ("a", "", "literal-v1", "414141"),
    ("b", "", "literal-v1", "424242"),
    ("c", "", "literal-v1", "434343"),
    ("d", "", "literal-v1", "444444"),
    ("e", "", "literal-v1", "454545"),
    ("f", "", "literal-v1", "464646"),
    ("g", "", "literal-v1", "474747"),
    ("h", "", "literal-v1", "484848"),
    ("tail", "a", "append-v1", "54"),
])
PARALLEL = spec_for([
    ("a", "", "literal-v1", "414141"),
    ("b", "", "literal-v1", "424242"),
    ("c", "a,b", "join-v1", "43"),
])


class DagSchedulerV5AbortWorkflowTests(unittest.TestCase):
    def run_cancelled(self, spec_bytes, workers, *, retry=0, wait_for_attempt=True):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-abort-") as directory:
            root = Path(directory)
            spec = root / "workflow.spec"
            bundle = root / "bundle"
            spec.write_bytes(spec_bytes)
            command = [BIN, "dag-scheduler-v5", "execute", str(spec),
                       "--output", str(bundle), "--workers", str(workers)]
            if retry:
                command += ["--retry", str(retry)]
            process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)
            run_root = Path(str(bundle) + ".run")
            log_path = run_root / "control" / "run-log.jsonl"
            deadline = time.monotonic() + 10
            while not log_path.exists() and time.monotonic() < deadline:
                time.sleep(0.001)
            self.assertTrue(log_path.exists(), "V5 run log was not created")
            if wait_for_attempt:
                while time.monotonic() < deadline:
                    events = [json.loads(line) for line in log_path.read_text().splitlines()]
                    if any(event["event"] == "ATTEMPT_STARTED" for event in events):
                        break
                    time.sleep(0.001)
                else:
                    self.fail("V5 run did not start a worker")
            cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                    capture_output=True, timeout=10)
            stdout, stderr = process.communicate(timeout=15)
            self.assertEqual(cancel.returncode, 0, cancel.stderr.decode())
            self.assertEqual(process.returncode, 1, stderr.decode())
            self.assertTrue(bundle.exists(), stderr.decode())
            report = json.loads(bundle.read_text())
            events = [json.loads(line) for line in log_path.read_text().splitlines()]
            if report["status"] != "CANCELLED":
                evidence = (Path.cwd() / "runs" / "logic-mod-artifact-dag-v5" /
                            "evidence" / f"scheduler-abort-unexpected-{root.name}")
                shutil.copytree(root, evidence)
            inspected = subprocess.run([BIN, "dag-scheduler-v5", "inspect", str(bundle)],
                                       capture_output=True, timeout=10)
            if inspected.returncode != 0:
                evidence = (Path.cwd() / "runs" / "logic-mod-artifact-dag-v5" /
                            "evidence" / f"scheduler-abort-inspect-{root.name}")
                shutil.copytree(root, evidence)
            self.assertEqual(inspected.returncode, 0, inspected.stderr.decode())
            self.assertEqual(inspected.stderr, b"")
            return cancel, report, events, json.loads(inspected.stdout), stdout, stderr

    def assert_cancelled_receipt(self, receipt, *, started=False):
        self.assertEqual(receipt["status"], "CANCELLED")
        self.assertEqual(receipt["failure_code"], "CANCELLED")
        if started:
            self.assertIsNotNone(self.receipt_attempt_id(receipt))
            self.assertEqual(len(receipt["attempts"]), 1)
            self.assertEqual(receipt["attempts"][0]["status"], "CANCELLED")
            self.assertFalse(receipt["attempts"][0]["accepted"])
        else:
            self.assertIsNone(receipt["attempt_id"])
            self.assertEqual(receipt["attempts"], [])
        self.assertIsNone(receipt["reference"])
        self.assertIsNone(receipt["payload_digest"])
        self.assertEqual(receipt["payload_size"], 0)

    @staticmethod
    def receipts_for(report):
        return report.get("receipts", [report])

    @staticmethod
    def receipt_attempt_id(receipt):
        if receipt.get("attempt_id") is not None:
            return receipt["attempt_id"]
        attempts = receipt["attempts"]
        return attempts[0]["attempt_id"] if attempts else None

    def assert_observed_cancel_contract(self, report, events, inspection):
        """Validate the parent-observation contract, not scheduling timing.

        A receipt can remain READY only when ARTIFACT_ACCEPTED was durably
        logged before the host first observed cancellation.  The marker does
        not retroactively invalidate that earlier acceptance.
        """
        self.assertEqual(report["status"], "CANCELLED")
        self.assertEqual(inspection["status"], "VALID")
        self.assertTrue(inspection["run_finished"])
        self.assertEqual(inspection["run_status"], "CANCELLED")

        cancel_positions = [index for index, event in enumerate(events)
                            if event["event"] == "CANCEL_REQUESTED"]
        self.assertEqual(events[-1]["event"], "RUN_FINISHED")
        self.assertEqual(events[-1]["status"], "CANCELLED")
        if cancel_positions:
            first_cancel = cancel_positions[0]
            self.assertFalse(any(event["event"] in {"ATTEMPT_STARTED", "ARTIFACT_ACCEPTED"}
                                 for event in events[first_cancel + 1:]),
                             "host started or accepted work after observing cancellation")
            acceptance_window = events[:first_cancel]
        else:
            # A marker observed only after every active attempt was accepted
            # has no active attempt to annotate.  The public run may still
            # cancel queued work, but every started receipt must remain a
            # documented earlier acceptance.
            acceptance_window = events

        accepted_before_cancel = {
            event["attempt_id"] for event in acceptance_window
            if event["event"] == "ARTIFACT_ACCEPTED"
        }
        started = [event["attempt_id"] for event in events
                   if event["event"] == "ATTEMPT_STARTED"]
        requested = [event["attempt_id"] for event in events
                     if event["event"] == "CANCEL_REQUESTED"]
        self.assertEqual(len(requested), len(set(requested)),
                         "an active worker received duplicate cancellation")
        for attempt_id in started:
            terminal_events = [event["event"] for event in events
                               if event.get("attempt_id") == attempt_id]
            self.assertIn("PROCESS_EXITED", terminal_events,
                          "started worker was not reaped")
            self.assertIn("RESULT_RECEIVED", terminal_events,
                          "started worker lacks a terminal result")
            if cancel_positions and attempt_id not in accepted_before_cancel:
                self.assertIn(attempt_id, requested,
                              "unaccepted active worker was not cancelled")

        for receipt in self.receipts_for(report):
            self.assertNotIn(receipt["status"], {"BLOCKED", "FAILED"},
                             "observed cancellation must not become a scheduler failure")
            attempt_id = self.receipt_attempt_id(receipt)
            if receipt["status"] == "READY":
                self.assertIsNotNone(attempt_id)
                self.assertIn(attempt_id, accepted_before_cancel,
                              "READY receipt was not accepted before cancellation")
                self.assertEqual(len(receipt["attempts"]), 1)
                self.assertTrue(receipt["attempts"][0]["accepted"])
            else:
                self.assert_cancelled_receipt(
                    receipt, started=attempt_id is not None)

    def test_single_active_worker_is_cancelled_and_logged_once(self):
        cancel, report, events, inspection, _, _ = self.run_cancelled(SINGLE, 1)
        self.assertIn(b'"status":"REQUESTED"', cancel.stdout)
        self.assert_observed_cancel_contract(report, events, inspection)

    def test_parallel_active_workers_are_all_cancelled_in_one_host_iteration(self):
        _, report, events, inspection, _, _ = self.run_cancelled(PARALLEL, 2)
        self.assertEqual([receipt["node_id"] for receipt in report["receipts"]], ["a", "b", "c"])
        self.assert_observed_cancel_contract(report, events, inspection)
        self.assertTrue(any(event["event"] == "CANCEL_REQUESTED" for event in events))

    def test_queued_and_dependent_nodes_never_start_after_cancel(self):
        _, report, events, inspection, _, _ = self.run_cancelled(QUEUED, 1)
        self.assert_observed_cancel_contract(report, events, inspection)
        self.assertTrue(any(self.receipt_attempt_id(receipt) is None and
                            receipt["status"] == "CANCELLED"
                            for receipt in self.receipts_for(report)),
                        "single-worker queue had no unstarted node cancelled by the host")

    def test_cancelled_attempt_is_never_retried(self):
        _, report, events, inspection, _, _ = self.run_cancelled(SINGLE, 1, retry=2)
        self.assert_observed_cancel_contract(report, events, inspection)
        self.assertTrue(all(len(receipt["attempts"]) <= 1
                            for receipt in self.receipts_for(report)),
                        "observed cancellation retried a node")


if __name__ == "__main__":
    unittest.main()
