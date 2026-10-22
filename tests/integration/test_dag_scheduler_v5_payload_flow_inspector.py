"""Real flow-v2 inspector workflows, including tamper and stream checks."""

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


BIN = str(Path(sys.argv.pop(1)).resolve())
INSPECT_KEYS = [
    "contract", "action", "status", "run_status", "run_id", "node_count",
    "attempt_count", "binding_count", "verified_input_bytes", "stream_v15",
    "input_completeness", "provenance_scope",
]


def flow_spec(*, max_started_nodes: int = 4, single: bool = False,
              request_hex: str = "41") -> bytes:
    tail = b"" if single else b"\nnode=b\ndepends=a\ncallback=append-v1\nrequest-hex=42\n"
    return (f"BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=flow-inspector\nmax-started-nodes={max_started_nodes}\nmax-total-receipt-bytes=4096\n\nnode=a\ndepends=\ncallback=literal-v1\nrequest-hex={request_hex}\n".encode() + tail)


class PayloadFlowInspectorWorkflows(unittest.TestCase):
    def call(self, *args):
        return subprocess.run([BIN, *map(str, args)], capture_output=True, timeout=30)

    def create_store(self, root: Path) -> Path:
        source = root / "store-source.bin"
        source.write_bytes(b"store-seed")
        source.chmod(0o600)
        store = root / "store"
        result = self.call("artifact-store-v1", "import", source, "--store", store)
        self.assertEqual(result.returncode, 0, result.stderr)
        return store

    def create_payload(self, root: Path, *, stream: bool = False,
                       max_started_nodes: int = 4, single: bool = False,
                       expect_success: bool = True, request_hex: str = "41",
                       max_output_bytes: int | None = None,
                       max_wall_ms: int | None = None, retry: int | None = None):
        spec = root / "flow.spec"
        spec.write_bytes(flow_spec(max_started_nodes=max_started_nodes, single=single,
                                   request_hex=request_hex))
        store = self.create_store(root)
        bundle = root / "flow-bundle"
        options = ["--flow-v2"]
        if stream:
            options.append("--stream-v15")
        if max_output_bytes is not None:
            options.extend(["--max-output-bytes", str(max_output_bytes)])
        if max_wall_ms is not None:
            options.extend(["--max-wall-ms", str(max_wall_ms)])
        if retry is not None:
            options.extend(["--retry", str(retry)])
        result = self.call("dag-scheduler-v5", "payload", spec, "--output", bundle,
                           "--store", store, *options)
        self.assertEqual(result.returncode, 0 if expect_success else 1, result.stderr)
        self.assertEqual(bundle.read_bytes(), result.stdout)
        return bundle

    def inspect_single(self, bundle: Path):
        result = self.call("dag-scheduler-v5", "payload-inspect", bundle, "--flow-v2")
        self.assertEqual(result.returncode, 0, result.stderr)
        value = json.loads(result.stdout)
        self.assertEqual(value["status"], "VALID")
        self.assertEqual(value["run_status"], "READY")
        self.assertEqual(value["node_count"], 1)
        self.assertEqual(value["attempt_count"], 1)
        return value

    def inspect(self, bundle: Path, expected: int = 0,
                expected_run_status: str = "READY", expected_attempt_count: int = 2,
                expected_binding_count: int = 1,
                expected_verified_input_bytes: int = 1):
        result = self.call("dag-scheduler-v5", "payload-inspect", bundle, "--flow-v2")
        self.assertEqual(result.returncode, expected, result.stderr)
        if expected == 2:
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')
            return None
        self.assertEqual(result.stderr, b"")
        value = json.loads(result.stdout)
        self.assertEqual(list(value), INSPECT_KEYS)
        self.assertEqual(result.stdout, json.dumps(value, separators=(",", ":")).encode() + b"\n")
        self.assertEqual(value["contract"], "dag-scheduler-v5-payload-flow-inspect-v2")
        self.assertEqual(value["action"], "payload-inspect")
        self.assertEqual(value["status"], "VALID")
        self.assertEqual(value["run_status"], expected_run_status)
        self.assertEqual(value["node_count"], 2)
        self.assertEqual(value["attempt_count"], expected_attempt_count)
        self.assertEqual(value["binding_count"], expected_binding_count)
        self.assertEqual(value["verified_input_bytes"], expected_verified_input_bytes)
        self.assertEqual(value["input_completeness"], "VERIFIED")
        self.assertEqual(value["provenance_scope"], "LOCAL_CONSISTENCY")
        return value

    def test_valid_flow_v2_inspector_and_tampered_sidecar(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-inspector-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            value = self.inspect(bundle)
            self.assertFalse(value["stream_v15"])
            again = self.call("dag-scheduler-v5", "payload-inspect", bundle, "--flow-v2")
            self.assertEqual((again.returncode, again.stdout, again.stderr),
                             (0, json.dumps(value, separators=(",", ":")).encode() + b"\n", b""))

            sidecar = Path(str(bundle) + ".flow-v2.json")
            original = sidecar.read_bytes()
            self.assertIn(b'"report_digest":"', original)
            sidecar.write_bytes(original.replace(b'"report_digest":"', b'"report_digest":"0', 1))
            self.inspect(bundle, expected=2)

    def test_single_node_adapter_id_layout_is_valid(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-single-") as directory:
            bundle = self.create_payload(Path(directory), single=True)
            self.inspect_single(bundle)

    def test_ready_top_level_payload_digest_and_size_are_bound(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-ready-binding-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            report = json.loads(bundle.read_bytes())
            report["receipts"][0]["payload_size"] += 1
            report_bytes = json.dumps(report, separators=(",", ":")).encode() + b"\n"
            bundle.write_bytes(report_bytes)
            sidecar = Path(str(bundle) + ".flow-v2.json")
            sidecar_value = json.loads(sidecar.read_bytes())
            sidecar_value["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar.write_bytes(json.dumps(sidecar_value, separators=(",", ":")).encode() + b"\n")
            self.inspect(bundle, expected=2)

    def test_terminal_receipt_cannot_downgrade_accepted_attempt(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-terminal-tamper-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            report = json.loads(bundle.read_bytes())
            receipt = report["receipts"][0]
            receipt.update({"status": "FAILED", "failure_code": "FAILED",
                            "result_code": "FAILED", "reference": None,
                            "payload_digest": None, "payload_size": 0})
            report_bytes = json.dumps(report, separators=(",", ":")).encode() + b"\n"
            bundle.write_bytes(report_bytes)
            sidecar = Path(str(bundle) + ".flow-v2.json")
            sidecar_value = json.loads(sidecar.read_bytes())
            sidecar_value["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar.write_bytes(json.dumps(sidecar_value, separators=(",", ":")).encode() + b"\n")
            self.inspect(bundle, expected=2)

    def test_report_status_must_match_run_finished(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-status-tamper-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            report = json.loads(bundle.read_bytes())
            report["status"] = "FAILED"
            report_bytes = json.dumps(report, separators=(",", ":")).encode() + b"\n"
            bundle.write_bytes(report_bytes)
            sidecar = Path(str(bundle) + ".flow-v2.json")
            sidecar_value = json.loads(sidecar.read_bytes())
            sidecar_value["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar.write_bytes(json.dumps(sidecar_value, separators=(",", ":")).encode() + b"\n")
            self.inspect(bundle, expected=2)

    def test_root_only_run_requires_payload_inputs_directory(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-root-layout-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root, single=True)
            shutil.rmtree(Path(str(bundle) + ".run/payload-inputs"))
            result = self.call("dag-scheduler-v5", "payload-inspect", bundle, "--flow-v2")
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_started_attempt_order_and_bidirectional_completeness_are_required(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-attempt-order-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            sidecar = Path(str(bundle) + ".flow-v2.json")
            value = json.loads(sidecar.read_bytes())
            value["attempts"].reverse()
            sidecar.write_bytes(json.dumps(value, separators=(",", ":")).encode() + b"\n")
            self.inspect(bundle, expected=2)

    def test_failed_and_blocked_receipts_are_valid_without_artifacts(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-blocked-") as directory:
            bundle = self.create_payload(Path(directory), expect_success=False,
                                         request_hex="4142", max_output_bytes=1)
            value = self.inspect(bundle, expected_run_status="FAILED",
                                 expected_attempt_count=1,
                                 expected_binding_count=0,
                                 expected_verified_input_bytes=0)
            self.assertEqual(value["run_status"], "FAILED")
            report = json.loads(bundle.read_bytes())
            self.assertEqual(report["receipts"][1]["status"], "BLOCKED")
            self.assertIsNone(report["receipts"][1]["attempt_id"])

    def test_failed_retry_history_and_blocked_descendant_are_valid(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-retry-") as directory:
            bundle = self.create_payload(Path(directory), expect_success=False,
                                         max_wall_ms=1, retry=2)
            value = self.inspect(bundle, expected_run_status="FAILED",
                                 expected_attempt_count=3,
                                 expected_binding_count=0,
                                 expected_verified_input_bytes=0)
            report = json.loads(bundle.read_bytes())
            self.assertEqual(len(report["receipts"][0]["attempts"]), 3)
            self.assertTrue(all(not attempt["accepted"]
                                for attempt in report["receipts"][0]["attempts"]))
            self.assertEqual(report["receipts"][1]["status"], "BLOCKED")

    def test_tampered_request_input_is_protocol_error(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-input-tamper-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            request = Path(str(bundle) + ".run/inputs/a.bin")
            report = json.loads(bundle.read_bytes())
            self.assertEqual(hashlib.sha256(request.read_bytes()).hexdigest(),
                             report["receipts"][0]["request_digest"])
            request.write_bytes(b"tampered")
            self.inspect(bundle, expected=2)

    def test_missing_request_input_is_protocol_error(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-input-missing-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root)
            request = Path(str(bundle) + ".run/inputs/a.bin")
            request.unlink()
            self.inspect(bundle, expected=2)

    def test_valid_stream_flow_and_tampered_compatibility_input(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-stream-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root, stream=True)
            value = self.inspect(bundle)
            self.assertTrue(value["stream_v15"])
            stream_root = Path(str(bundle) + ".run/payload-inputs/a-2-1/input-0")
            self.assertEqual((stream_root / "input.bin").read_bytes(), b"A")
            self.assertTrue((stream_root / "stream-payload/header.jsonl").exists())
            (stream_root / "input.bin").write_bytes(b"tampered")
            self.inspect(bundle, expected=2)

    def test_tampered_stream_chunk_is_protocol_error(self):
        with tempfile.TemporaryDirectory(prefix="bruce-flow-stream-chunk-tamper-") as directory:
            root = Path(directory)
            bundle = self.create_payload(root, stream=True)
            chunk = Path(str(bundle) + ".run/payload-inputs/a-2-1/input-0/stream-payload/") / \
                "chunk-00000000000000000000.bin"
            self.assertEqual(chunk.read_bytes(), b"A")
            chunk.write_bytes(b"tampered")
            self.inspect(bundle, expected=2)


if __name__ == "__main__":
    unittest.main()
