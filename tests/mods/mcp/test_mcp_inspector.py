"""Actual loopback Mcp HTTP workflows and alternate CLI build modes."""
import argparse
from contextlib import contextmanager
from html.parser import HTMLParser
import http.client
import json
import re
import shutil
from pathlib import Path
import socket
import subprocess
import time
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("binary", nargs="?")
parser.add_argument("--disabled", action="store_true")
parser.add_argument("--documentation-only", action="store_true")
parser.add_argument("--versioned-only", action="store_true")
parser.add_argument("--invalid-config-source")
args = parser.parse_args()


class ElementInventory(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.file_count = 0
        self.buttons = 0
        self.options = []

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        if "id" in attributes:
            self.ids.add(attributes["id"])
        self.file_count += tag == "input" and attributes.get("type") == "file"
        self.buttons += tag == "button"
        if tag == "option":
            self.options.append(attributes.get("value"))


class McpInspectorBoundaryTests(unittest.TestCase):
    def http(self, port, method, route, body=None):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        try:
            try:
                connection.request(method, route, body=body)
            except BrokenPipeError:
                # A size rejection can arrive while the client is still sending.
                # Read that server response rather than mistake it for no response.
                pass
            response = connection.getresponse()
            return response.status, response.getheader("Content-Type"), response.read()
        finally:
            connection.close()

    @contextmanager
    def server(self, port=None, default=False):
        if port is None:
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
        arguments = [] if default else ["--port", str(port)]
        child = subprocess.Popen([str(Path(args.binary).resolve()), "mcp", *arguments],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if child.poll() is not None:
                    out, err = child.communicate()
                    self.fail(f"Mcp exited before health: {child.returncode}; {out!r}; {err!r}")
                try:
                    status, _, body = self.http(port, "GET", "/api/v1/health")
                    if status == 200 and json.loads(body) == {"status": "OK"}:
                        break
                except (OSError, ValueError):
                    pass
                time.sleep(0.03)
            else:
                self.fail("Mcp did not become healthy on requested loopback port")
            yield port
        finally:
            if child.poll() is None:
                child.terminate()
            try:
                child.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.communicate(timeout=5)

    def canonical_bundle(self, version="v2"):
        writer = Path(args.binary).resolve().with_name("bruce_mcp_inspector_tests")
        with tempfile.TemporaryDirectory(prefix="bruce-mcp-bundle-") as directory:
            bundle = Path(directory) / "canonical.bundle"
            mode = {"v2":"--write-valid-bundle", "v3":"--write-valid-v3-bundle",
                    "v2-limit":"--write-v2-limit-bundle",
                    "v4":"--write-valid-dag-scheduler-v4-bundle"}[version]
            result = subprocess.run([str(writer), mode, str(bundle)],
                                    capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            return bundle.read_bytes()

    def call(self, *arguments):
        return subprocess.run([str(Path(args.binary).resolve()), *arguments],
                              capture_output=True, text=True, timeout=10)

    def error(self, arguments, expected):
        result = self.call("mcp", *arguments)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertEqual(json.loads(result.stderr)["error"], expected)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled CLI only")
    def test_malformed_ports_and_extra_arguments_are_rejected(self):
        cases = [
            (["--port", "0"], "InvalidMcpPort"),
            (["--port", "65536"], "InvalidMcpPort"),
            (["--port", "abc"], "InvalidMcpPort"),
            (["--port", "1x"], "InvalidMcpPort"),
            (["--port", "-1"], "InvalidMcpPort"),
            (["--port", "+1"], "InvalidMcpPort"),
            (["--port", " 1"], "InvalidMcpPort"),
            (["--port", ""], "InvalidMcpPort"),
            (["--port", "999999999999999999999"], "InvalidMcpPort"),
            (["--port", "1", "--port", "2"], "InvalidMcpArguments"),
            (["--port"], "InvalidMcpArguments"),
            (["--jobs"], "InvalidMcpArguments"),
            (["import", "file"], "InvalidMcpArguments"),
        ]
        for arguments, expected in cases:
            with self.subTest(arguments=arguments):
                self.error(arguments, expected)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled CLI only")
    def test_valid_ports_and_default_start_managed_service(self):
        for port, default in ((8080, True), (1, False), (65535, False)):
            with self.subTest(port=port, default=default):
                try:
                    with socket.socket() as reservation:
                        reservation.bind(("127.0.0.1", port))
                except PermissionError:
                    # Privileged ports still delegate to the server and fail cleanly
                    # under the host's permission policy, rather than CLI parsing.
                    result = self.call("mcp", "--port", str(port))
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn("InvalidMcp", result.stderr)
                    self.assertNotIn("McpInspectorUnavailable", result.stderr)
                    continue
                with self.server(port, default=default):
                    self.assertEqual(self.http(port, "GET", "/api/v1/health")[0], 200)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_embedded_ui_is_text_safe_and_has_six_views(self):
        with self.server() as port:
            status, content_type, body = self.http(port, "GET", "/")
        self.assertEqual(status, 200)
        self.assertEqual(content_type, "text/html; charset=utf-8")
        html = body.decode("utf-8")
        self.assertIn("BRUCE MCP — Phase-DAG V2 Inspector", html)
        self.assertIn("textContent", html)
        self.assertIn("/api/v1/inspect", html)
        self.assertNotIn("innerHTML", html)
        inventory = ElementInventory()
        inventory.feed(html)
        self.assertEqual(inventory.file_count, 1)
        self.assertEqual(inventory.buttons, 2)
        self.assertTrue({"status", "overview", "graph", "receipts", "trace", "validation", "scheduler"} <= inventory.ids)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_health_is_ok_json(self):
        with self.server() as port:
            status, content_type, body = self.http(port, "GET", "/api/v1/health")
        self.assertEqual((status, content_type), (200, "application/json"))
        self.assertEqual(json.loads(body), {"status": "OK"})

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_canonical_bundle_returns_valid_model(self):
        with self.server() as port:
            status, content_type, body = self.http(port, "POST", "/api/v1/inspect", self.canonical_bundle())
        self.assertEqual((status, content_type), (200, "application/json"))
        model = json.loads(body)
        self.assertEqual(model["status"], "VALID")
        self.assertEqual(model["code"], "VALID")
        self.assertEqual(model["dag_version"], "phase-dag-v2")
        self.assertEqual(model["dag_id"], "mcp-inspector-test")
        self.assertEqual(model["nodes"][0]["node_id"], "a")
        self.assertEqual(model["nodes"][0]["dependencies"], [])
        self.assertEqual(model["receipts"][0]["adapter_id"], "nanocell-phase-v1")
        self.assertEqual(model["receipts"][0]["outcome"], "READY")
        self.assertEqual([entry["event"] for entry in model["trace"]],
                         ["VALIDATED", "STARTED", "READY", "COMPLETED"])
        self.assertNotIn(b"nanocell-reference-context-v1", body)
        self.assertNotIn(b"BRUCE_PHASE_DAG_RUN_V2", body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_cli_executed_bundle_returns_valid_model_and_dependencies(self):
        # Catch CLI bundles that replay locally but do not interoperate with
        # the actual loopback inspector, or expose request/context payloads.
        with tempfile.TemporaryDirectory(prefix="bruce-mcp-cli-bundle-") as directory:
            root = Path(directory)
            contexts = (b"nanocell-reference-context-v1", b"private-cli-context\x00\xff")
            for index, context in enumerate(contexts):
                (root / f"context-{index}.bin").write_bytes(context)
            spec = root / "graph.spec"
            nodes = []
            for node, depends, context, technique in (
                ("a", "", 0, "masked-match-v1"),
                ("b", "a", 1, "rotate-match-v1"),
                ("c", "", 0, "masked-match-v1"),
            ):
                nodes.append(f"node={node}\ndepends={depends}\ncore=1020a54050607080\n"
                             "active-index=2\ncontext-id=reference.context_1\n"
                             f"context-file=context-{context}.bin\ntechnique={technique}\n"
                             "max-operations=16\nmax-trace-entries=18\n")
            spec.write_text("BRUCE_PHASE_DAG_SPEC_V1\ndag-id=cli-mcp-interoperability\n"
                            "max-started-nodes=3\nmax-receipt-bytes=65536\n"
                            "max-trace-entries=32\n\n" + "\n".join(nodes), encoding="utf-8")
            bundle_path = root / "cli.bundle"
            execution = self.call("phase-dag-v2", "execute", str(spec), "--output", str(bundle_path))
            self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
            self.assertEqual(execution.stderr, "")
            self.assertEqual(json.loads(execution.stdout)["action"], "execute")
            bundle = bundle_path.read_bytes()
            with self.server() as port:
                status, content_type, body = self.http(port, "POST", "/api/v1/inspect", bundle)
            self.assertEqual(bundle_path.read_bytes(), bundle)
        self.assertEqual((status, content_type), (200, "application/json"))
        model = json.loads(body)
        self.assertEqual(model["status"], "VALID")
        self.assertEqual(model["code"], "VALID")
        self.assertEqual(model["dag_version"], "phase-dag-v2")
        self.assertEqual(model["dag_id"], "cli-mcp-interoperability")
        self.assertEqual([(node["node_id"], node["dependencies"]) for node in model["nodes"]],
                         [("a", []), ("b", ["a"]), ("c", [])])
        self.assertEqual([(receipt["node_id"], receipt["outcome"]) for receipt in model["receipts"]],
                         [("a", "READY"), ("b", "READY"), ("c", "READY")])
        for marker in (*contexts, b"private-cli-context", b"nanocell-phase-request-v1",
                       b"nanocell-phase-output-v1", b"BRUCE_MOD_RUN_V1", b"BRUCE_PHASE_DAG_RUN_V2"):
            self.assertNotIn(marker, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_mutation_and_private_markers_are_rejected_without_disclosure(self):
        canonical = self.canonical_bundle()
        mutated = canonical[:-1] + bytes([canonical[-1] ^ 1])
        with self.server() as port:
            for data in (mutated, b"BRUCE_PHASE_DAG_RUN_V2\n/home/private/raw-path\nsecret-request"):
                with self.subTest(data_length=len(data)):
                    status, content_type, body = self.http(port, "POST", "/api/v1/inspect", data)
                    self.assertEqual((status, content_type), (422, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')
                    for secret in (b"/home/", b"secret-request", b"BRUCE_PHASE_DAG_RUN_V2", b"exception", b"stack"):
                        self.assertNotIn(secret, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_transport_limit_and_v2_independent_limit(self):
        with self.server() as port:
            for route in ("/api/v1/inspect", "/api/v3/inspect"):
                with self.subTest(route=route):
                    status, content_type, body = self.http(port, "POST", route, b"x" * (5 * 1024 * 1024 + 1))
                    self.assertEqual((status, content_type), (413, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')
                    status, content_type, body = self.http(port, "POST", route, b"x" * (5 * 1024 * 1024))
                    self.assertEqual((status, content_type), (422, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')
            for size in (1024 * 1024, 1024 * 1024 + 1):
                status, content_type, body = self.http(port, "POST", "/api/v1/inspect", b"x" * size)
                self.assertEqual((status, content_type), (422, "application/json"))
                self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_server_keeps_v2_limit_for_replayable_large_bundle(self):
        # The fixture writer verifies core replay before publishing exactly 1 MiB + 1 byte.
        bundle = self.canonical_bundle("v2-limit")
        self.assertEqual(len(bundle), 1024 * 1024 + 1)
        with self.server() as port:
            status, content_type, body = self.http(port, "POST", "/api/v1/inspect", bundle)
        self.assertEqual((status, content_type), (422, "application/json"))
        self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_unregistered_routes_and_methods_are_404(self):
        cases = (("POST", "/api/v1/jobs"), ("GET", "/api/v1/imports"), ("GET", "/arbitrary"),
                 ("GET", "/api/v1/inspect"), ("POST", "/"), ("POST", "/api/v1/health"),
                 ("HEAD", "/"), ("HEAD", "/api/v1/health"), ("PUT", "/api/v1/inspect"),
                 ("OPTIONS", "/api/v1/inspect"), ("DELETE", "/"), ("TRACE", "/"), ("BREW", "/"))
        cases += tuple((method, route) for method in ("GET", "HEAD", "PUT", "PATCH", "OPTIONS", "DELETE", "TRACE", "BREW")
                       for route in ("/api/v3/inspect", "/api/v3/health"))
        cases += (("POST", "/api/v3/health"), ("POST", "/api/v2/inspect"),
                  ("POST", "/api/v3/inspect/extra"))
        with self.server() as port:
            for method, route in cases:
                with self.subTest(method=method, route=route):
                    status, _, body = self.http(port, method, route)
                    self.assertEqual(status, 404, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_canonical_bundle_returns_only_metadata(self):
        with self.server() as port:
            status, content_type, body = self.http(port, "POST", "/api/v3/inspect", self.canonical_bundle("v3"))
        self.assertEqual((status, content_type), (200, "application/json"))
        model = json.loads(body)
        self.assertEqual((model["status"], model["code"], model["dag_version"], model["dag_id"]),
                         ("VALID", "VALID", "phase-dag-v3", "mcp-inspector-v3-test"))
        self.assertEqual([(node["node_id"], node["dependencies"]) for node in model["nodes"]],
                         [("a", []), ("gate", ["a"])])
        source = model["receipts"][0]["output"]
        consumed = model["receipts"][1]["input_refs"][0]
        self.assertGreater(source["payload_size"], 0)
        self.assertEqual(consumed["payload_size"], source["payload_size"])
        self.assertRegex(source["payload_digest"], r"^[0-9a-f]{64}$")
        self.assertEqual(consumed["producer_payload_digest"], source["payload_digest"])
        for marker in (b"payload_bytes", b"request_bytes", b"output_bytes", b"evidence_bytes", b"context_bytes",
                       b"BRUCE_PHASE_DAG_RUN_V3", b"nanocell-reference-context-v1", b"BRUCE_NANOCELL_PHASE_PAYLOAD_V1"):
            self.assertNotIn(marker, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_cross_version_and_mutation_are_safely_rejected(self):
        v2, v3 = self.canonical_bundle(), self.canonical_bundle("v3")
        cases = (("/api/v1/inspect", v3), ("/api/v3/inspect", v2),
                 ("/api/v3/inspect", v3[:-1] + bytes([v3[-1] ^ 1])),
                 ("/api/v3/inspect", b"BRUCE_PHASE_DAG_RUN_V3\n/private/raw-path\nsecret-payload"))
        with self.server() as port:
            for route, data in cases:
                with self.subTest(route=route, size=len(data)):
                    status, content_type, body = self.http(port, "POST", route, data)
                    self.assertEqual((status, content_type), (422, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v4_canonical_bundle_returns_only_declared_metadata(self):
        # Catch missing/miswired V4 inspection and disclosure of recorded bytes.
        bundle = self.canonical_bundle("v4")
        with tempfile.TemporaryDirectory(prefix="bruce-mcp-v4-cli-") as directory:
            path = Path(directory) / "canonical.bundle"
            path.write_bytes(bundle)
            result = self.call("dag-scheduler-v4", "inspect", str(path))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["action"], "inspect")
        with self.server() as port:
            status, content_type, body = self.http(port, "POST", "/api/dag-scheduler-v4/inspect", bundle)
        self.assertEqual((status, content_type), (200, "application/json"))
        model = json.loads(body)
        self.assertEqual(set(model), {"status", "code", "scheduler_version", "dag_id",
                                     "max_started_nodes", "max_total_receipt_bytes", "nodes", "receipts", "trace"})
        self.assertEqual((model["status"], model["code"], model["scheduler_version"], model["dag_id"],
                          model["max_started_nodes"], model["max_total_receipt_bytes"]),
                         ("VALID", "VALID", "dag-scheduler-v2", "mcp-scheduler-v4-test", 4, 4096))
        self.assertEqual([(node["node_id"], node["callback_id"], node["dependencies"], node["request_size"])
                          for node in model["nodes"]],
                         [("a", "literal-v1", [], 17), ("b", "append-v1", ["a"], 16),
                          ("c", "literal-v1", [], 17), ("d", "join-v1", ["b", "c"], 14)])
        for node in model["nodes"]:
            self.assertEqual(set(node), {"node_id", "callback_id", "dependencies", "request_size", "request_digest"})
            self.assertRegex(node["request_digest"], r"^[0-9a-f]{64}$")
        self.assertEqual([(receipt["node_id"], receipt["callback_id"], receipt["request_size"],
                          receipt["outcome"], receipt["output"]["result_code"], receipt["output"]["payload_size"])
                         for receipt in model["receipts"]],
                         [("a", "literal-v1", 17, "READY", "LITERAL_READY", 17),
                          ("b", "append-v1", 16, "READY", "APPENDED_READY", 33),
                          ("c", "literal-v1", 17, "READY", "LITERAL_READY", 17),
                          ("d", "join-v1", 14, "READY", "JOINED_READY", 64)])
        for receipt in model["receipts"]:
            self.assertEqual(set(receipt), {"node_id", "callback_id", "request_size", "request_digest",
                                            "input_refs", "outcome", "output", "failure_code"})
            self.assertEqual(set(receipt["output"]), {"output_version", "result_code", "payload_size", "payload_digest"})
            self.assertEqual(receipt["output"]["output_version"], "dag-scheduler-output-v2")
            for reference in receipt["input_refs"]:
                self.assertEqual(set(reference), {"producer_node_id", "result_code", "payload_size", "payload_digest"})
        self.assertEqual([(entry["event"], entry["node_id"]) for entry in model["trace"]],
                         [("VALIDATED", ""), ("STARTED", "a"), ("READY", "a"), ("STARTED", "b"),
                          ("READY", "b"), ("STARTED", "c"), ("READY", "c"), ("STARTED", "d"),
                          ("READY", "d"), ("COMPLETED", "")])
        for entry in model["trace"]:
            self.assertEqual(set(entry), {"event", "node_id"})
        for marker in (b"private-literal-a", b"private-append-b", b"private-literal-c", b"private-join-d",
                       b"request_bytes", b"payload_bytes", b"BRUCE_DAG_SCHEDULER_RUN_V4"):
            self.assertNotIn(marker, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v4_cross_version_matrix_and_mutation_are_safely_rejected(self):
        v4 = self.canonical_bundle("v4")
        cases = (("/api/dag-scheduler-v4/inspect", self.canonical_bundle("v2")),
                 ("/api/dag-scheduler-v4/inspect", self.canonical_bundle("v3")),
                 ("/api/v1/inspect", v4), ("/api/v3/inspect", v4),
                 ("/api/dag-scheduler-v4/inspect", v4[:-1] + bytes([v4[-1] ^ 1])),
                 ("/api/dag-scheduler-v4/inspect", b"BRUCE_DAG_SCHEDULER_RUN_V4\n/private/path\nsecret-request"))
        with self.server() as port:
            for route, data in cases:
                with self.subTest(route=route, size=len(data)):
                    status, content_type, body = self.http(port, "POST", route, data)
                    self.assertEqual((status, content_type), (422, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v4_transport_boundary_has_fixed_rejection(self):
        with self.server() as port:
            for size, expected_status in ((5 * 1024 * 1024, 422), (5 * 1024 * 1024 + 1, 413)):
                with self.subTest(size=size):
                    status, content_type, body = self.http(port, "POST", "/api/dag-scheduler-v4/inspect", b"x" * size)
                    self.assertEqual((status, content_type), (expected_status, "application/json"))
                    self.assertEqual(body, b'{"status":"REJECTED","code":"REJECTED"}')

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v4_unregistered_routes_and_methods_are_404(self):
        cases = tuple((method, "/api/dag-scheduler-v4/inspect") for method in
                      ("GET", "HEAD", "PUT", "PATCH", "OPTIONS", "DELETE", "TRACE", "BREW"))
        cases += tuple((method, route) for method in ("GET", "POST") for route in
                       ("/api/dag-scheduler-v4/health", "/api/dag-scheduler-v4/replay",
                        "/api/dag-scheduler-v4/execute", "/api/dag-scheduler-v4/inspect/extra", "/api/v4/inspect"))
        with self.server() as port:
            for method, route in cases:
                with self.subTest(method=method, route=route):
                    status, _, body = self.http(port, method, route)
                    self.assertEqual(status, 404, body)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v4_explicit_selection_projects_metadata_and_cancels_late_content(self):
        # Catch V4 falling back to V2/V3 or accidentally rendering unexpected bytes.
        with self.server() as port:
            status, _, body = self.http(port, "GET", "/")
            _, _, model_body = self.http(port, "POST", "/api/dag-scheduler-v4/inspect", self.canonical_bundle("v4"))
        self.assertEqual(status, 200)
        html = body.decode()
        inventory = ElementInventory()
        inventory.feed(html)
        self.assertEqual(inventory.options, ["v2", "v3", "v4"])
        node = shutil.which("node")
        if not node:
            self.skipTest("Node.js unavailable for embedded JavaScript DOM workflow; HTML selection checked")
        model = json.loads(model_body)
        secrets = ["private-request", "private-payload", "/private/local/path", "<script>alert(1)</script>"]
        # Inject extra fields at every model level, including the V3-only view.
        def inject(value):
            if isinstance(value, dict):
                for nested in list(value.values()):
                    inject(nested)
                value.update(zip(("request_bytes", "payload_bytes", "path", "script"), secrets))
            elif isinstance(value, list):
                for nested in value:
                    inject(nested)
        inject(model)
        model["scheduler_explanation"] = {"trace_model": secrets[3], "nodes": []}
        script = re.search(r"<script>(.*?)</script>", html, re.DOTALL).group(1)
        harness = r"""
const assert = require('node:assert/strict');
const vm = require('node:vm');
const input = JSON.parse(require('node:fs').readFileSync(0, 'utf8'));
const elements = Object.fromEntries(input.ids.map(id => [id, {
    value: id === 'version' ? 'v2' : '', files: [], disabled: false, textContent: '',
    handlers: {}, addEventListener(event, callback) { this.handlers[event] = callback; }
}]));
for (const element of Object.values(elements)) {
    Object.defineProperty(element, 'innerHTML', {set() { throw Error('unsafe HTML rendering'); }});
}
const document = {title:'', getElementById(id) { assert.ok(elements[id], id); return elements[id]; }};
let fetched = [];
let pauseRequest = false;
let releaseRequest;
const fetch = async (route, options) => {
    fetched.push({route, method:options.method});
    if (pauseRequest) await new Promise(resolve => { releaseRequest = resolve; });
    return {ok:true, json:async () => input.model};
};
vm.runInNewContext(input.script, {document, fetch});
const choose = version => { elements.version.value = version; elements.version.handlers.change(); };
const select = size => {
    elements.bundle.value = 'private-local.bundle';
    elements.bundle.files = [{size, arrayBuffer:async () => Buffer.from(input.secrets[0])}];
};
const click = () => elements.inspect.handlers.click();
const empty = () => {
    for (const id of ['status', 'overview', 'graph', 'receipts', 'trace', 'validation', 'scheduler'])
        assert.equal(elements[id].textContent, '', id);
    assert.equal(elements.bundle.value, '');
    assert.equal(elements.inspect.disabled, false);
};
(async () => {
    choose('v3');
    elements.scheduler.textContent = 'old V3 scheduler content';
    choose('v4');
    assert.equal(document.title, 'BRUCE MCP — DAG-Scheduler V4 Inspector');
    assert.equal(elements.title.textContent, document.title);
    assert.equal(elements.limit.textContent, '5 MiB');
    assert.equal(elements['scheduler-section'].hidden, true);
    assert.equal(elements.scheduler.textContent, '');
    select(5 * 1024 * 1024);
    await click();
    assert.deepEqual(fetched, [{route:'/api/dag-scheduler-v4/inspect', method:'POST'}]);
    assert.equal(elements.bundle.value, '');
    assert.deepEqual(JSON.parse(elements.overview.textContent), {
        status:'VALID', scheduler_version:'dag-scheduler-v2', dag_id:'mcp-scheduler-v4-test',
        max_started_nodes:4, max_total_receipt_bytes:4096, node_count:4
    });
    const graph = JSON.parse(elements.graph.textContent);
    const receipts = JSON.parse(elements.receipts.textContent);
    assert.deepEqual(graph.map(n => [n.node_id, n.callback_id, n.dependencies, n.request_size]),
        [['a', 'literal-v1', [], 17], ['b', 'append-v1', ['a'], 16],
         ['c', 'literal-v1', [], 17], ['d', 'join-v1', ['b', 'c'], 14]]);
    for (let i = 0; i < graph.length; ++i) {
        const n = graph[i], r = receipts[i];
        assert.deepEqual(Object.keys(n).sort(), ['callback_id', 'dependencies', 'node_id', 'request_digest', 'request_size']);
        assert.deepEqual(Object.keys(r).sort(), ['callback_id', 'failure_code', 'input_refs', 'node_id', 'outcome', 'output', 'request_digest', 'request_size']);
        assert.equal(r.callback_id, n.callback_id);
        assert.equal(r.request_size, n.request_size);
        assert.equal(r.request_digest, n.request_digest);
        assert.match(r.request_digest, /^[0-9a-f]{64}$/);
        assert.equal(r.outcome, 'READY');
        assert.deepEqual(Object.keys(r.output).sort(), ['output_version', 'payload_digest', 'payload_size', 'result_code']);
        for (const ref of r.input_refs)
            assert.deepEqual(Object.keys(ref).sort(), ['payload_digest', 'payload_size', 'producer_node_id', 'result_code']);
    }
    assert.deepEqual(receipts.map(r => [r.output.result_code, r.output.payload_size]),
        [['LITERAL_READY', 17], ['APPENDED_READY', 33], ['LITERAL_READY', 17], ['JOINED_READY', 64]]);
    assert.deepEqual(JSON.parse(elements.trace.textContent).map(e => [e.event, e.node_id]),
        [['VALIDATED', ''], ['STARTED', 'a'], ['READY', 'a'], ['STARTED', 'b'], ['READY', 'b'],
         ['STARTED', 'c'], ['READY', 'c'], ['STARTED', 'd'], ['READY', 'd'], ['COMPLETED', '']]);
    assert.equal(elements.validation.textContent, 'VALID: canonical bundle validation passed; not re-executed.');
    assert.equal(elements['scheduler-section'].hidden, true);
    assert.equal(elements.scheduler.textContent, '');
    for (const secret of input.secrets)
        assert.ok(!Object.values(elements).some(e => e.textContent.includes(secret)), secret);
    select(5 * 1024 * 1024 + 1);
    await click();
    assert.equal(fetched.length, 1);
    assert.match(elements.status.textContent, /REJECTED.*5 MiB/);
    elements.reset.handlers.click();
    empty();
    pauseRequest = true;
    select(100);
    const pending = click();
    await new Promise(resolve => setImmediate(resolve));
    elements.reset.handlers.click();
    releaseRequest();
    await pending;
    empty();
    select(100);
    const pendingSelection = click();
    await new Promise(resolve => setImmediate(resolve));
    choose('v2');
    releaseRequest();
    await pendingSelection;
    empty();
    assert.match(document.title, /Phase-DAG V2/);
    assert.equal(elements['scheduler-section'].hidden, true);
})().catch(error => { console.error(error); process.exitCode = 1; });
"""
        result = subprocess.run([node, "-e", harness], input=json.dumps({
            "script": script, "ids": sorted(inventory.ids), "model": model, "secrets": secrets}),
            text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_v3_explicit_selection_controls_route_title_limit_and_safe_dom(self):
        with self.server() as port:
            status, _, body = self.http(port, "GET", "/")
            _, _, model_body = self.http(port, "POST", "/api/v3/inspect", self.canonical_bundle("v3"))
        self.assertEqual(status, 200)
        html = body.decode()
        inventory = ElementInventory()
        inventory.feed(html)
        self.assertIn("version", inventory.ids)
        self.assertEqual(inventory.options, ["v2", "v3", "v4"])
        self.assertIn("Phase-DAG V2", html)
        self.assertIn("Phase-DAG V3", html)
        self.assertNotIn("innerHTML", html)
        node = shutil.which("node")
        if not node:
            self.skipTest("Node.js unavailable for embedded JavaScript DOM workflow; HTML selection checked")
        model = json.loads(model_body)
        model["scheduler_explanation"] = {
            "trace_model": "canonical-logical-v1",
            "parallelism_recorded": False,
            "nodes": [{
                "node_id": "blocked-node",
                "outcome": "BLOCKED",
                "failure_code": "BlockedByDependency",
                "dependencies": ["failed-node"],
                "blocking_predecessors": [{
                    "node_id": "failed-node",
                    "outcome": "FAILED",
                    "failure_code": "AdapterFailure",
                }],
            }],
        }
        # A future accidental backend byte field must not become DOM content.
        secret = "private-raw-payload-<script>alert(1)</script>"
        model["scheduler_explanation"]["raw_payload"] = secret
        model["scheduler_explanation"]["wave_width"] = 3
        model["scheduler_explanation"]["nodes"][0]["start_time"] = "private-time"
        model["scheduler_explanation"]["nodes"][0]["thread_id"] = "private-thread"
        model["scheduler_explanation"]["nodes"][0]["max_parallel_nodes"] = 4
        for receipt in model["receipts"]:
            receipt["request_bytes"] = secret
            receipt["output"]["payload_bytes"] = secret
            receipt["output"]["evidence_bytes"] = secret
            for reference in receipt["input_refs"]:
                reference["producer_payload_bytes"] = secret
        script = re.search(r"<script>(.*?)</script>", html, re.DOTALL).group(1)
        harness = r"""
const assert = require('node:assert/strict');
const vm = require('node:vm');
const input = JSON.parse(require('node:fs').readFileSync(0, 'utf8'));
const elements = Object.fromEntries(input.ids.map(id => [id, {
    value: id === 'version' ? 'v2' : '', files: [], disabled: false, textContent: '',
    handlers: {}, addEventListener(event, callback) { this.handlers[event] = callback; }
}]));
for (const element of Object.values(elements)) {
    Object.defineProperty(element, 'innerHTML', {set() { throw Error('unsafe HTML rendering'); }});
}
const document = {title: '', getElementById(id) { assert.ok(elements[id], id); return elements[id]; }};
let fetched = [];
let reply = input.model;
let failRequest = false;
let releaseRequest;
let pauseRequest = false;
const fetch = async (route, options) => {
    fetched.push({route, options});
    if (pauseRequest) await new Promise(resolve => { releaseRequest = resolve; });
    if (failRequest) throw Error(input.secret);
    return {ok: true, json: async () => reply};
};
vm.runInNewContext(input.script, {document, fetch});
const click = () => elements.inspect.handlers.click();
const choose = value => { elements.version.value = value; elements.version.handlers.change(); };
const select = size => {
    elements.bundle.value = 'local.bundle';
    elements.bundle.files = [{size, arrayBuffer: async () => Buffer.from('BRUCE_PHASE_DAG_RUN_V2')}];
};
(async () => {
    choose('v3');
    assert.match(document.title, /Phase-DAG V3/);
    assert.match(elements.title.textContent, /Phase-DAG V3/);
    select(1024 * 1024 + 1);
    await click();
    assert.equal(fetched.length, 1);
    assert.equal(fetched[0].route, '/api/v3/inspect');
    assert.equal(fetched[0].options.method, 'POST');
    assert.equal(elements.bundle.value, '');
    assert.match(elements.receipts.textContent, /payload_size/);
    assert.match(elements.receipts.textContent, /payload_digest/);
    assert.match(elements.receipts.textContent, /request_digest/);
    assert.match(elements.scheduler.textContent, /canonical-logical-v1/);
    assert.match(elements.scheduler.textContent, /not recorded/);
    assert.match(elements.scheduler.textContent, /blocked-node/);
    assert.match(elements.scheduler.textContent, /failed-node/);
    assert.ok(!Object.values(elements).some(element => element.textContent.includes(input.secret)));
    assert.ok(!elements.receipts.textContent.includes('payload_bytes'));
    assert.ok(!elements.scheduler.textContent.includes('raw_payload'));
    assert.ok(!elements.scheduler.textContent.includes('start_time'));
    assert.ok(!elements.scheduler.textContent.includes('thread_id'));
    assert.ok(!elements.scheduler.textContent.includes('max_parallel_nodes'));
    reply = {...input.model};
    delete reply.scheduler_explanation;
    select(100);
    await click();
    assert.equal(fetched.length, 2);
    assert.match(elements.scheduler.textContent, /No scheduler explanation/);
    reply = input.model;
    select(5 * 1024 * 1024 + 1);
    await click();
    assert.equal(fetched.length, 2);
    assert.match(elements.status.textContent, /5 MiB/);
    choose('v2');
    assert.match(document.title, /Phase-DAG V2/);
    select(1024 * 1024 + 1);
    await click();
    assert.equal(fetched.length, 2);
    assert.match(elements.status.textContent, /1 MiB/);
    select(1024 * 1024);
    await click();
    assert.equal(fetched[2].route, '/api/v1/inspect');
    choose('v3');
    select(5 * 1024 * 1024);
    failRequest = true;
    await click();
    assert.equal(fetched[3].route, '/api/v3/inspect');
    assert.equal(elements.bundle.value, '');
    assert.match(elements.status.textContent, /ERROR/);
    assert.ok(!elements.status.textContent.includes(input.secret));
    failRequest = false;
    pauseRequest = true;
    select(100);
    const pending = click();
    await new Promise(resolve => setImmediate(resolve));
    elements.reset.handlers.click();
    releaseRequest();
    await pending;
    assert.equal(elements.bundle.value, '');
    for (const id of ['status', 'overview', 'graph', 'receipts', 'trace', 'validation', 'scheduler']) {
        assert.equal(elements[id].textContent, '', id);
    }
    assert.equal(elements.inspect.disabled, false);
})().catch(error => { console.error(error); process.exitCode = 1; });
"""
        result = subprocess.run([node, "-e", harness], input=json.dumps({
            "script": script, "ids": sorted(inventory.ids), "model": model, "secret": secret}),
            text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled HTTP only")
    def test_occupied_loopback_port_exits_without_serving(self):
        with socket.socket() as occupied:
            occupied.bind(("127.0.0.1", 0))
            port = occupied.getsockname()[1]
            result = self.call("mcp", "--port", str(port))
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn("McpInspectorUnavailable", result.stderr)
            with self.assertRaises(OSError):
                self.http(port, "GET", "/api/v1/health")

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled CLI only")
    def test_help_has_mcp(self):
        result = self.call("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("bruce mcp [--port 1..65535]", result.stdout.splitlines())

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled CLI only")
    def test_legacy_cockpit_command_is_unknown(self):
        result = self.call("cockpit", "--port", "0")
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(json.loads(result.stderr), {"error": "UnknownCommand: cockpit"})

    @unittest.skipUnless(args.disabled, "disabled build only")
    def test_disabled_build_has_no_command_or_help(self):
        self.error([], "UnknownCommand: mcp")
        result = self.call("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("bruce mcp", result.stdout)

    @unittest.skipUnless(args.invalid_config_source, "invalid build configuration only")
    def test_enabled_mcp_requires_nanocell(self):
        source = Path(args.invalid_config_source).resolve()
        with tempfile.TemporaryDirectory(prefix="bruce-mcp-invalid-") as build:
            result = subprocess.run([
                str(source / "scripts/with-host-deps.sh"), "cmake", "-G", "Ninja",
                "-S", str(source), "-B", build, "-DBUILD_TESTING=OFF",
                "-DBRUCE_BUILD_MOD_MCP=ON", "-DBRUCE_BUILD_MOD_NANOCELL=OFF"],
                cwd=source, capture_output=True, text=True, timeout=60)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("McpRequiresNanoCell", result.stdout + result.stderr)


class McpInspectorDocumentationTests(unittest.TestCase):
    """Regressions: stale no-target prose or claims of broader Mcp work."""
    root = Path(__file__).resolve().parents[3]
    documents = (
        "README.md", "DEPENDENCIES.md", "docs/architecture/03-blueprint-consolidation.md",
        "docs/architecture/repository-structure.md", "docs/cli/README.md",
        "docs/cli/commands.md", "docs/cli/formats.md", "docs/implementation/README.md",
    )

    @unittest.skipIf(args.disabled or args.invalid_config_source, "enabled CLI only")
    def test_documented_command_matches_exact_cli_help(self):
        result = subprocess.run([str(Path(args.binary).resolve()), "--help"],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("bruce mcp [--port 1..65535]", result.stdout.splitlines())
        reference = (self.root / "docs/cli/commands.md").read_text()
        self.assertIn("bruce mcp [--port 1..65535]", reference)
        self.assertIn("8080", reference)

    def test_current_documents_describe_bounded_read_only_inspector(self):
        for name in self.documents:
            with self.subTest(document=name):
                text = (self.root / name).read_text()
                match = re.search(r"^## MCP Inspector\n(.*?)(?=^## |\Z)", text,
                                  re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(match, "missing current MCP Inspector scope")
                scope = " ".join(match.group(1).split())
                for token in ("BRUCE_BUILD_MOD_MCP=ON", "BRUCE_BUILD_MOD_NANOCELL=ON",
                              "127.0.0.1", "Phase-DAG V2", "Phase-DAG V3", "DAG-Scheduler V4", "NanoCell",
                              "1 MiB", "5 MiB", "read-only", "payload_size"):
                    self.assertIn(token, scope)
                self.assertRegex(scope, r"(?:no new DAG executions|keine neuen DAG-Ausführungen)")
                self.assertRegex(scope, r"(?:no jobs or imports|keine Jobs oder Importe)")
                self.assertRegex(scope, r"(?:request memory|Request-Speicher)")
                self.assertRegex(scope, r"(?:no persistence|keine Persistenz)")
                for route in ("POST /api/v1/inspect", "POST /api/v3/inspect",
                              "POST /api/dag-scheduler-v4/inspect"):
                    self.assertIn(f"`{route}`", scope)
                for field in ("request_bytes", "context_bytes", "output_bytes", "evidence_bytes", "payload_bytes"):
                    self.assertIn(field, scope)
                self.assertRegex(scope, r"(?:explicit selection|explizite Auswahl)")
                self.assertRegex(scope, r"(?:no automatic detection|keine automatische Erkennung)")
                self.assertRegex(scope, r"(?:no adapter plugins|keine Adapterplugins)")
                self.assertNotIn("V2-only", text)
                self.assertNotRegex(text, r"(?:defines|contains) no `BRUCE_BUILD_MOD_MCP`")

    def test_current_mcp_documents_preserve_v4_structural_privacy_boundary(self):
        for name in self.documents:
            with self.subTest(document=name):
                text = (self.root / name).read_text()
                match = re.search(r"^## MCP Inspector\n(.*?)(?=^## |\Z)", text,
                                  re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(match, "missing current MCP Inspector scope")
                scope = " ".join(match.group(1).split())
                for token in ("IMPLEMENTIERT UND BELEGT", "DAG-Scheduler V4",
                              "POST /api/dag-scheduler-v4/inspect", "5 MiB", "5242880",
                              "dag-scheduler-v2", "callback_id", "request_size", "request_digest",
                              "producer_node_id", "payload_size", "payload_digest", "result_code",
                              "scheduler_explanation", "DOKUMENTIERT / KONZEPT"):
                    self.assertIn(token, scope)
                for boundary in (
                    r"(?:metadata only|nur Metadaten)",
                    r"(?:no re-execution|keine erneute Ausführung)",
                    r"(?:local confidential artifacts|lokale vertrauliche Artefakte)",
                    r"(?:no encryption|keine Verschlüsselung)",
                    r"(?:no replay buttons or routes|keine Replay-Buttons oder -Routen)",
                    r"(?:no persistent uploads|keine persistenten Uploads)",
                    r"(?:no payload download|kein Payload-Download)",
                    r"(?:no dynamic plugins|keine dynamischen Plugins)",
                    r"(?:no external access|kein externer Zugriff)",
                ):
                    self.assertRegex(scope, boundary)
                self.assertNotRegex(scope, r"(?i)(?:POST|GET) /api/(?:dag-scheduler-v4/)?(?:replay|execute|download|upload)\b")
                self.assertNotRegex(scope, r"(?i)(?:replay button starts|persistent uploads are supported|payload download is supported|dynamic plugins are loaded|external access is enabled)")

    def test_shared_http_contract_distinguishes_v4_structure_from_v2_v3_replay(self):
        text = (self.root / "docs/cli/commands.md").read_text()
        scope = text.partition("## MCP Inspector\n")[2].split("\n## ", 1)[0]
        shared_http = scope.partition("Das gemeinsame Transportlimit")[2].partition(
            "Unerwarteter Dienstfehler")[0]
        shared_http = " ".join(shared_http.split())
        self.assertNotIn("ungültige oder manipulierte Bundles liefern ebenfalls 422", shared_http,
                         "Shared 422 prose must not promise V4 tamper detection")
        for contract in ("Versionsverwechslungen", "Struktur- oder Digestprüfung",
                         "Phase-DAG V2/V3", "Replay-Mismatch liefert 422",
                         "DAG-Scheduler V4", "strukturell gültige Änderung",
                         "korrekt neu berechneten Digests", "`VALID`",
                         "bestätigt nicht, dass der aufgezeichnete Lauf tatsächlich ausgeführt wurde"):
            with self.subTest(contract=contract):
                self.assertIn(contract, shared_http)

    def test_design_describes_the_implemented_transport(self):
        text = (self.root / "docs/superpowers/specs/2026-10-18-cockpit-to-mcp-rename-design.md").read_text()
        for token in ("BRUCE_BUILD_MOD_MCP", "BRUCE_BUILD_MOD_NANOCELL",
                      "bruce mcp [--port 1..65535]", "8080", "127.0.0.1", "HTTP"):
            self.assertIn(token, text, f"missing implemented design detail: {token}")
        self.assertIn("JSON-RPC- oder Stdio-MCP-Server", text)

    def test_historical_cockpit_provenance_is_preserved(self):
        plan = (self.root / "docs/superpowers/plans/2026-10-04-cockpit-v1.md").read_text()
        self.assertNotIn("v0.59.0", plan, "stale operational dependency in old Cockpit plan")
        self.assertIn("v0.58.0", plan)
        self.assertIn("12 October 2026", plan)
        audit = (self.root / "docs/audit/2026-10-06-repository-analysis-and-next-steps.md").read_text()
        self.assertIn("v0.58.0", audit, "dated audit needs a current provenance correction")

    def test_dependency_guide_describes_the_current_inspector(self):
        text = (self.root / "DEPENDENCIES.md").read_text()
        self.assertNotIn("controlled unavailable stub", text)
        scope = " ".join(text.split())
        for token in ("IMPLEMENTIERT UND BELEGT", "optional read-only Phase-DAG V2 and Phase-DAG V3 inspector",
                      "BRUCE_BUILD_MOD_MCP=ON", "BRUCE_BUILD_MOD_NANOCELL=ON",
                      "127.0.0.1", "1 MiB", "5 MiB", "request memory", "no persistence",
                      "POST /api/v1/inspect", "POST /api/v3/inspect", "payload_size",
                      "explicit selection", "no automatic detection", "no adapter plugins",
                      "no new DAG executions", "no jobs or imports",
                      "v0.58.0", "4f3f9ef19be83ae97a5d9a059432dc00e445b7ab",
                      "aa14e7e7bd2703694e0a6b6855af3b8c406102ab1fc56ac905fe33619b31faa5",
                      "4b45cbe16d7b71b89ae6127e26e0d90a029198ca5e958ad8e3d0b8bbed364d8b"):
            self.assertIn(token, scope)

    def test_generated_cli_artifacts_cover_mcp_parser(self):
        index = (self.root / "docs/cli/source-index.md").read_text()
        self.assertIn("### `mcp`", index)
        self.assertIn("`--port`", index)
        self.assertNotIn("Mcp bleibt V2-only", index)
        self.assertIn("/api/v1/inspect", index)
        self.assertIn("/api/v3/inspect", index)
        validation = (self.root / "docs/cli/validation.md").read_text()
        self.assertIn("1-MiB-V2", validation)
        self.assertIn("5-MiB-V3", validation)
        examples = json.loads((self.root / "docs/cli/examples.json").read_text())
        measured = {item["example"]: item for item in examples["examples"]}
        for label, error in (("mcp-invalid-port", "InvalidMcpPort"),
                             ("mcp-extra-arguments", "InvalidMcpArguments")):
            self.assertIn(label, measured)
            self.assertEqual(measured[label]["argv"][0], "mcp")
            self.assertEqual(measured[label]["exit_code"], 2)
            self.assertEqual(measured[label]["json"]["error"], error)

    def test_cli_reference_limits_routes_and_rejects_broader_claims(self):
        text = (self.root / "docs/cli/commands.md").read_text()
        match = re.search(r"^## MCP Inspector\n(.*?)(?=^## |\Z)", text,
                          re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(match, "missing Mcp command contract")
        scope = " ".join(match.group(1).split())
        for route in ("GET /", "GET /api/v1/health", "POST /api/v1/inspect", "POST /api/v3/inspect"):
            self.assertIn(f"`{route}`", scope)
        for token in ("65535", "8080", "InvalidMcpPort", "InvalidMcpArguments",
                      "422", "413", "500", "404", "VALID", "REJECTED", "ERROR",
                      "1048576", "5242880", "payload_size", "payload_digest"):
            self.assertIn(token, scope)
        self.assertRegex(scope, r"(?:no jobs or imports|keine Jobs oder Importe)")
        self.assertRegex(scope, r"(?:no new DAG executions|keine neuen DAG-Ausführungen)")
        self.assertNotIn("GET /api/v3/health", scope)
        self.assertIn("V2", scope)
        self.assertIn("V3", scope)
        self.assertNotRegex(scope, r"(?i)(?:starts new DAG executions|verwaltet Jobs|importiert Bundles|starts jobs)")

    def test_current_mcp_documents_describe_v3_logical_scheduler_explanation(self):
        documents = (
            "docs/architecture/03-blueprint-consolidation.md",
            "docs/cli/README.md",
            "docs/cli/commands.md",
        )
        for name in documents:
            with self.subTest(document=name):
                text = (self.root / name).read_text()
                match = re.search(r"^## MCP Inspector\n(.*?)(?=^## |\Z)", text,
                                  re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(match, "missing current MCP Inspector scope")
                scope = " ".join(match.group(1).split())
                for token in ("scheduler_explanation", "canonical-logical-v1",
                              "parallelism_recorded", "blocking_predecessors"):
                    self.assertIn(token, scope)
                self.assertRegex(scope, r"(?:not recorded|nicht.*gespeichert)")
                self.assertRegex(scope, r"(?:no timing|keine Zeit|ohne Zeit)")
                self.assertNotRegex(scope, r"(?:records a parallel limit|speichert.*Parallelitätsgrenze)")


if __name__ == "__main__":
    if not args.binary and not args.invalid_config_source:
        parser.error("binary or --invalid-config-source required")
    if args.versioned_only:
        suite = unittest.TestSuite(McpInspectorBoundaryTests(name) for name in
                                  unittest.defaultTestLoader.getTestCaseNames(McpInspectorBoundaryTests)
                                  if name.startswith("test_v3_"))
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        raise SystemExit(not result.wasSuccessful())
    unittest.main(argv=[__file__], defaultTest="McpInspectorDocumentationTests"
                  if args.documentation_only else None)
