import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BIN = str(Path(sys.argv.pop(1)).resolve())
USAGE = b'{"error":"Usage: bruce dag-scheduler-v3 execute|inspect SPEC"}\n'
SPEC = (
    b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
    b"dag-id=example\n"
    b"max-started-nodes=4\n"
    b"max-total-receipt-bytes=4096\n"
    b"\n"
    b"node=a-source\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=616c706861\n"
    b"\n"
    b"node=b-transform\n"
    b"depends=a-source\n"
    b"callback=append-v1\n"
    b"request-hex=3a62657461\n"
    b"\n"
    b"node=c-independent\n"
    b"depends=\n"
    b"callback=literal-v1\n"
    b"request-hex=0000\n"
    b"\n"
    b"node=d-join\n"
    b"depends=b-transform,c-independent\n"
    b"callback=join-v1\n"
    b"request-hex=73656e74696e656c2d6a6f696e2d7633\n"
)
FORBIDDEN = (b"alpha", b"beta", b"0000", b"616c706861", b"3a62657461",
             b"73656e74696e656c2d6a6f696e2d7633", b"sentinel-join-v3",
             b"request_bytes", b"payload_bytes")


def digest(value):
    return hashlib.sha256(value).hexdigest()


INSPECT = {
    "contract": "dag-scheduler-v3-inspect-v1", "action": "inspect",
    "spec_version": "dag-scheduler-spec-v3", "spec_digest": digest(SPEC),
    "scheduler_version": "dag-scheduler-v2", "dag_id": "example",
    "max_started_nodes": 4, "max_total_receipt_bytes": 4096,
    "nodes": [
        {"node_id": "a-source", "callback_id": "literal-v1",
         "dependencies": [], "request_size": 5,
         "request_digest": digest(b"alpha")},
        {"node_id": "b-transform", "callback_id": "append-v1",
         "dependencies": ["a-source"], "request_size": 5,
         "request_digest": digest(b":beta")},
        {"node_id": "c-independent", "callback_id": "literal-v1",
         "dependencies": [], "request_size": 2,
         "request_digest": digest(b"\x00\x00")},
        {"node_id": "d-join", "callback_id": "join-v1",
         "dependencies": ["b-transform", "c-independent"],
         "request_size": 16, "request_digest": digest(b"sentinel-join-v3")},
    ],
    "planned_start_order": ["a-source", "b-transform", "c-independent", "d-join"],
}


# Hand-derived callback outputs and direct inputs, independent of production code.
SOURCE = b"alpha"
TRANSFORM = b"alpha:beta"
INDEPENDENT = b"\x00\x00"
JOIN = b"alpha:beta" + b"sentinel-join-v3" + b"\x00\x00"
FLOW = {
    "contract": "dag-scheduler-v3-cli-v1", "action": "execute",
    "spec_version": "dag-scheduler-spec-v3", "spec_digest": digest(SPEC),
    "scheduler_version": "dag-scheduler-v2", "dag_id": "example",
    "max_started_nodes": 4, "max_total_receipt_bytes": 4096,
    "receipts": [
        {"node_id": "a-source", "callback_id": "literal-v1",
         "request_size": 5, "request_digest": digest(b"alpha"),
         "input_refs": [], "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "LITERAL_READY", "payload_size": 5,
                    "payload_digest": digest(SOURCE)}},
        {"node_id": "b-transform", "callback_id": "append-v1",
         "request_size": 5, "request_digest": digest(b":beta"),
         "input_refs": [
             {"producer_node_id": "a-source", "result_code": "LITERAL_READY",
              "payload_size": 5, "payload_digest": digest(SOURCE)}],
         "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "APPENDED_READY", "payload_size": 10,
                    "payload_digest": digest(TRANSFORM)}},
        {"node_id": "c-independent", "callback_id": "literal-v1",
         "request_size": 2, "request_digest": digest(INDEPENDENT),
         "input_refs": [], "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "LITERAL_READY", "payload_size": 2,
                    "payload_digest": digest(INDEPENDENT)}},
        {"node_id": "d-join", "callback_id": "join-v1",
         "request_size": 16, "request_digest": digest(b"sentinel-join-v3"),
         "input_refs": [
             {"producer_node_id": "b-transform", "result_code": "APPENDED_READY",
              "payload_size": 10, "payload_digest": digest(TRANSFORM)},
             {"producer_node_id": "c-independent", "result_code": "LITERAL_READY",
              "payload_size": 2, "payload_digest": digest(INDEPENDENT)}],
         "outcome": "READY", "failure_code": "",
         "output": {"output_version": "dag-scheduler-output-v2",
                    "result_code": "JOINED_READY", "payload_size": 28,
                    "payload_digest": digest(JOIN)}},
    ],
    "trace": [
        {"event": "VALIDATED", "node_id": ""},
        {"event": "STARTED", "node_id": "a-source"},
        {"event": "READY", "node_id": "a-source"},
        {"event": "STARTED", "node_id": "b-transform"},
        {"event": "READY", "node_id": "b-transform"},
        {"event": "STARTED", "node_id": "c-independent"},
        {"event": "READY", "node_id": "c-independent"},
        {"event": "STARTED", "node_id": "d-join"},
        {"event": "READY", "node_id": "d-join"},
        {"event": "COMPLETED", "node_id": ""},
    ],
}


