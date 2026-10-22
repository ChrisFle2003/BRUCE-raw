"""Explicit execution of a durable active Logic Mod."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

BRUCE = Path(sys.argv.pop(1)).resolve()
BUILDER = Path(sys.argv.pop(1)).resolve()
WORKER = Path(sys.argv.pop(1)).resolve()
SEED = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60"


class InstalledLogicRun(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-installed-logic-run-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.mods = self.root / "mods"
        signing = self.root / "signing.env"
        signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + SEED + "\n")
        signing.chmod(0o600)
        data = self.root / "data"; logic = self.root / "logic"
        data.mkdir(); logic.mkdir()
        (data / "manifest.json").write_text('{"mod_id":"user.logic-run","signing_key_id":"test-rfc8032","version":"1.0.0"}')
        (data / "tree.json").write_text('{"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]}')
        (data / "golden-cases.json").write_text('[{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}]')
        data_package, self.trust = self.root / "data.brucemod", self.root / "trust.pub"
        built = subprocess.run([BUILDER, "data", data, "--output", data_package,
            "--trust-store-output", self.trust, "--signing-env", signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(built.returncode, 0, built.stderr)
        shutil.copyfile(data_package, logic / "data.brucemod")
        worker = WORKER.read_bytes(); (logic / "worker.bin").write_bytes(worker)
        (logic / "logic.json").write_text(json.dumps({
            "capabilities": ["worker.execute"],
            "template_ids": ["math.rational.power.v1"],
            "worker_path": "worker.bin", "worker_sha256": hashlib.sha256(worker).hexdigest(),
        }, sort_keys=True, separators=(",", ":")))
        self.package = self.root / "logic.brucemod"
        built = subprocess.run([BUILDER, "logic", logic, "--output", self.package,
            "--trust-store-output", self.root / "logic.pub", "--signing-env", signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(built.returncode, 0, built.stderr)
        data_only = self.root / "data-only"; data_only.mkdir()
        (data_only / "manifest.json").write_text('{"mod_id":"user.data-only","signing_key_id":"test-rfc8032","version":"1.0.0"}')
        (data_only / "tree.json").write_text('{"mount":[1,1],"nodes":[{"aliases":["dataonly"],"claim_scope":"candidate_only","id":[1,1,9],"key":"data-only","route_pattern":["dataonly"],"template_id":"data.only.v1"}]}')
        (data_only / "golden-cases.json").write_text('[{"input":"dataonly","status":"MATCHED","template_id":"data.only.v1"}]')
        self.data_only = self.root / "data-only.brucemod"
        built = subprocess.run([BUILDER, "data", data_only, "--output", self.data_only,
            "--trust-store-output", self.root / "data-only.pub", "--signing-env", signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(built.returncode, 0, built.stderr)

    def call(self, *args, code=0):
        result = subprocess.run([BRUCE, *map(str, args)], cwd=self.root,
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, "")
        return json.loads(result.stdout if code == 0 else result.stderr)

    def run_mod(self, text="3/4 hoch 2", template="math.rational.power.v1", code=0):
        return self.call("mods", "run", "user.logic-run", "--template", template,
                         "--input", text, "--mod-root", self.mods, code=code)

    def test_requires_active_logic_and_runs_from_pinned_bytes(self):
        self.call("mods", "install", self.package, "--mod-root", self.mods,
                  "--trust-store", self.trust)
        self.assertIn("error", self.run_mod(code=2))
        self.call("mods", "activate", "user.logic-run", "1.0.0", "--mod-root", self.mods)
        first = self.run_mod()
        second = self.run_mod()
        self.assertEqual(first, second)
        self.assertEqual(first["contract"], "installed-logic-mod-run-v1")
        self.assertEqual((first["accepted"], first["result"], first["claim_scope"]),
                         (True, "9/16", "candidate_only"))
        self.assertNotIn("worker.bin", json.dumps(first))
        route = self.call("route", "--text", "3/4 hoch 2", "--mod-root", self.mods,
                          "--mod-details")
        self.assertEqual(route["selected_owner"]["id"], "user.logic-run")
        self.assertEqual(route["selected_owner"]["kind"], "logic")
        self.assertIn("error", self.run_mod(template="unknown.template", code=2))
        self.assertIn("error", self.run_mod("x" * 4097, code=2))
        self.assertEqual(self.run_mod("timeout", code=2)["error"], "TIMEOUT")
        self.assertIn("WORKER_SIGNAL", self.run_mod("crash", code=2)["error"])
        self.call("mods", "install", self.data_only, "--mod-root", self.mods,
                  "--trust-store", self.trust)
        data_error = self.call("mods", "run", "user.data-only", "--template", "data.only.v1",
                               "--input", "dataonly", "--mod-root", self.mods, code=2)
        self.assertEqual(data_error["error"], "InstalledLogicModRequired")
        self.call("mods", "disable", "user.logic-run", "--mod-root", self.mods)
        self.assertIn("error", self.run_mod(code=2))

    def test_rejects_builtin_before_root_and_tampered_installed_worker(self):
        missing = self.root / "missing"
        error = self.call("mods", "run", "mcp", "--template", "x", "--input", "x",
                          "--mod-root", missing, code=2)
        self.assertEqual(error["error"], "BuiltinModImmutable")
        self.assertFalse(missing.exists())
        self.call("mods", "install", self.package, "--mod-root", self.mods,
                  "--trust-store", self.trust)
        self.call("mods", "activate", "user.logic-run", "1.0.0", "--mod-root", self.mods)
        entry = self.call("mods", "status", "user.logic-run", "--mod-root", self.mods)["entries"][0]
        worker = self.mods / "logic-workers" / entry["package_digest"] / "worker.bin"
        worker.chmod(0o700); worker.write_bytes(b"tampered")
        self.assertIn("error", self.run_mod(code=2))

    def test_concurrent_disable_cannot_change_issued_worker_bytes(self):
        self.call("mods", "install", self.package, "--mod-root", self.mods,
                  "--trust-store", self.trust)
        self.call("mods", "activate", "user.logic-run", "1.0.0", "--mod-root", self.mods)
        process = subprocess.Popen([BRUCE, "mods", "run", "user.logic-run",
            "--template", "math.rational.power.v1", "--input", "timeout",
            "--mod-root", self.mods], cwd=self.root, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True)
        time.sleep(0.05)
        self.call("mods", "disable", "user.logic-run", "--mod-root", self.mods)
        stdout, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 2, stdout)
        self.assertEqual(stdout, "")
        self.assertIn(json.loads(stderr)["error"], ("TIMEOUT", "LogicModNotActive"))


if __name__ == "__main__":
    unittest.main()
