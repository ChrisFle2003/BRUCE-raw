import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
SOURCE = Path(__file__).resolve().parents[3]
HELP_LINES = [f"bruce phase-dag-v3 {command}" for command in (
    "execute SPEC --output BUNDLE", "inspect BUNDLE", "replay BUNDLE")]


class PhaseDagV3CliAvailabilityTests(unittest.TestCase):
    # Catch omitted routing/help and accidental exposure in NanoCell-off builds.
    def test_enabled_help_and_command_boundary(self):
        process = subprocess.run([BIN, "--help"], capture_output=True, text=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual([line for line in process.stdout.splitlines()
                          if "phase-dag-v3" in line], HELP_LINES)
        for suffix in ((), ("unknown",)):
            process = subprocess.run([BIN, "phase-dag-v3", *suffix],
                                     capture_output=True, text=True, timeout=10)
            self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
            self.assertEqual(json.loads(process.stderr), {
                "error": "Usage: bruce phase-dag-v3 execute|inspect|replay"})

    def test_nanocell_disabled_build_has_no_command_or_help(self):
        with tempfile.TemporaryDirectory(prefix="bruce-phase-dag-v3-off-") as tmp:
            source = Path(tmp) / "source"
            shutil.copytree(SOURCE, source)
            build = Path(tmp) / "off-build"
            cmake = ["cmake"]
            if os.access(source / "build-host-deps/root/usr/bin/pkgconf", os.X_OK):
                cmake.insert(0, str(source / "scripts/with-host-deps.sh"))
            for command in (
                [*cmake, "-G", "Ninja", "-S", str(source), "-B", str(build),
                 "-DBUILD_TESTING=OFF", "-DCMAKE_BUILD_TYPE=Release",
                 "-DBRUCE_BUILD_MOD_NANOCELL=OFF", "-DBRUCE_BUILD_MOD_MCP=OFF"],
                [*cmake, "--build", str(build), "--target", "bruce", "-j", "2"],
            ):
                process = subprocess.run(command, cwd=source, capture_output=True,
                                         text=True, timeout=300)
                self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
            binary = str(build / "bruce")
            process = subprocess.run([binary, "--help"], capture_output=True, text=True, timeout=10)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertNotIn("phase-dag-v3", process.stdout)
            for suffix in ((), ("unknown",)):
                process = subprocess.run([binary, "phase-dag-v3", *suffix],
                                         capture_output=True, text=True, timeout=10)
                self.assertEqual(process.returncode, 2)
                self.assertEqual(json.loads(process.stderr), {"error": "UnknownCommand: phase-dag-v3"})


class PhaseDagV3CliFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-phase-dag-v3-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.spec_dir = self.root / "spec"
        self.spec_dir.mkdir()
        (self.spec_dir / "context.bin").write_bytes(b"nanocell-reference-context-v1")
        self.spec = self.spec_dir / "graph.spec"
        self.output = self.root / "run.bundle"

    def node(self, node="a", depends="", technique="masked-match-v1", operations=16):
        return (f"node={node}\ndepends={depends}\ncore=1020a54050607080\n"
                "active-index=2\ncontext-id=reference.context_1\ncontext-file=context.bin\n"
                f"technique={technique}\nmax-operations={operations}\nmax-trace-entries=18\n")

    def gate(self, node="gate", depends="a"):
        return f"node={node}\ndepends={depends}\nadapter=payload-gate-v1\n"

    def spec_text(self, nodes=None, receipt_budget=65536):
        return ("BRUCE_PHASE_DAG_SPEC_V3\ndag-id=demo\nmax-started-nodes=3\n"
                f"max-receipt-bytes={receipt_budget}\nmax-trace-entries=32\n\n" +
                "\n".join(nodes or [self.node(), self.gate("gate1"), self.gate("gate2", "gate1")]))

    def call(self, *args):
        return subprocess.run([BIN, "phase-dag-v3", *map(str, args)], cwd=self.root,
                              capture_output=True, text=True, timeout=10)

    def execute(self, text):
        self.spec.write_text(text, encoding="utf-8")
        return self.call("execute", self.spec, "--output", self.output)

    def assert_private(self, process):
        for secret in ("private-context", "1020a54050607080", "nanocell-phase-payload-v1",
                       "payload-gate-payload-v1", "nanocell-reference-context-v1"):
            self.assertNotIn(secret, process.stdout + process.stderr)

    def assert_error(self, process, expected=None):
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        error = json.loads(process.stderr)
        self.assertEqual(set(error), {"error"})
        self.assertTrue(error["error"])
        self.assertNotEqual(error["error"], "UnknownCommand: phase-dag-v3")
        if expected is not None:
            self.assertEqual(error["error"], expected)
        self.assert_private(process)

    def assert_report(self, process, action="execute", outcomes=None):
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertEqual(process.stderr, "")
        report = json.loads(process.stdout)
        self.assertEqual(set(report), {"contract", "action", "dag_version", "dag_id",
                                       "node_count", "bundle_digest", "receipts"})
        self.assertEqual(report["contract"], "phase-dag-v3-cli-v1")
        self.assertEqual(report["action"], action)
        self.assertEqual(report["dag_version"], "phase-dag-v3")
        self.assertEqual(report["dag_id"], "demo")
        self.assertEqual(report["receipts"], outcomes or [
            {"node_id": node, "outcome": "READY", "failure_code": ""}
            for node in ("a", "gate1", "gate2")])
        self.assertEqual(report["node_count"], len(report["receipts"]))
        bundle = self.output.read_bytes()
        self.assertTrue(bundle.startswith(b"BRUCE_PHASE_DAG_RUN_V3\n"))
        self.assertEqual(report["bundle_digest"], hashlib.sha256(bundle).hexdigest())
        self.assert_private(process)
        return report


class PhaseDagV3CliWorkflowTests(PhaseDagV3CliFixture):
    # Catch missing gate-to-gate transport, failed publication, or external replay dependency.
    def test_nanocell_two_gates_portable_and_deterministic(self):
        text = self.spec_text()
        original = self.assert_report(self.execute(text))
        bundle = self.output.read_bytes()
        self.assertIn(b"nanocell-phase-payload-v1", bundle)
        self.assertIn(b"payload-gate-payload-v1", bundle)
        self.assertIn(b"22:payload-gate-output-v18:COMPLETE1:1", bundle)
        second = self.root / "second.bundle"
        process = self.call("execute", self.spec, "--output", second)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertEqual(second.read_bytes(), bundle)
        shutil.rmtree(self.spec_dir)
        for action in ("inspect", "replay"):
            report = self.assert_report(self.call(action, self.output), action)
            self.assertEqual({**report, "action": "execute"}, original)
            self.assertEqual(self.output.read_bytes(), bundle)

    def test_noncomplete_parent_makes_gate_rejected_and_replays(self):
        for technique, operations in (("context-conflict-v1", 16), ("masked-match-v1", 0)):
            with self.subTest(technique=technique):
                self.output = self.root / (technique + ".bundle")
                self.assert_report(self.execute(self.spec_text([
                    self.node(technique=technique, operations=operations),
                    self.gate("gate1"), self.gate("gate2", "gate1")])))
                self.assertIn(b"22:payload-gate-output-v18:REJECTED1:1", self.output.read_bytes())
                self.assertIn(b"CONTEXT_REJECTED" if technique == "context-conflict-v1" else b"INCOMPLETE",
                              self.output.read_bytes())
                self.assert_report(self.call("replay", self.output), "replay")

    def test_receipt_budget_failure_blocks_gate_chain(self):
        outcomes = [
            {"node_id": "a", "outcome": "FAILED", "failure_code": "DagReceiptBudgetExceeded"},
            {"node_id": "gate1", "outcome": "BLOCKED", "failure_code": ""},
            {"node_id": "gate2", "outcome": "BLOCKED", "failure_code": ""}]
        self.assert_report(self.execute(self.spec_text(receipt_budget=1)), outcomes=outcomes)
        self.assert_report(self.call("replay", self.output), "replay", outcomes)

    def test_existing_output_is_never_overwritten(self):
        self.output.write_bytes(b"existing")
        self.assert_error(self.execute(self.spec_text()))
        self.assertEqual(self.output.read_bytes(), b"existing")
        self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])


