"""Real CLI coverage for strict Data/Logic package detection and verification."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BRUCE = Path(sys.argv.pop(1)).resolve()
FIXTURE_TOOL = Path(sys.argv.pop(1)).resolve()
WORKER = Path(sys.argv.pop(1)).resolve()
FIXTURES = Path(__file__).resolve().parents[1] / "fixtures/runtime_mods"
DATA = FIXTURES / "math-rational-power-v1.brucemod"
TRUST = FIXTURES / "test-truststore-v1.pub"


class PackageInspectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-package-inspection-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run([FIXTURE_TOOL, WORKER, self.root, "export"], check=True,
                       capture_output=True, text=True, timeout=30)
        self.logic = self.root / "demo.brucemod"
        self.logic_trust = self.root / "trust.txt"

    def call(self, *args, code=0, timeout=5):
        result = subprocess.run([BRUCE, *map(str, args)], cwd=self.root,
                                capture_output=True, text=True, timeout=timeout)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, "")
        return json.loads(result.stdout if code == 0 else result.stderr)

    def test_inspects_and_verifies_both_package_kinds(self):
        data = self.call("mods", "inspect", DATA)
        self.assertEqual(data, {
            "contract": "mod-package-inspection-v2",
            "kind": "data",
            "mod_id": "user.math-rational-power",
            "version": "1.0.0",
            "package_digest": hashlib.sha256(DATA.read_bytes()).hexdigest(),
            "capabilities": ["dataset.read", "tree.extend"],
        })
        logic = self.call("mods", "inspect", self.logic)
        self.assertEqual(logic["contract"], "mod-package-inspection-v2")
        self.assertEqual(logic["kind"], "logic")
        self.assertEqual(logic["mod_id"], "user.logic")
        self.assertEqual(logic["capabilities"], ["worker.execute"])
        self.assertEqual(logic["package_digest"],
                         hashlib.sha256(self.logic.read_bytes()).hexdigest())

        for package, trust, kind in ((DATA, TRUST, "data"),
                                     (self.logic, self.logic_trust, "logic")):
            verified = self.call("mods", "verify", package, "--trust-store", trust)
            self.assertEqual(verified["contract"], "mod-package-verification-v2")
            self.assertEqual(verified["kind"], kind)
            self.assertTrue(verified["verified"])

    def test_rejects_malformed_untrusted_and_invalid_invocations(self):
        missing = self.root / "missing.brucemod"
        self.assertEqual(self.call("mods", "inspect", missing, code=2)["error"],
                         "ModPackageFileRejected")
        for name, content in (
            ("wrong.brucemod", b"not a BRUCE package"),
            ("truncated.brucemod", b"BRUCE_RUNTIME_DATA_MOD_V1"),
            ("ambiguous.brucemod", DATA.read_bytes() + self.logic.read_bytes()),
            ("oversized.brucemod", b"x" * (1024 * 1024 + 1)),
        ):
            path = self.root / name
            path.write_bytes(content)
            self.assertIn("error", self.call("mods", "inspect", path, code=2))

        damaged = self.root / "damaged.brucemod"
        raw = bytearray(DATA.read_bytes())
        raw[-2] = ord("0") if raw[-2] != ord("0") else ord("1")
        damaged.write_bytes(raw)
        self.assertEqual(self.call("mods", "verify", damaged, "--trust-store", TRUST,
                                   code=2)["error"], "RuntimeModSignatureInvalid")
        self.assertIn("error", self.call("mods", "inspect", code=2))
        self.assertIn("error", self.call("mods", "inspect", DATA, "--bogus", "x", code=2))

    def test_rejects_fifo_and_symlink_without_blocking(self):
        fifo = self.root / "package.fifo"
        os.mkfifo(fifo)
        self.assertEqual(self.call("mods", "inspect", fifo, code=2, timeout=2)["error"],
                         "ModPackageFileRejected")
        link = self.root / "package-link.brucemod"
        link.symlink_to(DATA)
        self.assertEqual(self.call("mods", "inspect", link, code=2)["error"],
                         "ModPackageFileRejected")


if __name__ == "__main__":
    unittest.main()
