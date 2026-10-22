"""Verify the Stage-1 compatibility-family management adapter boundary."""

import json
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import tempfile
import time
import unittest


BIN = Path(__import__("sys").argv.pop(1)).resolve()
class ManagementCompatibilityAdapter(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run(
            [str(BIN), *map(str, args)],
            capture_output=True,
            timeout=30,
        )

    def assert_error(self, result, expected):
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(json.loads(result.stderr), {"error": expected})

    def create_rule_store(self, root):
        store = root / "rules.store"
        new_rules = root / "new.rules"
        proposal = root / "proposal.rules"
        new_rules.write_text(
            "BRUCE_RULES_V1\n"
            "snapshot stage1\n"
            "rule first positive value ge 0 0 1 manual -\n"
        )
        staged = self.run_cli("rules-stage", store, new_rules, proposal)
        self.assertEqual(staged.returncode, 0, staged.stderr)

        master, slave = pty.openpty()
        process = subprocess.Popen(
            [str(BIN), "rules-activate", str(store), str(proposal)],
            stdin=slave,
            stdout=slave,
            stderr=slave,
        )
        os.close(slave)
        output = b""
        deadline = time.monotonic() + 5
        while b"\n> " not in output and time.monotonic() < deadline:
            if select.select([master], [], [], 0.1)[0]:
                output += os.read(master, 65536)
        self.assertIn(b"CONFIRM ", output)
        token = re.search(rb"CONFIRM ([0-9a-f]+)", output).group(1)
        os.write(master, b"CONFIRM " + token + b"\n")
        process.wait(timeout=5)
        os.close(master)
        self.assertEqual(process.returncode, 0)
        self.assertTrue(store.exists())
        return store

    def create_experience_snapshot(self, root):
        train = root / "train.csv"
        holdout = root / "holdout.csv"
        source = root / "source.bruce"
        snapshot = root / "experience.snapshot"
        train.write_text("x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n")
        holdout.write_text("x,y\n5,7/6\n")
        researched = self.run_cli(
            "research", "--strategy", "portfolio-rational", "--max-degree", "2",
            "--train", train, "--holdout", holdout, "--output", source,
        )
        self.assertEqual(researched.returncode, 0, researched.stderr)
        created = self.run_cli(
            "experience", "add", "--from-run", source, "--output", snapshot
        )
        self.assertEqual(created.returncode, 0, created.stderr)
        return snapshot

    def test_explicit_management_family_preserves_experience_inspect_bytes(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-management-") as directory:
            root = Path(directory)
            snapshot = self.create_experience_snapshot(root)
            before = snapshot.read_bytes()

            legacy = self.run_cli("experience", "inspect", snapshot)
            explicit = self.run_cli(
                "experience", "inspect", snapshot,
                "--compatibility-family", "management",
            )
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            self.assertEqual(explicit.stdout, legacy.stdout)
            self.assertEqual(explicit.stderr, legacy.stderr)
            self.assertNotIn(b"common-runtime-compatibility", explicit.stdout)
            self.assertEqual(snapshot.read_bytes(), before)

    def test_explicit_management_family_preserves_rules_status_bytes(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-management-") as directory:
            root = Path(directory)
            store = self.create_rule_store(root)
            before = store.read_bytes()

            legacy = self.run_cli("rules-status", store)
            explicit = self.run_cli(
                "rules-status", store,
                "--compatibility-family", "management",
            )
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            self.assertEqual(explicit.stdout, legacy.stdout)
            self.assertEqual(explicit.stderr, legacy.stderr)
            self.assertNotIn(b"common-runtime-compatibility", explicit.stdout)
            self.assertEqual(store.read_bytes(), before)

    def test_help_advertises_management_family_on_both_routes(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        help_text = result.stdout.decode()
        self.assertIn(
            "bruce experience inspect SNAPSHOT [--compatibility-family management]",
            help_text,
        )
        self.assertIn(
            "bruce rules-status STORE [--compatibility-family management]",
            help_text,
        )

    def test_family_validation_precedes_file_or_store_access(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-management-") as directory:
            root = Path(directory)
            missing_snapshot = root / "missing.snapshot"
            missing_store = root / "missing.store"
            for route, target in (
                (("experience", "inspect"), missing_snapshot),
                (("rules-status",), missing_store),
            ):
                for family, expected in (
                    ("session", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("transfer", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
                ):
                    result = self.run_cli(
                        *route, target, "--compatibility-family", family
                    )
                    self.assert_error(result, expected)
                    self.assertFalse(target.exists())

    def test_parser_and_legacy_option_contracts_remain_unchanged(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-management-") as directory:
            target = Path(directory) / "missing"
            cases = (
                (("experience", "inspect", target, "--compatibility-family"),
                 "Usage: bruce experience inspect SNAPSHOT"),
                (("rules-status", target, "--compatibility-family"),
                 "Usage: rules-status STORE"),
                (("experience", "inspect", target,
                  "--compatibility-family", "management",
                  "--compatibility-family", "management"),
                 "DuplicateOption: --compatibility-family"),
                (("rules-status", target,
                  "--compatibility-family", "management",
                  "--compatibility-family", "management"),
                 "DuplicateOption: --compatibility-family"),
                (("experience", "inspect", target,
                  "--compatibility-family", "management",
                  "--max-transfer-checkpoints", "1"),
                 "Usage: bruce experience inspect SNAPSHOT"),
                (("rules-status", target,
                  "--compatibility-family", "management",
                  "--max-transfer-checkpoints", "1"),
                 "Usage: rules-status STORE"),
            )
            for args, expected in cases:
                self.assert_error(self.run_cli(*args), expected)

    def test_management_budget_options_remain_combinable(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-management-") as directory:
            root = Path(directory)
            snapshot = self.create_experience_snapshot(root)
            result = self.run_cli(
                "experience", "inspect", snapshot,
                "--compatibility-family", "management",
                "--max-management-checkpoints", "1000000",
                "--management-cancellation", "none",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["contract"], "management-inspection-v1")
            self.assertEqual(payload["usage"]["model"], "management-checkpoints-v1")


if __name__ == "__main__":
    unittest.main()
