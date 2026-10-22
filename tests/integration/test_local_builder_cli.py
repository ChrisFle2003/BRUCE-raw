"""Builder CLI contracts; only published RFC test material is used."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BUILDER = Path(sys.argv.pop(1)).resolve()
FAULT_BUILDER = Path(sys.argv.pop(1)).resolve()
# Public RFC 8032 vector, NEVER a private operator signing key.
TEST_SEED = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60"


class BuilderCLI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-builder-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        env = self.root / ".env"
        env.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + TEST_SEED + "\n")
        env.chmod(0o600)
        source = {
            "manifest.json": {"mod_id": "user.example", "signing_key_id": "test-rfc8032", "version": "1.0.0"},
            "tree.json": {"mount": [1, 1], "nodes": [{"aliases": ["hoch"], "claim_scope": "candidate_only", "id": [1, 1, 5], "key": "power", "route_pattern": ["<rational>", "hoch", "<integer>"], "template_id": "math.rational.power.v1"}]},
            "golden.json": [{"input": "3/4 hoch 2", "status": "MATCHED", "template_id": "math.rational.power.v1"}],
        }
        for name, value in source.items():
            (self.root / name).write_text(json.dumps(value, sort_keys=True, separators=(",", ":")))
        self.source = self.root / "data-source"
        self.source.mkdir()
        for old, new in (("manifest.json", "manifest.json"), ("tree.json", "tree.json"),
                         ("golden.json", "golden-cases.json")):
            (self.source / new).write_bytes((self.root / old).read_bytes())
        (self.source / "ignored.txt").write_text("not packaged")
        self.args = ["--manifest", "manifest.json", "--tree", "tree.json", "--golden", "golden.json", "--output", "example.brucemod", "--trust-store-output", "trust.pub"]

    def run_builder(self, *args):
        return subprocess.run([str(BUILDER), *args], cwd=self.root, capture_output=True, text=True)

    def modern_args(self):
        return ["data", self.source, "--output", "modern.brucemod",
                "--trust-store-output", "modern.pub", "--signing-env", self.root / ".env"]

    def assert_failure(self, args, *, modern=True):
        result = subprocess.run([BUILDER, *args], cwd=self.root,
                                capture_output=True, timeout=30)
        diagnostic = result.stdout + result.stderr
        for secret in (TEST_SEED, "private-sentinel", str(self.root), str(self.source),
                       ".env", "manifest.json", "tree.json", "golden-cases.json"):
            self.assertNotIn(secret.encode(), diagnostic)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(result.stderr, b'{"error":"LocalModBuildFailed"}\n'
                         if modern else b"LocalModBuildFailed\n")
        self.assertEqual(result.returncode, 2 if modern else 1)

    def test_modern_argument_errors_are_bounded_json_exit_two(self):
        base = self.modern_args()
        cases = {
            "missing source": ["data"],
            "empty source": ["data", "", *base[2:]],
            "missing signing option": base[:-2],
            "missing option value": base[:-1],
            "empty option value": [*base[:-1], ""],
            "duplicate option": [*base, "--output", "private-sentinel"],
            "unexpected option": [*base, "--private-sentinel", TEST_SEED],
            "unexpected positional": [*base, "private-sentinel"],
        }
        for name, args in cases.items():
            with self.subTest(name=name):
                self.assert_failure(args)
        self.assertFalse((self.root / "modern.brucemod").exists())
        self.assertFalse((self.root / "modern.pub").exists())

    def test_modern_missing_unsafe_and_malformed_signing_are_redacted(self):
        signing = self.root / ".env"
        link = self.root / "private-sentinel-key-link"
        link.symlink_to(signing)
        for name, path in (("missing", self.root / "private-sentinel-missing-key"),
                           ("symlink", link)):
            with self.subTest(name=name):
                self.assert_failure([*self.modern_args()[:-1], path])
        signing.chmod(0o644)
        self.assert_failure(self.modern_args())
        signing.chmod(0o600)
        signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + TEST_SEED + "private-sentinel\n")
        self.assert_failure(self.modern_args())
        self.assertFalse((self.root / "modern.brucemod").exists())
        self.assertFalse((self.root / "modern.pub").exists())

    def test_modern_missing_unsafe_and_malformed_sources_are_redacted(self):
        base = self.modern_args()
        link = self.root / "private-sentinel-source-link"
        link.symlink_to(self.source, target_is_directory=True)
        for source in (self.root / "private-sentinel-missing-source", link):
            with self.subTest(source=source.name):
                self.assert_failure(["data", source, *base[2:]])
        for name in ("manifest.json", "tree.json", "golden-cases.json"):
            path = self.source / name
            original = path.read_bytes()
            for kind in ("missing", "symlink", "malformed"):
                with self.subTest(source=name, kind=kind):
                    path.unlink()
                    if kind == "symlink":
                        path.symlink_to(self.root / "manifest.json")
                    elif kind == "malformed":
                        path.write_text('{"private-sentinel":')
                    try:
                        self.assert_failure(base)
                    finally:
                        if path.is_symlink() or path.exists():
                            path.unlink()
                        path.write_bytes(original)
        self.assertFalse((self.root / "modern.brucemod").exists())
        self.assertFalse((self.root / "modern.pub").exists())

    def test_modern_output_collisions_are_redacted_and_preserve_existing_bytes(self):
        for name in ("modern.brucemod", "modern.pub"):
            path = self.root / name
            path.write_bytes(b"private-sentinel-existing-output")
            try:
                with self.subTest(output=name):
                    self.assert_failure(self.modern_args())
                    self.assertEqual(path.read_bytes(), b"private-sentinel-existing-output")
                    other = "modern.pub" if name == "modern.brucemod" else "modern.brucemod"
                    self.assertFalse((self.root / other).exists())
            finally:
                path.unlink()
        base = self.modern_args()
        self.assert_failure([*base[:5], "modern.brucemod", *base[6:]])
        self.assertFalse((self.root / "modern.brucemod").exists())

    def test_data_report_serialization_failure_leaves_no_outputs_and_retry_succeeds(self):
        result = subprocess.run([FAULT_BUILDER, *self.modern_args()], cwd=self.root,
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stderr, b'{"error":"LocalModBuildFailed"}\n')
        self.assertEqual(result.stdout, b"")
        self.assertFalse((self.root / "modern.brucemod").exists())
        self.assertFalse((self.root / "modern.pub").exists())
        retry = subprocess.run([BUILDER, *self.modern_args()], cwd=self.root,
                               capture_output=True, timeout=30)
        self.assertEqual(retry.returncode, 0, retry.stderr)
        self.assertTrue((self.root / "modern.brucemod").exists())
        self.assertTrue((self.root / "modern.pub").exists())

    def test_modern_unsafe_output_paths_are_redacted(self):
        unsafe = self.root / "private-sentinel-unsafe-directory"
        unsafe.mkdir(mode=0o700)
        unsafe.chmod(0o777)
        link = self.root / "private-sentinel-output-link"
        link.symlink_to(self.source, target_is_directory=True)
        for directory in (unsafe, link, self.root / "private-sentinel-missing-directory"):
            with self.subTest(directory=directory.name):
                base = self.modern_args()
                self.assert_failure([*base[:3], directory / "output.brucemod", *base[4:]])
                self.assertFalse((directory / "output.brucemod").exists())
                self.assertFalse((self.root / "modern.pub").exists())

    def test_legacy_argument_errors_remain_plaintext_exit_one(self):
        for args in ([], self.args[:-2], [*self.args, "--output", "private-sentinel"],
                     [*self.args, "--private-sentinel", TEST_SEED]):
            with self.subTest(args=args):
                self.assert_failure(args, modern=False)

    def test_legacy_source_and_collision_errors_remain_plaintext_exit_one(self):
        manifest = self.root / "manifest.json"
        original = manifest.read_bytes()
        manifest.write_bytes(b"private-sentinel-malformed-input")
        self.assert_failure(self.args, modern=False)
        manifest.write_bytes(original)
        package = self.root / "example.brucemod"
        package.write_bytes(b"private-sentinel-existing-output")
        self.assert_failure(self.args, modern=False)
        self.assertEqual(package.read_bytes(), b"private-sentinel-existing-output")
        self.assertFalse((self.root / "trust.pub").exists())

    def test_builds_from_current_project_key_and_reports_only_public_identity(self):
        result = self.run_builder(*self.args)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["mod_id"], "user.example")
        self.assertEqual(report["version"], "1.0.0")
        self.assertEqual(len(report["package_digest"]), 64)
        self.assertNotIn(TEST_SEED, result.stdout + result.stderr)
        self.assertTrue((self.root / "example.brucemod").exists())
        self.assertTrue((self.root / "trust.pub").exists())
        before = (self.root / "example.brucemod").read_bytes()
        again = self.run_builder(*self.args)
        self.assertNotEqual(again.returncode, 0)
        self.assertEqual((self.root / "example.brucemod").read_bytes(), before)

    def test_data_subcommand_is_byte_equivalent_to_legacy_grammar(self):
        legacy = self.run_builder(*self.args)
        self.assertEqual(legacy.returncode, 0, legacy.stderr)
        modern = self.run_builder("data", self.source, "--output", "modern.brucemod",
                                  "--trust-store-output", "modern.pub",
                                  "--signing-env", self.root / ".env")
        self.assertEqual(modern.returncode, 0, modern.stderr)
        report = json.loads(modern.stdout)
        expected = ('{"contract":"local-data-mod-build-v1","mod_id":"user.example",'
                    '"version":"1.0.0","package_digest":"' + report["package_digest"] + '"}\n')
        self.assertEqual(modern.stdout, expected)
        self.assertEqual(legacy.stdout, expected)
        self.assertEqual((self.root / "modern.brucemod").read_bytes(),
                         (self.root / "example.brucemod").read_bytes())
        self.assertEqual((self.root / "modern.pub").read_bytes(),
                         (self.root / "trust.pub").read_bytes())

    def test_data_subcommand_requires_exact_options_and_safe_signing_file(self):
        base = ["data", self.source, "--output", "modern.brucemod",
                "--trust-store-output", "modern.pub", "--signing-env", self.root / ".env"]
        for args in (base[:-2], base + ["--unknown", "private-sentinel"],
                     ["data", self.source, *base[2:], "--output", "other.brucemod"]):
            result = self.run_builder(*map(str, args))
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn("private-sentinel", result.stdout + result.stderr)
        link = self.root / "key-link.env"
        link.symlink_to(self.root / ".env")
        unsafe = [str(value) if value != self.root / ".env" else str(link) for value in base]
        result = self.run_builder(*unsafe)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "modern.brucemod").exists())

    def test_help_needs_no_key(self):
        (self.root / ".env").unlink()
        result = self.run_builder("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("--trust-store-output", result.stdout)

    def test_secret_override_and_unknown_duplicate_options_fail_without_echo(self):
        for extra in (["--env", "private-sentinel"], ["--key", TEST_SEED], ["--output", "other.brucemod"]):
            result = self.run_builder(*self.args, *extra)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn(TEST_SEED, result.stdout + result.stderr)
            self.assertNotIn("private-sentinel", result.stdout + result.stderr)
            self.assertFalse((self.root / "example.brucemod").exists())
            self.assertFalse((self.root / "trust.pub").exists())

    def test_missing_key_has_no_outputs(self):
        (self.root / ".env").unlink()
        self.assert_failure(self.args, modern=False)
        self.assertFalse((self.root / "example.brucemod").exists())
        self.assertFalse((self.root / "trust.pub").exists())


if __name__ == "__main__":
    unittest.main()
