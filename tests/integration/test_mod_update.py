"""Atomic side-by-side update and explicit rollback for Data and Logic Mods."""
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


class ModUpdate(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-mod-update-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.signing = self.root / "signing.env"
        self.signing.write_text("BRUCE_MOD_ED25519_SEED_HEX=" + SEED + "\n")
        self.signing.chmod(0o600)

    def command(self, *args, code=0):
        result = subprocess.run([BRUCE, *map(str, args)], cwd=self.root,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr if code == 0 else result.stdout, "")
        return json.loads(result.stdout if code == 0 else result.stderr)

    def data(self, identifier, version, tag, address=5):
        source = self.root / ("data-source-" + tag); source.mkdir()
        template = identifier + ".template"
        (source / "manifest.json").write_text(json.dumps({"mod_id": identifier,
            "signing_key_id": "test-rfc8032", "version": version},
            sort_keys=True, separators=(",", ":")))
        (source / "tree.json").write_text(json.dumps({"mount": [1, 1], "nodes": [{
            "aliases": ["token" + str(address)], "claim_scope": "candidate_only",
            "id": [1, 1, address], "key": "key" + str(address),
            "route_pattern": ["token" + str(address)], "template_id": template}]},
            sort_keys=True, separators=(",", ":")))
        (source / "golden-cases.json").write_text(json.dumps([{"input": "token" + str(address),
            "status": "MATCHED", "template_id": template}], sort_keys=True, separators=(",", ":")))
        package, trust = self.root / (tag + ".data.brucemod"), self.root / (tag + ".pub")
        result = subprocess.run([BUILDER, "data", source, "--output", package,
            "--trust-store-output", trust, "--signing-env", self.signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return package, trust, template

    def logic(self, data_package, template, tag):
        source = self.root / ("logic-source-" + tag); source.mkdir()
        shutil.copyfile(data_package, source / "data.brucemod")
        worker = WORKER.read_bytes(); (source / "worker.bin").write_bytes(worker)
        (source / "logic.json").write_text(json.dumps({"capabilities": ["worker.execute"],
            "template_ids": [template], "worker_path": "worker.bin",
            "worker_sha256": hashlib.sha256(worker).hexdigest()}, sort_keys=True, separators=(",", ":")))
        package, trust = self.root / (tag + ".logic.brucemod"), self.root / (tag + ".logic.pub")
        result = subprocess.run([BUILDER, "logic", source, "--output", package,
            "--trust-store-output", trust, "--signing-env", self.signing],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return package, trust

    def assert_update_cycle(self, packages, trust, mod_root, identifier, journal):
        first = self.command("mods", "update", packages[0], "--mod-root", mod_root,
                             "--trust-store", trust)
        installed = [entry for entry in first["entries"] if entry["id"] == identifier]
        self.assertEqual([(entry["version"], entry["lifecycle_state"]) for entry in installed],
                         [("1.0.0", "active")])
        before = journal.read_bytes()
        self.command("mods", "update", packages[0], "--mod-root", mod_root,
                     "--trust-store", trust)
        self.assertEqual(journal.read_bytes(), before)
        second = self.command("mods", "update", packages[1], "--mod-root", mod_root,
                              "--trust-store", trust)
        installed = [entry for entry in second["entries"] if entry["id"] == identifier]
        self.assertEqual([(entry["version"], entry["lifecycle_state"]) for entry in installed],
                         [("1.0.0", "disabled"), ("2.0.0", "active")])
        self.command("mods", "activate", identifier, "1.0.0", "--mod-root", mod_root)
        rolled = self.command("mods", "status", identifier, "--mod-root", mod_root)["entries"]
        self.assertEqual([(entry["version"], entry["lifecycle_state"]) for entry in rolled],
                         [("1.0.0", "active"), ("2.0.0", "disabled")])

    def test_data_and_logic_updates_are_atomic_and_rollbackable(self):
        d1, trust, _ = self.data("user.update-data", "1.0.0", "data-v1", 5)
        d2, trust2, _ = self.data("user.update-data", "2.0.0", "data-v2", 5)
        self.assertEqual(trust.read_bytes(), trust2.read_bytes())
        data_root = self.root / "data-mods"
        self.assert_update_cycle((d1, d2), trust, data_root, "user.update-data",
                                 data_root / "journal.v1")

        embedded1, logic_trust, template = self.data("user.update-logic", "1.0.0", "logic-data-v1", 6)
        embedded2, logic_trust2, _ = self.data("user.update-logic", "2.0.0", "logic-data-v2", 6)
        self.assertEqual(logic_trust.read_bytes(), logic_trust2.read_bytes())
        l1, outer_trust = self.logic(embedded1, template, "logic-v1")
        l2, outer_trust2 = self.logic(embedded2, template, "logic-v2")
        self.assertEqual(outer_trust.read_bytes(), outer_trust2.read_bytes())
        logic_root = self.root / "logic-mods"
        self.assert_update_cycle((l1, l2), outer_trust, logic_root, "user.update-logic",
                                 logic_root / "logic-journal.v1")

    def test_same_identity_different_digest_is_rejected_without_state_change(self):
        good, trust, _ = self.data("user.conflict", "1.0.0", "conflict-good", 10)
        bad, _, _ = self.data("user.conflict", "1.0.0", "conflict-bad", 11)
        mod_root = self.root / "conflict-mods"
        self.command("mods", "update", good, "--mod-root", mod_root, "--trust-store", trust)
        before = (mod_root / "journal.v1").read_bytes()
        error = self.command("mods", "update", bad, "--mod-root", mod_root,
                             "--trust-store", trust, code=2)
        self.assertEqual(error["error"], "InstalledModVersionConflict")
        self.assertEqual((mod_root / "journal.v1").read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
