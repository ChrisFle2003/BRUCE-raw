"""End-to-end durable Logic lifecycle through the unified `mods` CLI."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

BRUCE = Path(sys.argv.pop(1)).resolve()
BUILDER = Path(sys.argv.pop(1)).resolve()
WORKER = Path(sys.argv.pop(1)).resolve()
SEED = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60"


class LogicLifecycle(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-logic-lifecycle-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.mods = self.root / "mods"
        self.signing = self.root / "signing.env"
        self.signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + SEED + "\n")
        self.signing.chmod(0o600)

    def call(self, *args, code=0):
        result = subprocess.run([BRUCE, *map(str, args)], cwd=self.root,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, "")
        return json.loads(result.stdout if code == 0 else result.stderr)

    def package(self, version):
        tag = version.replace(".", "-")
        data = self.root / ("data-" + tag)
        logic = self.root / ("logic-" + tag)
        data.mkdir(); logic.mkdir()
        (data / "manifest.json").write_text(json.dumps({
            "mod_id": "user.logic-cli", "signing_key_id": "test-rfc8032", "version": version,
        }, sort_keys=True, separators=(",", ":")))
        (data / "tree.json").write_text(json.dumps({
            "mount": [1, 1], "nodes": [{"aliases": ["hoch"], "claim_scope": "candidate_only",
                "id": [1, 1, 8], "key": "logic-power", "route_pattern": ["logic", "hoch"],
                "template_id": "logic.power.v1"}],
        }, sort_keys=True, separators=(",", ":")))
        (data / "golden-cases.json").write_text(json.dumps([
            {"input": "logic hoch", "status": "MATCHED", "template_id": "logic.power.v1"}
        ], sort_keys=True, separators=(",", ":")))
        data_package, data_trust = self.root / ("data-" + tag + ".brucemod"), self.root / ("data-" + tag + ".pub")
        result = subprocess.run([BUILDER, "data", data, "--output", data_package,
            "--trust-store-output", data_trust, "--signing-env", self.signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        shutil.copyfile(data_package, logic / "data.brucemod")
        worker = WORKER.read_bytes()
        (logic / "worker.bin").write_bytes(worker)
        (logic / "logic.json").write_text(json.dumps({
            "capabilities": ["worker.execute"], "template_ids": ["logic.power.v1"],
            "worker_path": "worker.bin", "worker_sha256": hashlib.sha256(worker).hexdigest(),
        }, sort_keys=True, separators=(",", ":")))
        package, trust = self.root / ("logic-" + tag + ".brucemod"), self.root / ("logic-" + tag + ".pub")
        result = subprocess.run([BUILDER, "logic", logic, "--output", package,
            "--trust-store-output", trust, "--signing-env", self.signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return package, trust

    def test_install_activate_update_side_by_side_rollback_and_disable(self):
        one, trust = self.package("1.0.0")
        two, trust_two = self.package("2.0.0")
        self.assertEqual(trust.read_bytes(), trust_two.read_bytes())
        installed = self.call("mods", "install", one, "--mod-root", self.mods,
                              "--trust-store", trust)
        entry = [item for item in installed["entries"] if item["origin"] == "installed"][0]
        self.assertEqual((entry["kind"], entry["lifecycle_state"]), ("logic", "installed_inactive"))
        self.call("mods", "activate", "user.logic-cli", "1.0.0", "--mod-root", self.mods)
        self.call("mods", "install", two, "--mod-root", self.mods, "--trust-store", trust)
        status = self.call("mods", "status", "user.logic-cli", "--mod-root", self.mods)["entries"]
        self.assertEqual([(item["version"], item["lifecycle_state"]) for item in status],
                         [("1.0.0", "active"), ("2.0.0", "installed_inactive")])
        self.call("mods", "activate", "user.logic-cli", "2.0.0", "--mod-root", self.mods)
        self.call("mods", "activate", "user.logic-cli", "1.0.0", "--mod-root", self.mods)
        status = self.call("mods", "status", "user.logic-cli", "--mod-root", self.mods)["entries"]
        self.assertEqual([(item["version"], item["lifecycle_state"]) for item in status],
                         [("1.0.0", "active"), ("2.0.0", "disabled")])
        self.call("mods", "disable", "user.logic-cli", "--mod-root", self.mods)
        self.assertTrue(all(item["lifecycle_state"] == "disabled" for item in
                            self.call("mods", "status", "user.logic-cli", "--mod-root", self.mods)["entries"]))

    def test_builtin_mutation_fails_before_root_access(self):
        missing = self.root / "must-not-exist"
        error = self.call("mods", "activate", "mcp", "1.0.0", "--mod-root", missing, code=2)
        self.assertEqual(error["error"], "BuiltinModImmutable")
        self.assertFalse(missing.exists())


if __name__ == "__main__":
    unittest.main()
