import hashlib
import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


class ArtifactStoreV1WorkflowTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=15)

    def test_import_duplicate_inspect_verify_and_no_payload_or_path_leak(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-store-") as directory:
            root = Path(directory)
            source = root / "source.bin"
            store = root / "store"
            payload = b"artifact-store-v1 payload\x00\x01"
            source.write_bytes(payload)
            source.chmod(0o600)
            imported = self.run_cli("artifact-store-v1", "import", str(source), "--store", str(store))
            self.assertEqual(imported.returncode, 0, imported.stderr.decode())
            report = json.loads(imported.stdout)
            self.assertEqual(report["contract"], "artifact-store-v1")
            self.assertEqual(report["action"], "import")
            self.assertEqual(report["status"], "IMPORTED")
            digest = hashlib.sha256(payload).hexdigest()
            self.assertEqual(report["sha256"], digest)
            self.assertEqual(report["size_bytes"], len(payload))
            self.assertNotIn(payload, imported.stdout)
            self.assertNotIn(str(root).encode(), imported.stdout)
            object_path = store / "objects" / digest[:2] / (digest + ".blob")
            self.assertEqual(object_path.read_bytes(), payload)
            before = object_path.stat()
            duplicate = self.run_cli("artifact-store-v1", "import", str(source), "--store", str(store))
            self.assertEqual(duplicate.returncode, 0, duplicate.stderr.decode())
            self.assertEqual(json.loads(duplicate.stdout), report)
            after = object_path.stat()
            self.assertEqual(after.st_ino, before.st_ino)
            self.assertEqual(after.st_mtime_ns, before.st_mtime_ns)
            for action in ("inspect", "verify"):
                checked = self.run_cli("artifact-store-v1", action, digest, "--store", str(store))
                self.assertEqual(checked.returncode, 0, checked.stderr.decode())
                checked_report = json.loads(checked.stdout)
                self.assertEqual(checked_report["action"], action)
                self.assertEqual(checked_report["size_bytes"], len(payload))
                self.assertNotIn(str(root).encode(), checked.stdout)
                self.assertNotIn(payload, checked.stdout)

    def test_corruption_wrong_digest_and_wrong_shapes_fail_without_overwrite(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-store-") as directory:
            root = Path(directory)
            source = root / "source"
            store = root / "store"
            source.write_bytes(b"good")
            source.chmod(0o600)
            imported = self.run_cli("artifact-store-v1", "import", str(source), "--store", str(store))
            digest = json.loads(imported.stdout)["sha256"]
            object_path = store / "objects" / digest[:2] / (digest + ".blob")
            object_path.write_bytes(b"bad")
            checked = self.run_cli("artifact-store-v1", "verify", digest, "--store", str(store))
            self.assertEqual(checked.returncode, 2)
            self.assertEqual(json.loads(checked.stderr)["error"], "PROTOCOL_ERROR")
            self.assertNotIn(str(root).encode(), checked.stderr)
            for args in (("inspect", "../" + digest), ("verify", "A" * 64),
                         ("inspect", digest, "--store", str(store), "--extra")):
                bad = self.run_cli("artifact-store-v1", *args)
                self.assertEqual(bad.returncode, 2)
                self.assertEqual(json.loads(bad.stderr)["error"], "PROTOCOL_ERROR")

    def test_symlink_hardlink_permissions_and_missing_objects_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-store-") as directory:
            root = Path(directory)
            source = root / "source"
            store = root / "store"
            source.write_bytes(b"source")
            source.chmod(0o600)
            hard = root / "hard"
            os.link(source, hard)
            rejected = self.run_cli("artifact-store-v1", "import", str(hard), "--store", str(store))
            self.assertEqual(rejected.returncode, 2)
            link = root / "link"
            link.symlink_to(source)
            rejected = self.run_cli("artifact-store-v1", "import", str(link), "--store", str(store))
            self.assertEqual(rejected.returncode, 2)
            missing = self.run_cli("artifact-store-v1", "verify", "0" * 64, "--store", str(store))
            self.assertEqual(missing.returncode, 2)
            store.mkdir(exist_ok=True)
            os.chmod(store, 0o500)
            try:
                denied = self.run_cli("artifact-store-v1", "import", str(source), "--store", str(store))
                self.assertEqual(denied.returncode, 2)
                self.assertNotIn(str(root).encode(), denied.stderr)
            finally:
                os.chmod(store, 0o700)


if __name__ == "__main__":
    unittest.main()
