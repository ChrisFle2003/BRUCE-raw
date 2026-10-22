import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=workflow\n"
    b"max-started-nodes=8\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=41\n\n"
    b"node=b\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=42\n\n"
    b"node=c\n"
    b"depends=a,b\n"
    b"callback=join-v1\n"
    b"request-hex=2d\n"
)

SINGLE_SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=single-retry\n"
    b"max-started-nodes=1\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=4142\n"
)

BUDGET_SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=receipt-budget\n"
    b"max-started-nodes=4\n"
    b"max-total-receipt-bytes=1\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=41\n\n"
    b"node=b\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=42\n"
)

START_BUDGET_SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=start-budget-independent\n"
    b"max-started-nodes=2\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=41\n\n"
    b"node=b\n"
    b"depends=a\n"
    b"callback=append-v1\n"
    b"request-hex=42\n\n"
    b"node=c\n"
    b"depends=b\n"
    b"callback=append-v1\n"
    b"request-hex=2d\n\n"
    b"node=z\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=5a\n"
)


class DagSchedulerV5CliTests(unittest.TestCase):
    def call(self, spec, *options):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-integration-") as directory:
            root = Path(directory)
            spec_path = root / "workflow.spec"
            bundle = root / "bundle"
            spec_path.write_bytes(spec)
            result = subprocess.run(
                [BIN, "dag-scheduler-v5", "execute", str(spec_path),
                 "--output", str(bundle), *options],
                capture_output=True, timeout=15,
            )
            report = json.loads(bundle.read_text()) if bundle.exists() else None
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            log = [json.loads(line) for line in log_path.read_text().splitlines()] \
                if log_path.exists() else []
            return result, report, log

    def test_bounded_multi_worker_flow_and_dependency_gating(self):
        result, report, log = self.call(SPEC, "--workers", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([receipt["node_id"] for receipt in report["receipts"]],
                         ["a", "b", "c"])
        self.assertEqual(report["status"], "READY")
        self.assertNotIn(b"A-B", result.stdout)
        self.assertNotIn(b"payload_bytes", result.stdout)
        started = [event["attempt_id"] for event in log
                   if event["event"] == "ATTEMPT_STARTED"]
        self.assertEqual(started, ["a-1-1", "a-2-1", "a-3-1"])
        self.assertEqual([event["seq"] for event in log],
                         list(range(1, len(log) + 1)))
        self.assertIn("STATUS", [event["event"] for event in log])
        self.assertIn("PROCESS_EXITED", [event["event"] for event in log])
        c_start = next(i for i, event in enumerate(log)
                       if event.get("attempt_id") == "a-3-1" and
                       event["event"] == "ATTEMPT_STARTED")
        self.assertGreater(c_start, next(i for i, event in enumerate(log)
                                         if event.get("attempt_id") == "a-1-1" and
                                         event["event"] == "ARTIFACT_ACCEPTED"))
        self.assertGreater(c_start, next(i for i, event in enumerate(log)
                                         if event.get("attempt_id") == "a-2-1" and
                                         event["event"] == "ARTIFACT_ACCEPTED"))

    def test_workers_one_is_deterministic_and_retry_is_opt_in(self):
        first, first_report, first_log = self.call(SPEC, "--workers", "1")
        second, second_report, second_log = self.call(SPEC, "--workers", "1")
        self.assertEqual(first.returncode, 0)
        self.assertEqual(second.returncode, 0)
        self.assertEqual(
            [(item["node_id"], item["payload_digest"]) for item in first_report["receipts"]],
            [(item["node_id"], item["payload_digest"]) for item in second_report["receipts"]],
        )
        self.assertEqual(
            [event["event"] for event in first_log],
            [event["event"] for event in second_log],
        )
        blocked_spec = SPEC.replace(b"request-hex=41", b"request-hex=4142", 1)
        failed, report, log = self.call(blocked_spec, "--workers", "2",
                                        "--max-output-bytes", "1")
        self.assertEqual(failed.returncode, 1)
        self.assertEqual(report["receipts"][0]["failure_code"],
                         "OUTPUT_LIMIT_EXCEEDED")
        self.assertEqual(report["receipts"][2]["failure_code"], "BLOCKED")
        self.assertEqual(sum(event["event"] == "ATTEMPT_STARTED" for event in log), 2)
        self.assertNotIn("a-3-1", [event.get("attempt_id") for event in log])

    def test_timeout_retry_uses_distinct_attempts_only_when_requested(self):
        result, report, log = self.call(SPEC.replace(
            b"request-hex=41", b"request-hex=4142", 1),
            "--workers", "1", "--max-wall-ms", "1", "--retry", "2")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["receipts"][0]["failure_code"], "TIMEOUT")
        attempts = [event["attempt_id"] for event in log
                    if event["event"] == "ATTEMPT_STARTED"]
        self.assertEqual(attempts, ["a-1-1", "a-1-2", "a-1-3",
                                    "a-2-1", "a-2-2", "a-2-3"])

    def test_single_node_report_contains_complete_retry_history(self):
        result, report, log = self.call(
            SINGLE_SPEC, "--workers", "1", "--max-wall-ms", "1", "--retry", "2")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["failure_code"], "TIMEOUT")
        self.assertEqual(
            [attempt["attempt_id"] for attempt in report["attempts"]],
            ["a-1", "a-1-2", "a-1-3"],
        )
        self.assertEqual(len(report["attempts"]), 3)
        self.assertTrue(all("usage" in attempt for attempt in report["attempts"]))
        self.assertNotIn("attempts/a-", result.stdout.decode())

    def test_shared_receipt_budget_is_canonical_under_parallel_completion(self):
        observed = []
        for _ in range(4):
            result, report, _ = self.call(BUDGET_SPEC, "--workers", "2")
            self.assertEqual(result.returncode, 1)
            observed.append([
                (receipt["node_id"], receipt["status"], receipt["failure_code"])
                for receipt in report["receipts"]
            ])
        self.assertEqual(observed, [[
            ("a", "READY", "READY"),
            ("b", "FAILED", "OUTPUT_LIMIT_EXCEEDED"),
        ]] * 4)

    def test_start_budget_blocks_only_dependents_and_finishes_independent_branch(self):
        one, report_one, log_one = self.call(START_BUDGET_SPEC, "--workers", "1")
        two, report_two, log_two = self.call(START_BUDGET_SPEC, "--workers", "2")
        self.assertEqual(one.returncode, 1)
        self.assertEqual(two.returncode, 1)
        summary_one = [(item["node_id"], item["status"], item["failure_code"],
                        item["result_code"]) for item in report_one["receipts"]]
        summary_two = [(item["node_id"], item["status"], item["failure_code"],
                        item["result_code"]) for item in report_two["receipts"]]
        self.assertEqual(summary_one, summary_two)
        receipts = {receipt["node_id"]: receipt for receipt in report_two["receipts"]}
        self.assertEqual(receipts["a"]["status"], "READY")
        self.assertEqual(receipts["b"]["status"], "READY")
        self.assertEqual(receipts["c"]["failure_code"], "FAILED")
        self.assertEqual(receipts["c"]["result_code"], "START_BUDGET_EXCEEDED")
        self.assertEqual(receipts["z"]["failure_code"], "FAILED")
        self.assertEqual(receipts["z"]["result_code"], "START_BUDGET_EXCEEDED")
        started = [event["attempt_id"] for event in log_two
                   if event["event"] == "ATTEMPT_STARTED"]
        self.assertEqual(started, ["a-1-1", "a-2-1"])
        self.assertEqual(
            [event["attempt_id"] for event in log_one
             if event["event"] == "ATTEMPT_STARTED"], started)


if __name__ == "__main__":
    unittest.main()
