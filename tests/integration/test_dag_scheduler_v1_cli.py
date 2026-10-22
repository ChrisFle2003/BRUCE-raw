import json
from pathlib import Path
import subprocess
import sys
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
HELP_LINES = [
    "bruce dag-scheduler-v1 demo mixed",
    "bruce dag-scheduler-v1 demo budget",
    "bruce dag-scheduler-v1 demo cancelled",
]
USAGE = "Usage: bruce dag-scheduler-v1 demo mixed|budget|cancelled"

# Hand-derived complete fixtures. Wrong routes, omitted receipts, trace sorting,
# incorrect direct inputs, or starting a sentinel callback must break these.
MIXED = {
    "contract": "dag-scheduler-v1-cli-v1", "scenario": "mixed",
    "scheduler_version": "dag-scheduler-v1", "dag_id": "mixed-v1",
    "max_started_nodes": 5,
    "receipts": [
        {"node_id": "a-root", "callback_id": "root-ready-v1", "input_refs": [],
         "outcome": "READY", "terminal_code": "ROOT_READY"},
        {"node_id": "b-fail", "callback_id": "fail-v1", "input_refs": [
            {"producer_node_id": "a-root", "result_code": "ROOT_READY"}],
         "outcome": "FAILED", "terminal_code": "EXPECTED_FAILURE"},
        {"node_id": "c-independent", "callback_id": "independent-ready-v1",
         "input_refs": [], "outcome": "READY", "terminal_code": "INDEPENDENT_READY"},
        {"node_id": "d-blocked", "callback_id": "must-not-start-v1", "input_refs": [],
         "outcome": "BLOCKED", "terminal_code": "DEPENDENCY_NOT_READY"},
        {"node_id": "e-finish", "callback_id": "finish-v1", "input_refs": [
            {"producer_node_id": "c-independent", "result_code": "INDEPENDENT_READY"}],
         "outcome": "READY", "terminal_code": "FINISHED"},
    ],
    "trace": [
        {"event": "VALIDATED", "node_id": ""},
        {"event": "STARTED", "node_id": "a-root"},
        {"event": "READY", "node_id": "a-root"},
        {"event": "STARTED", "node_id": "b-fail"},
        {"event": "FAILED", "node_id": "b-fail"},
        {"event": "BLOCKED", "node_id": "d-blocked"},
        {"event": "STARTED", "node_id": "c-independent"},
        {"event": "READY", "node_id": "c-independent"},
        {"event": "STARTED", "node_id": "e-finish"},
        {"event": "READY", "node_id": "e-finish"},
        {"event": "COMPLETED", "node_id": ""},
    ],
}
BUDGET = {
    "contract": "dag-scheduler-v1-cli-v1", "scenario": "budget",
    "scheduler_version": "dag-scheduler-v1", "dag_id": "budget-v1",
    "max_started_nodes": 1,
    "receipts": [
        {"node_id": "a-root", "callback_id": "root-ready-v1", "input_refs": [],
         "outcome": "READY", "terminal_code": "ROOT_READY"},
        {"node_id": "b-child", "callback_id": "must-not-start-v1", "input_refs": [],
         "outcome": "CANCELLED", "terminal_code": "START_BUDGET_EXHAUSTED"},
        {"node_id": "c-independent", "callback_id": "must-not-start-v1", "input_refs": [],
         "outcome": "CANCELLED", "terminal_code": "START_BUDGET_EXHAUSTED"},
    ],
    "trace": [
        {"event": "VALIDATED", "node_id": ""},
        {"event": "STARTED", "node_id": "a-root"},
        {"event": "READY", "node_id": "a-root"},
        {"event": "CANCELLED", "node_id": "b-child"},
        {"event": "CANCELLED", "node_id": "c-independent"},
        {"event": "COMPLETED", "node_id": ""},
    ],
}
CANCELLED = {
    "contract": "dag-scheduler-v1-cli-v1", "scenario": "cancelled",
    "scheduler_version": "dag-scheduler-v1", "dag_id": "cancelled-v1",
    "max_started_nodes": 3,
    "receipts": [
        {"node_id": "a-root", "callback_id": "cancel-root-v1", "input_refs": [],
         "outcome": "READY", "terminal_code": "ROOT_READY"},
        {"node_id": "b-child", "callback_id": "must-not-start-v1", "input_refs": [],
         "outcome": "CANCELLED", "terminal_code": "CONTROLLED_CANCELLATION"},
        {"node_id": "c-independent", "callback_id": "must-not-start-v1", "input_refs": [],
         "outcome": "CANCELLED", "terminal_code": "CONTROLLED_CANCELLATION"},
    ],
    "trace": [
        {"event": "VALIDATED", "node_id": ""},
        {"event": "STARTED", "node_id": "a-root"},
        {"event": "READY", "node_id": "a-root"},
        {"event": "CANCELLED", "node_id": "b-child"},
        {"event": "CANCELLED", "node_id": "c-independent"},
        {"event": "COMPLETED", "node_id": ""},
    ],
}


