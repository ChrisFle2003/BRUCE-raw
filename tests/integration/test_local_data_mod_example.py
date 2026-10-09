"""The checked-in example must build, verify and extend only an active Tree.

Catches broken example schemas, wrong child addresses/templates, implicit
activation, and stale replay. Every signing operation uses public test material
inside a fresh temporary directory; the operator's project .env is never read.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BUILDER = Path(sys.argv.pop(1)).resolve()
BRUCE = Path(sys.argv.pop(1)).resolve()
EXAMPLE = Path(__file__).resolve().parents[2] / "examples/local-data-mods/math-rational-power"
# Published RFC 8032 test vector, unsuitable for an operator's signing key.
TEST_SEED = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60"
MOD = "user.local-math-rational-power"


class LocalExampleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-local-example-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        env = self.work / ".env"
        env.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + TEST_SEED + "\n")
        env.chmod(0o600)
        self.package = self.work / "math-rational-power-v1.brucemod"
        self.trust = self.work / "local-math-v1.pub"
        self.mods = self.work / "mods"

    def call(self, binary, *args, code=0):
        result = subprocess.run([str(binary), *map(str, args)], cwd=self.work,
                                capture_output=True, text=True, timeout=30)
        # Check diagnostics first so an accidental disclosure fails silently.
        self.assertFalse(TEST_SEED in result.stdout + result.stderr,
                         "Signing material appeared in diagnostics")
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, "")
        return json.loads(result.stdout if code == 0 else result.stderr)

    def build(self):
        for filename in ("manifest.json", "tree.json", "golden-cases.json"):
            self.assertTrue((EXAMPLE / filename).is_file(),
                            "Missing installable example input: " + filename)
        return self.call(BUILDER, "--manifest", EXAMPLE / "manifest.json",
                         "--tree", EXAMPLE / "tree.json",
                         "--golden", EXAMPLE / "golden-cases.json",
                         "--output", self.package,
                         "--trust-store-output", self.trust)

    def route(self, text, *extra):
        return self.call(BRUCE, "route", "--text", text, "--mod-root", self.mods, *extra)

    def test_example_lifecycle_and_version_bound_replay(self):
        built = self.build()
        digest = hashlib.sha256(self.package.read_bytes()).hexdigest()
        self.assertEqual(built["mod_id"], MOD)
        self.assertEqual(built["version"], "1.0.0")
        self.assertEqual(built["package_digest"], digest)
        for output in (self.package, self.trust):
            self.assertFalse(TEST_SEED.encode() in output.read_bytes(),
                             "Signing material appeared in public output")
            self.assertEqual(output.stat().st_mode & 0o777, 0o600)
        verified = self.call(BRUCE, "mods", "verify", self.package,
                             "--trust-store", self.trust)
        self.assertTrue(verified["verified"])
        self.assertEqual(verified["package_digest"], digest)
        self.call(BRUCE, "mods", "install", self.package, "--mod-root", self.mods,
                  "--trust-store", self.trust)
        records = self.call(BRUCE, "mods", "list", "--mod-root", self.mods)["records"]
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["state"], "installed_inactive")
        self.assertEqual(self.route("3/4 hoch 2")["status"], "UNKNOWN")
        self.call(BRUCE, "mods", "activate", MOD, "1.0.0", "--mod-root", self.mods)
        report = self.work / "power.route.json"
        routed = self.route("3/4 hoch 2", "--output", report)
        self.assertEqual(routed["status"], "MATCHED")
        self.assertEqual(routed["candidate_addresses"], [[1, 1, 5]])
        self.assertEqual(routed["selected_template"], "math.rational.power.v1")
        self.assertEqual(routed["active_packages"], [
            {"mod_id": MOD, "version": "1.0.0", "package_digest": digest}])
        self.assertEqual(self.call(BRUCE, "route", "replay", report,
                                   "--mod-root", self.mods), routed)
        self.assertEqual(self.route("3/4\tHOCH\n2")["status"], "MATCHED")
        for text in ("3/0 hoch 2", "3/4 potenz 2"):
            unknown = self.route(text)
            self.assertEqual(unknown["status"], "UNKNOWN")
            self.assertIsNone(unknown["selected_template"])
        # The child leaves an existing Core route available.
        self.assertEqual(self.route("3/4 plus 5/6")["status"], "MATCHED")
        self.call(BRUCE, "mods", "disable", MOD, "--mod-root", self.mods)
        self.assertEqual(self.route("3/4 hoch 2")["status"], "UNKNOWN")
        self.assertEqual(self.call(BRUCE, "route", "replay", report,
                                   "--mod-root", self.mods, code=2)["error"],
                         "RouteRegistryDigestMismatch")


if __name__ == "__main__":
    unittest.main()
