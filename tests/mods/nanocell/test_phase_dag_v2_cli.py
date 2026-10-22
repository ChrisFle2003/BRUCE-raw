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
HELP_LINES = [
    "bruce phase-dag-v2 execute SPEC --output BUNDLE",
    "bruce phase-dag-v2 inspect BUNDLE",
    "bruce phase-dag-v2 replay BUNDLE",
]


class PhaseDagV2CliAvailabilityTests(unittest.TestCase):
    """Catch missing enabled routing/help and accidental NanoCell-off exposure."""

    def call(self, binary, *args):
        return subprocess.run([str(binary), *args], capture_output=True,
                              text=True, timeout=10)

    def assert_error(self, process, expected):
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertEqual(json.loads(process.stderr), {"error": expected})

    def test_enabled_help_lists_exactly_three_commands(self):
        process = self.call(BIN, "--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, "")
        self.assertEqual([line for line in process.stdout.splitlines()
                          if "phase-dag-v2" in line], HELP_LINES)

    def test_enabled_missing_subcommand_reaches_command_boundary(self):
        self.assert_error(self.call(BIN, "phase-dag-v2"),
                          "Usage: bruce phase-dag-v2 execute|inspect|replay")

    def test_enabled_unknown_subcommand_reaches_command_boundary(self):
        self.assert_error(self.call(BIN, "phase-dag-v2", "unknown"),
                          "Usage: bruce phase-dag-v2 execute|inspect|replay")

    def test_nanocell_disabled_build_has_no_command_or_help(self):
        # Keep bundled build-host-deps; a full copy avoids pkg-config path
        # splitting in checkouts containing spaces or GVFS colons.
        with tempfile.TemporaryDirectory(prefix="bruce-phase-dag-v2-off-") as tmp:
            source = Path(tmp) / "source"
            shutil.copytree(SOURCE, source,
                            ignore=shutil.ignore_patterns("build", "runs", ".worktrees"))
            build = Path(tmp) / "build"
            cmake = ["cmake"]
            pkgconf = source / "build-host-deps/root/usr/bin/pkgconf"
            if os.access(pkgconf, os.X_OK):
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
            binary = build / "bruce"
            process = self.call(binary, "--help")
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertNotIn("phase-dag-v2", process.stdout)
            for suffix in ((), ("unknown",)):
                with self.subTest(suffix=suffix):
                    self.assert_error(self.call(binary, "phase-dag-v2", *suffix),
                                      "UnknownCommand: phase-dag-v2")


class PhaseDagV2CliFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="bruce-phase-dag-v2-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.spec_dir = self.root / "spec"
        self.spec_dir.mkdir()
        (self.spec_dir / "a.bin").write_bytes(b"nanocell-reference-context-v1")
        (self.spec_dir / "b.bin").write_bytes(b"private-context\x00\xff")
        self.spec = self.spec_dir / "graph.spec"
        self.output = self.root / "run.bundle"

    def node(self, node="a", depends="", context="a.bin", technique="masked-match-v1"):
        return (f"node={node}\ndepends={depends}\ncore=1020a54050607080\n"
                f"active-index=2\ncontext-id=reference.context_1\ncontext-file={context}\n"
                f"technique={technique}\nmax-operations=16\nmax-trace-entries=18\n")

    def spec_text(self, nodes=None, receipt_budget=65536):
        return ("BRUCE_PHASE_DAG_SPEC_V1\ndag-id=demo\nmax-started-nodes=3\n"
                f"max-receipt-bytes={receipt_budget}\nmax-trace-entries=32\n\n" +
                "\n".join(nodes or [self.node()]))

    def execute(self, text):
        self.spec.write_text(text, encoding="utf-8")
        return subprocess.run([BIN, "phase-dag-v2", "execute", str(self.spec),
                               "--output", str(self.output)], cwd=self.root,
                              capture_output=True, text=True, timeout=10)

    def assert_report(self, process, expected_receipts):
        self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
        self.assertEqual(process.stderr, "")
        report = json.loads(process.stdout)
        self.assertEqual(set(report), {"contract", "action", "dag_version", "dag_id",
                                       "node_count", "bundle_digest", "receipts"})
        self.assertEqual(report["contract"], "phase-dag-v2-cli-v1")
        self.assertEqual(report["action"], "execute")
        self.assertEqual(report["dag_version"], "phase-dag-v2")
        self.assertEqual(report["dag_id"], "demo")
        self.assertEqual(report["node_count"], len(expected_receipts))
        self.assertEqual(report["receipts"], expected_receipts)
        bundle = self.output.read_bytes()
        self.assertTrue(bundle.startswith(b"BRUCE_PHASE_DAG_RUN_V2\n"))
        self.assertEqual(report["bundle_digest"], hashlib.sha256(bundle).hexdigest())
        self.assertNotIn("private-context", process.stdout)
        self.assertNotIn("1020a54050607080", process.stdout)


class PhaseDagV2CliExecutionTests(PhaseDagV2CliFixture):
    # Catch dropped dependencies, missing bundle publication, and leaked payloads.
    def test_mixed_dependencies_write_bundle_and_metadata_report(self):
        process = self.execute(self.spec_text([
            self.node("a"), self.node("b", "a", "b.bin", "rotate-match-v1"),
            self.node("c")]))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "b", "outcome": "READY", "failure_code": ""},
            {"node_id": "c", "outcome": "READY", "failure_code": ""}])

    def test_context_conflict_is_ready_and_does_not_block_child(self):
        process = self.execute(self.spec_text([
            self.node("a", technique="context-conflict-v1"), self.node("b", "a")]))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "b", "outcome": "READY", "failure_code": ""}])
        self.assertIn(b"CONTEXT_REJECTED", self.output.read_bytes())

    def test_evidence_gate_accepts_complete_predecessor(self):
        """Catch an unregistered gate or a parser that cannot construct its node."""
        gate = "node=gate\ndepends=a\nadapter=evidence-gate-v1\n"
        process = self.execute(self.spec_text([self.node("a"), gate]))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "gate", "outcome": "READY", "failure_code": ""}])
        self.assertIn(b"evidence-gate-output-v1", self.output.read_bytes())

    def test_evidence_gate_rejects_rejected_predecessor(self):
        """Catch a gate that treats a valid REJECTED predecessor as accepted evidence."""
        gate = "node=gate\ndepends=a\nadapter=evidence-gate-v1\n"
        process = self.execute(self.spec_text([
            self.node("a", technique="context-conflict-v1"), gate]))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "gate", "outcome": "READY", "failure_code": ""}])
        bundle = self.output.read_bytes()
        self.assertIn(b"evidence-gate-output-v1", bundle)
        self.assertIn(b"REJECTED", bundle)

    # A conflict is a valid REJECTED output; a tiny valid receipt budget makes
    # this path FAILED and blocks its child without making the process fail.
    def test_failed_parent_and_blocked_child_still_publish_bundle(self):
        process = self.execute(self.spec_text([
            self.node("a", technique="context-conflict-v1"), self.node("b", "a")],
            receipt_budget=1))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "FAILED", "failure_code": "DagReceiptBudgetExceeded"},
            {"node_id": "b", "outcome": "BLOCKED", "failure_code": ""}])