class PhaseDagV3CliParallelTests(PhaseDagV3CliFixture):
    # Catch rejected valid limits, relaxed numeric parsing, or changed parallel bundle bytes.
    def test_default_and_parallel_limits_preserve_multiwave_bundle_and_replay(self):
        nodes = [self.gate("a", "b"), self.node("b"), self.node("c"),
                 self.node("d"), self.node("e"), self.gate("f", "c,d,e"),
                 self.gate("g", "a,f"), self.gate("h", "g")]
        text = self.spec_text(nodes).replace("max-started-nodes=3", "max-started-nodes=8")
        outcomes = [{"node_id": node, "outcome": "READY", "failure_code": ""}
                    for node in "abcdefgh"]
        original = self.assert_report(self.execute(text), outcomes=outcomes)
        bundle = self.output.read_bytes()
        for limit in (1, 2, 3, 4):
            with self.subTest(limit=limit):
                self.output = self.root / f"parallel-{limit}.bundle"
                result = self.call("execute", self.spec, "--output", self.output,
                                   "--max-parallel-nodes", limit)
                self.assertEqual(self.assert_report(result, outcomes=outcomes), original)
                self.assertEqual(self.output.read_bytes(), bundle)
        shutil.rmtree(self.spec_dir)
        for action in ("inspect", "replay"):
            report = self.assert_report(self.call(action, self.output), action, outcomes)
            self.assertEqual(report, {**original, "action": action})
            self.assertEqual(self.output.read_bytes(), bundle)

    def test_invalid_parallel_limits_do_not_publish(self):
        self.spec.write_text(self.spec_text(), encoding="utf-8")
        for limit in ("0", "5", "-1", "x", "", "+2", " 2", "2 ", "2x", "2.0", "0x2",
                      "18446744073709551616"):
            with self.subTest(limit=limit):
                result = self.call("execute", self.spec, "--output", self.output,
                                   "--max-parallel-nodes", limit)
                self.assert_error(result, "InvalidPhaseDagParallelLimit")
                self.assertFalse(self.output.exists())
                self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    def test_parallel_option_is_rejected_by_inspect_and_replay(self):
        self.assert_report(self.execute(self.spec_text()))
        bundle = self.output.read_bytes()
        for action in ("inspect", "replay"):
            with self.subTest(action=action):
                self.assert_error(self.call(action, self.output, "--max-parallel-nodes", 2),
                                  f"Usage: bruce phase-dag-v3 {action} BUNDLE")
                self.assertEqual(self.output.read_bytes(), bundle)

    def test_parallel_option_requires_exact_execute_argument_shape(self):
        self.spec.write_text(self.spec_text(), encoding="utf-8")
        for args in (("execute", self.spec, "--output", self.output, "--max-parallel-nodes"),
                     ("execute", self.spec, "--max-parallel-nodes", "2", "--output", self.output),
                     ("execute", self.spec, "--output", self.output, "--unknown", "2"),
                     ("execute", self.spec, "--output", self.output, "--max-parallel-nodes", "2",
                      "--max-parallel-nodes", "4")):
            with self.subTest(args=args):
                self.assert_error(self.call(*args))
                self.assertFalse(self.output.exists())
                self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    def test_parallel_execute_never_overwrites_existing_output(self):
        self.spec.write_text(self.spec_text(), encoding="utf-8")
        self.output.write_bytes(b"existing")
        result = self.call("execute", self.spec, "--output", self.output,
                           "--max-parallel-nodes", 4)
        self.assert_error(result)
        self.assertTrue(json.loads(result.stderr)["error"].startswith("ExportExistsOrUnavailable:"))
        self.assertEqual(self.output.read_bytes(), b"existing")
        self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])


