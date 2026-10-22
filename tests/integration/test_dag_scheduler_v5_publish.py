import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())

SINGLE_SPEC = b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=publish-single
max-started-nodes=1
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=7075626c6973682d7061796c6f6164
"""

MULTI_SPEC = b"""BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=publish-multi
max-started-nodes=8
max-total-receipt-bytes=4096

node=a
depends=
callback=literal-v1
request-hex=41

node=b
depends=
callback=literal-v1
request-hex=42

node=c
depends=a,b
callback=join-v1
request-hex=2d
"""


class DagSchedulerV5PublishWorkflowTests(unittest.TestCase):
    def execute(self, root, spec_bytes=SINGLE_SPEC, name="bundle", extra=()):
        spec = root / (name + ".spec")
        bundle = root / name
        spec.write_bytes(spec_bytes)
        result = subprocess.run(
            [BIN, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle), *extra],
            capture_output=True, timeout=20,
        )
        return result, bundle

    @staticmethod
    def publish(bundle, store):
        return subprocess.run(
            [BIN, "dag-scheduler-v5", "publish", str(bundle), "--store", str(store)],
            capture_output=True, timeout=20,
        )

    def assert_protocol(self, result):
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_single_multi_and_idempotent_publish_are_metadata_only(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-publish-") as directory:
            root = Path(directory)
            executed, bundle = self.execute(root)
            self.assertEqual(executed.returncode, 0, executed.stderr.decode())
            store = root / "store"
            first = self.publish(bundle, store)
            second = self.publish(bundle, store)
            self.assertEqual(first.returncode, 0, first.stderr.decode())
            self.assertEqual(first.stderr, b"")
            self.assertEqual(first.stdout, second.stdout)
            report = json.loads(first.stdout)
            self.assertEqual(list(report), ["contract", "action", "status", "run_id", "manifests"])
            self.assertEqual(report["contract"], "dag-scheduler-v5-publish-v1")
            self.assertEqual(report["status"], "PUBLISHED")
            self.assertEqual(list(report["manifests"][0]),
                             ["node_id", "attempt_id", "reference", "manifest_id", "sha256", "size_bytes"])
            text = first.stdout.decode()
            self.assertNotIn("publish-payload", text)
            self.assertNotIn(str(root), text)
            self.assertNotIn("attempts/", text)
            multi_executed, multi_bundle = self.execute(root, MULTI_SPEC, "multi", ("--workers", "2"))
            self.assertEqual(multi_executed.returncode, 0, multi_executed.stderr.decode())
            multi = self.publish(multi_bundle, root / "multi-store")
            self.assertEqual(multi.returncode, 0, multi.stderr.decode())
            multi_report = json.loads(multi.stdout)
            self.assertEqual([item["node_id"] for item in multi_report["manifests"]], ["a", "b", "c"])

    def test_manifest_and_object_conflicts_are_rejected_without_overwrite(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-publish-") as directory:
            root = Path(directory)
            executed, bundle = self.execute(root)
            self.assertEqual(executed.returncode, 0)
            store = root / "store"
            first = self.publish(bundle, store)
            self.assertEqual(first.returncode, 0)
            published = json.loads(first.stdout)["manifests"][0]
            manifest_path = store / "manifests" / (published["manifest_id"] + ".manifest")
            original_manifest = manifest_path.read_bytes()
            manifest_path.write_bytes(b"conflict")
            os.chmod(manifest_path, 0o600)
            self.assert_protocol(self.publish(bundle, store))
            self.assertEqual(manifest_path.read_bytes(), b"conflict")
            object_store = root / "object-store"
            object_path = object_store / "objects" / published["sha256"][:2] / (published["sha256"] + ".blob")
            object_path.parent.mkdir(parents=True)
            object_path.write_bytes(b"wrong")
            os.chmod(object_path, 0o600)
            self.assert_protocol(self.publish(bundle, object_store))
            self.assertEqual(object_path.read_bytes(), b"wrong")
            manifest_path.write_bytes(original_manifest)

    def test_incomplete_failure_and_manipulated_runs_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-publish-") as directory:
            root = Path(directory)
            executed, bundle = self.execute(root)
            self.assertEqual(executed.returncode, 0)
            log = Path(str(bundle) + ".run/control/run-log.jsonl")
            complete_log = log.read_bytes()
            log.write_bytes(b"".join(complete_log.splitlines(keepends=True)[:-1]))
            self.assert_protocol(self.publish(bundle, root / "incomplete-store"))
            log.write_bytes(complete_log)
            original_bundle = bundle.read_bytes()
            bundle.write_bytes(original_bundle.replace(b'"reference":"artifact/a"', b'"reference":"artifact/b"', 1))
            self.assert_protocol(self.publish(bundle, root / "report-store"))
            bundle.write_bytes(original_bundle)
            failed, failed_bundle = self.execute(root, name="failed", extra=("--max-output-bytes", "1"))
            self.assertEqual(failed.returncode, 1)
            self.assert_protocol(self.publish(failed_bundle, root / "failed-store"))

    def test_missing_symlink_hardlink_and_store_under_run_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-publish-") as directory:
            root = Path(directory)
            for mode in ("missing", "symlink", "hardlink"):
                executed, bundle = self.execute(root, name=mode)
                self.assertEqual(executed.returncode, 0)
                artifact = Path(str(bundle) + ".run/attempts/a-1/output/result.bin")
                if mode == "missing":
                    artifact.unlink()
                elif mode == "symlink":
                    outside = root / (mode + ".outside")
                    outside.write_bytes(b"outside")
                    artifact.unlink()
                    artifact.symlink_to(outside)
                else:
                    second = artifact.with_name("second-link.bin")
                    second.hardlink_to(artifact)
                self.assert_protocol(self.publish(bundle, root / (mode + "-store")))
            executed, bundle = self.execute(root, name="nested")
            self.assertEqual(executed.returncode, 0)
            self.assert_protocol(self.publish(bundle, Path(str(bundle) + ".run/store")))

    def test_syntax_and_store_permissions_are_compact(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-publish-") as directory:
            root = Path(directory)
            executed, bundle = self.execute(root)
            self.assertEqual(executed.returncode, 0)
            cases = (
                [BIN, "dag-scheduler-v5", "publish"],
                [BIN, "dag-scheduler-v5", "publish", str(bundle) + ".run", "--store", str(root / "x")],
                [BIN, "dag-scheduler-v5", "publish", str(bundle), "--store", str(root / "x"), "--extra", "x"],
            )
            for command in cases:
                self.assert_protocol(subprocess.run(command, capture_output=True, timeout=20))
            store = root / "bad-perms"
            store.mkdir()
            os.chmod(store, 0o755)
            self.assert_protocol(self.publish(bundle, store))


if __name__ == "__main__":
    unittest.main()