class PhaseDagV2CliInspectionTests(PhaseDagV2CliFixture):
    """Catch unconfirmed views, digest-only validation, leaks and bundle writes."""

    def view(self, action, *arguments):
        return subprocess.run([BIN, "phase-dag-v2", action, *map(str, arguments)],
                              cwd=self.root, capture_output=True, text=True, timeout=10)

    def assert_private_bytes_absent(self, process):
        for marker in ("BRUCE_PHASE_DAG_RUN_V2", "nanocell-reference-context-v1",
                       "private-context", "nanocell-phase-request-v1",
                       "nanocell-phase-output-v1", "BRUCE_MOD_RUN_V1", "1020a54050607080"):
            self.assertNotIn(marker, process.stdout + process.stderr)

    def assert_confirmed_views(self, execution):
        self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
        expected = json.loads(execution.stdout)
        original = self.output.read_bytes()
        # Bundles must be portable; inspect/replay cannot reopen Spec or contexts.
        shutil.rmtree(self.spec_dir)
        for action in ("inspect", "replay"):
            with self.subTest(action=action):
                process = self.view(action, self.output)
                self.assertEqual(process.returncode, 0, process.stdout + process.stderr)
                self.assertEqual(process.stderr, "")
                self.assertEqual(json.loads(process.stdout), {**expected, "action": action})
                self.assert_private_bytes_absent(process)
                self.assertEqual(self.output.read_bytes(), original)
                self.assertEqual(list(self.root.iterdir()), [self.output])

    def test_inspect_and_replay_match_execute_without_modifying_bundle(self):
        execution = self.execute(self.spec_text([
            self.node("a"), self.node("b", "a", "b.bin", "rotate-match-v1"), self.node("c")]))
        self.assert_report(execution, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "b", "outcome": "READY", "failure_code": ""},
            {"node_id": "c", "outcome": "READY", "failure_code": ""}])
        self.assert_confirmed_views(execution)

    def test_failed_and_blocked_receipts_are_confirmed_successfully(self):
        execution = self.execute(self.spec_text([
            self.node("a", technique="context-conflict-v1"), self.node("b", "a")],
            receipt_budget=1))
        self.assert_report(execution, [
            {"node_id": "a", "outcome": "FAILED", "failure_code": "DagReceiptBudgetExceeded"},
            {"node_id": "b", "outcome": "BLOCKED", "failure_code": ""}])
        self.assert_confirmed_views(execution)

    def assert_view_rejected(self, action, bundle, expected_error):
        self.output.write_bytes(bundle)
        process = self.view(action, self.output)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertEqual(json.loads(process.stderr), {"error": expected_error})
        self.assert_private_bytes_absent(process)
        self.assertEqual(self.output.read_bytes(), bundle)
        self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    def test_malformed_bundles_are_rejected_without_disclosure(self):
        execution = self.execute(self.spec_text([self.node(context="b.bin")]))
        self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
        bundle = self.output.read_bytes()
        header_end = bundle.index(b"\n") + 1
        digest = bundle[header_end:header_end + 64]
        changed_digest = (b"0" if digest[:1] != b"0" else b"1") + digest[1:]
        cases = {
            "header": (b"X" + bundle[1:], "InvalidPhaseDagBundleHeader"),
            "digest": (bundle[:header_end] + changed_digest + bundle[header_end + 64:],
                       "PhaseDagBundleDigestMismatch"),
            "payload": (bundle[:-1] + bytes([bundle[-1] ^ 1]), "PhaseDagBundleDigestMismatch"),
        }
        for action in ("inspect", "replay"):
            for case, (data, error) in cases.items():
                with self.subTest(action=action, case=case):
                    self.assert_view_rejected(action, data, error)

    def test_digest_valid_forged_receipt_still_requires_deterministic_replay(self):
        execution = self.execute(self.spec_text())
        self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
        bundle = self.output.read_bytes()
        header_end = bundle.index(b"\n") + 1
        payload = bundle[header_end + 65:]
        self.assertIn(b"5:READY", payload)
        forged_payload = payload.replace(b"5:READY", b"6:FAILED", 1)
        forged = bundle[:header_end] + hashlib.sha256(forged_payload).hexdigest().encode() + b"\n" + forged_payload
        for action in ("inspect", "replay"):
            with self.subTest(action=action):
                self.assert_view_rejected(action, forged, "PhaseDagReplayMismatch")

    def test_bundle_loading_accepts_limit_and_rejects_one_byte_above(self):
        marker = b"BRUCE_PHASE_DAG_RUN_V2\nprivate-context"
        exact_limit = marker + b"x" * (5 * 1024 * 1024 - len(marker))
        for action in ("inspect", "replay"):
            with self.subTest(action=action, size="exact-limit"):
                self.assert_view_rejected(action, exact_limit, "InvalidPhaseDagBundleDigest")
            with self.subTest(action=action, size="over-limit"):
                self.assert_view_rejected(action, exact_limit + b"x", "InputByteLimit")

    def test_views_reject_wrong_arity_and_options(self):
        for action in ("inspect", "replay"):
            for arguments in ((), ("bundle", "extra"), ("bundle", "--output", "other")):
                with self.subTest(action=action, arguments=arguments):
                    process = self.view(action, *arguments)
                    self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
                    self.assertEqual(process.stdout, "")
                    self.assertEqual(json.loads(process.stderr), {
                        "error": f"Usage: bruce phase-dag-v2 {action} BUNDLE"})
        self.assertFalse(self.output.exists())

    def test_views_reject_options_even_when_matching_files_exist(self):
        execution = self.execute(self.spec_text())
        self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
        for option in ("--output", "--adapter"):
            (self.root / option).write_bytes(self.output.read_bytes())
            for action in ("inspect", "replay"):
                with self.subTest(action=action, option=option):
                    process = self.view(action, option)
                    self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
                    self.assertEqual(process.stdout, "")
                    self.assertEqual(json.loads(process.stderr), {
                        "error": f"Usage: bruce phase-dag-v2 {action} BUNDLE"})


