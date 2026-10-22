"""Verify the Stage-1 compatibility-family gate adapter boundary."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


BIN = Path(__import__("sys").argv[1]).resolve()
FIXTURE = Path(__file__).resolve().parents[1] / "fixtures" / "research-v1.bruce"
__import__("sys").argv = [__import__("sys").argv[0]]


class GateCompatibilityAdapter(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run(
            [str(BIN), *map(str, args)],
            capture_output=True,
            timeout=30,
        )

    def test_explicit_gate_is_byte_identical_to_legacy_route(self):
        legacy = self.run_cli("gate-run", FIXTURE)
        explicit = self.run_cli(
            "gate-run", FIXTURE, "--compatibility-family", "gate"
        )
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertEqual(legacy.stdout, explicit.stdout)
        self.assertEqual(legacy.stderr, explicit.stderr)

    def test_help_advertises_only_the_gate_family_value(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        help_text = result.stdout.decode()
        self.assertIn(
            "bruce gate-run RUN.bruce [--compatibility-family gate]",
            help_text,
        )

    def test_existing_gate_options_remain_combinable(self):
        result = self.run_cli(
            "gate-run", FIXTURE,
            "--compatibility-family", "gate",
            "--max-replay-bytes", FIXTURE.stat().st_size,
            "--max-session-checkpoints", "1000000",
            "--cancellation", "none",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["result"],
                         json.loads(self.run_cli("gate-run", FIXTURE).stdout))
        self.assertEqual(payload["usage"]["model"], "session-checkpoints-v1")

    def test_family_validation_precedes_input_file_read(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-gate-") as directory:
            missing = Path(directory) / "must-not-be-read.bruce"
            for family, expected in (
                ("session", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
            ):
                result = self.run_cli(
                    "gate-run", missing, "--compatibility-family", family
                )
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(json.loads(result.stderr), {"error": expected})
                self.assertFalse(missing.exists())

    def test_option_contract_rejects_duplicates_missing_values_and_other_families(self):
        cases = (
            (("gate-run", FIXTURE, "--compatibility-family"),
             "MissingOptionValue: --compatibility-family"),
            (("gate-run", FIXTURE, "--compatibility-family", "gate",
              "--compatibility-family", "gate"),
             "DuplicateOption: --compatibility-family"),
            (("gate-run", FIXTURE, "--compatibility-family", "gate",
              "--max-transfer-checkpoints", "1"),
             "UnknownOption: --max-transfer-checkpoints"),
        )
        for args, expected in cases:
            result = self.run_cli(*args)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(json.loads(result.stderr), {"error": expected})

    def test_adapter_option_is_not_added_to_other_routes(self):
        result = self.run_cli("replay", FIXTURE, "--compatibility-family", "gate")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(json.loads(result.stderr),
                         {"error": "Usage: bruce replay RUN.bruce"})


if __name__ == "__main__":
    unittest.main()
