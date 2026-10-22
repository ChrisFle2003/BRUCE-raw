import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_dag_scheduler_v3_cli import BIN, SPEC, FLOW, FORBIDDEN, digest


USAGE = (b'{"error":"Usage: bruce dag-scheduler-v4 execute SPEC --output BUNDLE | '
         b'inspect BUNDLE | replay BUNDLE"}\n')
HEADER = b"BRUCE_DAG_SCHEDULER_RUN_V4\n"


class DagSchedulerV4CliTests(unittest.TestCase):
    """Real processes: publication, read-only verification and confidential bytes."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="bruce-v4-cli-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.spec = self.root / "graph.spec"
        self.bundle = self.root / "run.bundle"
        self.spec.write_bytes(SPEC)

    def call(self, *args):
        return subprocess.run([BIN, "dag-scheduler-v4", *map(str, args)],
                              capture_output=True, timeout=10)

    def export(self, target=None):
        return self.call("execute", self.spec, "--output", target or self.bundle)

    def report(self, process, action, bundle):
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertTrue(process.stdout.endswith(b"\n"))
        self.assertFalse(process.stdout.endswith(b"\n\n"))
        report = json.loads(process.stdout)
        expected = {key: value for key, value in FLOW.items()
                    if key not in ("contract", "action", "spec_version", "spec_digest")}
        expected.update(contract=f"dag-scheduler-v4-{action}-v1", action=action,
                        bundle_digest=bundle.split(b"\n", 2)[1].decode())
        self.assertEqual(report, expected)
        self.assertEqual(report["bundle_digest"], digest(bundle.split(b"\n", 2)[2]))
        for forbidden in (*FORBIDDEN, b"request-hex", b"\x00"):
            self.assertNotIn(forbidden, process.stdout)
        return report

    def test_export_canonical_bytes_and_all_reports_are_metadata_only(self):
        process = self.export()
        self.assertEqual(process.returncode, 0, process.stderr)
        bundle = self.bundle.read_bytes()
        self.assertTrue(bundle.startswith(HEADER))
        self.assertIn(b"alpha", bundle)
        self.assertIn(b"\x00\x00", bundle)
        self.report(process, "execute", bundle)
        # Publication leaves only the explicitly requested file, with private mode.
        self.assertEqual(set(self.root.iterdir()), {self.spec, self.bundle})
        self.assertEqual(self.bundle.stat().st_mode & 0o777, 0o600)
        for action in ("inspect", "replay"):
            before = (self.bundle.stat().st_mtime_ns, self.bundle.stat().st_ino,
                      self.bundle.read_bytes(), set(self.root.iterdir()))
            first = self.call(action, self.bundle)
            self.report(first, action, bundle)
            self.assertEqual(self.call(action, self.bundle).stdout, first.stdout)
            self.assertEqual(before, (self.bundle.stat().st_mtime_ns,
                                      self.bundle.stat().st_ino,
                                      self.bundle.read_bytes(), set(self.root.iterdir())))
        second = self.root / "second.bundle"
        self.report(self.export(second), "execute", bundle)
        self.assertEqual(second.read_bytes(), bundle)
        # Optional textual final LF is not part of the semantic run bundle.
        self.spec.write_bytes(SPEC[:-1])
        third = self.root / "third.bundle"
        self.report(self.export(third), "execute", bundle)
        self.assertEqual(third.read_bytes(), bundle)

    def test_existing_target_and_symlink_are_never_replaced(self):
        self.bundle.write_bytes(b"existing-evidence")
        for target in (self.bundle, self.root / "alias.bundle"):
            if target != self.bundle:
                target.symlink_to(self.bundle)
            process = self.export(target)
            self.assertEqual(process.returncode, 2, process.stderr)
            self.assertEqual(process.stdout, b"")
            self.assertIn(b"ExportExistsOrUnavailable", process.stderr)
            self.assertEqual(self.bundle.read_bytes(), b"existing-evidence")
            self.assertFalse(list(self.root.glob("*.tmp.*")))
        self.assertTrue((self.root / "alias.bundle").is_symlink())

    def test_concurrent_exports_have_exactly_one_complete_winner(self):
        args = [BIN, "dag-scheduler-v4", "execute", str(self.spec),
                "--output", str(self.bundle)]
        processes = [subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                     for _ in range(4)]
        outputs = [process.communicate(timeout=10) for process in processes]
        self.assertEqual(sorted(process.returncode for process in processes), [0, 2, 2, 2])
        bundle = self.bundle.read_bytes()
        self.report(self.call("replay", self.bundle), "replay", bundle)
        for process, (stdout, stderr) in zip(processes, outputs):
            self.assertEqual(bool(stdout), process.returncode == 0)
            self.assertEqual(bool(stderr), process.returncode != 0)
        self.assertEqual(set(self.root.iterdir()), {self.spec, self.bundle})

    def test_invalid_input_or_unwritable_destination_publishes_nothing(self):
        self.spec.write_bytes(SPEC.replace(b"literal-v1", b"unknown-v1", 1))
        process = self.export()
        self.assertEqual(process.returncode, 2, process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertEqual(process.stderr, b'{"error":"InvalidDagSchedulerV3SpecCallback"}\n')
        self.assertEqual(set(self.root.iterdir()), {self.spec})
        self.spec.write_bytes(SPEC)
        process = self.export(self.root / "missing" / "run.bundle")
        self.assertEqual(process.returncode, 2, process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertIn(b"CannotCreateExport", process.stderr)
        self.assertEqual(set(self.root.iterdir()), {self.spec})

    def test_corrupt_bundles_fail_read_only_without_raw_bytes(self):
        self.assertEqual(self.export().returncode, 0)
        original = self.bundle.read_bytes()
        payload = original.split(b"\n", 2)[2]
        def envelope(value):
            return HEADER + digest(value).encode() + b"\n" + value
        trace_prefix, trace_event, trace_suffix = payload.rpartition(b"1\n8:a-source")
        self.assertTrue(trace_event)
        mutations = {
            "header": original.replace(b"RUN_V4", b"RUN_V3", 1),
            "digest": HEADER + b"0" * 64 + b"\n" + payload,
            "length": envelope(b"99999999999999999999:" + payload),
            "request": envelope(payload.replace(b"5:alpha", b"5:omega", 1)),
            "payload": envelope(payload.replace(b"10:alpha:beta", b"10:omega:beta", 1)),
            "receipt": envelope(payload.replace(b"13:LITERAL_READY", b"13:ALTERED_READY", 1)),
            "trace": envelope(trace_prefix + b"2\n8:a-source" + trace_suffix),
            "trailing": envelope(payload + b"\n"),
            "truncated": original[:-1],
            "oversized": b"x" * (5 * 1024 * 1024 + 1),
        }
        for label, modified in mutations.items():
            self.assertNotEqual(modified, original, label)
            self.bundle.write_bytes(modified)
            # Structurally valid forged result codes/trace can be inspected;
            # only reexecution proves they differ from the fixed callback run.
            actions = ("replay",) if label in ("receipt", "trace") else ("inspect", "replay")
            before = self.bundle.stat().st_mtime_ns
            if label in ("receipt", "trace"):
                inspected = self.call("inspect", self.bundle)
                self.assertEqual(inspected.returncode, 0, inspected.stderr)
                report = json.loads(inspected.stdout)
                if label == "receipt":
                    self.assertEqual(report["receipts"][0]["output"]["result_code"],
                                     "ALTERED_READY")
                else:
                    self.assertEqual(report["trace"][1]["event"], "READY")
            for action in actions:
                with self.subTest(mutation=label, action=action):
                    process = self.call(action, self.bundle)
                    self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
                    self.assertEqual(process.stdout, b"")
                    self.assertNotIn(b"Usage:", process.stderr)
                    for forbidden in (*FORBIDDEN, b"omega"):
                        self.assertNotIn(forbidden, process.stderr)
            self.assertEqual(self.bundle.stat().st_mtime_ns, before)
            self.assertEqual(self.bundle.read_bytes(), modified)
            self.assertEqual(set(self.root.iterdir()), {self.spec, self.bundle})

    def test_help_and_exact_route_usage(self):
        help_process = subprocess.run([BIN, "--help"], capture_output=True, timeout=10)
        self.assertEqual(help_process.returncode, 0)
        self.assertEqual([line for line in help_process.stdout.decode().splitlines()
                          if "dag-scheduler-v4" in line], [
                              "bruce dag-scheduler-v4 execute SPEC --output BUNDLE",
                              "bruce dag-scheduler-v4 inspect BUNDLE",
                              "bruce dag-scheduler-v4 replay BUNDLE"])
        cases = [(), ("--help",), ("unknown", "file"), ("EXECUTE", "file")]
        for action in ("inspect", "replay"):
            cases += [(action,), (action, ""), (action, "--help"),
                      (action, "file", "extra"), (action, "file", "--output", "out")]
        cases += [("execute",), ("execute", "spec"), ("execute", "spec", "out"),
                  ("execute", "spec", "--output"), ("execute", "spec", "--out", "out"),
                  ("execute", "spec", "--output", "out", "extra"),
                  ("execute", "", "--output", "out"),
                  ("execute", "--help", "--output", "out"),
                  ("execute", "spec", "--output", ""),
                  ("execute", "spec", "--output", "-out")]
        for args in cases:
            with self.subTest(args=args):
                process = self.call(*args)
                self.assertEqual(process.returncode, 2)
                self.assertEqual(process.stdout, b"")
                self.assertEqual(process.stderr, USAGE)

    def test_v3_execute_bytes_are_unchanged(self):
        process = subprocess.run([BIN, "dag-scheduler-v3", "execute", str(self.spec)],
                                 capture_output=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertEqual(json.loads(process.stdout), FLOW)
        self.assertEqual(digest(process.stdout),
                         "184193ad0ce6dc81f58d5608204e0ce1e54994b4f79c0e6366868d76bcd2a2d0")


if __name__ == "__main__":
    unittest.main()