class DagSchedulerV3CliTests(unittest.TestCase):
    """Real-file execute/inspect contracts, canonical parsing and strict routes."""

    def call(self, *args):
        return subprocess.run([BIN, *args], capture_output=True, timeout=10)

    def with_spec(self, action, spec):
        with tempfile.TemporaryDirectory(prefix="bruce-v3-spec-") as directory:
            path = Path(directory) / "graph.spec"
            path.write_bytes(spec)
            return self.call("dag-scheduler-v3", action, str(path))

    def execute(self, spec):
        return self.with_spec("execute", spec)

    def inspect(self, spec):
        return self.with_spec("inspect", spec)

    def successful_report(self, spec=SPEC, action="execute"):
        process = self.inspect(spec) if action == "inspect" else self.execute(spec)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertTrue(process.stdout.endswith(b"\n"))
        self.assertFalse(process.stdout.endswith(b"\n\n"))
        return process.stdout, json.loads(process.stdout)

    def test_inspect_full_report_is_canonical_metadata_only(self):
        raw, report = self.successful_report(action="inspect")
        self.assertEqual(report, INSPECT)
        _, executed = self.successful_report()
        self.assertEqual(report["spec_digest"], executed["spec_digest"])
        for forbidden in (*FORBIDDEN, b"request-hex", b"\x00",
                          b'"receipts"', b'"trace"', b'"payload_size"'):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, raw)

    def test_repeated_inspect_is_raw_byte_stable(self):
        first, report = self.successful_report(action="inspect")
        self.assertEqual(report, INSPECT)
        for _ in range(5):
            raw, report = self.successful_report(action="inspect")
            self.assertEqual(raw, first)
            self.assertEqual(report, INSPECT)

    def test_inspect_empty_and_nul_requests_remain_metadata(self):
        spec = SPEC.replace(b"request-hex=616c706861", b"request-hex=", 1)
        raw, report = self.successful_report(spec, action="inspect")
        expected = dict(INSPECT, spec_digest=digest(spec), nodes=[
            dict(INSPECT["nodes"][0], request_size=0, request_digest=digest(b"")),
            *INSPECT["nodes"][1:],
        ])
        self.assertEqual(report, expected)
        for forbidden in (*FORBIDDEN, b"request-hex", b"\x00"):
            self.assertNotIn(forbidden, raw)

    def test_inspect_plan_does_not_simulate_execution_budgets(self):
        spec = SPEC.replace(b"max-started-nodes=4", b"max-started-nodes=1").replace(
            b"max-total-receipt-bytes=4096", b"max-total-receipt-bytes=1")
        _, report = self.successful_report(spec, action="inspect")
        self.assertEqual(report, dict(INSPECT, spec_digest=digest(spec),
                                     max_started_nodes=1, max_total_receipt_bytes=1))

    def assert_usage_error(self, *args):
        process = self.call("dag-scheduler-v3", *args)
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, b"")
        self.assertEqual(process.stderr, USAGE)

    def test_complete_flow_has_only_metadata_and_direct_sorted_inputs(self):
        raw, report = self.successful_report()
        self.assertEqual(report, FLOW)
        for forbidden in FORBIDDEN:
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, raw)

    def test_repeated_flow_is_raw_byte_stable(self):
        first, _ = self.successful_report()
        # Captured from the successful pre-inspector execute binary; also pin
        # formatting so a parse-equivalent serialization change cannot pass.
        self.assertEqual(digest(first),
                         "184193ad0ce6dc81f58d5608204e0ce1e54994b4f79c0e6366868d76bcd2a2d0")
        for _ in range(5):
            raw, report = self.successful_report()
            self.assertEqual(raw, first)
            self.assertEqual(report, FLOW)

    def test_optional_final_lf_changes_only_exact_file_digest(self):
        raw, report = self.successful_report(SPEC[:-1])
        expected = dict(FLOW, spec_digest=digest(SPEC[:-1]))
        self.assertEqual(report, expected)
        for forbidden in FORBIDDEN:
            self.assertNotIn(forbidden, raw)
        _, inspected = self.successful_report(SPEC[:-1], action="inspect")
        self.assertEqual(inspected, dict(INSPECT, spec_digest=digest(SPEC[:-1])))

    def test_help_lists_exactly_the_execute_and_inspect_routes(self):
        process = self.call("--help")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stderr, b"")
        self.assertEqual([line for line in process.stdout.decode().splitlines()
                          if "dag-scheduler-v3" in line],
                         ["bruce dag-scheduler-v3 execute SPEC",
                          "bruce dag-scheduler-v3 inspect SPEC"])

    def test_missing_wrong_extra_and_option_like_route_arguments(self):
        cases = ((), ("execute",), ("inspect",), ("unknown",), ("demo", "flow"),
                 ("EXECUTE", "graph.spec"), ("", "graph.spec"),
                 ("execute", ""), ("execute", "graph.spec", "extra"),
                 ("execute", "graph.spec", "--help"), ("--help",),
                 ("execute", "--help"), ("execute", "--budget"),
                 ("execute", "-spec"), ("INSPECT", "graph.spec"),
                 ("inspect", ""), ("inspect", "graph.spec", "extra"),
                 ("inspect", "graph.spec", "--help"),
                 ("inspect", "--help"), ("inspect", "--budget"),
                 ("inspect", "-spec"))
        for args in cases:
            with self.subTest(args=args):
                self.assert_usage_error(*args)

    def test_input_byte_limit_accepts_65536_and_rejects_65537_canonical_bytes(self):
        # Removing the file-read limit must fail this test. The only difference
        # between the fixtures is the grammar's permitted optional final LF.
        prefix = (
            b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
            b"dag-id=a\n"
            b"max-started-nodes=1\n"
            b"max-total-receipt-bytes=65536\n\n"
            b"node=a\n"
            b"depends=\n"
            b"callback=literal-v1\n"
            b"request-hex="
        )
        hex_size = 65536 - len(prefix)
        self.assertEqual(hex_size % 2, 0)
        request = b"a" * (hex_size // 2)
        at_limit = prefix + b"61" * len(request)
        over_limit = at_limit + b"\n"
        self.assertEqual(len(at_limit), 65536)
        self.assertEqual(len(over_limit), 65537)

        _, report = self.successful_report(at_limit)
        self.assertEqual(report["spec_digest"], digest(at_limit))
        self.assertEqual(report["receipts"], [
            {"node_id": "a", "callback_id": "literal-v1",
             "request_size": len(request), "request_digest": digest(request),
             "input_refs": [], "outcome": "READY", "failure_code": "",
             "output": {"output_version": "dag-scheduler-output-v2",
                        "result_code": "LITERAL_READY",
                        "payload_size": len(request),
                        "payload_digest": digest(request)}}])
        self.assertEqual(report["trace"][-1],
                         {"event": "COMPLETED", "node_id": ""})

        _, inspected = self.successful_report(at_limit, action="inspect")
        self.assertEqual(inspected, {
            "contract": "dag-scheduler-v3-inspect-v1", "action": "inspect",
            "spec_version": "dag-scheduler-spec-v3", "spec_digest": digest(at_limit),
            "scheduler_version": "dag-scheduler-v2", "dag_id": "a",
            "max_started_nodes": 1, "max_total_receipt_bytes": 65536,
            "nodes": [{"node_id": "a", "callback_id": "literal-v1",
                       "dependencies": [], "request_size": len(request),
                       "request_digest": digest(request)}],
            "planned_start_order": ["a"],
        })
        for action in ("execute", "inspect"):
            with self.subTest(action=action):
                process = self.with_spec(action, over_limit)
                self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
                self.assertEqual(process.stdout, b"")
                self.assertEqual(process.stderr, b'{"error":"InputByteLimit"}\n')

    def test_rejects_noncanonical_specs_without_echoing_requests(self):
        blocks = SPEC.rstrip(b"\n").split(b"\n\n")
        cases = {
            "incorrect header": SPEC.replace(b"SPEC_V3", b"SPEC_V2", 1),
            "CRLF": SPEC.replace(b"\n", b"\r\n"),
            "NUL": SPEC.replace(b"dag-id=example", b"dag-id=exam\x00ple"),
            "non-ASCII": SPEC.replace(b"dag-id=example", b"dag-id=exam\xffple"),
            "leading separator": b"\n" + SPEC,
            "double separator": SPEC.replace(b"\n\n", b"\n\n\n", 1),
            "trailing separator": SPEC + b"\n",
            "missing separator": SPEC.replace(b"\n\n", b"\n", 1),
            "unknown global key": SPEC.replace(b"dag-id=", b"unknown=", 1),
            "unknown node key": SPEC.replace(b"request-hex=", b"unknown=", 1),
            "duplicate global key": SPEC.replace(b"dag-id=example\n", b"dag-id=example\ndag-id=example\n", 1),
            "duplicate node key": SPEC.replace(b"node=a-source\n", b"node=a-source\nnode=a-source\n", 1),
            "wrong global field order": SPEC.replace(b"dag-id=example\nmax-started-nodes=4", b"max-started-nodes=4\ndag-id=example", 1),
            "wrong node field order": SPEC.replace(b"node=a-source\ndepends=", b"depends=\nnode=a-source", 1),
            "unsorted node blocks": b"\n\n".join([blocks[0], blocks[2], blocks[1], *blocks[3:]]) + b"\n",
            "unsorted dependencies": SPEC.replace(b"depends=b-transform,c-independent", b"depends=c-independent,b-transform"),
            "odd hex": SPEC.replace(b"616c706861", b"616c70686", 1),
            "uppercase hex": SPEC.replace(b"616c706861", b"616C706861", 1),
            "nonhex": SPEC.replace(b"616c706861", b"616g706861", 1),
            "unknown callback": SPEC.replace(b"callback=literal-v1", b"callback=unknown-v1", 1),
            "literal with dependency": SPEC.replace(b"depends=\ncallback=literal-v1", b"depends=c-independent\ncallback=literal-v1", 1),
            "append with no dependency": SPEC.replace(b"depends=a-source\ncallback=append-v1", b"depends=\ncallback=append-v1", 1),
            "append with two dependencies": SPEC.replace(b"depends=a-source\ncallback=append-v1", b"depends=a-source,c-independent\ncallback=append-v1", 1),
            "join with no dependencies": SPEC.replace(b"depends=b-transform,c-independent", b"depends="),
            "join with one dependency": SPEC.replace(b"depends=b-transform,c-independent", b"depends=b-transform"),
            # Valid callback arities but a-source <-> b-transform forms a cycle.
            "cycle": SPEC.replace(b"depends=\ncallback=literal-v1", b"depends=b-transform\ncallback=append-v1", 1),
        }
        source_ids = [f"a{i:02d}" for i in range(16)]
        too_many_inputs = (
            b"BRUCE_DAG_SCHEDULER_SPEC_V3\n"
            b"dag-id=too-many-inputs\n"
            b"max-started-nodes=17\n"
            b"max-total-receipt-bytes=4096\n\n"
            + b"\n\n".join(
                (f"node={node}\ndepends=\ncallback=literal-v1\n"
                 "request-hex=616c706861").encode() for node in source_ids)
            + b"\n\nnode=z-join\ndepends="
            + ",".join(source_ids).encode()
            + b"\ncallback=join-v1\nrequest-hex=3a62657461\n"
        )
        cases["join with sixteen dependencies"] = too_many_inputs
        for action in ("execute", "inspect"):
            for label, spec in cases.items():
                with self.subTest(action=action, case=label):
                    process = self.with_spec(action, spec)
                    self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
                    self.assertEqual(process.stdout, b"")
                    # Inspect must reach parsing; a usage-only rejection cannot pass.
                    self.assertNotIn(b"Usage:", process.stderr)
                    for forbidden in FORBIDDEN:
                        self.assertNotIn(forbidden, process.stderr)
                    for line in spec.split(b"\n"):
                        if line.startswith(b"request-hex="):
                            request_hex = line.removeprefix(b"request-hex=")
                            if request_hex:
                                self.assertNotIn(request_hex, process.stderr)


if __name__ == "__main__":
    unittest.main()