class PhaseDagV2CliValidationTests(PhaseDagV2CliFixture):
    def assert_rejected(self, process):
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        error = json.loads(process.stderr)
        self.assertEqual(set(error), {"error"})
        self.assertTrue(error["error"])
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    # Catch accepting malformed syntax or invalid node requests as saved FAILED
    # outcomes; technical input errors must leave no new output.
    def test_invalid_specs_leave_output_absent(self):
        base = self.spec_text()
        graph = self.spec_text([self.node("a"), self.node("b"), self.node("c", "a,b")])
        (self.spec_dir / "large.bin").write_bytes(b"x" * 4097)
        cases = {
            "invalid-header": base.replace("BRUCE_PHASE_DAG_SPEC_V1", "BRUCE_PHASE_DAG_SPEC_V2"),
            "header-whitespace": " " + base,
            "unknown-global": base.replace("dag-id=demo", "dag-id=demo\nextra=x"),
            "unknown-node": base + "extra=x\n",
            "duplicate-global": base.replace("dag-id=demo", "dag-id=demo\ndag-id=other"),
            "duplicate-node-key": base + "node=other\n",
            "missing-global": base.replace("dag-id=demo\n", ""),
            "missing-node-key": base.replace("technique=masked-match-v1\n", ""),
            "incomplete-block": base + "\nnode=other\n",
            "no-node-boundary": base.replace("\n\nnode=a", "\nnode=a"),
            "no-nodes": base.split("\n\n")[0] + "\n",
            "duplicate-node-id": self.spec_text([self.node(), self.node()]),
            "invalid-node-id": base.replace("node=a", "node=a b"),
            "assignment-without-equals": base.replace("dag-id=demo", "dag-id demo"),
            "assignment-extra-equals": base.replace("dag-id=demo", "dag-id=de=mo"),
            "short-core": base.replace("1020a54050607080", "1020"),
            "invalid-core-hex": base.replace("1020a54050607080", "1020g54050607080"),
            "missing-context": base.replace("context-file=a.bin", "context-file=missing.bin"),
            "oversize-context": base.replace("context-file=a.bin", "context-file=large.bin"),
            "empty-context-file": base.replace("context-file=a.bin", "context-file="),
            "nul-context-file": base.replace("context-file=a.bin", "context-file=a.bin\x00other.bin"),
            "invalid-context-id": base.replace("context-id=reference.context_1", "context-id=bad id"),
            "empty-context-id": base.replace("context-id=reference.context_1", "context-id="),
            "unknown-technique": base.replace("masked-match-v1", "unknown-v1"),
            "empty-technique": base.replace("masked-match-v1", ""),
            "invalid-active-index": base.replace("active-index=2", "active-index=8"),
            "active-index-byte-overflow": base.replace("active-index=2", "active-index=256"),
            "unsorted-dependencies": graph.replace("depends=a,b", "depends=b,a"),
            "duplicate-dependencies": graph.replace("depends=a,b", "depends=a,a"),
            "empty-dependency-leading": graph.replace("depends=a,b", "depends=,a"),
            "empty-dependency-middle": graph.replace("depends=a,b", "depends=a,,b"),
            "empty-dependency-trailing": graph.replace("depends=a,b", "depends=a,"),
            "unknown-dependency": base.replace("depends=", "depends=missing"),
            "self-dependency": base.replace("depends=", "depends=a"),
            "cycle": self.spec_text([self.node("a", "b"), self.node("b", "a")]),
            "invalid-dag-id": base.replace("dag-id=demo", "dag-id="),
            "started-budget": base.replace("max-started-nodes=3", "max-started-nodes=17"),
            "zero-receipt-budget": base.replace("max-receipt-bytes=65536", "max-receipt-bytes=0"),
            "large-receipt-budget": base.replace("max-receipt-bytes=65536", "max-receipt-bytes=262145"),
            "small-global-trace": base.replace("max-trace-entries=32", "max-trace-entries=1"),
            "large-global-trace": base.replace("max-trace-entries=32", "max-trace-entries=513"),
            "large-operations": base.replace("max-operations=16", "max-operations=17"),
            "small-node-trace": base.replace("max-trace-entries=18", "max-trace-entries=1"),
            "large-node-trace": base.replace("max-trace-entries=18", "max-trace-entries=19"),
            "negative-number": base.replace("max-started-nodes=3", "max-started-nodes=-1"),
            "positive-sign-number": base.replace("max-started-nodes=3", "max-started-nodes=+1"),
            "space-number": base.replace("max-started-nodes=3", "max-started-nodes= 1"),
            "empty-number": base.replace("max-started-nodes=3", "max-started-nodes="),
            "trailing-number": base.replace("max-started-nodes=3", "max-started-nodes=1x"),
            "overflow-number": base.replace("max-started-nodes=3", "max-started-nodes=18446744073709551616"),
            "oversize-spec": base + "\n" * 65536,
        }
        for name, text in cases.items():
            with self.subTest(case=name):
                self.assert_rejected(self.execute(text))
            # Remove only this fixture's output so one failed rejection does not
            # turn every later case into a misleading collision success.
            if self.output.exists():
                self.output.unlink()

    def test_existing_output_bytes_are_preserved(self):
        original = b"existing\x00private\xff"
        self.output.write_bytes(original)
        process = self.execute(self.spec_text())
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, "")
        self.assertIn("error", json.loads(process.stderr))
        self.assertEqual(self.output.read_bytes(), original)
        self.assertEqual(list(self.root.glob("run.bundle.tmp.*")), [])

    def test_parent_relative_context_is_resolved_from_spec_directory(self):
        context_dir = self.root / "contexts"
        context_dir.mkdir()
        (context_dir / "a.bin").write_bytes(b"nanocell-reference-context-v1")
        process = self.execute(self.spec_text([self.node(context="../contexts/a.bin")]))
        self.assert_report(process, [{"node_id": "a", "outcome": "READY", "failure_code": ""}])

    def test_node_order_is_canonicalized_and_report_order_is_not_execution_order(self):
        process = self.execute(self.spec_text([
            self.node("c"), self.node("b", "a,c"), self.node("a", "c")]))
        self.assert_report(process, [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "b", "outcome": "READY", "failure_code": ""},
            {"node_id": "c", "outcome": "READY", "failure_code": ""}])

    def test_context_limit_accepts_exactly_4096_bytes(self):
        (self.spec_dir / "a.bin").write_bytes(b"x" * 4096)
        process = self.execute(self.spec_text())
        self.assert_report(process, [{"node_id": "a", "outcome": "READY", "failure_code": ""}])

    def test_execute_rejects_wrong_arity_and_option(self):
        for args in (("execute",), ("execute", "spec"),
                     ("execute", "spec", "--wrong", "bundle"),
                     ("execute", "spec", "--output", "bundle", "extra")):
            with self.subTest(args=args):
                process = subprocess.run([BIN, "phase-dag-v2", *args], cwd=self.root,
                                         capture_output=True, text=True, timeout=10)
                self.assert_rejected(process)


