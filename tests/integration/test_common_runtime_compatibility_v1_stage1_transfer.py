"""Verify the Stage-1 compatibility-family transfer adapter boundary."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


BIN = Path(__import__("sys").argv[1]).resolve()
__import__("sys").argv = [__import__("sys").argv[0]]


class TransferCompatibilityAdapter(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run(
            [str(BIN), *map(str, args)],
            capture_output=True,
            timeout=60,
        )

    def assert_error(self, result, expected):
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(json.loads(result.stderr), {"error": expected})

    def make_inputs(self, root):
        train = root / "train.csv"
        holdout = root / "holdout.csv"
        train.write_text("x,y\n0,2\n1,3/2\n2,4/3\n3,5/4\n")
        holdout.write_text("x,y\n5,7/6\n")
        source = root / "source.bruce"
        result = self.run_cli(
            "research", "--strategy", "portfolio-rational", "--max-degree", "2",
            "--train", train, "--holdout", holdout, "--output", source,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        rules = root / "rules"
        rules.write_text(
            "BRUCE_RULES_V1\n"
            "snapshot a\n"
            "rule positive positive value ge 0 0 1 manual -\n"
            "rule context context layer eq 0 0 0 manual -\n"
        )
        compatibility_source = Path(__file__).resolve().parents[1] / "fixtures" / "research-v1.bruce"
        return source, compatibility_source, rules

    def test_explicit_transfer_is_byte_identical_on_all_routes(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-transfer-") as directory:
            root = Path(directory)
            source, compatibility_source, rules = self.make_inputs(root)

            cases = [
                (
                    ["compatibility-plan"],
                    ["--from-run", compatibility_source, "--rules", rules, "--at", "3"],
                    root / "compat-legacy.plan", root / "compat-explicit.plan",
                ),
                (
                    ["experience", "plan-add"],
                    ["--from-run", source],
                    root / "experience-plan-legacy", root / "experience-plan-explicit",
                ),
            ]
            for command, options, legacy_output, explicit_output in cases:
                legacy = self.run_cli(*command, *options, "--output", legacy_output)
                explicit = self.run_cli(
                    *command, *options, "--compatibility-family", "transfer",
                    "--output", explicit_output,
                )
                self.assertEqual(legacy.returncode, 0, legacy.stderr)
                self.assertEqual(explicit.returncode, 0, explicit.stderr)
                self.assertEqual(explicit.stdout, legacy.stdout)
                self.assertEqual(explicit.stderr, legacy.stderr)
                self.assertEqual(explicit_output.read_bytes(), legacy_output.read_bytes())

            compatibility_apply = [
                "compatibility-apply", root / "compat-legacy.plan",
                "--from-run", compatibility_source, "--rules", rules,
            ]
            legacy = self.run_cli(*compatibility_apply, "--output", root / "apply-legacy")
            explicit = self.run_cli(
                *compatibility_apply, "--compatibility-family", "transfer",
                "--output", root / "apply-explicit",
            )
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            self.assertEqual(explicit.stdout, legacy.stdout)
            self.assertEqual(explicit.stderr, legacy.stderr)
            self.assertEqual(
                (root / "apply-explicit").read_bytes(),
                (root / "apply-legacy").read_bytes(),
            )

            baseline = self.run_cli(
                "experience", "add", "--from-run", source,
                "--output", root / "baseline",
            )
            self.assertEqual(baseline.returncode, 0, baseline.stderr)
            baseline_report = json.loads(baseline.stdout)
            record_id = baseline_report["records"][0]["id"]

            plan_apply = [
                "experience", "apply-add", root / "experience-plan-legacy",
                "--from-run", source,
            ]
            legacy = self.run_cli(*plan_apply, "--output", root / "apply-add-legacy")
            explicit = self.run_cli(
                *plan_apply, "--compatibility-family", "transfer",
                "--output", root / "apply-add-explicit",
            )
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            self.assertEqual(explicit.stdout, legacy.stdout)
            self.assertEqual(explicit.stderr, legacy.stderr)
            self.assertEqual(
                (root / "apply-add-explicit").read_bytes(),
                (root / "apply-add-legacy").read_bytes(),
            )

            for action, args, legacy_name, explicit_name in (
                (
                    "add", ["--from-run", source],
                    "add-legacy", "add-explicit",
                ),
                (
                    "revoke", ["--store", root / "baseline", "--id", record_id,
                                "--reason", "stage1"],
                    "revoke-legacy", "revoke-explicit",
                ),
            ):
                legacy = self.run_cli(
                    "experience", action, *args, "--output", root / legacy_name,
                )
                explicit = self.run_cli(
                    "experience", action, *args,
                    "--compatibility-family", "transfer",
                    "--output", root / explicit_name,
                )
                self.assertEqual(legacy.returncode, 0, legacy.stderr)
                self.assertEqual(explicit.returncode, 0, explicit.stderr)
                self.assertEqual(explicit.stdout, legacy.stdout)
                self.assertEqual(explicit.stderr, legacy.stderr)
                self.assertEqual(
                    (root / explicit_name).read_bytes(),
                    (root / legacy_name).read_bytes(),
                )

            # Replace uses the same valid source as the existing active record;
            # use a changed source so the native replacement contract is exercised.
            changed_train = root / "changed-train.csv"
            changed_holdout = root / "changed-holdout.csv"
            changed_train.write_text("x,y\n0,3\n1,5/2\n2,7/3\n3,9/4\n")
            changed_holdout.write_text("x,y\n5,13/6\n")
            changed_source = root / "changed-source.bruce"
            changed = self.run_cli(
                "research", "--strategy", "portfolio-rational", "--max-degree", "2",
                "--train", changed_train, "--holdout", changed_holdout,
                "--output", changed_source,
            )
            self.assertEqual(changed.returncode, 0, changed.stderr)
            replace_args = [
                "--store", root / "baseline", "--id", record_id,
                "--reason", "stage1", "--from-run", changed_source,
            ]
            legacy = self.run_cli(
                "experience", "replace", *replace_args,
                "--output", root / "replace-legacy",
            )
            explicit = self.run_cli(
                "experience", "replace", *replace_args,
                "--compatibility-family", "transfer",
                "--output", root / "replace-explicit",
            )
            self.assertEqual(legacy.returncode, 0, legacy.stderr)
            self.assertEqual(explicit.returncode, 0, explicit.stderr)
            self.assertEqual(explicit.stdout, legacy.stdout)
            self.assertEqual(explicit.stderr, legacy.stderr)
            self.assertEqual(
                (root / "replace-explicit").read_bytes(),
                (root / "replace-legacy").read_bytes(),
            )

    def test_help_lists_transfer_family_on_all_routes(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        help_text = result.stdout.decode()
        for fragment in (
            "bruce compatibility-plan --from-run RUN --rules RULES --at X --output PLAN [--compatibility-family transfer]",
            "bruce compatibility-apply PLAN --from-run RUN --rules RULES [--output RUN] [--compatibility-family transfer]",
            "bruce experience plan-add --from-run RUN [--store STORE] [--max-source-bytes N] [--max-store-bytes N] --output PLAN [--compatibility-family transfer]",
            "bruce experience apply-add PLAN --from-run RUN [--store STORE] --output NEW [--compatibility-family transfer]",
            "bruce experience add --from-run RUN.bruce [--store OLD.experience] --output NEW.experience [--compatibility-family transfer]",
            "bruce experience revoke --store OLD.experience --id ID --reason TEXT --output NEW.experience [--compatibility-family transfer]",
            "bruce experience replace --store STORE --id ID --reason TEXT --from-run RUN --output STORE [--compatibility-family transfer]",
        ):
            self.assertIn(fragment, help_text)

    def test_family_validation_precedes_io(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-transfer-") as directory:
            root = Path(directory)
            missing = root / "must-not-be-read"
            routes = (
                ("compatibility-plan", "--from-run", missing, "--rules", missing,
                 "--at", "3", "--output", root / "plan-out"),
                ("compatibility-apply", missing, "--from-run", missing,
                 "--rules", missing, "--output", root / "apply-out"),
                ("experience", "plan-add", "--from-run", missing,
                 "--output", root / "experience-plan-out"),
                ("experience", "apply-add", missing, "--from-run", missing,
                 "--output", root / "experience-apply-out"),
                ("experience", "add", "--from-run", missing,
                 "--output", root / "add-out"),
                ("experience", "revoke", "--store", missing, "--id", "missing",
                 "--reason", "stage1", "--output", root / "revoke-out"),
                ("experience", "replace", "--store", missing, "--id", "missing",
                 "--reason", "stage1", "--from-run", missing,
                 "--output", root / "replace-out"),
            )
            for route in routes:
                for family, expected in (
                    ("session", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("gate", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("management", "CommonRuntimeCompatibilityV1FamilyMismatch"),
                    ("unknown", "UnknownCommonRuntimeCompatibilityV1Family"),
                ):
                    result = self.run_cli(*route, "--compatibility-family", family)
                    self.assert_error(result, expected)
                    self.assertFalse(any(path.exists() for path in root.iterdir()
                                         if path.name.endswith("-out")))

    def test_transfer_options_remain_combinable(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-transfer-") as directory:
            root = Path(directory)
            source, compatibility_source, rules = self.make_inputs(root)
            result = self.run_cli(
                "compatibility-plan", "--from-run", compatibility_source, "--rules", rules,
                "--at", "3", "--compatibility-family", "transfer",
                "--max-transfer-checkpoints", "1000000",
                "--transfer-cancellation", "none", "--output", root / "plan",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["contract"], "controlled-transfer-v1")
            self.assertEqual(payload["usage"]["model"], "transfer-checkpoints-v1")

            budget = self.run_cli(
                "experience", "add", "--from-run", source,
                "--compatibility-family", "transfer",
                "--max-transfer-checkpoints", "0", "--output", root / "stopped",
            )
            self.assertEqual(budget.returncode, 2)
            self.assertEqual(budget.stdout, b"")
            self.assertEqual(
                json.loads(budget.stderr)["error"],
                "TransferCheckpointBudgetExceeded: checkpoints=0 limit=0",
            )
            self.assertFalse((root / "stopped").exists())

    def test_duplicate_missing_and_foreign_options_keep_route_contract(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-transfer-") as directory:
            root = Path(directory)
            target = root / "missing"
            missing_value = self.run_cli(
                "compatibility-plan", "--compatibility-family",
            )
            self.assert_error(missing_value,
                              "MissingOptionValue: --compatibility-family")
            duplicate = self.run_cli(
                "experience", "add", "--from-run", target,
                "--compatibility-family", "transfer",
                "--compatibility-family", "transfer", "--output", root / "out",
            )
            self.assert_error(duplicate, "DuplicateOption: --compatibility-family")
            foreign = self.run_cli(
                "compatibility-plan", "--compatibility-family", "transfer",
                "--max-management-checkpoints", "1",
            )
            self.assert_error(foreign, "UnknownOption: --max-management-checkpoints")
            unrelated = self.run_cli(
                "compatibility", "--compatibility-family", "transfer",
            )
            # The generic compatibility route is now a Session-adapter target;
            # transfer remains a known but foreign family there.
            self.assert_error(unrelated,
                              "CommonRuntimeCompatibilityV1FamilyMismatch")

    def test_explicit_transfer_does_not_emit_descriptor(self):
        with tempfile.TemporaryDirectory(prefix="bruce-stage1-transfer-") as directory:
            root = Path(directory)
            source, compatibility_source, rules = self.make_inputs(root)
            before_source = compatibility_source.read_bytes()
            before_rules = rules.read_bytes()
            result = self.run_cli(
                "compatibility-plan", "--from-run", compatibility_source, "--rules", rules,
                "--at", "3", "--compatibility-family", "transfer",
                "--output", root / "plan",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn(b"common-runtime-compatibility", result.stdout)
            self.assertEqual(compatibility_source.read_bytes(), before_source)
            self.assertEqual(rules.read_bytes(), before_rules)


if __name__ == "__main__":
    unittest.main()
