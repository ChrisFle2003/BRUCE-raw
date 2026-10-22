import json
from pathlib import Path
import subprocess
import sys
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
HELP_LINES = [
    "bruce signed-affine-dag-v1 demo merge",
    "bruce signed-affine-dag-v1 demo cancellation",
]


class SignedAffineDagV1CliTests(unittest.TestCase):
    """Exercise fixed demos, complete provenance, ordering and command boundaries."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True,
                              text=True, timeout=10)

    def demo(self, scenario):
        process = self.call("signed-affine-dag-v1", "demo", scenario)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        return process.stdout, json.loads(process.stdout)

    def assert_demo(self, scenario, second_weight, balance):
        _, report = self.demo(scenario)
        self.assertEqual(set(report), {"contract", "scenario", "states"})
        self.assertEqual(report["contract"], "signed-affine-dag-v1-cli-v1")
        self.assertEqual(report["scenario"], scenario)
        self.assertEqual(report["states"], [
            {"value": 2, "weight_balance": 1, "paths": [
                {"node_id": "source", "predecessor_node_id": None,
                 "operator_id": "", "input_value": 2,
                 "output_value": 2, "path_weight": 1},
            ]},
            {"value": 4, "weight_balance": balance, "paths": [
                {"node_id": "a-add", "predecessor_node_id": "source",
                 "operator_id": "add-two-v1", "input_value": 2,
                 "output_value": 4, "path_weight": 1},
                {"node_id": "z-double", "predecessor_node_id": "source",
                 "operator_id": "double-v1", "input_value": 2,
                 "output_value": 4, "path_weight": second_weight},
            ]},
        ])

    def assert_usage_error(self, *args):
        process = self.call("signed-affine-dag-v1", *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        error = json.loads(process.stderr)
        self.assertEqual(set(error), {"error"})
        self.assertIsInstance(error["error"], str)
        self.assertIn("signed-affine-dag-v1", error["error"])
        self.assertNotIn("UnknownCommand", error["error"])

    def test_help_lists_exactly_the_two_fixed_demos(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        self.assertEqual([line for line in process.stdout.splitlines()
                          if "signed-affine-dag-v1" in line], HELP_LINES)

    def test_merge_preserves_both_sorted_complete_paths(self):
        self.assert_demo("merge", 1, 2)

    def test_cancellation_retains_zero_balance_and_both_paths(self):
        self.assert_demo("cancellation", -1, 0)

    def test_repeated_demos_are_byte_stable(self):
        for scenario in ("merge", "cancellation"):
            with self.subTest(scenario=scenario):
                first, _ = self.demo(scenario)
                for _ in range(3):
                    self.assertEqual(self.demo(scenario)[0], first)

    def test_missing_action_or_scenario_is_controlled(self):
        for args in ((), ("demo",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_action_does_not_reach_legacy_routing(self):
        for args in (("unknown",), ("execute", "merge"), ("merge",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_scenario_is_controlled(self):
        for scenario in ("unknown", "", "MERGE", "graph.json"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)

    def test_extra_arguments_are_rejected_for_both_demos(self):
        for scenario in ("merge", "cancellation"):
            for suffix in (("extra",), ("--output", "run.json")):
                with self.subTest(scenario=scenario, suffix=suffix):
                    self.assert_usage_error("demo", scenario, *suffix)


if __name__ == "__main__":
    unittest.main()
