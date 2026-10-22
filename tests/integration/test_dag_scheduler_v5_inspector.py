import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())

SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=inspect\n"
    b"max-started-nodes=1\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=696e73706563742d7061796c6f6164\n"
)

MULTI_SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=inspect-multi\n"
    b"max-started-nodes=8\n"
    b"max-total-receipt-bytes=4096\n\n"
    b"node=a\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=41\n\n"
    b"node=b\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=42\n"
)


class WorkerRunInspectorWorkflowTests(unittest.TestCase):
    def execute(self, root):
        spec = root / "workflow.spec"
        bundle = root / "bundle"
        spec.write_bytes(SPEC)
        result = subprocess.run(
            [BIN, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle)],
            capture_output=True, timeout=15,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertTrue(bundle.exists())
        return bundle

    @staticmethod
    def snapshot(root):
        return {
            path.relative_to(root): (path.read_bytes(), path.stat().st_mode, path.stat().st_nlink)
            for path in root.rglob("*") if path.is_file()
        }

    def inspect(self, bundle):
        return subprocess.run(
            [BIN, "dag-scheduler-v5", "inspect", str(bundle)],
            capture_output=True, timeout=15,
        )

    def test_valid_report_is_metadata_only_stable_and_read_only(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            bundle = self.execute(root)
            before = self.snapshot(root)
            first = self.inspect(bundle)
            second = self.inspect(bundle)
            self.assertEqual(first.returncode, 0)
            self.assertEqual(first.stderr, b"")
            self.assertEqual(first.stdout, second.stdout)
            report = json.loads(first.stdout)
            self.assertEqual(report["contract"], "dag-scheduler-v5-inspect-v1")
            self.assertEqual(report["status"], "VALID")
            self.assertTrue(report["run_finished"])
            self.assertEqual(report["run_status"], "READY")
            self.assertTrue(report["attempts"][0]["accepted"])
            self.assertTrue(report["attempts"][0]["artifact_verified"])
            public = first.stdout.decode()
            self.assertNotIn(b"inspect-payload", first.stdout)
            self.assertNotIn(str(root), public)
            self.assertNotIn("attempts/", public)
            self.assertNotIn("result.bin", public)
            self.assertEqual(before, self.snapshot(root))
            for spelling in ("./bundle", "sub/../bundle"):
                normalized = subprocess.run(
                    [BIN, "dag-scheduler-v5", "inspect", spelling], cwd=root,
                    capture_output=True, timeout=15,
                )
                self.assertEqual(normalized.returncode, 0)
                self.assertEqual(normalized.stderr, b"")
                self.assertEqual(normalized.stdout, first.stdout)

    def test_incomplete_run_is_diagnostic_and_does_not_resume(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            bundle = self.execute(root)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            lines = log_path.read_bytes().splitlines(keepends=True)
            self.assertTrue(lines[-1].find(b'"event":"RUN_FINISHED"') >= 0)
            log_path.write_bytes(b"".join(lines[:-1]))
            before = self.snapshot(root)
            result = self.inspect(bundle)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stderr, b"")
            report = json.loads(result.stdout)
            self.assertEqual(report["status"], "INCOMPLETE")
            self.assertFalse(report["run_finished"])
            self.assertEqual(before, self.snapshot(root))
            self.assertEqual(result.stdout, self.inspect(bundle).stdout)

    def test_protocol_errors_are_compact_for_truncation_digest_symlink_and_hardlink(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            bundle = self.execute(root)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            original_log = log_path.read_bytes()
            artifact = Path(str(bundle) + ".run/attempts/a-1/output/result.bin")
            original_artifact = artifact.read_bytes()
            original_bundle = bundle.read_bytes()
            metadata = json.loads(original_bundle)
            request_digest = metadata["request_digest"].encode()
            payload_digest = metadata["payload_digest"].encode()

            def assert_protocol():
                result = self.inspect(bundle)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

            log_path.write_bytes(original_log[:-1])
            assert_protocol()
            log_path.write_bytes(original_log)

            altered = original_bundle.replace(b'"reference":"artifact/a"',
                                               b'"reference":"artifact/b"', 1)
            bundle.write_bytes(altered)
            assert_protocol()
            bundle.write_bytes(original_bundle)

            # The execute report has a fixed root shape. A syntactically valid
            # but truncated four-field object must fail before indexed access.
            bundle.write_bytes(
                b'{"contract":"dag-scheduler-v5-cli-v1","action":"execute",'
                b'"spec_digest":"' + b"0" * 64 + b'","dag_id":"inspect"}\n'
            )
            assert_protocol()
            bundle.write_bytes(original_bundle)
            for old, new in (
                (b'"retry":0', b'"retry":4'),
                (b'"max_output_bytes":65536', b'"max_output_bytes":5242881'),
                (b'"request_digest":"' + request_digest,
                 b'"request_digest":"' + b'0' * 64),
                (b'"payload_digest":"' + payload_digest,
                 b'"payload_digest":"' + b'0' * 64),
            ):
                self.assertIn(old, original_bundle)
                bundle.write_bytes(original_bundle.replace(old, new, 1))
                assert_protocol()
                bundle.write_bytes(original_bundle)
            bundle.write_bytes(original_bundle[:-2] + b"\n")
            assert_protocol()
            bundle.write_bytes(original_bundle + b"\n")
            assert_protocol()
            bundle.write_bytes(original_bundle)

            text = original_log.decode()
            marker = '"digest":"'
            position = text.index(marker) + len(marker)
            text = text[:position] + ("0" * 64) + text[position + 64:]
            log_path.write_text(text)
            assert_protocol()
            log_path.write_bytes(original_log)

            # A valid second attempt payload must not become the source of the
            # first attempt merely because its digest and size match.
            cross_output = Path(str(bundle) + ".run/attempts/a-2/output")
            cross_output.mkdir(parents=True)
            cross_output.parent.chmod(0o700)
            cross_output.chmod(0o700)
            cross_artifact = cross_output / "result.bin"
            cross_artifact.write_bytes(original_artifact)
            cross_artifact.chmod(0o600)
            crossed_log = original_log.replace(
                b"attempts/a-1/output/result.bin",
                b"attempts/a-2/output/result.bin",
            )
            log_path.write_bytes(crossed_log)
            assert_protocol()
            log_path.write_bytes(original_log)

            outside = root / "outside.bin"
            outside.write_bytes(original_artifact)
            artifact.unlink()
            artifact.symlink_to(outside)
            assert_protocol()
            artifact.unlink()
            artifact.write_bytes(original_artifact)

            hardlink = artifact.with_name("second-link.bin")
            hardlink.hardlink_to(artifact)
            assert_protocol()
            hardlink.unlink()

    def test_multi_receipt_metadata_is_bound_to_its_run_log_node(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            spec = root / "multi.spec"
            bundle = root / "multi-bundle"
            spec.write_bytes(MULTI_SPEC)
            executed = subprocess.run(
                [BIN, "dag-scheduler-v5", "execute", str(spec), "--output", str(bundle),
                 "--workers", "2"],
                capture_output=True, timeout=15,
            )
            self.assertEqual(executed.returncode, 0, executed.stderr.decode())
            original = bundle.read_bytes()
            report = json.loads(original)
            self.assertEqual([receipt["node_id"] for receipt in report["receipts"]], ["a", "b"])
            self.assertEqual(len(report["receipts"]), 2)

            def assert_protocol():
                result = self.inspect(bundle)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

            # Keep each receipt's node_id, request_digest and public reference
            # in place while moving all attempt-owned metadata to the other
            # independent node. The run-log-derived attempt identity must win.
            for field in (
                "attempt_id", "status", "failure_code", "result_code",
                "payload_digest", "payload_size", "usage", "attempts",
            ):
                report["receipts"][0][field], report["receipts"][1][field] = (
                    report["receipts"][1][field], report["receipts"][0][field]
                )
            bundle.write_text(json.dumps(report, separators=(",", ":")) + "\n")
            assert_protocol()

            # A second mutation preserves the attempt IDs and public
            # references, exercising the digest/usage/history binding itself.
            report = json.loads(original)
            for field in (
                "status", "failure_code", "result_code", "payload_digest",
                "payload_size", "usage", "attempts",
            ):
                report["receipts"][0][field], report["receipts"][1][field] = (
                    report["receipts"][1][field], report["receipts"][0][field]
                )
            bundle.write_text(json.dumps(report, separators=(",", ":")) + "\n")
            assert_protocol()
            bundle.write_bytes(original)

    def test_deep_bundle_json_is_bounded_and_fails_closed(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            bundle = self.execute(root)
            original = bundle.read_bytes()
            marker = b'"attempts":['
            prefix, suffix = original.split(marker, 1)
            for nested in (
                b"[" * 100 + b"]" * 100,
                b'{"x":' * 100 + b"1" + b"}" * 100,
            ):
                bundle.write_bytes(prefix + marker + nested + b"," + suffix)
                result = self.inspect(bundle)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')
            bundle.write_bytes(original)

    def test_exact_route_and_bundle_binding_usage_errors(self):
        with tempfile.TemporaryDirectory(prefix="bruce-v5-inspect-") as directory:
            root = Path(directory)
            bundle = self.execute(root)
            for arguments in (
                ["dag-scheduler-v5", "inspect"],
                ["dag-scheduler-v5", "inspect", str(bundle), "extra"],
                ["dag-scheduler-v5", "inspect", str(bundle), "--output", str(root / "x")],
                ["dag-scheduler-v5", "inspect", str(bundle) + ".run"],
            ):
                result = subprocess.run([BIN, *arguments], capture_output=True, timeout=15)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"Usage: bruce dag-scheduler-v5 inspect BUNDLE", result.stderr)

            copied = root / "copied-bundle"
            shutil.copyfile(bundle, copied)
            result = self.inspect(copied)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

            alias = root / "bundle-alias"
            alias.symlink_to(bundle)
            result = self.inspect(alias)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')


if __name__ == "__main__":
    unittest.main()
