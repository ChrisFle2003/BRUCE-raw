"""Real payload-inspect workflows: wrong status, unchecked streams or writes fail these tests."""
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BIN = str(Path(sys.argv.pop(1)).resolve())
SECRET = b"private-inspector-payload"
KEYS = ["contract", "action", "status", "run_finished", "run_status", "source_contract",
        "run_id", "last_seq", "node_count", "attempt_count", "accepted_artifact_count",
        "stream_check", "verified_stream_count", "verified_stream_bytes", "input_completeness",
        "external_provenance"]


def spec_bytes(nodes):
    return ("BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=payload-inspector\nmax-started-nodes=8\n"
            "max-total-receipt-bytes=16384\n\n" + "\n".join(
                f"node={name}\ndepends={deps}\ncallback={callback}\nrequest-hex={payload.hex()}\n"
                for name, deps, callback, payload in nodes)).encode()


class PayloadInspectorWorkflows(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-payload-inspector-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.store = self.root / "store"
        source = self.root / "source"
        source.write_bytes(b"seed"); source.chmod(0o600)
        result = self.call("artifact-store-v1", "import", source, "--store", self.store)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.counter = 0

    def call(self, *args):
        return subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=40)

    def payload(self, nodes=None, *options, expected=0):
        self.counter += 1
        spec = self.root / f"spec-{self.counter}"
        spec.write_bytes(spec_bytes(nodes or [("a", "", "literal-v1", SECRET)]))
        bundle = self.root / f"bundle-{self.counter}"
        result = self.call("dag-scheduler-v5", "payload", spec, "--output", bundle,
                           "--store", self.store, *options)
        self.assertEqual(result.returncode, expected, result.stderr)
        return bundle, json.loads(result.stdout)

    def snapshot(self):
        # atime is intentionally excluded; include directories and membership.
        result = {}
        for path in [self.root, *self.root.rglob("*")]:
            stat = path.lstat()
            result[str(path.relative_to(self.root))] = (
                stat.st_mode, stat.st_nlink, stat.st_mtime_ns, stat.st_size,
                hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None)
        return result

    def inspect(self, bundle, streams=False, expected=0):
        before = self.snapshot()
        result = self.call("dag-scheduler-v5", "payload-inspect", bundle,
                           *(["--stream-v15"] if streams else []))
        self.assertEqual(result.returncode, expected, result.stderr)
        self.assertEqual(before, self.snapshot(), "inspection changed artifacts")
        for private in [str(self.root).encode(), SECRET, SECRET.hex().encode(), b"payload-inputs",
                        b"stream-payload", b"header.jsonl", b"seal.json", b"input.bin", b"result.bin"]:
            self.assertNotIn(private, result.stdout + result.stderr)
        if expected == 2:
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')
            return None
        self.assertEqual(result.stderr, b"")
        value = json.loads(result.stdout)
        self.assertEqual(list(value), KEYS)
        self.assertEqual(result.stdout, json.dumps(value, separators=(",", ":")).encode() + b"\n")
        self.assertEqual(value["contract"], "dag-scheduler-v5-payload-inspect-v1")
        self.assertEqual(value["action"], "payload-inspect")
        self.assertEqual(value["status"], "INCOMPLETE" if expected == 1 else "VALID")
        self.assertEqual(value["input_completeness"], "NOT_PROVABLE")
        self.assertEqual(value["external_provenance"], "NOT_CHECKED")
        self.assertEqual(value["stream_check"], "VERIFIED_PRESENT" if streams else "NOT_REQUESTED")
        again = self.call("dag-scheduler-v5", "payload-inspect", bundle,
                          *(["--stream-v15"] if streams else []))
        self.assertEqual((again.returncode, again.stdout, again.stderr),
                         (result.returncode, result.stdout, result.stderr))
        self.assertEqual(before, self.snapshot())
        return value

    def test_solo_multi_v13_v14_stream_matrix(self):
        sidecar = self.root / "empty-inputs"
        sidecar.write_bytes(b'{"contract":"bruce-dag-scheduler-v5-external-inputs-v14","bindings":[]}')
        sidecar.chmod(0o600)
        for version in (13, 14):
            for multi in (False, True):
                for streams in (False, True):
                    with self.subTest(version=version, multi=multi, streams=streams):
                        nodes = [("a", "", "literal-v1", SECRET)]
                        if multi: nodes.append(("b", "a", "append-v1", b"!"))
                        options = ["--external-inputs", sidecar] if version == 14 else []
                        if streams: options.append("--stream-v15")
                        if multi: options.extend(["--workers", "2"])
                        bundle, _ = self.payload(nodes, *options)
                        value = self.inspect(bundle, streams)
                        self.assertEqual(value["source_contract"], f"dag-scheduler-v5-payload-v{version}")
                        self.assertEqual(value["node_count"], len(nodes))
                        self.assertEqual(value["accepted_artifact_count"], len(nodes))
                        self.assertEqual(value["verified_stream_count"], int(multi and streams))
                        self.assertEqual(value["verified_stream_bytes"], len(SECRET) if multi and streams else 0)
                        self.assertEqual(value["run_status"], "READY")
                        if not streams and multi: self.inspect(bundle, True, expected=2)

    def test_mixed_inputs_and_store_independence(self):
        _, _ = self.payload()
        manifest_path = next((self.store / "manifests").glob("*.manifest"))
        manifest = dict(line.split("=", 1) for line in manifest_path.read_text().splitlines() if "=" in line)
        sidecar = self.root / "external-inputs"
        sidecar.write_text(json.dumps({"contract": "bruce-dag-scheduler-v5-external-inputs-v14",
            "bindings": [{"node_id": "b", "inputs": [{"manifest_id": manifest_path.stem,
                "object_index": 0, "sha256": manifest["object_0_sha256"], "size_bytes": len(SECRET),
                "source_run_id": manifest["run_id"], "source_node_id": manifest["node_id"],
                "source_attempt_id": manifest["attempt_id"]}]}]}, separators=(",", ":")))
        sidecar.chmod(0o600)
        bundle, _ = self.payload([("a", "", "literal-v1", b"direct"),
                                 ("b", "a", "join-v1", b"|")],
                                "--stream-v15", "--external-inputs", sidecar)
        value = self.inspect(bundle, True)
        self.assertEqual(value["verified_stream_count"], 2)
        self.assertEqual(value["verified_stream_bytes"], len(SECRET) + 6)
        shutil.rmtree(self.store)
        self.assertEqual(self.inspect(bundle, True), value)

    def test_incomplete_and_truncation(self):
        bundle, _ = self.payload()
        log = next(Path(str(bundle) + ".run").glob("control/*.jsonl"))
        lines = log.read_bytes().splitlines(keepends=True)
        self.assertIn(b"RUN_FINISHED", lines[-1])
        log.write_bytes(b"".join(lines[:-1]))
        value = self.inspect(bundle, expected=1)
        self.assertFalse(value["run_finished"])
        self.assertEqual(value["run_status"], "INCOMPLETE")
        log.write_bytes(log.read_bytes()[:-3])
        self.inspect(bundle, expected=2)

    def test_failed_preflight_and_blocked_are_valid(self):
        bundle, raw = self.payload([("a", "", "literal-v1", b"A"),
                                   ("b", "a", "append-v1", b"B"),
                                   ("c", "b", "append-v1", b"C")],
                                  "--max-input-bytes", "1", expected=1)
        self.assertEqual([r["status"] for r in raw["receipts"]], ["READY", "FAILED", "BLOCKED"])
        value = self.inspect(bundle)
        self.assertEqual(value["run_status"], "FAILED")
        self.assertEqual(value["attempt_count"], 1)

    def test_failed_worker_retries_are_consistent(self):
        bundle, raw = self.payload(None, "--max-wall-ms", "1", "--retry", "2", expected=1)
        self.assertEqual(raw["failure_code"], "TIMEOUT")
        self.assertEqual([attempt["attempt_id"] for attempt in raw["attempts"]],
                         ["a-1", "a-1-2", "a-1-3"])
        value = self.inspect(bundle)
        self.assertEqual(value["run_status"], "FAILED")
        self.assertEqual(value["attempt_count"], 3)
        self.assertEqual(value["accepted_artifact_count"], 0)

    def test_publication_failure_keeps_verified_unaccepted_output(self):
        digest = hashlib.sha256(SECRET).hexdigest()
        prefix = self.store / "objects" / digest[:2]
        prefix.mkdir(mode=0o700, exist_ok=True)
        blob = prefix / (digest + ".blob")
        blob.write_bytes(b"corrupt existing publication destination")
        blob.chmod(0o600)
        bundle, raw = self.payload(expected=1)
        self.assertEqual(raw["status"], "FAILED")
        root = Path(str(bundle) + ".run")
        events = [json.loads(line) for line in next(root.glob("control/*.jsonl")).read_bytes().splitlines()]
        self.assertTrue(any(e["event"] == "RESULT_RECEIVED" and e["status"] == "READY" for e in events))
        self.assertFalse(any(e["event"] == "ARTIFACT_ACCEPTED" for e in events))
        value = self.inspect(bundle)
        self.assertEqual(value["run_status"], "FAILED")
        self.assertEqual(value["accepted_artifact_count"], 0)
        output = next(root.glob("attempts/*/output/result.bin"))
        self.assertEqual(output.read_bytes(), SECRET)
        output.write_bytes(b"altered unaccepted output")
        self.inspect(bundle, expected=2)

    def test_stream_corruption_is_opt_in_and_missing_tree_is_not_proof(self):
        bundle, _ = self.payload([("a", "", "literal-v1", SECRET),
                                 ("b", "a", "append-v1", b"!")], "--stream-v15")
        root = Path(str(bundle) + ".run") / "payload-inputs"
        compatibility = next(root.rglob("input.bin"))
        compatibility.write_bytes(b"wrong")
        self.inspect(bundle)
        self.inspect(bundle, True, expected=2)
        shutil.rmtree(root)
        value = self.inspect(bundle, True)
        self.assertEqual(value["verified_stream_count"], 0)

    def test_request_output_report_corruption_rejected(self):
        for target in ("request", "output", "report"):
            with self.subTest(target=target):
                bundle, _ = self.payload()
                root = Path(str(bundle) + ".run")
                path = bundle if target == "report" else next(root.glob(
                    "inputs/*.bin" if target == "request" else "attempts/*/output/result.bin"))
                path.write_bytes(path.read_bytes() + b"private-corruption")
                self.inspect(bundle, expected=2)

    def test_route_separation_and_bad_arguments(self):
        bundle, _ = self.payload()
        old = self.call("dag-scheduler-v5", "inspect", bundle)
        self.assertEqual(old.returncode, 2)
        spec = self.root / "execute-spec"; spec.write_bytes(spec_bytes([("a", "", "literal-v1", SECRET)]))
        execute = self.root / "execute-bundle"
        result = self.call("dag-scheduler-v5", "execute", spec, "--output", execute)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.inspect(execute, expected=2)
        self.assertEqual(self.call("dag-scheduler-v5", "inspect", execute).returncode, 0)
        for args in [[], [bundle, "--stream-v15", "--stream-v15"], ["--stream-v15", bundle],
                     [bundle, "--store", self.store], [bundle, "--retry", "1"], [bundle, "extra"],
                     [str(bundle) + ".run"], [self.root / "missing-private-name"]]:
            result = self.call("dag-scheduler-v5", "payload-inspect", *args)
            self.assertEqual((result.returncode, result.stdout, result.stderr),
                             (2, b"", b'{"error":"PROTOCOL_ERROR"}\n'))


if __name__ == "__main__":
    unittest.main()
