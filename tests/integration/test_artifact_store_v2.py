import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
ERROR = b'{"contract":"artifact-store-fetch-v2","error":"PROTOCOL_ERROR"}\n'
LIMIT = 32 * 1024 * 1024


class ArtifactStoreV2WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="bruce-v2-private-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.store = self.root / "local-private-store"
        self.mirror = self.root / "private-mirror"
        self.config = self.root / "private-providers.json"
        self.payload = b"PRIVATE-PAYLOAD-v2\x00\xff"
        self.sha = hashlib.sha256(self.payload).hexdigest()
        self.object = self.mirror / "objects" / self.sha[:2] / (self.sha + ".blob")
        self.object.parent.mkdir(parents=True, mode=0o700)
        self.mirror.chmod(0o700)
        self.object.parent.parent.chmod(0o700)
        self.object.write_bytes(self.payload)
        self.object.chmod(0o600)
        self.write_config()

    def write_config(self, **overrides):
        config = {"contract": "bruce-artifact-store-providers-v2",
                  "providers": [{"id": "mirror-a", "root": str(self.mirror)}]}
        config.update(overrides)
        self.config.write_text(json.dumps(config, separators=(",", ":")) + "\n")
        self.config.chmod(0o600)

    def run_cli(self, *args):
        return subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=15)

    def args(self, sha=None, size=None, provider="mirror-a"):
        return ["artifact-store-v2", "fetch", self.sha if sha is None else sha,
                str(len(self.payload) if size is None else size), "--store", str(self.store),
                "--provider", provider, "--providers", str(self.config)]

    def fetch(self, **kwargs):
        return self.run_cli(*self.args(**kwargs))

    def assert_error(self, result):
        self.assertEqual((result.returncode, result.stdout, result.stderr), (2, b"", ERROR))

    def expected(self, sha, size):
        return ('{"contract":"artifact-store-fetch-v2","action":"fetch","status":"FETCHED",'
                '"provider":"mirror-a","sha256":"%s","size_bytes":%d}\n' % (sha, size)).encode()

    @staticmethod
    def snapshot(root):
        if not root.exists():
            return None
        result = []
        for path in [root, *sorted(root.rglob("*"))]:
            info = path.lstat()
            regular = stat.S_ISREG(info.st_mode)
            content = hashlib.sha256(path.read_bytes()).hexdigest() if regular else None
            # V1 publication creates/removes a temp entry even on an idempotent
            # fetch. Directory size/mtime can change without changing objects.
            result.append((str(path.relative_to(root)), info.st_mode, info.st_ino,
                           info.st_size if regular else None,
                           info.st_mtime_ns if regular else None, info.st_nlink, content))
        return result

    def test_fetch_metadata_idempotence_and_v1_verification(self):
        result = self.fetch()
        self.assertEqual((result.returncode, result.stdout, result.stderr),
                         (0, self.expected(self.sha, len(self.payload)), b""))
        published = self.store / "objects" / self.sha[:2] / (self.sha + ".blob")
        self.assertEqual(published.read_bytes(), self.payload)
        before = self.snapshot(self.store)
        repeat = self.fetch()
        self.assertEqual((repeat.returncode, repeat.stdout, repeat.stderr),
                         (0, result.stdout, b""))
        self.assertEqual(self.snapshot(self.store), before)
        for secret in (self.payload, str(self.root).encode(), str(self.mirror).encode(),
                       str(self.config).encode(), str(self.store).encode()):
            self.assertNotIn(secret, result.stdout + result.stderr)
        verified = self.run_cli("artifact-store-v1", "verify", self.sha, "--store", self.store)
        self.assertEqual(verified.returncode, 0, verified.stderr)

    def test_empty_and_maximum_objects(self):
        for payload in (b"", b"x" * LIMIT):
            with self.subTest(size=len(payload)):
                sha = hashlib.sha256(payload).hexdigest()
                path = self.mirror / "objects" / sha[:2] / (sha + ".blob")
                path.parent.mkdir(mode=0o700, exist_ok=True)
                path.write_bytes(payload)
                path.chmod(0o600)
                result = self.fetch(sha=sha, size=len(payload))
                self.assertEqual((result.returncode, result.stdout, result.stderr),
                                 (0, self.expected(sha, len(payload)), b""))
                self.assertEqual((self.store / "objects" / sha[:2] / path.name).read_bytes(), payload)

    def test_fetched_object_resolves_and_materializes_through_v14_locally(self):
        self.assertEqual(self.fetch().returncode, 0)
        manifest = self.store / "manifests" / "fetched.manifest"
        manifest.write_text("BRUCE_ARTIFACT_MANIFEST_V1\nrun_id=run-1\nnode_id=node-1\n"
                            "attempt_id=attempt-1\nstatus=READY\nresult_code=OK\nobject_count=1\n"
                            f"object_0_sha256={self.sha}\nobject_0_size_bytes={len(self.payload)}\n"
                            "object_0_media_type=-\nobject_0_origin=-\n")
        manifest.chmod(0o600)
        # The mirror is unavailable after fetch: consumers must use only V1.
        self.object.unlink()
        resolved = self.run_cli("artifact-store-v1", "resolve", "fetched", "--store", self.store)
        self.assertEqual(resolved.returncode, 0, resolved.stderr)
        self.assertEqual(json.loads(resolved.stdout)["objects"],
                         [{"sha256": self.sha, "size_bytes": len(self.payload), "origin": ""}])
        spec = self.root / "consumer.spec"
        spec.write_bytes(b"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=fetched-consumer\n"
                         b"max-started-nodes=1\nmax-total-receipt-bytes=4096\n\n"
                         b"node=a\ndepends=\ncallback=append-v1\nrequest-hex=21\n")
        sidecar = self.root / "external-inputs.json"
        sidecar.write_text(json.dumps({
            "contract": "bruce-dag-scheduler-v5-external-inputs-v14",
            "bindings": [{"node_id": "a", "inputs": [{"manifest_id": "fetched", "object_index": 0,
                "sha256": self.sha, "size_bytes": len(self.payload), "source_run_id": "run-1",
                "source_node_id": "node-1", "source_attempt_id": "attempt-1"}]}],
        }, separators=(",", ":")))
        sidecar.chmod(0o600)
        bundle = self.root / "consumer-bundle"
        result = self.run_cli("dag-scheduler-v5", "payload", spec, "--output", bundle,
                              "--store", self.store, "--external-inputs", sidecar)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["contract"], "dag-scheduler-v5-payload-v14")
        self.assertEqual(report["external_input_count"], 1)
        self.assertEqual(report["external_input_bytes"], len(self.payload))
        run_root = bundle.with_name(bundle.name + ".run")
        self.assertEqual((run_root / "attempts/a-1/output/result.bin").read_bytes(), self.payload + b"!")
        for secret in (self.payload, str(self.root).encode(), str(self.mirror).encode()):
            self.assertNotIn(secret, result.stdout + result.stderr + resolved.stdout)

    def test_closed_grammar_rejected_before_config_fifo_open(self):
        # A parser that touches configuration before rejecting arguments will block here.
        self.config.unlink()
        os.mkfifo(self.config, 0o600)
        valid = self.args()
        malformed = [["artifact-store-v2"], ["artifact-store-v2", "--help"]]
        malformed += [valid[:count] for count in range(2, len(valid))]
        for index, values in {
            1: ("import", "inspect", "verify", "resolve", "publish", "delete"),
            2: ("A" * 64, "0" * 63, "../digest"),
            3: ("-1", "+1", "01", "1.0", "33554433", "18446744073709551616"),
            4: ("--provider", "--source"), 5: ("", "--option"),
            6: ("--root", "--store"), 7: ("../mirror", "https://mirror", "", "-a", "a b"),
            8: ("--provider", "--url"), 9: ("", "--option"),
        }.items():
            for value in values:
                args = valid.copy()
                args[index] = value
                malformed.append(args)
        malformed += [valid + ["extra"], valid + ["--provider", "mirror-b"],
                      valid[:4] + ["--provider", "mirror-a", "--store", str(self.store),
                                   "--providers", str(self.config)]]
        for args in malformed:
            with self.subTest(args=args):
                self.assert_error(self.run_cli(*args))
                self.assertFalse(self.store.exists())

    def test_unknown_provider_bad_config_and_unsafe_config_are_private(self):
        self.assert_error(self.fetch(provider="unknown"))
        canonical = self.config.read_bytes()
        for config in (canonical.rstrip(b"\n"), canonical + b"\n", b"{}\n",
                       canonical.replace(b'"providers":', b'"extra":0,"providers":'),
                       canonical.replace(b'"mirror-a"', b'"../escape"'),
                       canonical.replace(str(self.mirror).encode(), b"/safe/../escape")):
            self.config.write_bytes(config)
            self.assert_error(self.fetch())
            self.assertFalse(self.store.exists())
        self.config.write_bytes(canonical)
        self.config.chmod(0o644)
        self.assert_error(self.fetch())
        self.config.chmod(0o600)
        hard = self.root / "config-hardlink"
        os.link(self.config, hard)
        self.assert_error(self.fetch())
        hard.unlink()
        self.config.rename(hard)
        self.config.symlink_to(hard)
        self.assert_error(self.fetch())
        self.config.unlink()
        os.mkfifo(self.config, 0o600)
        self.assert_error(self.fetch())
        self.config.unlink()
        self.config.mkdir(mode=0o700)
        self.assert_error(self.fetch())
        self.config.rmdir()
        self.assert_error(self.fetch())
        self.assertFalse(self.store.exists())

    def test_unsafe_mirror_families_create_no_destination(self):
        for directory in (self.mirror, self.object.parent.parent, self.object.parent):
            with self.subTest(insecure_directory=directory):
                directory.chmod(0o755)
                self.assert_error(self.fetch())
                directory.chmod(0o700)
            with self.subTest(symlink_directory=directory):
                moved = directory.with_name(directory.name + "-real")
                directory.rename(moved)
                directory.symlink_to(moved, target_is_directory=True)
                self.assert_error(self.fetch())
                directory.unlink()
                moved.rename(directory)
        self.object.chmod(0o644)
        self.assert_error(self.fetch())
        self.object.chmod(0o600)
        hard = self.root / "object-hardlink"
        os.link(self.object, hard)
        self.assert_error(self.fetch())
        hard.unlink()
        self.object.rename(hard)
        self.object.symlink_to(hard)
        self.assert_error(self.fetch())
        self.object.unlink()
        os.mkfifo(self.object, 0o600)
        self.assert_error(self.fetch())
        self.object.unlink()
        self.object.mkdir(mode=0o700)
        self.assert_error(self.fetch())
        self.object.rmdir()
        self.assert_error(self.fetch())
        self.object.write_bytes(b"x" * (LIMIT + 1))
        self.object.chmod(0o600)
        self.assert_error(self.fetch())
        self.object.write_bytes(self.payload)
        moved = self.root / "saved-mirror"
        self.mirror.rename(moved)
        self.assert_error(self.fetch())
        moved.rename(self.mirror)
        ancestor_link = self.root / "ancestor-link"
        ancestor_link.symlink_to(self.root, target_is_directory=True)
        canonical = self.config.read_bytes()
        self.config.write_bytes(canonical.replace(str(self.mirror).encode(),
                                                str(ancestor_link / self.mirror.name).encode()))
        self.assert_error(self.fetch())
        self.assertFalse(self.store.exists())

    def test_digest_size_and_store_conflicts_preserve_full_store(self):
        for size in (len(self.payload) - 1, len(self.payload) + 1):
            self.assert_error(self.fetch(size=size))
            self.assertFalse(self.store.exists())
        self.object.write_bytes(b"X" + self.payload[1:])
        self.assert_error(self.fetch())
        self.assertFalse(self.store.exists())
        self.object.write_bytes(self.payload)
        source = self.root / "unrelated"
        source.write_bytes(b"unrelated existing object")
        source.chmod(0o600)
        imported = self.run_cli("artifact-store-v1", "import", source, "--store", self.store)
        self.assertEqual(imported.returncode, 0, imported.stderr)
        before = self.snapshot(self.store)
        for size in (len(self.payload) - 1, len(self.payload) + 1):
            self.assert_error(self.fetch(size=size))
            self.assertEqual(self.snapshot(self.store), before)
        self.object.write_bytes(b"X" + self.payload[1:])
        self.assert_error(self.fetch())
        self.assertEqual(self.snapshot(self.store), before)
        self.object.write_bytes(self.payload)
        conflict = self.store / "objects" / self.sha[:2] / (self.sha + ".blob")
        conflict.parent.mkdir(mode=0o700, exist_ok=True)
        conflict.write_bytes(b"preexisting corrupt object")
        conflict.chmod(0o600)
        before = self.snapshot(self.store)
        self.assert_error(self.fetch())
        self.assertEqual(self.snapshot(self.store), before)

    def test_v1_and_v12_exact_bytes_before_and_after_v2(self):
        source = self.root / "legacy-source"
        source.write_bytes(b"abc")
        source.chmod(0o600)
        sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        commands = [("import", source, "IMPORTED"), ("inspect", sha, "INSPECTED"),
                    ("verify", sha, "VERIFIED")]
        for round_number in range(2):
            for action, value, status in commands:
                result = self.run_cli("artifact-store-v1", action, value, "--store", self.store)
                expected = ('{"contract":"artifact-store-v1","action":"%s","status":"%s",'
                            '"sha256":"%s","size_bytes":3}\n' % (action, status, sha)).encode()
                self.assertEqual((result.returncode, result.stdout, result.stderr), (0, expected, b""))
            manifest = self.store / "manifests" / "legacy.manifest"
            manifest.write_text("BRUCE_ARTIFACT_MANIFEST_V1\nrun_id=run-1\nnode_id=node-1\n"
                                "attempt_id=attempt-1\nstatus=READY\nresult_code=OK\nobject_count=1\n"
                                f"object_0_sha256={sha}\nobject_0_size_bytes=3\n"
                                "object_0_media_type=-\nobject_0_origin=-\n")
            manifest.chmod(0o600)
            resolved = self.run_cli("artifact-store-v1", "resolve", "legacy", "--store", self.store)
            expected = ('{"contract":"artifact-store-resolve-v1","action":"resolve","status":"RESOLVED",'
                        '"manifest_id":"legacy","run_id":"run-1","node_id":"node-1",'
                        '"attempt_id":"attempt-1","manifest_status":"READY","result_code":"OK",'
                        '"objects":[{"sha256":"%s","size_bytes":3,"origin":""}]}\n' % sha).encode()
            self.assertEqual((resolved.returncode, resolved.stdout, resolved.stderr), (0, expected, b""))
            for action, contract in (("verify", "artifact-store-v1"),
                                     ("resolve", "artifact-store-resolve-v1")):
                failure = self.run_cli("artifact-store-v1", action, "../bad", "--store", self.store)
                self.assertEqual((failure.returncode, failure.stdout, failure.stderr),
                                 (2, b"", ('{"contract":"%s","error":"PROTOCOL_ERROR"}\n' % contract).encode()))
            if round_number == 0:
                self.assertEqual(self.fetch().returncode, 0)


if __name__ == "__main__":
    unittest.main()
