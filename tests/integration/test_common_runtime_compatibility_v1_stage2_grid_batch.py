"""Verify the bounded Stage-2 Common Runtime host grid-batch probe."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


BIN = Path(__import__("sys").argv[1]).resolve()
__import__("sys").argv = [__import__("sys").argv[0]]


class CommonRuntimeHostGridBatch(unittest.TestCase):
    BASE = ("grid-batch", "--paths", "2,6;4,3;2,6")

    def run_cli(self, *args):
        return subprocess.run([str(BIN), *map(str, args)], capture_output=True,
                              timeout=60)

    def assert_byte_identical(self, legacy, explicit):
        self.assertEqual(explicit.returncode, legacy.returncode)
        self.assertEqual(explicit.stdout, legacy.stdout)
        self.assertEqual(explicit.stderr, legacy.stderr)

    def test_success_and_cache_are_byte_identical(self):
        for extra in ((), ("--cache-bytes", "0"), ("--mode", "metric"),
                      ("--mode", "modular", "--modulus", "97")):
            legacy = self.run_cli(*self.BASE, *extra)
            explicit = self.run_cli(*self.BASE, *extra,
                                    "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            result = json.loads(explicit.stdout)
            self.assertEqual(result["queries"], json.loads(legacy.stdout)["queries"])

    def test_complete_and_partial_bundles_are_byte_identical(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage2-grid-batch-") as directory:
            root = Path(directory)
            for extra in ((), ("--max-query-steps", "1")):
                legacy_path = root / ("legacy-partial" if extra else "legacy-complete")
                explicit_path = root / ("explicit-partial" if extra else "explicit-complete")
                legacy = self.run_cli(*self.BASE, *extra, "--output", legacy_path)
                explicit = self.run_cli(*self.BASE, *extra, "--output", explicit_path,
                                        "--compatibility-family", "session")
                self.assert_byte_identical(legacy, explicit)
                self.assertEqual(legacy.returncode, 0, legacy.stderr)
                self.assertEqual(legacy_path.read_bytes(), explicit_path.read_bytes())

    def test_route_owned_errors_remain_byte_identical(self):
        cases = (
            (*self.BASE, "--max-query-steps", "1025"),
            (*self.BASE, "--cache-bytes", "1048577"),
            ("grid-batch", "--paths", "2,6;"),
            ("grid-batch", "--paths", "2,6", "--max-bits", "0"),
        )
        for args in cases:
            legacy = self.run_cli(*args)
            explicit = self.run_cli(*args, "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
            self.assertEqual(legacy.returncode, 2)

    def test_replay_and_serialization_checkpoint_modes_remain_route_owned(self):
        args = (*self.BASE, "--verification", "replay",
                "--verification-cancellation", "grid-query-boundaries",
                "--serialization-cancellation", "grid-record-checkpoints",
                "--max-session-checkpoints", "1000000")
        legacy = self.run_cli(*args)
        explicit = self.run_cli(*args, "--compatibility-family", "session")
        self.assert_byte_identical(legacy, explicit)
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        self.assertTrue(json.loads(explicit.stdout)["execution"]["replay_verified"])

    def test_family_validation_precedes_paths_and_route_processing(self):
        for family, error in (
            ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
            ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
        ):
            result = self.run_cli("grid-batch", "--paths", "missing-invalid-path",
                                  "--cache-bytes", "bad", "--compatibility-family", family)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(json.loads(result.stderr), {"error": error})

    def test_parser_contract_and_non_target_boundaries_remain(self):
        duplicate = self.run_cli(*self.BASE, "--compatibility-family", "session",
                                  "--compatibility-family", "session")
        self.assertEqual(json.loads(duplicate.stderr),
                         {"error": "DuplicateOption: --compatibility-family"})
        missing = self.run_cli(*self.BASE, "--compatibility-family")
        self.assertEqual(json.loads(missing.stderr),
                         {"error": "MissingOptionValue: --compatibility-family"})
        for command, args in (
            ("compare", ("--left-numerator", "1", "--left-denominator", "1",
                          "--right-numerator", "1", "--right-denominator", "1")),
            ("grid", ("--path", "2,6")),
        ):
            legacy = self.run_cli(command, *args)
            explicit = self.run_cli(command, *args, "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
        for command, args in (
            ("grid-resume", ("--from-run", "missing.bruce", "--additional-queries", "1")),
            ("research", ("--train", "missing.train")),
            ("compatibility", ("--rules", "missing.rules", "--candidates", "missing.candidates")),
        ):
            legacy = self.run_cli(command, *args)
            explicit = self.run_cli(command, *args, "--compatibility-family", "session")
            self.assert_byte_identical(legacy, explicit)
        replay = self.run_cli("replay", "missing.bruce", "--compatibility-family", "session")
        self.assertEqual(replay.returncode, 2)
        self.assertIn(b"Usage: bruce replay", replay.stderr)


if __name__ == "__main__":
    unittest.main()