class PhaseDagV2CliDocumentationTests(unittest.TestCase):
    """Catch omitted public usage and missing measured Phase-DAG evidence."""

    def section(self, name, heading):
        text = (SOURCE / name).read_text()
        self.assertTrue(heading in text, "Missing section in " + name + ": " + heading)
        return text.split(heading, 1)[1].split("\n## ", 1)[0]

    def test_manual_describes_commands_format_and_bounded_contract(self):
        commands = self.section("docs/cli/commands.md", "## `phase-dag-v2`")
        formats = self.section("docs/cli/formats.md", "## Phase-DAG V2 Spec und Bundle")
        results = self.section("docs/cli/results-and-control.md", "## Phase-DAG V2 Ergebnis und Teilbefunde")
        for command in HELP_LINES:
            with self.subTest(command=command):
                self.assertIn(command, commands)
        for term in ("BRUCE_PHASE_DAG_SPEC_V1", "context-file", "Spec-Verzeichnis",
                     "4096", "nanocell-phase-v1", "evidence-gate-v1", "max-started-nodes"):
            with self.subTest(format_term=term):
                self.assertIn(term, formats)
        for term in ("phase-dag-v2-cli-v1", "READY", "FAILED", "BLOCKED",
                     "CANCELLED", "Exit 0", "bundle_digest"):
            with self.subTest(result_term=term):
                self.assertIn(term, results)
        self.assertIn("exklusiv", commands)
        self.assertIn("seriell", commands)

    def test_capability_entry_points_explain_the_cli(self):
        for name in ("docs/cli/README.md", "docs/architecture/03-blueprint-consolidation.md"):
            with self.subTest(document=name):
                text = (SOURCE / name).read_text()
                self.assertIn("phase-dag-v2", text)
                self.assertIn("apps/phase_dag_v2_cli.hpp", text)

    def test_generated_index_has_dedicated_phase_dag_command(self):
        index = (SOURCE / "docs/cli/source-index.md").read_text()
        self.assertIn("### `phase-dag-v2`", index)
        self.assertIn("apps/phase_dag_v2_cli.hpp", index)

    def test_measured_examples_include_three_confirmed_views_and_rejection(self):
        evidence = json.loads((SOURCE / "docs/cli/examples.json").read_text())
        examples = {record["example"]: record for record in evidence["examples"]}
        expected_receipts = [
            {"node_id": "a", "outcome": "READY", "failure_code": ""},
            {"node_id": "b", "outcome": "READY", "failure_code": ""},
            {"node_id": "gate", "outcome": "READY", "failure_code": ""},
        ]
        for action in ("execute", "inspect", "replay"):
            with self.subTest(action=action):
                label = "phase-dag-v2-" + action
                self.assertTrue(label in examples, "Missing measured example: " + label)
                record = examples[label]
                self.assertEqual(record["argv"][:2], ["phase-dag-v2", action])
                self.assertEqual(record["exit_code"], 0)
                self.assertEqual(record["stream"], "stdout")
                self.assertEqual(set(record["json"]), {"contract", "action", "dag_version",
                                                      "dag_id", "node_count", "bundle_digest", "receipts"})
                self.assertEqual(record["json"]["contract"], "phase-dag-v2-cli-v1")
                self.assertEqual(record["json"]["action"], action)
                self.assertEqual(record["json"]["node_count"], 3)
                self.assertEqual(record["json"]["receipts"], expected_receipts)
        label = "phase-dag-v2-invalid-spec"
        self.assertTrue(label in examples, "Missing measured example: " + label)
        self.assertEqual(examples[label]["exit_code"], 2)
        self.assertEqual(examples[label]["stream"], "stderr")
        self.assertEqual(examples[label]["json"], {"error": "InvalidPhaseDagSpecHeader"})
        self.assertIn("BRUCE_PHASE_DAG_SPEC_V1", evidence["inputs"]["phase-dag-v2.spec"])


if __name__ == "__main__":
    unittest.main()
