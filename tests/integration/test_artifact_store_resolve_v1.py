import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


def digest(payload):
    return hashlib.sha256(payload).hexdigest()


def manifest_text(run_id, node_id, attempt_id, status, result_code, objects):
    lines = [
        "BRUCE_ARTIFACT_MANIFEST_V1",
        f"run_id={run_id}",
        f"node_id={node_id}",
        f"attempt_id={attempt_id}",
        f"status={status}",
        f"result_code={result_code}",
        f"object_count={len(objects)}",
    ]
    for index, (sha, size, origin) in enumerate(objects):
        lines.extend([
            f"object_{index}_sha256={sha}",
            f"object_{index}_size_bytes={size}",
            "object_%d_media_type=-" % index,
            f"object_{index}_origin={origin if origin else '-'}",
        ])
    return "\n".join(lines) + "\n"


class ArtifactStoreResolveV1WorkflowTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=15)

    def publish_manifest(self, store, manifest_id, **kwargs):
        path = store / "manifests" / f"{manifest_id}.manifest"
        path.write_text(manifest_text(**kwargs), encoding="ascii")
        path.chmod(0o600)

    def import_payload(self, root, store, name, payload):
        source = root / name
        source.write_bytes(payload)
        source.chmod(0o600)
        result = self.run_cli("artifact-store-v1", "import", str(source), "--store", str(store))
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return json.loads(result.stdout)

    @staticmethod
    def snapshot(root):
        result = []
        for directory, dirs, files in os.walk(root, followlinks=False):
            dirs.sort()
            files.sort()
            for name in dirs + files:
                path = Path(directory) / name
                stat = path.lstat()
                result.append((str(path.relative_to(root)), stat.st_mode, stat.st_size,
                               stat.st_ino, stat.st_mtime_ns, stat.st_nlink))
        return result

    def test_one_multiple_zero_objects_and_empty_origin_are_canonical(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root, store = Path(directory), Path(directory) / "store"
            first = self.import_payload(root, store, "first", b"first payload")
            second = self.import_payload(root, store, "second", b"second payload")
            first_id = (first["sha256"], first["size_bytes"], "")
            second_id = (second["sha256"], second["size_bytes"], "second-origin")
            self.publish_manifest(store, "one", run_id="run-one", node_id="node-one",
                                  attempt_id="attempt-one", status="READY", result_code="OK",
                                  objects=[first_id])
            self.publish_manifest(store, "many", run_id="run-many", node_id="node-many",
                                  attempt_id="attempt-many", status="READY", result_code="OK",
                                  objects=[second_id, first_id])
            self.publish_manifest(store, "zero", run_id="run-zero", node_id="node-zero",
                                  attempt_id="attempt-zero", status="READY", result_code="EMPTY",
                                  objects=[])
            for manifest_id, expected_count in (("one", 1), ("many", 2), ("zero", 0)):
                before = self.snapshot(store)
                result = self.run_cli("artifact-store-v1", "resolve", manifest_id,
                                      "--store", str(store))
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(result.stderr, b"")
                self.assertEqual(self.snapshot(store), before)
                report = json.loads(result.stdout)
                self.assertEqual(report["contract"], "artifact-store-resolve-v1")
                self.assertEqual(report["action"], "resolve")
                self.assertEqual(report["status"], "RESOLVED")
                self.assertEqual(report["manifest_id"], manifest_id)
                self.assertEqual(len(report["objects"]), expected_count)
                self.assertNotIn(str(root).encode(), result.stdout)
                self.assertNotIn(b"payload", result.stdout)
            one = json.loads(self.run_cli("artifact-store-v1", "resolve", "one",
                                          "--store", str(store)).stdout)
            self.assertEqual(one["objects"][0]["origin"], "")
            many = json.loads(self.run_cli("artifact-store-v1", "resolve", "many",
                                           "--store", str(store)).stdout)
            self.assertEqual([item["sha256"] for item in many["objects"]],
                             [second["sha256"], first["sha256"]])

    def test_repeated_resolution_is_byte_identical(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root, store = Path(directory), Path(directory) / "store"
            imported = self.import_payload(root, store, "payload", b"stable payload")
            self.publish_manifest(store, "stable", run_id="run", node_id="node",
                                  attempt_id="attempt", status="READY", result_code="OK",
                                  objects=[(imported["sha256"], imported["size_bytes"], "origin")])
            first = self.run_cli("artifact-store-v1", "resolve", "stable", "--store", str(store))
            second = self.run_cli("artifact-store-v1", "resolve", "stable", "--store", str(store))
            self.assertEqual(first.returncode, 0)
            self.assertEqual(first.stdout, second.stdout)
            self.assertTrue(first.stdout.endswith(b"\n"))

    def test_missing_store_is_not_created_by_resolve(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root = Path(directory)
            missing_store = root / "missing-store"
            result = self.run_cli("artifact-store-v1", "resolve", "missing",
                                  "--store", str(missing_store))
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr,
                             b'{"contract":"artifact-store-resolve-v1",'
                             b'"error":"PROTOCOL_ERROR"}\n')
            self.assertFalse(missing_store.exists())

    def test_invalid_ids_and_failures_use_only_resolve_error_contract(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root, store = Path(directory), Path(directory) / "store"
            imported = self.import_payload(root, store, "payload", b"secret payload")
            self.publish_manifest(store, "late", run_id="run", node_id="node", attempt_id="attempt",
                                  status="READY", result_code="BROKEN", objects=[
                                      (imported["sha256"], imported["size_bytes"], "first"),
                                      ("f" * 64, 4, "late"),
                                  ])
            cases = [
                ("../late", ("--store", str(store))),
                ("/late", ("--store", str(store))),
                ("", ("--store", str(store))),
                ("late", ()),
                ("late", ("--store", str(store), "--extra", "x")),
                ("missing", ("--store", str(store))),
                ("late", ("--store", str(store))),
            ]
            for manifest_id, options in cases:
                result = self.run_cli("artifact-store-v1", "resolve", manifest_id, *options)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr,
                                 b'{"contract":"artifact-store-resolve-v1",'
                                 b'"error":"PROTOCOL_ERROR"}\n')
                self.assertNotIn(str(root).encode(), result.stderr)
                self.assertNotIn(b"secret payload", result.stderr)

    def test_late_corrupt_symlink_hardlink_and_permission_failures_are_read_only(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root, store = Path(directory), Path(directory) / "store"
            imported = self.import_payload(root, store, "payload", b"integrity payload")
            self.publish_manifest(store, "checked", run_id="run", node_id="node", attempt_id="attempt",
                                  status="READY", result_code="OK", objects=[
                                      (imported["sha256"], imported["size_bytes"], "origin")])
            object_path = store / "objects" / imported["sha256"][:2] / (imported["sha256"] + ".blob")
            object_path.chmod(0o644)
            before = self.snapshot(store)
            result = self.run_cli("artifact-store-v1", "resolve", "checked", "--store", str(store))
            self.assertEqual(result.returncode, 2)
            self.assertEqual(self.snapshot(store), before)
            object_path.chmod(0o600)

            os.link(object_path, root / "hardlink")
            result = self.run_cli("artifact-store-v1", "resolve", "checked", "--store", str(store))
            self.assertEqual(result.returncode, 2)
            (root / "hardlink").unlink()

            object_path.unlink()
            object_path.symlink_to(root / "payload")
            result = self.run_cli("artifact-store-v1", "resolve", "checked", "--store", str(store))
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr,
                             b'{"contract":"artifact-store-resolve-v1",'
                             b'"error":"PROTOCOL_ERROR"}\n')

    def test_oversized_manifest_and_old_routes_remain_separate(self):
        with tempfile.TemporaryDirectory(prefix="bruce-artifact-resolve-") as directory:
            root, store = Path(directory), Path(directory) / "store"
            imported = self.import_payload(root, store, "payload", b"compatibility payload")
            path = store / "manifests" / "oversized.manifest"
            path.write_bytes(b"x" * (32 * 1024 * 1024 + 1))
            path.chmod(0o600)
            failed = self.run_cli("artifact-store-v1", "resolve", "oversized", "--store", str(store))
            self.assertEqual(failed.returncode, 2)
            self.assertEqual(failed.stderr,
                             b'{"contract":"artifact-store-resolve-v1",'
                             b'"error":"PROTOCOL_ERROR"}\n')
            source = root / "source"
            source.write_bytes(b"old route")
            source.chmod(0o600)
            old = self.run_cli("artifact-store-v1", "inspect", imported["sha256"], "--store", str(store))
            self.assertEqual(old.returncode, 0)
            self.assertEqual(json.loads(old.stdout)["contract"], "artifact-store-v1")


if __name__ == "__main__":
    unittest.main()
