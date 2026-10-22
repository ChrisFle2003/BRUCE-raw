"""Verify the Stage-1 compatibility-family session adapter boundary."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


BIN = Path(__import__("sys").argv[1]).resolve()
__import__("sys").argv = [__import__("sys").argv[0]]


class SessionCompatibilityAdapter(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([str(BIN), *map(str, args)], capture_output=True,
                              timeout=60)

    def assert_success_pair(self, legacy, explicit):
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertEqual(explicit.stdout, legacy.stdout)
        self.assertEqual(explicit.stderr, legacy.stderr)
        self.assertNotIn(b"common-runtime-compatibility", explicit.stdout)

    def test_explicit_session_is_byte_identical_across_six_routes(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-session-") as directory:
            root = Path(directory)
            train = root / "train.csv"
            train.write_text("x,y\n0,2\n1,3\n2,4\n")
            rules = root / "rules"
            rules.write_text(
                "BRUCE_RULES_V1\n"
                "snapshot demo-v1\n"
                "rule positive positive value ge 0 0 1 manual -\n"
            )
            candidates = root / "candidates.csv"
            candidates.write_text("id,domain,layer,x,y,value\n1,math,0,0,0,2\n")
            research_source = root / "research-source"
            generated = self.run_cli("research", "--train", train,
                                     "--output", research_source)
            self.assertEqual(generated.returncode, 0, generated.stderr)

            cases = [
                (["compare", "--left-numerator", "1", "--left-denominator", "1",
                  "--right-numerator", "1", "--right-denominator", "1"], "compare"),
                (["grid", "--path", "2,6"], "grid"),
                (["grid-batch", "--paths", "2,6;4,3"], "grid-batch"),
                (["research", "--train", train], "research"),
                (["compatibility", "--rules", rules, "--candidates", candidates],
                 "compatibility"),
                (["compatibility", "--rules", rules, "--from-run", research_source,
                  "--at", "1"], "compatibility-run"),
            ]
            for args, name in cases:
                legacy = self.run_cli(*args)
                explicit = self.run_cli(*args, "--compatibility-family", "session")
                self.assert_success_pair(legacy, explicit)

            parent = root / "parent"
            legacy = self.run_cli("grid-batch", "--paths", "2,6;4,3",
                                  "--max-query-steps", "1", "--output", parent)
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            legacy_resume = root / "legacy-resume"
            explicit_resume = root / "explicit-resume"
            old = self.run_cli("grid-resume", "--from-run", parent,
                               "--additional-queries", "1", "--output", legacy_resume)
            new = self.run_cli("grid-resume", "--from-run", parent,
                               "--additional-queries", "1", "--output", explicit_resume,
                               "--compatibility-family", "session")
            self.assert_success_pair(old, new)
            self.assertEqual(old.stdout, new.stdout)
            self.assertEqual(legacy_resume.read_bytes(), explicit_resume.read_bytes())

    def test_explicit_session_covers_research_variants(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-session-") as directory:
            root = Path(directory)
            train = root / "train.csv"
            train.write_text("x,y\n0,2\n1,3\n2,4\n")
            exact = root / "exact"
            grid = root / "grid"
            grid_train = root / "grid-train"
            generated = self.run_cli("grid-batch", "--paths", "0,2;1,2;2,2",
                                     "--output", grid_train)
            self.assertEqual(generated.returncode, 0, generated.stderr)
            self.assert_success_pair(
                self.run_cli("research", "--domain", "rational", "--train", train),
                self.run_cli("research", "--domain", "rational", "--train", train,
                             "--compatibility-family", "session"),
            )
            self.assert_success_pair(
                self.run_cli("research", "--strategy", "polynomial", "--domain", "rational",
                             "--max-degree", "2", "--grid-train", grid_train, "--output", grid),
                self.run_cli("research", "--strategy", "polynomial", "--domain", "rational",
                             "--max-degree", "2", "--grid-train", grid_train, "--output", exact,
                             "--compatibility-family", "session"),
            )

    def test_family_validation_precedes_file_or_bundle_access(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-session-") as directory:
            missing = Path(directory) / "must-not-be-read"
            cases = [
                ("research", "--train", missing),
                ("grid-resume", "--from-run", missing, "--additional-queries", "1"),
                ("compatibility", "--rules", missing, "--candidates", missing),
            ]
            for route in cases:
                for family, expected in (
                    ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
                ):
                    result = self.run_cli(*route, "--compatibility-family", family)
                    self.assertEqual(result.returncode, 2, result.stdout)
                    self.assertEqual(result.stdout, b"")
                    self.assertEqual(json.loads(result.stderr), {"error": expected})
                    self.assertFalse(missing.exists())

    def test_session_confirmation_does_not_require_replay_and_controls_combine(self):
        result = self.run_cli("grid", "--path", "2,6",
                              "--compatibility-family", "session")
        self.assertEqual(result.returncode, 0, result.stderr)
        replay = self.run_cli("grid-batch", "--paths", "2,6;4,3",
                              "--compatibility-family", "session", "--verification", "replay",
                              "--max-session-checkpoints", "1000000",
                              "--verification-cancellation", "grid-query-boundaries")
        self.assertEqual(replay.returncode, 0, replay.stderr)

    def test_non_target_routes_reject_the_option_and_help_advertises_session(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        help_text = result.stdout.decode()
        self.assertIn("[--compatibility-family session]", help_text)
        result = self.run_cli("replay", Path(__file__).parents[1] / "fixtures" /
                              "research-v1.bruce", "--compatibility-family", "session")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertIn(b"Usage: bruce replay", result.stderr)

    def test_duplicate_and_missing_family_values_keep_parser_contract(self):
        missing = self.run_cli("grid", "--path", "2,6", "--compatibility-family")
        self.assertEqual(json.loads(missing.stderr),
                         {"error": "MissingOptionValue: --compatibility-family"})
        duplicate = self.run_cli("grid", "--path", "2,6",
                                 "--compatibility-family", "session",
                                 "--compatibility-family", "session")
        self.assertEqual(json.loads(duplicate.stderr),
                         {"error": "DuplicateOption: --compatibility-family"})


if __name__ == "__main__":
    unittest.main()
