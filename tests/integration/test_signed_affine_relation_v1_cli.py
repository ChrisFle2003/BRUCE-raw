import json
from pathlib import Path
import subprocess
import sys
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
COMMAND = "signed-affine-relation-v1"
USAGE = "Usage: bruce signed-affine-relation-v1 demo merge"


class SignedAffineRelationV1CliTests(unittest.TestCase):
    """Real local toy witness, complete derived provenance and fixed boundaries."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=10)

    def demo(self):
        process = self.call(COMMAND, "demo", "merge")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertTrue(process.stdout.endswith(b"\n"))
        self.assertFalse(process.stdout.endswith(b"\n\n"))
        return process.stdout, json.loads(process.stdout)

    def assert_usage_error(self, *args):
        process = self.call(COMMAND, *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertEqual(json.loads(process.stderr), {"error": USAGE})

    def test_help_lists_exactly_the_fixed_merge_demo(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertEqual([line for line in process.stdout.decode().splitlines()
                          if COMMAND in line],
                         ["bruce signed-affine-relation-v1 demo merge"])

    def test_merge_has_complete_forms_toy_states_collision_and_verified_relation(self):
        _, report = self.demo()
        self.assertEqual(report, {
            "contract": "signed-affine-relation-v1-cli-v1",
            "scenario": "merge",
            "source": {"node_id": "source", "value": 2},
            "group": {"modulus": 101, "generator": 1, "public_q": 2},
            "left": {
                "node_id": "a-add", "source_node_id": "source",
                "source_value": 2, "multiplier": 1, "offset": 2,
                "output_value": 4,
                "toy_provenance": {"provenance_id": "a-add",
                                   "coefficient_state": {
                                       "a": 2, "b": 1, "canonical_point": 4}},
            },
            "right": {
                "node_id": "z-double", "source_node_id": "source",
                "source_value": 2, "multiplier": 2, "offset": 0,
                "output_value": 4,
                "toy_provenance": {"provenance_id": "z-double",
                                   "coefficient_state": {
                                       "a": 0, "b": 2, "canonical_point": 4}},
            },
            "collision": {"canonical_point": 4,
                          "existing_provenance_id": "a-add",
                          "new_provenance_id": "z-double"},
            "relation": {"modulus": 101, "coefficient_delta": 100,
                         "value_delta": 99, "candidate_k": 2,
                         "verification_status": "VERIFIED"},
            "status": "VERIFIED",
        })

    def test_repeated_merge_reports_are_byte_identical(self):
        first, _ = self.demo()
        for _ in range(4):
            self.assertEqual(self.demo()[0], first)

    def test_omitted_arguments_are_error_only(self):
        for args in ((), ("demo",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_actions_are_error_only(self):
        for args in (("unknown",), ("merge",), ("execute", "merge"),
                     ("", "merge"), ("--help",), ("help",)):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_unknown_scenarios_are_error_only(self):
        for scenario in ("unknown", "", "cancellation", "noninvertible"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)

    def test_case_altered_tokens_are_error_only(self):
        for args in (("DEMO", "merge"), ("Demo", "merge"),
                     ("demo", "MERGE"), ("demo", "Merge")):
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_extra_arguments_are_error_only(self):
        for suffix in (("extra",), ("",), ("demo", "merge"),
                       ("--output", "run.json")):
            with self.subTest(suffix=suffix):
                self.assert_usage_error("demo", "merge", *suffix)

    def test_parameter_like_tokens_are_error_only(self):
        for option, value in (("--modulus", "101"),
                              ("--input", "graph.json"),
                              ("--search", "100")):
            for args in ((option, value), ("demo", option),
                         ("demo", "merge", option, value),
                         ("demo", "merge", option + "=" + value)):
                with self.subTest(args=args):
                    self.assert_usage_error(*args)

    def test_file_like_scenarios_are_error_only(self):
        for scenario in ("graph.json", "./merge", "/tmp/merge.json",
                         "merge.json"):
            with self.subTest(scenario=scenario):
                self.assert_usage_error("demo", scenario)


if __name__ == "__main__":
    unittest.main()