class DagSchedulerV1CliTests(unittest.TestCase):
    """Real fixed demos: complete reports, raw bytes and strict arguments."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=10)

    def demo(self, scenario):
        process = self.call("dag-scheduler-v1", "demo", scenario)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertTrue(process.stdout.endswith(b"\n"))
        self.assertFalse(process.stdout.endswith(b"\n\n"))
        return process.stdout, json.loads(process.stdout)

    def assert_usage_error(self, *args):
        process = self.call("dag-scheduler-v1", *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertEqual(process.stderr, ('{"error": "' + USAGE + '"}\n').encode())

    def test_help_lists_exactly_the_three_fixed_demos(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertEqual([line for line in process.stdout.decode().splitlines()
                          if "dag-scheduler-v1" in line], HELP_LINES)

    def test_mixed_has_complete_sorted_receipts_and_chronological_trace(self):
        self.assertEqual(self.demo("mixed")[1], MIXED)

    def test_budget_cancels_successor_and_independent_without_callback_effects(self):
        self.assertEqual(self.demo("budget")[1], BUDGET)

    def test_control_cancels_successor_and_independent_without_callback_effects(self):
        self.assertEqual(self.demo("cancelled")[1], CANCELLED)

    def test_repeated_demos_are_raw_byte_stable(self):
        for scenario in ("mixed", "budget", "cancelled"):
            with self.subTest(scenario=scenario):
                first, _ = self.demo(scenario)
                for _ in range(5):
                    self.assertEqual(self.demo(scenario)[0], first)

    def test_cancelled_invocations_remain_stable_between_other_scenarios(self):
        first, _ = self.demo("cancelled")
        for _ in range(3):
            self.assertEqual(self.demo("mixed")[1], MIXED)
            self.assertEqual(self.demo("budget")[1], BUDGET)
            raw, report = self.demo("cancelled")
            self.assertEqual(raw, first)
            self.assertEqual(report, CANCELLED)

    def test_missing_action_or_scenario_is_error_only(self):
        for args in ((), ("demo",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_actions_are_error_only(self):
        for args in (("unknown",), ("mixed",), ("", "mixed"),
                     ("execute", "mixed"), ("DEMO", "budget"),
                     ("--help",), ("help",), ("inspect", "cancelled"),
                     ("replay", "mixed")):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_scenarios_are_error_only(self):
        for scenario in ("unknown", "", "MIXED", "BUDGET", "CANCELLED",
                         "graph.json", "--help", "--callback", "--budget", "1"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)

    def test_extra_arguments_are_rejected_for_every_demo(self):
        for scenario in ("mixed", "budget", "cancelled"):
            for suffix in (("extra",), ("",), ("--budget", "1"),
                           ("--max-started-nodes", "64"), ("--callback", "root-ready-v1"),
                           ("--input", "graph.json"), ("--output", "run.json"),
                           ("--cancellation", "none"), ("--help",)):
                with self.subTest(scenario=scenario, suffix=suffix):
                    self.assert_usage_error("demo", scenario, *suffix)


if __name__ == "__main__":
    unittest.main()
