import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
USAGE = "Usage: bruce dag-scheduler-v2 demo flow"

# Independently derived from the fixed four-node flow. A wrong request, payload,
# direct input, missing receipt, raw-byte leak or reordered trace breaks this.
FLOW = {
    "contract": "dag-scheduler-v2-cli-v1", "scenario": "flow",
    "scheduler_version": "dag-scheduler-v2", "dag_id": "flow-v2",
    "max_started_nodes": 4, "max_total_receipt_bytes": 4096,
    "receipts": [
        {"node_id": "a-source", "callback_id": "source-ready-v2",
         "request_size": 14,
         "request_digest": hashlib.sha256(b"source-request").hexdigest(),
         "input_refs": [], "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "SOURCE_READY", "payload_size": 5,
                    "payload_digest": hashlib.sha256(b"alpha").hexdigest()}},
        {"node_id": "b-transform", "callback_id": "transform-ready-v2",
         "request_size": 17,
         "request_digest": hashlib.sha256(b"transform-request").hexdigest(),
         "input_refs": [
             {"producer_node_id": "a-source", "result_code": "SOURCE_READY",
              "payload_size": 5,
              "payload_digest": hashlib.sha256(b"alpha").hexdigest()}],
         "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "TRANSFORM_READY", "payload_size": 10,
                    "payload_digest": hashlib.sha256(b"alpha:beta").hexdigest()}},
        {"node_id": "c-independent", "callback_id": "independent-ready-v2",
         "request_size": 19,
         "request_digest": hashlib.sha256(b"independent-request").hexdigest(),
         "input_refs": [], "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "INDEPENDENT_READY", "payload_size": 5,
                    "payload_digest": hashlib.sha256(b"gamma").hexdigest()}},
        {"node_id": "d-join", "callback_id": "join-ready-v2",
         "request_size": 12,
         "request_digest": hashlib.sha256(b"join-request").hexdigest(),
         "input_refs": [
             {"producer_node_id": "b-transform", "result_code": "TRANSFORM_READY",
              "payload_size": 10,
              "payload_digest": hashlib.sha256(b"alpha:beta").hexdigest()},
             {"producer_node_id": "c-independent", "result_code": "INDEPENDENT_READY",
              "payload_size": 5,
              "payload_digest": hashlib.sha256(b"gamma").hexdigest()}],
         "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "JOIN_READY", "payload_size": 16,
                    "payload_digest": hashlib.sha256(b"alpha:beta|gamma").hexdigest()}},
    ],
    "trace": [
        {"event": "VALIDATED", "node_id": ""},
        {"event": "STARTED", "node_id": "a-source"},
        {"event": "READY", "node_id": "a-source"},
        {"event": "STARTED", "node_id": "b-transform"},
        {"event": "READY", "node_id": "b-transform"},
        {"event": "STARTED", "node_id": "c-independent"},
        {"event": "READY", "node_id": "c-independent"},
        {"event": "STARTED", "node_id": "d-join"},
        {"event": "READY", "node_id": "d-join"},
        {"event": "COMPLETED", "node_id": ""},
    ],
}


class DagSchedulerV2CliTests(unittest.TestCase):
    """Complete real-binary metadata report, strict route and stable bytes."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=10)

    def demo(self):
        process = self.call("dag-scheduler-v2", "demo", "flow")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertTrue(process.stdout.endswith(b"\n"))
        self.assertFalse(process.stdout.endswith(b"\n\n"))
        return process.stdout, json.loads(process.stdout)

    def assert_usage_error(self, *args):
        process = self.call("dag-scheduler-v2", *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertEqual(process.stderr, ('{"error":"' + USAGE + '"}\n').encode())

    def test_complete_flow_has_only_metadata_and_direct_sorted_inputs(self):
        raw, report = self.demo()
        self.assertEqual(report, FLOW)
        for forbidden in (b"alpha", b"beta", b"gamma", b"source-request",
                          b"transform-request", b"independent-request", b"join-request",
                          b"request_bytes", b"payload_bytes"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, raw)

    def test_repeated_flow_is_raw_byte_stable(self):
        first, _ = self.demo()
        for _ in range(5):
            raw, report = self.demo()
            self.assertEqual(raw, first)
            self.assertEqual(report, FLOW)

    def test_help_lists_exactly_the_one_fixed_demo(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertEqual([line for line in process.stdout.decode().splitlines()
                          if "dag-scheduler-v2" in line],
                         ["bruce dag-scheduler-v2 demo flow"])

    def test_missing_action_or_scenario_is_error_only(self):
        for args in ((), ("demo",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_actions_are_error_only(self):
        for args in (("unknown",), ("flow",), ("", "flow"), ("execute", "flow"),
                     ("DEMO", "flow"), ("--help",), ("help",),
                     ("inspect", "flow"), ("replay", "flow")):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_scenarios_are_error_only(self):
        for scenario in ("unknown", "", "FLOW", "mixed", "budget", "cancelled",
                         "graph.json", "--help", "--callback", "--budget", "1"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)

    def test_extra_arguments_never_allow_free_inputs(self):
        for suffix in (("extra",), ("",), ("--budget", "1"),
                       ("--max-started-nodes", "64"), ("--max-total-receipt-bytes", "1"),
                       ("--callback", "source-ready-v2"), ("--input", "graph.json"),
                       ("--output", "run.json"), ("--request", "alpha"),
                       ("--payload", "beta"), ("--cancellation", "none"), ("--help",)):
            with self.subTest(suffix=suffix):
                self.assert_usage_error("demo", "flow", *suffix)


if __name__ == "__main__":
    unittest.main()
