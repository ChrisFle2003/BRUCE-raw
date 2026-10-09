"""Builder CLI contracts; only published RFC test material is used."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BUILDER = Path(sys.argv.pop(1)).resolve()
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
        self.args = ["--manifest", "manifest.json", "--tree", "tree.json", "--golden", "golden.json", "--output", "example.brucemod", "--trust-store-output", "trust.pub"]

    def run_builder(self, *args):
        return subprocess.run([str(BUILDER), *args], cwd=self.root, capture_output=True, text=True)

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
        result = self.run_builder(*self.args)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "example.brucemod").exists())
        self.assertFalse((self.root / "trust.pub").exists())


if __name__ == "__main__":
    unittest.main()
