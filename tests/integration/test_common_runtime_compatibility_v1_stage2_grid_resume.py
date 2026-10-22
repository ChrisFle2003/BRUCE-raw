"""Exercise the bounded grid-resume host against the real CLI and bundles."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = Path(sys.argv.pop(1)).resolve()


class CommonRuntimeHostGridResume(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="bruce-stage2-grid-resume-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.parent = self.root / "parent"
        generated = self.run_cli("grid-batch", "--paths", "2,6;02,06;0,0;4,3;2,6",
                                 "--zero-zero", "stop", "--max-query-steps", "1",
                                 "--output", self.parent)
        self.assertEqual(generated.returncode, 0, generated.stderr)
        self.assertEqual(json.loads(generated.stdout)["status"], "INCOMPLETE")

    def run_cli(self, *args):
        return subprocess.run([str(BIN), *map(str, args)], capture_output=True,
                              timeout=60)

    def resume_args(self, additional="2", parent=None):
        return ("grid-resume", "--from-run", parent or self.parent,
                "--additional-queries", str(additional))

    def assert_pair(self, args, code=0):
        legacy = self.run_cli(*args)
        explicit = self.run_cli(*args, "--compatibility-family", "session")
        self.assertEqual(legacy.returncode, code, legacy.stderr)
        self.assertEqual(explicit.returncode, legacy.returncode, explicit.stderr)
        self.assertEqual(explicit.stdout, legacy.stdout)
        self.assertEqual(explicit.stderr, legacy.stderr)
        return json.loads(legacy.stdout if code == 0 else legacy.stderr)

    def test_complete_partial_and_chained_bundles_are_byte_identical(self):
        for additional, status, generation in ((2, "INCOMPLETE", 1),
                                                (4, "COMPLETED", 1)):
            with self.subTest(additional=additional):
                old = self.root / f"legacy-{additional}"
                new = self.root / f"explicit-{additional}"
                args = self.resume_args(additional)
                legacy = self.run_cli(*args, "--output", old)
                explicit = self.run_cli(*args, "--output", new,
                                        "--compatibility-family", "session")
                self.assertEqual(legacy.returncode, 0, legacy.stderr)
                self.assertEqual(explicit.returncode, 0, explicit.stderr)
                self.assertEqual(explicit.stdout, legacy.stdout)
                self.assertEqual(explicit.stderr, legacy.stderr)
                self.assertEqual(new.read_bytes(), old.read_bytes())
                result = json.loads(explicit.stdout)
                self.assertEqual(result["status"], status)
                self.assertEqual(result["generation"], generation)
                replay = self.run_cli("replay", new)
                self.assertEqual(replay.returncode, 0, replay.stderr)
                self.assertEqual(json.loads(replay.stdout), result)
        final = self.assert_pair(self.resume_args(2, self.root / "explicit-2"))
        self.assertEqual(final["status"], "COMPLETED")
        self.assertEqual(final["generation"], 2)

    def test_native_and_checked_errors_keep_envelopes_and_exit_codes(self):
        invalid = self.root / "invalid"
        invalid.write_text("invalid parent bundle")
        complete = self.root / "complete"
        generated = self.run_cli("grid-batch", "--paths", "2,6", "--output", complete)
        self.assertEqual(generated.returncode, 0, generated.stderr)
        cases = [self.resume_args(n) for n in (0, 1025, "bad", -1)]
        cases.extend(self.resume_args(parent=p) for p in
                     (self.root / "missing", invalid, complete))
        cases.extend((*self.resume_args(parent=p), "--verification", "replay")
                     for p in (invalid, complete))
        for index, args in enumerate(cases):
            target = self.root / f"denied-{index}"
            with self.subTest(args=args):
                report = self.assert_pair((*args, "--output", target), code=2)
                self.assertIn("error", report)
                self.assertFalse(target.exists())

    def test_replay_checkpoint_modes_and_export_bytes_remain_route_owned(self):
        old = self.root / "plain"
        new = self.root / "checked"
        args = (*self.resume_args(), "--verification", "replay",
                "--execution-cancellation", "resume-import-checkpoints",
                "--verification-cancellation", "grid-query-boundaries",
                "--serialization-cancellation", "grid-record-checkpoints",
                "--max-session-checkpoints", "1000000")
        legacy = self.run_cli(*args, "--output", old)
        explicit = self.run_cli(*args, "--output", new,
                                "--compatibility-family", "session")
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertEqual(explicit.stdout, legacy.stdout)
        self.assertEqual(explicit.stderr, legacy.stderr)
        self.assertEqual(old.read_bytes(), new.read_bytes())
        result = json.loads(explicit.stdout)
        self.assertTrue(result["execution"]["replay_verified"])
        self.assertEqual(result["execution"]["contract"], "application-execution-v14")
        self.assertEqual(result["execution"]["execution_cancellation"],
                         "resume-import-checkpoints")
        self.assertEqual(result["result"]["status"], "INCOMPLETE")

    def test_session_and_byte_budget_failures_preserve_output(self):
        target = self.root / "preserved"
        cases = (
            (("--max-session-checkpoints", "0"), "EXECUTING"),
            (("--max-replay-bytes", "0"), "REPLAYING"),
            (("--max-serialized-bytes", "0"), "SERIALIZING"),
        )
        for extra, phase in cases:
            for exists in (False, True):
                with self.subTest(extra=extra, exists=exists):
                    if exists:
                        target.write_bytes(b"keep existing output")
                    args = (*self.resume_args(), "--verification", "replay",
                            *extra, "--output", target)
                    report = self.assert_pair(args, code=2)
                    self.assertEqual(report["execution"]["failed_at"], phase)
                    self.assertNotIn("result", report)
                    if exists:
                        self.assertEqual(target.read_bytes(), b"keep existing output")
                        target.unlink()
                    else:
                        self.assertFalse(target.exists())

    def test_control_admission_errors_are_byte_identical(self):
        for extra in (("--execution-cancellation", "resume-import-checkpoints"),
                      ("--verification-cancellation", "grid-query-boundaries"),
                      ("--max-session-checkpoints", "1"),
                      ("--verification", "replay", "--execution-cancellation", "unknown"),
                      ("--verification", "replay", "--serialization-cancellation", "unknown")):
            with self.subTest(extra=extra):
                self.assert_pair((*self.resume_args(), *extra), code=2)

    def test_family_validation_precedes_parent_read_queries_and_output(self):
        invalid = self.root / "invalid"
        invalid.write_bytes(b"not a bundle")
        target = self.root / "output"
        for parent in (self.root / "missing", invalid):
            for family, error in (
                ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
                ("Session", "UnknownCommonRuntimeCompatibilityV1Family"),
            ):
                for exists in (False, True):
                    with self.subTest(parent=parent, family=family, exists=exists):
                        if exists:
                            target.write_bytes(b"preserve")
                        result = self.run_cli(*self.resume_args("bad", parent),
                                              "--output", target,
                                              "--compatibility-family", family)
                        self.assertEqual(result.returncode, 2)
                        self.assertEqual(result.stdout, b"")
                        self.assertEqual(json.loads(result.stderr), {"error": error})
                        if exists:
                            self.assertEqual(target.read_bytes(), b"preserve")
                            target.unlink()
                        else:
                            self.assertFalse(target.exists())

    def test_parent_read_still_precedes_additional_query_parsing(self):
        missing = self.root / "missing"
        read_error = self.assert_pair(self.resume_args(1, missing), code=2)
        self.assertEqual(self.assert_pair(self.resume_args("bad", missing), code=2),
                         read_error)
        # With readable but invalid bytes, integer parsing still precedes decoding.
        invalid = self.root / "invalid"
        invalid.write_bytes(b"not a bundle")
        parsed = self.assert_pair(self.resume_args("bad", invalid), code=2)
        self.assertEqual(parsed, {"error": "InvalidInteger: bad"})

    def test_parser_boundaries_and_other_routes_keep_their_contracts(self):
        for suffix, error in (
            (("--compatibility-family",), "MissingOptionValue: --compatibility-family"),
            (("--compatibility-family", "session", "--compatibility-family", "session"),
             "DuplicateOption: --compatibility-family"),
        ):
            result = self.run_cli(*self.resume_args(), *suffix)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(json.loads(result.stderr), {"error": error})
        self.assert_pair(("grid-batch", "--paths", "2,6;4,3", "--max-query-steps", "1"))
        self.assert_pair(("research", "--train", self.root / "missing.csv"), code=2)
        self.assert_pair(("compatibility", "--rules", self.root / "missing.rules",
                          "--candidates", self.root / "missing.csv"), code=2)
        replay = self.run_cli("replay", self.parent, "--compatibility-family", "session")
        self.assertEqual(replay.returncode, 2)
        self.assertEqual(replay.stdout, b"")
        self.assertIn(b"Usage: bruce replay", replay.stderr)


if __name__ == "__main__":
    unittest.main()
