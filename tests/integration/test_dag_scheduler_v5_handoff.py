import hashlib
import json
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import time
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())


def spec_for(nodes):
    lines = ["BRUCE_DAG_SCHEDULER_SPEC_V3", "dag-id=handoff-workflow",
             f"max-started-nodes={len(nodes)}", "max-total-receipt-bytes=4096", ""]
    for node_id, dependencies, callback, request in nodes:
        lines += [f"node={node_id}", f"depends={dependencies}",
                  f"callback={callback}", f"request-hex={request.hex()}", ""]
    return "\n".join(lines).encode()


class ExecuteHandoffLineageV1Workflows(unittest.TestCase):
    def inspect_handoff(self, bundle):
        return subprocess.run([BIN, "dag-scheduler-v5", "inspect", str(bundle),
                               "--handoff-v1"], capture_output=True, timeout=10)

    def test_inspect_valid_fanin_fanout_and_read_only(self):
        nodes = [("a", "", "literal-v1", b"A"),
                 ("b", "a", "append-v1", b"B"),
                 ("c", "a", "append-v1", b"C"),
                 ("d", "b,c", "join-v1", b"D")]
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-inspect-") as directory:
            _, bundle, execute = self.run_execute(directory, nodes,
                                                  "--workers", "2", "--handoff-v1")
            self.assertEqual(execute.returncode, 0, execute.stderr)
            paths = [bundle, Path(str(bundle) + ".handoff-v1.json"),
                     Path(str(bundle) + ".run/control/run-log.jsonl")]
            before = [(path.read_bytes(), path.stat().st_mtime_ns) for path in paths]
            result = self.inspect_handoff(bundle)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, b"")
            sidecar = json.loads(paths[1].read_bytes())
            self.assertEqual(result.stdout,
                (f'{{"contract":"dag-scheduler-v5-handoff-inspect-v1","action":"inspect",'
                 f'"status":"VALID","run_status":"READY","run_id":"{sidecar["run_id"]}",'
                 '"node_count":4,"attempt_count":4,"binding_count":4,'
                 '"verified_input_bytes":6,"lineage_scope":"LOCAL_CONSISTENCY"}\n').encode())
            self.assertEqual(before, [(path.read_bytes(), path.stat().st_mtime_ns)
                                      for path in paths])

    def test_inspect_forward_dependency_uses_node_identity(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-forward-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "z", "append-v1", b"A"),
                 ("z", "", "literal-v1", b"Z")], "--handoff-v1")
            self.assertEqual(execute.returncode, 0, execute.stderr)
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 0, inspection.stderr)
            self.assertEqual(json.loads(inspection.stdout)["binding_count"], 1)

    def test_inspect_allows_ordinary_parent_ending_in_run(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-parent-") as directory:
            parent = Path(directory) / "workspace.run"
            parent.mkdir(mode=0o755)
            parent.chmod(0o755)
            _, bundle, execute = self.run_execute(parent,
                [("a", "", "literal-v1", b"A")], "--handoff-v1")
            self.assertEqual(execute.returncode, 0, execute.stderr)
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 0, inspection.stderr)

    def test_inspect_rejects_forged_failed_aggregate_when_all_receipts_ready(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-status-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "", "literal-v1", b"A"),
                 ("b", "", "literal-v1", b"B")], "--handoff-v1")
            self.assertEqual(execute.returncode, 0, execute.stderr)
            report = json.loads(bundle.read_bytes())
            self.assertEqual([receipt["status"] for receipt in report["receipts"]], ["READY", "READY"])
            report["status"] = "FAILED"
            report_bytes = (json.dumps(report, separators=(",", ":")) + "\n").encode()
            bundle.write_bytes(report_bytes)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            lines = log_path.read_bytes().splitlines()
            finished = json.loads(lines[-1])
            self.assertEqual(finished["event"], "RUN_FINISHED")
            finished["status"] = "FAILED"
            lines[-1] = json.dumps(finished, separators=(",", ":")).encode()
            log_path.write_bytes(b"\n".join(lines) + b"\n")
            sidecar_path = Path(str(bundle) + ".handoff-v1.json")
            sidecar = json.loads(sidecar_path.read_bytes())
            sidecar["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar_path.write_bytes((json.dumps(sidecar, separators=(",", ":")) + "\n").encode())
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 2, inspection.stdout)
            self.assertEqual(inspection.stdout, b"")
            self.assertEqual(inspection.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_inspect_rejects_forged_cancelled_aggregate_without_host_cancel(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-cancel-tamper-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "", "literal-v1", b"A"),
                 ("b", "a", "append-v1", b"B")],
                "--max-wall-ms", "1", "--handoff-v1")
            self.assertEqual(execute.returncode, 1, execute.stderr)
            report = json.loads(bundle.read_bytes())
            self.assertEqual(report["status"], "FAILED")
            self.assertIn("BLOCKED", [receipt["status"] for receipt in report["receipts"]])
            report["status"] = "CANCELLED"
            report_bytes = (json.dumps(report, separators=(",", ":")) + "\n").encode()
            bundle.write_bytes(report_bytes)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            lines = log_path.read_bytes().splitlines()
            self.assertFalse(any(b'"event":"CANCEL_REQUESTED"' in line for line in lines))
            finished = json.loads(lines[-1])
            self.assertEqual(finished["event"], "RUN_FINISHED")
            finished["status"] = "CANCELLED"
            lines[-1] = json.dumps(finished, separators=(",", ":")).encode()
            log_path.write_bytes(b"\n".join(lines) + b"\n")
            sidecar_path = Path(str(bundle) + ".handoff-v1.json")
            sidecar = json.loads(sidecar_path.read_bytes())
            sidecar["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar_path.write_bytes((json.dumps(sidecar, separators=(",", ":")) + "\n").encode())
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 2, inspection.stdout)
            self.assertEqual(inspection.stdout, b"")
            self.assertEqual(inspection.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_inspect_rejects_forged_blocked_aggregate_with_failed_receipt(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-blocked-tamper-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "", "literal-v1", b"A"),
                 ("b", "a", "append-v1", b"B")],
                "--max-wall-ms", "1", "--handoff-v1")
            self.assertEqual(execute.returncode, 1, execute.stderr)
            report = json.loads(bundle.read_bytes())
            self.assertEqual(report["status"], "FAILED")
            self.assertIn("FAILED", [receipt["status"] for receipt in report["receipts"]])
            report["status"] = "BLOCKED"
            report_bytes = (json.dumps(report, separators=(",", ":")) + "\n").encode()
            bundle.write_bytes(report_bytes)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            lines = log_path.read_bytes().splitlines()
            finished = json.loads(lines[-1])
            self.assertEqual(finished["event"], "RUN_FINISHED")
            finished["status"] = "BLOCKED"
            lines[-1] = json.dumps(finished, separators=(",", ":")).encode()
            log_path.write_bytes(b"\n".join(lines) + b"\n")
            sidecar_path = Path(str(bundle) + ".handoff-v1.json")
            sidecar = json.loads(sidecar_path.read_bytes())
            sidecar["report_digest"] = hashlib.sha256(report_bytes).hexdigest()
            sidecar_path.write_bytes((json.dumps(sidecar, separators=(",", ":")) + "\n").encode())
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 2, inspection.stdout)
            self.assertEqual(inspection.stdout, b"")
            self.assertEqual(inspection.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_inspect_rejects_missing_mutated_and_aliased_sidecar(self):
        for mutation in ("missing", "truncated", "digest", "symlink", "hardlink", "permissions", "readonly"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(
                    prefix="bruce-handoff-corrupt-") as directory:
                _, bundle, execute = self.run_execute(directory,
                    [("a", "", "literal-v1", b"A")], "--handoff-v1")
                self.assertEqual(execute.returncode, 0, execute.stderr)
                sidecar = Path(str(bundle) + ".handoff-v1.json")
                if mutation == "missing":
                    sidecar.unlink()
                elif mutation == "truncated":
                    sidecar.write_bytes(sidecar.read_bytes()[:-1])
                elif mutation == "digest":
                    data = json.loads(sidecar.read_bytes())
                    data["report_digest"] = "0" * 64
                    sidecar.write_bytes((json.dumps(data, separators=(",", ":")) + "\n").encode())
                elif mutation == "symlink":
                    target = Path(directory) / "copy"
                    sidecar.rename(target)
                    sidecar.symlink_to(target)
                elif mutation == "hardlink":
                    (Path(directory) / "other").hardlink_to(sidecar)
                elif mutation == "permissions":
                    sidecar.chmod(0o644)
                elif mutation == "readonly":
                    sidecar.chmod(0o400)
                result = self.inspect_handoff(bundle)
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_inspect_consistent_unfinished_log_is_incomplete(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-incomplete-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "", "literal-v1", b"A")], "--handoff-v1")
            self.assertEqual(execute.returncode, 0, execute.stderr)
            log = Path(str(bundle) + ".run/control/run-log.jsonl")
            lines = log.read_bytes().splitlines(keepends=True)
            self.assertIn(b'"event":"RUN_FINISHED"', lines[-1])
            log.write_bytes(b"".join(lines[:-1]))
            result = self.inspect_handoff(bundle)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertEqual(json.loads(result.stdout)["status"], "INCOMPLETE")
            self.assertEqual(json.loads(result.stdout)["run_status"], "INCOMPLETE")

    def test_inspect_rejects_binding_and_file_mutations(self):
        for mutation in ("missing_binding", "wrong_digest", "wrong_size", "wrong_reference",
                         "wrong_node", "artifact_bytes", "report_bytes", "request_bytes",
                         "request_parent_symlink"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(
                    prefix="bruce-handoff-binding-") as directory:
                _, bundle, execute = self.run_execute(directory,
                    [("a", "", "literal-v1", b"A"),
                     ("b", "a", "append-v1", b"B")], "--handoff-v1")
                self.assertEqual(execute.returncode, 0, execute.stderr)
                sidecar_path = Path(str(bundle) + ".handoff-v1.json")
                sidecar = json.loads(sidecar_path.read_bytes())
                binding = next(a for a in sidecar["attempts"] if a["consumer_node_id"] == "b")["inputs"][0]
                if mutation == "missing_binding":
                    next(a for a in sidecar["attempts"] if a["consumer_node_id"] == "b")["inputs"] = []
                elif mutation == "wrong_digest":
                    binding["sha256"] = "0" * 64
                elif mutation == "wrong_size":
                    binding["size_bytes"] += 1
                elif mutation == "wrong_reference":
                    binding["reference"] = "attempts/a-1/output/other.bin"
                elif mutation == "wrong_node":
                    sidecar["nodes"][1]["callback_id"] = "literal-v1"
                elif mutation == "artifact_bytes":
                    (Path(str(bundle) + ".run") / binding["reference"]).write_bytes(b"X")
                elif mutation == "report_bytes":
                    bundle.write_bytes(bundle.read_bytes().replace(b'"dag_id":"handoff-workflow"',
                                                                  b'"dag_id":"changed"'))
                elif mutation == "request_bytes":
                    (Path(str(bundle) + ".run/inputs/a.bin")).write_bytes(b"X")
                elif mutation == "request_parent_symlink":
                    inputs = Path(str(bundle) + ".run/inputs")
                    moved = Path(str(bundle) + ".run/inputs-real")
                    inputs.rename(moved)
                    inputs.symlink_to(moved, target_is_directory=True)
                if mutation in {"missing_binding", "wrong_digest", "wrong_size", "wrong_reference", "wrong_node"}:
                    sidecar_path.write_bytes((json.dumps(sidecar, separators=(",", ":")) + "\n").encode())
                result = self.inspect_handoff(bundle)
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')

    def test_inspect_blocked_run(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-blocked-") as directory:
            _, bundle, execute = self.run_execute(directory,
                [("a", "", "literal-v1", b"A"),
                 ("b", "a", "append-v1", b"B")],
                "--max-wall-ms", "1", "--handoff-v1")
            self.assertEqual(execute.returncode, 1, execute.stderr)
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 0, inspection.stderr)
            self.assertIn("BLOCKED", [r["status"] for r in json.loads(bundle.read_bytes())["receipts"]])
            self.assertEqual(json.loads(inspection.stdout)["run_status"], "FAILED")

    def run_execute(self, directory, nodes, *options):
        root = Path(directory)
        spec = root / "workflow.spec"
        bundle = root / "bundle"
        spec.write_bytes(spec_for(nodes))
        result = subprocess.run([BIN, "dag-scheduler-v5", "execute", str(spec),
                                 "--output", str(bundle), *options],
                                capture_output=True, timeout=20)
        return spec, bundle, result

    def test_root_chain_fanout_and_fanin_bind_direct_accepted_attempts(self):
        nodes = [("a", "", "literal-v1", b"A"),
                 ("b", "a", "append-v1", b"B"),
                 ("c", "a", "append-v1", b"C"),
                 ("d", "b,c", "join-v1", b"D")]
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-v1-") as directory:
            spec, bundle, result = self.run_execute(directory, nodes,
                                                     "--workers", "2", "--handoff-v1")
            self.assertEqual(result.returncode, 0, result.stderr)
            report_bytes = bundle.read_bytes()
            self.assertEqual(result.stdout, report_bytes)
            sidecar_path = Path(str(bundle) + ".handoff-v1.json")
            sidecar_bytes = sidecar_path.read_bytes()
            self.assertTrue(sidecar_bytes.endswith(b"\n"))
            self.assertEqual(sidecar_bytes.count(b"\n"), 1)
            self.assertEqual(stat.S_IMODE(sidecar_path.stat().st_mode), 0o600)
            self.assertEqual(sidecar_path.stat().st_nlink, 1)
            sidecar = json.loads(sidecar_bytes)
            self.assertEqual(sidecar["contract"], "bruce-execute-handoff-v1")
            self.assertEqual(sidecar["spec_digest"], hashlib.sha256(spec.read_bytes()).hexdigest())
            self.assertEqual(sidecar["report_digest"], hashlib.sha256(report_bytes).hexdigest())
            self.assertEqual([(n["node_id"], n["dependencies"]) for n in sidecar["nodes"]],
                             [("a", []), ("b", ["a"]), ("c", ["a"]), ("d", ["b", "c"])])
            attempts = {a["consumer_node_id"]: a for a in sidecar["attempts"]}
            self.assertEqual(set(attempts), {"a", "b", "c", "d"})
            self.assertEqual(attempts["a"]["inputs"], [])
            self.assertEqual(attempts["b"]["inputs"][0]["producer_attempt_id"],
                             attempts["a"]["consumer_attempt_id"])
            self.assertEqual(attempts["c"]["inputs"][0]["producer_attempt_id"],
                             attempts["a"]["consumer_attempt_id"])
            self.assertEqual([i["producer_node_id"] for i in attempts["d"]["inputs"]], ["b", "c"])
            self.assertEqual([i["input_index"] for i in attempts["d"]["inputs"]], [0, 1])
            for attempt in sidecar["attempts"]:
                for binding in attempt["inputs"]:
                    expected = f"attempts/{binding['producer_attempt_id']}/output/result.bin"
                    self.assertEqual(binding["reference"], expected)
                    content = (Path(str(bundle) + ".run") / expected).read_bytes()
                    self.assertEqual(binding["size_bytes"], len(content))
                    self.assertEqual(binding["sha256"], hashlib.sha256(content).hexdigest())

    def test_failed_retries_have_one_empty_lineage_entry_per_started_attempt(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-retry-") as directory:
            _, bundle, result = self.run_execute(directory,
                [("a", "", "literal-v1", b"A")],
                "--max-wall-ms", "1", "--retry", "2", "--handoff-v1")
            self.assertEqual(result.returncode, 1, result.stderr)
            sidecar = json.loads(Path(str(bundle) + ".handoff-v1.json").read_bytes())
            events = [json.loads(line) for line in
                      Path(str(bundle) + ".run/control/run-log.jsonl").read_text().splitlines()]
            started = [e["attempt_id"] for e in events if e["event"] == "ATTEMPT_STARTED"]
            self.assertEqual(started, ["a-1", "a-1-2", "a-1-3"])
            self.assertEqual([a["consumer_attempt_id"] for a in sidecar["attempts"]], started)
            self.assertTrue(all(a["inputs"] == [] for a in sidecar["attempts"]))
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 0, inspection.stderr)
            inspected = json.loads(inspection.stdout)
            self.assertEqual(inspected["status"], "VALID")
            self.assertEqual(inspected["run_status"], "FAILED")
            self.assertEqual(inspected["attempt_count"], 3)

    def test_host_cancel_has_no_lineage_entry_after_cancel(self):
        nodes = [("a", "", "literal-v1", b"A"),
                 ("b", "", "literal-v1", b"B"),
                 ("c", "a", "append-v1", b"C")]
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-cancel-") as directory:
            root = Path(directory)
            spec = root / "workflow.spec"
            bundle = root / "bundle"
            spec.write_bytes(spec_for(nodes))
            process = subprocess.Popen([BIN, "dag-scheduler-v5", "execute", str(spec),
                "--output", str(bundle), "--workers", "1", "--handoff-v1"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            log_path = Path(str(bundle) + ".run/control/run-log.jsonl")
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if log_path.exists() and b'"event":"ATTEMPT_STARTED"' in log_path.read_bytes():
                    break
                time.sleep(0.001)
            else:
                process.kill()
                self.fail("worker did not start")
            cancel = subprocess.run([BIN, "dag-scheduler-v5", "cancel", str(bundle)],
                                    capture_output=True, timeout=10)
            stdout, stderr = process.communicate(timeout=15)
            self.assertEqual(cancel.returncode, 0, cancel.stderr)
            self.assertEqual(process.returncode, 1, stderr)
            self.assertEqual(stdout, bundle.read_bytes())
            sidecar = json.loads(Path(str(bundle) + ".handoff-v1.json").read_bytes())
            events = [json.loads(line) for line in log_path.read_text().splitlines()]
            started = [e["attempt_id"] for e in events if e["event"] == "ATTEMPT_STARTED"]
            self.assertEqual([a["consumer_attempt_id"] for a in sidecar["attempts"]], started)
            self.assertEqual(json.loads(stdout)["status"], "CANCELLED")
            inspection = self.inspect_handoff(bundle)
            self.assertEqual(inspection.returncode, 0, inspection.stderr)
            self.assertEqual(json.loads(inspection.stdout)["run_status"], "CANCELLED")

    def test_syntax_rejection_precedes_spec_access_and_plain_execute_has_no_sidecar(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-syntax-") as directory:
            root = Path(directory)
            missing = root / "missing.spec"
            bundle = root / "bundle"
            prefix = [BIN, "dag-scheduler-v5"]
            invalid = [
                ["execute", str(missing), "--output", str(bundle), "--handoff-v1", "--handoff-v1"],
                ["execute", str(missing), "--output", str(bundle), "--retry", "--handoff-v1", "2"],
                ["execute", str(missing), "--output", str(bundle), "--handoff-v1", "--unknown"],
                ["execute", str(missing), "--output", str(bundle) + ".run", "--handoff-v1"],
                ["payload", str(missing), "--output", str(bundle), "--store", str(root / "store"), "--handoff-v1"],
                ["inspect", str(bundle), "--handoff-v1", "--handoff-v1"],
                ["inspect", "--handoff-v1", str(bundle)],
                ["inspect", str(bundle), "--handoff-v1", "extra"],
                ["publish", str(bundle), "--store", str(root / "store"), "--handoff-v1"],
            ]
            for args in invalid:
                with self.subTest(args=args):
                    result = subprocess.run(prefix + args, capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(result.stdout, b"")
                    self.assertEqual(result.stderr, b'{"error":"PROTOCOL_ERROR"}\n')
            spec, bundle, result = self.run_execute(directory,
                [("a", "", "literal-v1", b"A")])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, bundle.read_bytes())
            self.assertFalse(Path(str(bundle) + ".handoff-v1.json").exists())

    def test_existing_sidecar_fails_adoption_without_stdout_or_overwrite(self):
        with tempfile.TemporaryDirectory(prefix="bruce-handoff-adopt-") as directory:
            sidecar = Path(directory) / "bundle.handoff-v1.json"
            sidecar.write_bytes(b"existing")
            _, bundle, result = self.run_execute(directory,
                [("a", "", "literal-v1", b"A")], "--handoff-v1")
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(result.stderr, b'{"error":"HANDOFF_RECORD_INVALID"}\n')
            self.assertTrue(bundle.exists())
            self.assertEqual(sidecar.read_bytes(), b"existing")


if __name__ == "__main__":
    unittest.main()
