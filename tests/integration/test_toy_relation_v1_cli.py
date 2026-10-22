import json
from pathlib import Path
import subprocess
import sys
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
HELP_LINES = [
    "bruce toy-relation-v1 demo collision",
    "bruce toy-relation-v1 demo noninvertible",
]
USAGE = "Usage: bruce toy-relation-v1 demo collision|noninvertible"


class ToyRelationV1CliTests(unittest.TestCase):
    """Real fixed-demo reports, stable bytes and strict public boundaries."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True,
                              text=True, timeout=10)

    def demo(self, scenario):
        process = self.call("toy-relation-v1", "demo", scenario)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        return process.stdout, json.loads(process.stdout)

    def assert_usage_error(self, *args):
        process = self.call("toy-relation-v1", *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertEqual(json.loads(process.stderr), {"error": USAGE})

    def test_help_lists_exactly_the_two_fixed_demos(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        self.assertEqual([line for line in process.stdout.splitlines()
                          if "toy-relation-v1" in line], HELP_LINES)

    def test_collision_has_complete_sorted_provenance_and_verified_relation(self):
        _, report = self.demo("collision")
        self.assertEqual(report, {
            "contract": "toy-relation-v1-cli-v1",
            "scenario": "collision",
            "group": {"modulus": 101, "generator": 1, "public_q": 17},
            "provenances": [
                {"provenance_id": "left", "coefficient_state":
                    {"a": 2, "b": 3, "canonical_point": 53}},
                {"provenance_id": "right", "coefficient_state":
                    {"a": 36, "b": 1, "canonical_point": 53}},
            ],
            "collision": {"canonical_point": 53,
                          "existing_provenance_id": "left",
                          "new_provenance_id": "right"},
            "relation": {"modulus": 101, "coefficient_delta": 2,
                         "value_delta": 34, "candidate_k": 17,
                         "verification_status": "VERIFIED"},
            "status": "VERIFIED",
        })

    def test_noninvertible_retains_collision_without_relation_or_candidate(self):
        raw, report = self.demo("noninvertible")
        self.assertEqual(report, {
            "contract": "toy-relation-v1-cli-v1",
            "scenario": "noninvertible",
            "group": {"modulus": 15, "generator": 1, "public_q": 4},
            "provenances": [
                {"provenance_id": "left", "coefficient_state":
                    {"a": 0, "b": 5, "canonical_point": 5}},
                {"provenance_id": "right", "coefficient_state":
                    {"a": 5, "b": 0, "canonical_point": 5}},
            ],
            "collision": {"canonical_point": 5,
                          "existing_provenance_id": "left",
                          "new_provenance_id": "right"},
            "status": "NO_UNIQUE_RELATION",
            "reason": "NonInvertibleToyRelationCoefficient",
        })
        self.assertNotIn("candidate_k", raw)
        self.assertNotIn("relation", report)

    def test_repeated_demos_are_byte_stable(self):
        for scenario in ("collision", "noninvertible"):
            with self.subTest(scenario=scenario):
                first, _ = self.demo(scenario)
                for _ in range(3):
                    self.assertEqual(self.demo(scenario)[0], first)

    def test_missing_action_or_scenario_is_error_only(self):
        for args in ((), ("demo",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_actions_are_error_only(self):
        for args in (("unknown",), ("collision",), ("", "collision"),
                     ("execute", "collision"), ("DEMO", "noninvertible"),
                     ("--help",), ("help",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_scenarios_are_error_only(self):
        for scenario in ("unknown", "", "COLLISION", "NONINVERTIBLE",
                         "group.json", "--help", "--modulus", "17"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)

    def test_extra_arguments_are_rejected_for_both_demos(self):
        for scenario in ("collision", "noninvertible"):
            for suffix in (("extra",), ("",), ("--modulus", "101"),
                           ("--input", "group.json"), ("--search", "100"),
                           ("--output", "run.json")):
                with self.subTest(scenario=scenario, suffix=suffix):
                    self.assert_usage_error("demo", scenario, *suffix)


if __name__ == "__main__":
    unittest.main()