class PhaseDagV3CliParserTests(PhaseDagV3CliFixture):
    # Catch relaxed header/block parsing and turning invalid requests into saved FAILED receipts.
    def test_invalid_specs_leave_no_bundle(self):
        base = self.spec_text()
        cases = {
            "wrong-header": base.replace("SPEC_V3", "SPEC_V1"),
            "missing-header": base.split("\n", 1)[1],
            "header-whitespace": " " + base,
            "unknown-global": base.replace("dag-id=demo", "dag-id=demo\nextra=x"),
            "duplicate-key": base.replace("dag-id=demo", "dag-id=demo\ndag-id=other"),
            "unknown-node": base + "extra=x\n",
            "missing-node-field": base.replace("core=1020a54050607080\n", ""),
            "gate-extra": base + "request=x\n",
            "gate-missing-depends": base.replace("depends=gate1\n", ""),
            "gate-unknown-adapter": base.replace("payload-gate-v1", "evidence-gate-v1"),
            "missing-node-boundary": base.replace("\n\nnode=gate1", "\nnode=gate1"),
            "extra-equals": base.replace("dag-id=demo", "dag-id=de=mo"),
            "no-equals": base.replace("dag-id=demo", "dag-id demo"),
            "invalid-core": base.replace("1020a54050607080", "1020g54050607080"),
            "bad-context": base.replace("context-file=context.bin", "context-file=missing.bin"),
            "bad-technique": base.replace("masked-match-v1", "unknown"),
            "negative-budget": base.replace("max-started-nodes=3", "max-started-nodes=-1"),
            "dependency-empty": base.replace("depends=gate1", "depends=a,"),
            "dependency-order": self.spec_text([self.node("a"), self.node("b"), self.gate("gate", "b,a")]),
            "dependency-duplicate": base.replace("depends=gate1", "depends=a,a"),
            "cycle": self.spec_text([self.node("a", "gate"), self.gate()]),
            "duplicate-node": self.spec_text([self.node(), self.node()]),
            "no-nodes": base.split("\n\n")[0] + "\n",
        }
        for name, text in cases.items():
            with self.subTest(case=name):
                self.assert_error(self.execute(text))
                self.assertFalse(self.output.exists())
                self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    def test_context_and_spec_read_limits(self):
        (self.spec_dir / "context.bin").write_bytes(b"x" * 4097)
        self.assert_error(self.execute(self.spec_text()), "InputByteLimit")
        self.assert_error(self.execute(self.spec_text() + "x" * 65536), "InputByteLimit")
        self.assertFalse(self.output.exists())

    def test_wrong_arity_and_options(self):
        for args in (("execute",), ("execute", "spec", "--adapter", "x", "--output", "out"),
                     ("execute", "spec", "--wrong", "out"), ("inspect",),
                     ("replay", "bundle", "extra"), ("inspect", "--output")):
            with self.subTest(args=args):
                self.assert_error(self.call(*args))
        self.assertFalse(self.output.exists())


