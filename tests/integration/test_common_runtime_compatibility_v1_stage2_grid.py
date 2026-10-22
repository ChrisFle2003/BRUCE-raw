"""Verify the bounded Stage-2 Common Runtime host grid probe."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


BIN = Path(__import__("sys").argv[1]).resolve()
__import__("sys").argv = [__import__("sys").argv[0]]


class CommonRuntimeHostGrid(unittest.TestCase):
    BASE = ("grid", "--path", "2,6/4,3")

    def run_cli(self, *args):
        return subprocess.run([str(BIN), *map(str, args)], capture_output=True,
                              timeout=60)

    def assert_byte_identical(self, legacy, explicit):
        self.assertEqual(explicit.returncode, legacy.returncode)
        self.assertEqual(explicit.stdout, legacy.stdout)
        self.assertEqual(explicit.stderr, legacy.stderr)

    def test_success_is_byte_identical(self):
        self.assert_byte_identical(
            self.run_cli(*self.BASE),
            self.run_cli(*self.BASE, "--compatibility-family", "session"),
        )

    def test_output_bundle_is_byte_identical(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage2-grid-") as directory:
            root = Path(directory)
            legacy_path = root / "legacy.bruce"
            explicit_path = root / "explicit.bruce"
            legacy = self.run_cli(*self.BASE, "--output", legacy_path)
            explicit = self.run_cli(*self.BASE, "--output", explicit_path,
                                    "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(legacy_path.read_bytes(), explicit_path.read_bytes())

    def test_existing_grid_error_is_byte_identical(self):
        args = (*self.BASE, "--max-bits", "0")
        legacy = self.run_cli(*args)
        explicit = self.run_cli(*args, "--compatibility-family", "session")
        self.assert_byte_identical(legacy, explicit)
        self.assertEqual(legacy.returncode, 2)
        self.assertEqual(json.loads(legacy.stderr), {"error": "InvalidBitLimit"})

    def test_replay_and_session_budget_options_remain_valid(self):
        args = (*self.BASE, "--verification", "replay",
                "--max-session-checkpoints", "1000000",
                "--verification-cancellation", "phase-boundaries")
        result = self.run_cli(*args, "--compatibility-family", "session")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["execution"]["replay_verified"])

    def test_grid_modes_remain_route_owned(self):
        for mode_args in (
            (),
            ("--mode", "modular", "--modulus", "97"),
            ("--mode", "metric"),
        ):
            legacy = self.run_cli(*self.BASE, *mode_args)
            explicit = self.run_cli(*self.BASE, *mode_args,
                                    "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
            self.assertEqual(legacy.returncode, 0, legacy.stderr)

    def test_wrong_unknown_duplicate_and_missing_family_values(self):
        for family, error in (
            ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
        ):
            result = self.run_cli(*self.BASE, "--compatibility-family", family)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(json.loads(result.stderr), {"error": error})
        duplicate = self.run_cli(*self.BASE, "--compatibility-family", "session",
                                 "--compatibility-family", "session")
        self.assertEqual(json.loads(duplicate.stderr),
                         {"error": "DuplicateOption: --compatibility-family"})
        missing = self.run_cli(*self.BASE, "--compatibility-family")
        self.assertEqual(json.loads(missing.stderr),
                         {"error": "MissingOptionValue: --compatibility-family"})

    def test_host_validation_precedes_grid_route_processing(self):
        missing_path = self.run_cli("grid", "--compatibility-family", "gate")
        self.assertEqual(missing_path.returncode, 2)
        self.assertEqual(json.loads(missing_path.stderr),
                         {"error": "CommonRuntimeCompatibilityV1FamilyMismatch"})

    def test_non_target_routes_keep_their_existing_boundaries(self):
        cases = (
            ("grid-batch", ("--paths", "2,6/4,3")),
            ("grid-resume", ("--from-run", "missing.bruce", "--additional-queries", "1")),
            ("research", ("--train", "missing.train", "--holdout", "missing.holdout")),
            ("compatibility", ("--rules", "missing.rules", "--candidates", "missing.candidates")),
        )
        for command, args in cases:
            legacy = self.run_cli(command, *args)
            explicit = self.run_cli(command, *args,
                                    "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
        replay = self.run_cli("replay", "missing.bruce", "--compatibility-family", "session")
        self.assertEqual(replay.returncode, 2)
        self.assertIn(b"Usage: bruce replay", replay.stderr)


if __name__ == "__main__":
    unittest.main()