class PhaseDagV3CliReplayTests(PhaseDagV3CliFixture):
    # Catch digest-only inspection that forgets the deterministic replay comparison.
    def test_corrupt_and_nonidentical_bundle_rejected_without_payload_disclosure(self):
        self.assert_report(self.execute(self.spec_text()))
        bundle = self.output.read_bytes()
        header, digest, payload = bundle.split(b"\n", 2)
        forged_payload = payload.replace(b"5:READY", b"6:FAILED", 1)
        self.assertNotEqual(payload, forged_payload)
        forged = header + b"\n" + hashlib.sha256(forged_payload).hexdigest().encode() + b"\n" + forged_payload
        cases = {
            "header": (b"X" + bundle[1:], "InvalidPhaseDagBundleHeader"),
            "wrong-version": (bundle.replace(b"RUN_V3", b"RUN_V2", 1), "InvalidPhaseDagBundleHeader"),
            "corrupt": (bundle[:-1] + bytes([bundle[-1] ^ 1]), "PhaseDagBundleDigestMismatch"),
            "forged-receipt": (forged, "PhaseDagReplayMismatch"),
        }
        for action in ("inspect", "replay"):
            for name, (data, error) in cases.items():
                with self.subTest(action=action, case=name):
                    self.output.write_bytes(data)
                    self.assert_error(self.call(action, self.output), error)
                    self.assertEqual(self.output.read_bytes(), data)

    def test_real_v2_bundle_is_rejected_by_v3(self):
        self.spec.write_text(self.spec_text([self.node()]).replace("SPEC_V3", "SPEC_V1"))
        process = subprocess.run([BIN, "phase-dag-v2", "execute", str(self.spec),
                                  "--output", str(self.output)],
                                 capture_output=True, text=True, timeout=10)
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertTrue(self.output.read_bytes().startswith(b"BRUCE_PHASE_DAG_RUN_V2\n"))
        for action in ("inspect", "replay"):
            self.assert_error(self.call(action, self.output), "InvalidPhaseDagBundleHeader")

    def test_bundle_read_limit(self):
        marker = b"BRUCE_PHASE_DAG_RUN_V3\nprivate-context"
        limit = marker + b"x" * (5 * 1024 * 1024 - len(marker))
        for action in ("inspect", "replay"):
            self.output.write_bytes(limit)
            self.assert_error(self.call(action, self.output), "InvalidPhaseDagBundleDigest")
            self.output.write_bytes(limit + b"x")
            self.assert_error(self.call(action, self.output), "InputByteLimit")


class PhaseDagV3CliDocumentationTests(unittest.TestCase):
    # Catch missing V3 user guidance and examples no longer matching the real CLI.
    def test_human_authored_v3_contract_and_mcp_boundary(self):
        chapters = {
            "docs/cli/README.md": ("phase-dag-v3",),
            "docs/cli/commands.md": (*HELP_LINES, "nanocell-phase-v3", "payload-gate-v1",
                                     "5 MiB"),
            "docs/cli/formats.md": ("BRUCE_PHASE_DAG_SPEC_V3", "BRUCE_PHASE_DAG_RUN_V3",
                                    "65536", "adapter=payload-gate-v1"),
            "docs/cli/results-and-control.md": ("phase-dag-v3-cli-v1", "Payload-Rohbytes",
                                                "READY", "REJECTED", "Exit 0"),
            "docs/architecture/03-blueprint-consolidation.md": (
                "phase-dag-v3", "nanocell-phase-v3", "payload-gate-v1", "65536"),
        }
        for name, evidence in chapters.items():
            text = (SOURCE / name).read_text()
            for literal in evidence:
                with self.subTest(chapter=name, evidence=literal):
                    self.assertIn(literal, text)
            self.assertIn("IMPLEMENTIERT UND BELEGT", text)
            if name != "docs/cli/results-and-control.md":
                scope = text.split("## MCP Inspector\n", 1)[1].split("\n## ", 1)[0]
                for literal in ("POST /api/v1/inspect", "POST /api/v3/inspect",
                                "Phase-DAG V2", "Phase-DAG V3", "1 MiB", "5 MiB",
                                "read-only", "payload_size", "payload_bytes"):
                    with self.subTest(chapter=name, mcp_boundary=literal):
                        self.assertIn(literal, scope)
                self.assertNotIn("V2-only", text)

    def test_bounded_parallel_scheduler_documentation_contract(self):
        # Catch missing execute-only limits or misleading timing/performance guidance.
        chapters = {
            "docs/cli/README.md": (
                "--max-parallel-nodes", "default 1", "1..4", "waves",
                "byte-identical", "serial replay", "No performance improvement is claimed"),
            "docs/cli/commands.md": (
                "bruce phase-dag-v3 execute SPEC --output BUNDLE [--max-parallel-nodes N]",
                "Standard 1", "1..4", "InvalidPhaseDagParallelLimit", "Wellen",
                "bytegleich", "seriellen", "Keine Performanceverbesserung"),
            "docs/cli/formats.md": (
                "max_parallel_nodes", "Standard 1", "1..4", "Wellen", "bytegleich",
                "nicht im Spec oder Bundle", "Keine Performanceverbesserung"),
            "docs/architecture/03-blueprint-consolidation.md": (
                "max_parallel_nodes", "default 1", "1..4", "waves", "byte-identical",
                "serial replay", "No performance improvement is claimed"),
        }
        for name, evidence in chapters.items():
            text = (SOURCE / name).read_text()
            for literal in evidence:
                with self.subTest(chapter=name, scheduler_contract=literal):
                    self.assertIn(literal, text)
        commands = (SOURCE / "docs/cli/commands.md").read_text()
        scope = commands.split("## `phase-dag-v3`", 1)[1].split("\n## ", 1)[0]
        self.assertIn("nur für `execute`", scope)
        self.assertIn("`inspect` und `replay`", scope)
        self.assertIn("ohne Optionen", scope)
        self.assertNotIn("16 Knoten seriell", scope)
        self.assertNotIn("oder paralleler\nScheduler ist implementiert", scope)
        blueprint = (SOURCE / "docs/architecture/03-blueprint-consolidation.md").read_text()
        self.assertNotIn("or parallel execution is implemented", blueprint)

    def test_generated_v3_examples_match_live_portable_workflow(self):
        saved = json.loads((SOURCE / "docs/cli/examples.json").read_text())
        examples = {entry["example"]: entry for entry in saved["examples"]}
        labels = ["phase-dag-v3-" + action for action in ("execute", "inspect", "replay")]
        labels += ["phase-dag-v3-invalid-spec", "phase-dag-v3-reject-v2"]
        self.assertTrue(set(labels).issubset(examples), "missing measured V3 examples")
        with tempfile.TemporaryDirectory(prefix="bruce-v3-doc-example-") as directory:
            root = Path(directory)
            execute = examples[labels[0]]
            spec_name = execute["argv"][2]
            text = saved["inputs"][spec_name]
            self.assertTrue(text.startswith("BRUCE_PHASE_DAG_SPEC_V3\n"))
            self.assertEqual(text.count("adapter=payload-gate-v1"), 2)
            (root / spec_name).write_text(text)
            for line in text.splitlines():
                if line.startswith("context-file="):
                    name = line.split("=", 1)[1]
                    (root / name).write_text(saved["inputs"][name])
            reports = []
            bundle_path = root / execute["argv"][-1]
            for label in labels[:3]:
                entry = examples[label]
                result = subprocess.run([BIN, *entry["argv"]], cwd=root,
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, entry["exit_code"], result.stderr)
                self.assertEqual(result.stderr, "")
                reply = json.loads(result.stdout)
                self.assertEqual(reply, entry["json"])
                self.assertEqual(set(reply), {"contract", "action", "dag_version", "dag_id",
                                             "node_count", "bundle_digest", "receipts"})
                self.assertEqual(reply["contract"], "phase-dag-v3-cli-v1")
                self.assertEqual(reply["bundle_digest"],
                                 hashlib.sha256(bundle_path.read_bytes()).hexdigest())
                self.assertTrue(bundle_path.read_bytes().startswith(b"BRUCE_PHASE_DAG_RUN_V3\n"))
                for receipt in reply["receipts"]:
                    self.assertEqual(set(receipt), {"node_id", "outcome", "failure_code"})
                    self.assertEqual(receipt["outcome"], "READY")
                for raw in ("nanocell-phase-payload-v1", "payload-gate-payload-v1",
                            "1020a54050607080", "nanocell-reference-context-v1"):
                    self.assertNotIn(raw, result.stdout)
                reports.append(reply)
                if label == labels[0]:
                    for path in root.iterdir():
                        if path != bundle_path:
                            path.unlink()
            self.assertEqual(reports[1], {**reports[0], "action": "inspect"})
            self.assertEqual(reports[2], {**reports[0], "action": "replay"})
        for label in labels[3:]:
            self.assertEqual(examples[label]["exit_code"], 2)
            self.assertEqual(examples[label]["stream"], "stderr")
            self.assertEqual(set(examples[label]["json"]), {"error"})

    def test_generated_source_index_and_validation_include_v3(self):
        index = (SOURCE / "docs/cli/source-index.md").read_text()
        validation = (SOURCE / "docs/cli/validation.md").read_text()
        for literal in ("### `phase-dag-v3`", "apps/phase_dag_v3_cli.hpp",
                        "nanocell-phase-v3", "payload-gate-v1"):
            self.assertIn(literal, index)
        v3_index = index.split("### `phase-dag-v3`", 1)[1].split("\n### ", 1)[0]
        self.assertIn("verpflichtend `--output`", v3_index)
        self.assertIn("optional `--max-parallel-nodes N`", v3_index)
        self.assertIn("Phase-DAG-V3", validation)
        self.assertIn("Payload-Rohbytes", validation)


if __name__ == "__main__":
    unittest.main()
