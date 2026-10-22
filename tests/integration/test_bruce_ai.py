"""Local assistant boundary tests; no Ollama installation or model required."""

import argparse
import base64
import copy
import hashlib
from dataclasses import FrozenInstanceError, replace
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from bruce_ai.config import AssistantError, AssistantProfile, load_config


class ConfigTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="bruce-ai-config-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.data = self.root / "data"
        self.data.mkdir()
        self.program = self.root / "bruce"
        self.program.write_text("test executable fixture", encoding="utf-8")
        self.program.chmod(0o700)
        self.path = self.root / "config.json"
        self.document = {
            "schema": "bruce-local-ai-config-v1",
            "active_profile": "default",
            "bruce_program": str(self.program),
            "index_path": str(self.data / "index.json"),
            "allowed_roots": [str(self.data)],
            "require_confirmation": True,
            "profiles": {
                "default": {
                    "ollama_url": "http://127.0.0.1:11434",
                    "embedding_model": "all-minilm",
                    "response_model": "qwen3.5",
                    "top_k": 5,
                    "connect_timeout_ms": 1000,
                    "request_timeout_ms": 30000,
                },
                "alternate": {
                    "ollama_url": "http://[::1]:11435",
                    "embedding_model": "other-embedding:latest",
                    "response_model": "other-chat:latest",
                    "top_k": 2,
                    "connect_timeout_ms": 500,
                    "request_timeout_ms": 10000,
                },
            },
        }

    def load(self, document=None, override=None):
        self.path.write_text(json.dumps(self.document if document is None else document),
                             encoding="utf-8")
        return load_config(self.path, override)

    def assert_code(self, code, operation):
        with self.assertRaises(AssistantError) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)
        self.assertEqual(str(caught.exception), code)
        self.assertNotIn(str(self.root), repr(caught.exception))

    def test_exact_schema_profiles_and_immutable_selection(self):
        config = self.load()
        self.assertEqual(config.active_profile, "default")
        self.assertEqual(config.profile.embedding_model, "all-minilm")
        self.assertEqual(config.profile.response_model, "qwen3.5")
        self.assertEqual(config.bruce_program, self.program.resolve())
        self.assertEqual(config.allowed_roots, (self.data.resolve(),))
        with self.assertRaises(FrozenInstanceError):
            config.profile.top_k = 20
        with self.assertRaises(TypeError):
            config.profiles["new"] = config.profile
        alternate = self.load(override="alternate")
        self.assertEqual(alternate.active_profile, "alternate")
        self.assertEqual(alternate.profile.embedding_model, "other-embedding:latest")
        self.assertEqual(json.loads(self.path.read_text())["active_profile"], "default")

    def test_unknown_profile_is_fixed_error(self):
        self.assert_code("UnknownAssistantProfile", lambda: self.load(override="secret-path"))

    def test_cloud_tags_are_rejected_in_every_profile(self):
        for profile in ("default", "alternate"):
            for field in ("embedding_model", "response_model"):
                for model in ("qwen3:cloud", "gpt-oss:120b-cloud", "org/model:CLOUD"):
                    with self.subTest(profile=profile, field=field, model=model):
                        case = copy.deepcopy(self.document)
                        case["profiles"][profile][field] = model
                        self.assert_code("InvalidAssistantConfig", lambda: self.load(case))

    def test_rejects_unknown_missing_fields_and_bad_shapes(self):
        cases = [[], None, {}, {**self.document, "schema": "wrong"},
                 {**self.document, "secret": "never echo"},
                 {**self.document, "profiles": []},
                 {**self.document, "profiles": {}},
                 {**self.document, "active_profile": 2}]
        for key in self.document:
            case = copy.deepcopy(self.document)
            del case[key]
            cases.append(case)
        for profile_change in ({"extra": 1}, {"top_k": None}, {"embedding_model": ""},
                               {"response_model": "bad model"}):
            case = copy.deepcopy(self.document)
            case["profiles"]["default"].update(profile_change)
            cases.append(case)
        for case in cases:
            with self.subTest(case=case):
                # None is intentionally a malformed top-level JSON value here.
                self.path.write_text(json.dumps(case), encoding="utf-8")
                self.assert_code("InvalidAssistantConfig", lambda: load_config(self.path, None))

    def test_rejects_non_loopback_or_ambiguous_urls(self):
        for url in ("https://127.0.0.1:11434", "http://localhost:11434",
                    "http://127.0.0.2:11434", "http://example.com:11434",
                    "http://127.0.0.1", "http://127.0.0.1:0",
                    "http://127.0.0.1:65536", "http://user:secret@127.0.0.1:11434",
                    "http://127.0.0.1:11434/path", "http://127.0.0.1:11434?q=secret",
                    "http://127.0.0.1:11434#secret", " http://127.0.0.1:11434",
                    "http://127.0.0.1:11434\n", "http://[::ffff:127.0.0.1]:11434"):
            with self.subTest(url=url):
                case = copy.deepcopy(self.document)
                case["profiles"]["default"]["ollama_url"] = url
                self.assert_code("InvalidAssistantConfig", lambda: self.load(case))

    def test_rejects_nonpositive_boolean_and_noninteger_limits(self):
        for field in ("top_k", "connect_timeout_ms", "request_timeout_ms"):
            for value in (0, -1, True, 1.5, "1"):
                with self.subTest(field=field, value=value):
                    case = copy.deepcopy(self.document)
                    case["profiles"]["alternate"][field] = value
                    self.assert_code("InvalidAssistantConfig", lambda: self.load(case))
        for value in (False, 1, "true", None):
            self.assert_code("InvalidAssistantConfig", lambda: self.load(
                {**self.document, "require_confirmation": value}))

    def test_rejects_relative_empty_and_symlink_escaping_paths(self):
        for field in ("bruce_program", "index_path"):
            for value in ("relative", "", 5):
                self.assert_code("UnsafeAssistantPath", lambda: self.load(
                    {**self.document, field: value}))
        for roots in ([], [""], ["relative"], [str(self.program)]):
            self.assert_code("UnsafeAssistantPath", lambda: self.load(
                {**self.document, "allowed_roots": roots}))
        link = self.data / "escape"
        link.symlink_to(self.root, target_is_directory=True)
        self.assert_code("UnsafeAssistantPath", lambda: self.load(
            {**self.document, "index_path": str(link / "index.json")}))
        self.assert_code("UnsafeAssistantPath", lambda: self.load(
            {**self.document, "bruce_program": str(link / "bruce")}))
        self.assertFalse((self.data / "index.json").exists())

    def test_rejects_duplicate_keys_malformed_and_nonfinite_json(self):
        for raw in ("{", '{"schema":NaN}', '{"schema":"x","schema":"y"}',
                    json.dumps(self.document).replace('"default": {',
                        '"default": {}, "default": {', 1)):
            self.path.write_text(raw, encoding="utf-8")
            self.assert_code("InvalidAssistantConfig", lambda: load_config(self.path, None))
        self.path.write_bytes(b"\xff")
        self.assert_code("InvalidAssistantConfig", lambda: load_config(self.path, None))


class OllamaTests(unittest.TestCase):
    def setUp(self):
        from bruce_ai.ollama import LocalOllamaClient

        self.client = LocalOllamaClient()
        self.requests = []
        self.response = {"model": "all-minilm", "embeddings": [[0.5, -0.25, 1]],
                         "total_duration": 100, "load_duration": 10, "prompt_eval_count": 2}
        # Documented /api/show local GGUF response, without inference content.
        self.metadata = {
            "license": "fixture license", "modelfile": "FROM /local/model.gguf",
            "parameters": "", "template": "{{ .Prompt }}",
            "details": {"parent_model": "", "format": "gguf", "family": "bert",
                        "families": ["bert"], "parameter_size": "23M",
                        "quantization_level": "F16"},
            "model_info": {"general.architecture": "bert", "general.parameter_count": 23000000},
            "capabilities": ["embedding"], "modified_at": "2026-10-19T10:00:00Z",
        }
        self.metadata_status = 200
        self.status = 200
        self.delay = 0
        self.trickle = False
        self.length_adjustment = 0
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                body = self.rfile.read(int(self.headers["Content-Length"]))
                owner.requests.append((self.path, json.loads(body)))
                time.sleep(owner.delay)
                response = owner.metadata if self.path == "/api/show" else owner.response
                status = owner.metadata_status if self.path == "/api/show" else owner.status
                raw = response if isinstance(response, bytes) else json.dumps(response).encode()
                try:
                    self.send_response(status)
                    if status in (301, 302, 303, 307, 308):
                        self.send_header("Location", f"http://127.0.0.1:{self.server.server_port}/forbidden")
                    self.send_header("Content-Length", str(len(raw) + owner.length_adjustment))
                    self.end_headers()
                    if owner.trickle:
                        for byte in raw:
                            self.wfile.write(bytes([byte]))
                            self.wfile.flush()
                            time.sleep(0.03)
                    else:
                        self.wfile.write(raw)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def log_message(self, *args):
                pass

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        self.thread = threading.Thread(target=self.server.serve_forever,
                                       kwargs={"poll_interval": 0.01}, daemon=True)
        self.thread.start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)
        self.profile = AssistantProfile(f"http://127.0.0.1:{self.server.server_port}",
                                        "all-minilm", "qwen3.5", 5, 500, 1000)

    def assert_code(self, code, operation):
        with self.assertRaises(AssistantError) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)
        self.assertEqual(str(caught.exception), code)
        self.assertNotIn("sensitive", repr(caught.exception))

    def chat_response(self, content):
        return {"model": "qwen3.5", "created_at": "2026-10-19T10:00:00Z",
                "message": {"role": "assistant", "content": content}, "done": True,
                "done_reason": "stop", "total_duration": 100, "load_duration": 10,
                "prompt_eval_count": 3, "prompt_eval_duration": 20,
                "eval_count": 5, "eval_duration": 50}

    def test_embed_uses_current_endpoint_model_input_and_finite_vector(self):
        with patch("subprocess.Popen", side_effect=AssertionError("process launch")), \
                patch.object(os, "system", side_effect=AssertionError("shell launch")):
            self.assertEqual(self.client.embed("Rechne lokal", self.profile), [0.5, -0.25, 1.0])
        self.assertEqual(self.requests, [("/api/show", {"model": "all-minilm"}),
                                        ("/api/embed", {"model": "all-minilm", "input": "Rechne lokal"})])
        alternate = replace(self.profile, embedding_model="other:tag")
        self.response["model"] = "other:tag"
        self.assertEqual(len(self.client.embed("text", alternate)), 3)
        self.assertEqual(self.requests[-1][1]["model"], "other:tag")

    def test_chat_requests_nonstreaming_json_without_thinking_and_returns_object(self):
        self.response = self.chat_response('{"kind":"needs_input","missing_inputs":["x"]}')
        messages = [{"role": "system", "content": "Return JSON"},
                    {"role": "user", "content": "Rechne"}]
        with patch("subprocess.Popen", side_effect=AssertionError("process launch")), \
                patch.object(os, "system", side_effect=AssertionError("shell launch")):
            result = self.client.chat_json(messages, self.profile)
        self.assertEqual(result, {"kind": "needs_input", "missing_inputs": ["x"]})
        self.assertEqual(self.requests, [("/api/show", {"model": "qwen3.5"}),
                                        ("/api/chat", {"model": "qwen3.5", "messages": messages,
                                                       "stream": False, "format": "json", "think": False})])
        self.assertIs(self.requests[-1][1]["think"], False)

    def test_remote_aliases_and_unverified_metadata_never_receive_inference_content(self):
        valid = copy.deepcopy(self.metadata)
        invalid = [{}, {"model_info": {}},
                   {**valid, "model_info": None}, {**valid, "model_info": {}},
                   {**valid, "model_info": {"general.architecture": ""}},
                   {**valid, "model_info": {"general.architecture": True}},
                   {**valid, "details": None}, {**valid, "details": {}},
                   {**valid, "details": {"format": "unknown"}}]
        for field in ("remote_host", "remote_model"):
            for value in ("sensitive-remote-alias", None, False, 0, [], {}):
                invalid.append({**valid, field: value})
        for method in ("embed", "chat"):
            self.response = ({"embeddings": [[1.0]]} if method == "embed"
                             else self.chat_response("{}"))
            for metadata in invalid:
                with self.subTest(method=method, metadata=metadata):
                    self.metadata = metadata
                    self.requests.clear()
                    operation = (lambda: self.client.embed("sensitive", self.profile)) if method == "embed" else (
                        lambda: self.client.chat_json([{"role": "user", "content": "sensitive"}], self.profile))
                    self.assert_code("OllamaModelNotLocal", operation)
                    model = "all-minilm" if method == "embed" else "qwen3.5"
                    self.assertEqual(self.requests, [("/api/show", {"model": model})])

    def test_metadata_errors_fail_closed_before_both_inference_endpoints(self):
        for method in ("embed", "chat"):
            self.response = ({"embeddings": [[1.0]]} if method == "embed"
                             else self.chat_response("{}"))
            for status, metadata, code in ((404, {}, "OllamaModelUnavailable"),
                                           (500, {}, "OllamaUnavailable"),
                                           (302, {}, "OllamaInvalidResponse"),
                                           (200, b"sensitive malformed", "OllamaInvalidResponse"),
                                           (200, b"[]", "OllamaInvalidResponse")):
                with self.subTest(method=method, status=status, metadata=metadata):
                    self.metadata_status, self.metadata = status, metadata
                    self.requests.clear()
                    operation = (lambda: self.client.embed("sensitive", self.profile)) if method == "embed" else (
                        lambda: self.client.chat_json([{"role": "user", "content": "sensitive"}], self.profile))
                    self.assert_code(code, operation)
                    self.assertEqual([path for path, body in self.requests], ["/api/show"])

    def test_model_locality_is_rechecked_before_each_request(self):
        self.assertEqual(self.client.embed("first", self.profile), [0.5, -0.25, 1.0])
        self.metadata["remote_model"] = "sensitive-remote-alias"
        self.assert_code("OllamaModelNotLocal", lambda: self.client.embed("sensitive", self.profile))
        self.assertEqual([path for path, body in self.requests],
                         ["/api/show", "/api/embed", "/api/show"])

    def test_refused_connection_is_unavailable_and_redacted(self):
        with socket.socket() as unused:
            unused.bind(("127.0.0.1", 0))
            port = unused.getsockname()[1]
        profile = replace(self.profile, ollama_url=f"http://127.0.0.1:{port}")
        self.assert_code("OllamaUnavailable", lambda: self.client.embed("sensitive", profile))

    def test_missing_model_and_http_errors_never_echo_server_body(self):
        self.response = {"error": "sensitive local model and path"}
        for status, code in ((404, "OllamaModelUnavailable"), (500, "OllamaUnavailable")):
            self.status = status
            self.assert_code(code, lambda: self.client.embed("sensitive", self.profile))

    def test_redirects_are_never_followed(self):
        for status in (301, 302, 303, 307, 308):
            self.status = status
            self.assert_code("OllamaInvalidResponse", lambda: self.client.embed("text", self.profile))
        self.assertEqual([path for path, body in self.requests], ["/api/show", "/api/embed"] * 5)

    def test_proxy_environment_cannot_change_loopback_destination(self):
        with patch.dict(os.environ, {"http_proxy": "http://sensitive.invalid:1",
                                     "HTTP_PROXY": "http://sensitive.invalid:1",
                                     "ALL_PROXY": "http://sensitive.invalid:1", "NO_PROXY": ""}):
            self.assertEqual(self.client.embed("text", self.profile), [0.5, -0.25, 1.0])
        self.assertEqual([path for path, body in self.requests], ["/api/show", "/api/embed"])

    def test_truncated_http_body_is_invalid_even_when_json_is_complete(self):
        self.length_adjustment = 10
        self.assert_code("OllamaInvalidResponse", lambda: self.client.embed("text", self.profile))

    def test_delayed_response_and_slow_trickle_obey_total_deadline(self):
        profile = replace(self.profile, request_timeout_ms=100)
        self.delay = 0.3
        started = time.monotonic()
        self.assert_code("OllamaTimeout", lambda: self.client.embed("text", profile))
        self.assertLess(time.monotonic() - started, 0.7)
        self.delay = 0
        self.trickle = True
        started = time.monotonic()
        self.assert_code("OllamaTimeout", lambda: self.client.embed("text", profile))
        self.assertLess(time.monotonic() - started, 0.7)

    def test_malformed_nonfinite_empty_and_oversized_embeddings(self):
        for response in (b"sensitive malformed JSON", b"\xff", b"[]",
                         {"embeddings": []}, {"embeddings": [[True]]},
                         {"embeddings": [["0.5"]]}, {"embeddings": [[], []]},
                         {"embeddings": [[math.nan]]}, {"embeddings": [[math.inf]]},
                         {"embeddings": [[0.2], [0.4]]},
                         b'{"embeddings":[[1e999]]}',
                         b'{"embeddings":[[0.5]],"embeddings":[[1]]}',
                         b" " * (2 * 1024 * 1024)):
            with self.subTest(response_type=type(response)):
                self.response = response
                self.assert_code("OllamaInvalidResponse", lambda: self.client.embed("text", self.profile))

    def test_chat_refusal_nonobject_partial_and_invalid_json_are_rejected(self):
        responses = [self.chat_response("I refuse: sensitive text"),
                     self.chat_response("[]"), self.chat_response('{"value":NaN}'),
                     self.chat_response('{"x":1,"x":2}'),
                     self.chat_response('{"value":1e999}'),
                     {**self.chat_response("{}"), "done": False},
                     {"done": True, "message": {"role": "user", "content": "{}"}},
                     {"done": True, "message": {"role": "assistant", "content": "{}",
                                                 "refusal": "sensitive"}}]
        for response in responses:
            self.response = response
            self.assert_code("OllamaInvalidResponse", lambda: self.client.chat_json(
                [{"role": "user", "content": "text"}], self.profile))

    def test_large_or_malformed_requests_fail_before_http(self):
        self.assert_code("OllamaInvalidRequest", lambda: self.client.embed("x" * (2 * 1024 * 1024), self.profile))
        for messages in ([], [{"role": "user", "content": "x", "extra": "secret"}],
                         [{"role": "unknown", "content": "x"}],
                         [{"role": "user", "content": 2}]):
            self.assert_code("OllamaInvalidRequest", lambda: self.client.chat_json(messages, self.profile))
        self.assertEqual(self.requests, [])


class CatalogTests(unittest.TestCase):
    def setUp(self):
        from bruce_ai.catalog import load_catalog
        self.load_catalog = load_catalog
        self.path = Path(__file__).resolve().parents[2] / "tools/bruce_ai_command_catalog_v1.json"

    def test_v1_inventory_and_strict_named_arguments_are_source_backed(self):
        catalog = self.load_catalog(self.path)
        expected = {
            "calculate": ("calculate",), "coordinate": ("coordinate",),
            "grid": ("grid",), "grid-batch": ("grid-batch",),
            "grid-resume": ("grid-resume",), "compare": ("compare",),
            "research": ("research",), "inspect-run": ("inspect-run",),
            "replay": ("replay",), "assess-run": ("assess-run",), "gate-run": ("gate-run",),
            "signed-affine-dag-v1-merge": ("signed-affine-dag-v1", "demo", "merge"),
            "signed-affine-dag-v1-cancellation": ("signed-affine-dag-v1", "demo", "cancellation"),
            "signed-affine-relation-v1-merge": ("signed-affine-relation-v1", "demo", "merge"),
            "toy-relation-v1-collision": ("toy-relation-v1", "demo", "collision"),
            "toy-relation-v1-noninvertible": ("toy-relation-v1", "demo", "noninvertible"),
            "dag-scheduler-v1-mixed": ("dag-scheduler-v1", "demo", "mixed"),
            "dag-scheduler-v1-budget": ("dag-scheduler-v1", "demo", "budget"),
            "dag-scheduler-v1-cancelled": ("dag-scheduler-v1", "demo", "cancelled"),
            "dag-scheduler-v2-flow": ("dag-scheduler-v2", "demo", "flow"),
            "mods-list": ("mods", "list"), "mods-inspect": ("mods", "inspect"),
            "mods-verify": ("mods", "verify"), "mods-trust-status": ("mods", "trust", "status"),
        }
        self.assertEqual({entry.command_id: entry.argv for entry in catalog}, expected)
        for entry in catalog:
            self.assertFalse(entry.arguments["additionalProperties"])
            self.assertNotIn("output", entry.arguments["properties"])
            self.assertTrue(entry.synopsis)
            self.assertIn(entry.side_effect, ("compute", "read", "fixed-demo"))
            for source in entry.sources:
                self.assertTrue((self.path.parents[1] / source).is_file(), source)
            with self.assertRaises(TypeError):
                entry.arguments["properties"]["unsafe"] = {}

    def test_calculate_synopsis_covers_german_bounded_arithmetic_intents(self):
        entry = next(entry for entry in self.load_catalog(self.path)
                     if entry.command_id == "calculate")
        synopsis = entry.synopsis.casefold()
        # Require the supported meanings, without prescribing every synonym:
        # unnecessary words also change the real routing embedding.
        meanings = {
            "calculation": r"\b(?:berechne|rechne)\b",
            "addition": r"\b(?:plus|addier\w*|addition)\b",
            "subtraction": r"\b(?:minus|subtrahier\w*|subtraktion)\b",
            "multiplication": r"\b(?:mal|multiplizier\w*|multiplikation)\b",
            "division": r"\b(?:geteilt durch|dividier\w*|division)\b",
            "remainder": r"\brest\b",
            "power": r"\bpotenz(?:en)?\b",
            "modular power": r"\bmodular\w* potenz(?:en)?\b",
            "modular operands": r"\bbasis hoch exponent modulo (?:positiver|positiven) modulus\b",
            "exactness": r"\bexakt\w*\b",
            "integer domain": r"\b(?:ganzen(?: oder rationalen)? zahlen|ganzzahl\w*)\b",
            "rational domain": r"\brational\w*\b",
            "boundedness": r"\bbegrenzt\w*\b",
        }
        for meaning, pattern in meanings.items():
            with self.subTest(meaning=meaning):
                self.assertRegex(synopsis, pattern)
        self.assertLessEqual(len(entry.synopsis), 230)
        self.assertEqual(entry.sources, ("apps/bruce_cli.cpp",
                                         "tests/integration/test_exact_domains.py"))
        for source in entry.sources:
            self.assertTrue((self.path.parents[1] / source).is_file(), source)

    def test_routing_embedding_is_only_the_human_catalog_synopsis(self):
        for entry in self.load_catalog(self.path):
            with self.subTest(command=entry.command_id):
                self.assertEqual(entry.embedding_text, entry.synopsis)
                self.assertNotIn('"arguments":', entry.embedding_text)
                self.assertNotIn('"sources":', entry.embedding_text)
                self.assertNotIn(entry.syntax, entry.embedding_text)
                for source in entry.sources:
                    self.assertNotIn(source, entry.embedding_text)

    def test_catalog_loader_rejects_mutations_unknown_fields_and_malformed_schema(self):
        original = json.loads(self.path.read_text())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "catalog.json"
            changes = []
            for argv in (["mods", "install"], ["mods", "activate"], ["mods", "update"],
                         ["mods", "rollback"], ["mods", "trust", "activate"],
                         ["mods", "trust", "prepare"], ["release", "sign"],
                         ["rules-activate"], ["rules-status"], ["mcp"],
                         ["dag-scheduler-v3", "execute"]):
                case = copy.deepcopy(original)
                case["entries"][0]["argv"] = argv
                changes.append(case)
            for field, value in (("command_id", "mods-install"), ("side_effect", "write"),
                                 ("extra", "sensitive"), ("arguments", {})):
                case = copy.deepcopy(original)
                case["entries"][0][field] = value
                changes.append(case)
            case = copy.deepcopy(original)
            case["entries"][0]["arguments"]["properties"]["output"] = {"type": "string"}
            changes.extend([case, {**original, "schema": "v2"},
                            {**original, "entries": original["entries"] * 2}])
            for case in changes:
                path.write_text(json.dumps(case))
                with self.assertRaises(AssistantError) as caught:
                    self.load_catalog(path)
                self.assertEqual(caught.exception.code, "AssistantCatalogInvalid")
            for raw in ('{"schema":NaN}', '{"schema":"x","schema":"y"}', '{', '[]'):
                path.write_text(raw)
                with self.assertRaises(AssistantError):
                    self.load_catalog(path)


class IndexTests(unittest.TestCase):
    def setUp(self):
        from bruce_ai.catalog import load_catalog
        from bruce_ai import index
        self.index = index
        path = Path(__file__).resolve().parents[2] / "tools/bruce_ai_command_catalog_v1.json"
        self.catalog = load_catalog(path)
        self.profile = AssistantProfile("http://127.0.0.1:11434", "all-minilm", "qwen3.5", 5, 500, 1000)
        self.directory = tempfile.TemporaryDirectory(prefix="bruce-ai-index-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "index.json"

    def build(self, vector=(3, 4)):
        class Client:
            def embed(inner, text, profile):
                return list(vector)
        return self.index.build_index(self.catalog, self.profile, Client(), profile_name="local")

    def assert_code(self, code, operation):
        with self.assertRaises(AssistantError) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)
        self.assertEqual(str(caught.exception), code)

    def test_build_normalizes_and_embeds_only_catalog_text(self):
        owner = self
        embedded = []
        class Client:
            def embed(self, text, profile):
                embedded.append(text)
                owner.assertEqual(profile, owner.profile)
                return [3, 4]
        index = self.index.build_index(self.catalog, self.profile, Client(), profile_name="local")
        self.assertEqual(embedded, [entry.synopsis for entry in sorted(
            self.catalog, key=lambda entry: entry.command_id)])
        self.assertEqual(index.schema, "bruce-ai-command-index-v2")
        self.assertEqual(index.dimension, 2)
        self.assertEqual(index.embedding_model, "all-minilm")
        self.assertEqual(index.profile_name, "local")
        self.assertEqual(len(index.entries), 24)
        self.assertEqual(index.entries[0].vector, (0.6, 0.8))
        self.assertEqual(len(index.catalog_digest), 64)
        self.assertEqual(index.catalog_digest, self.index.catalog_digest(list(reversed(self.catalog))))
        changed = [replace(self.catalog[0], synopsis="Changed meaning"), *self.catalog[1:]]
        self.assertNotEqual(index.catalog_digest, self.index.catalog_digest(changed))

    def test_cosine_rank_normalizes_query_and_breaks_ties_by_id(self):
        index = self.build()
        ranked = self.index.rank(index, [6, 8], 3)
        self.assertEqual([entry.command_id for entry in ranked], ["assess-run", "calculate", "compare"])
        for entry in ranked:
            self.assertAlmostEqual(entry.score, 1.0)
        opposite = replace(index, entries=(replace(index.entries[0], vector=(-0.6, -0.8)),
                                          replace(index.entries[1], vector=(0.6, 0.8))))
        self.assertEqual(self.index.rank(opposite, [3, 4], 1)[0].command_id,
                         index.entries[1].command_id)

    def test_all_vectors_must_be_finite_nonzero_numeric_and_consistent(self):
        for vector in ([], [0, 0], [True, 1], ["3", 4], [math.nan, 1], [math.inf, 1],
                       [1, -math.inf], [10 ** 1000, 1], [1e308, math.nan]):
            self.assert_code("AssistantIndexInvalid", lambda: self.build(vector))
        for vector in ([], [0, 0], [True, 1], [1, "2"], [math.nan, 1], [1, math.inf]):
            self.assert_code("AssistantIndexInvalid", lambda: self.index.rank(self.build(), vector, 2))
        for vector in ([1e308, 1e308], [1e-308, 1e-308]):
            self.assertAlmostEqual(self.index.rank(self.build(vector), vector, 1)[0].score, 1.0)
        self.assert_code("AssistantIndexStale", lambda: self.index.rank(self.build(), [1], 1))
        for limit in (0, -1, True, 1.5):
            self.assert_code("AssistantIndexInvalid", lambda: self.index.rank(self.build(), [3, 4], limit))
        class MixedClient:
            count = 0
            def embed(self, text, profile):
                self.count += 1
                return [1, 0] if self.count == 1 else [1]
        self.assert_code("AssistantIndexInvalid", lambda: self.index.build_index(
            self.catalog, self.profile, MixedClient()))

    def test_load_rejects_stale_catalog_model_dimension_and_profile(self):
        self.index.write_index(self.path, self.build())
        index = self.index.load_index(self.path, self.catalog, self.profile, 2, profile_name="local")
        self.assertEqual(index, self.build())
        self.assert_code("AssistantIndexStale", lambda: self.index.load_index(
            self.path, self.catalog, replace(self.profile, embedding_model="other:tag")))
        self.assert_code("AssistantIndexStale", lambda: self.index.load_index(
            self.path, self.catalog, self.profile, 3))
        self.assert_code("AssistantIndexStale", lambda: self.index.load_index(
            self.path, self.catalog, self.profile, profile_name="other"))
        changed = [replace(self.catalog[0], synopsis="Changed meaning"), *self.catalog[1:]]
        self.assert_code("AssistantIndexStale", lambda: self.index.load_index(
            self.path, changed, self.profile))
        self.assert_code("AssistantIndexMissing", lambda: self.index.load_index(
            self.path.with_name("missing"), self.catalog, self.profile))

    def test_v1_index_is_stale_even_when_all_freshness_metadata_matches(self):
        index = self.build()
        legacy = replace(index, schema="bruce-ai-command-index-v1")
        self.assert_code("AssistantIndexStale", lambda: self.index.validate_index(
            legacy, self.catalog, self.profile, 2, profile_name="local"))
        self.path.write_text(json.dumps(legacy.to_dict()), encoding="utf-8")
        self.assert_code("AssistantIndexStale", lambda: self.index.load_index(
            self.path, self.catalog, self.profile, 2, profile_name="local"))

    def test_index_rejects_corruption_and_nonunit_stored_vectors(self):
        self.index.write_index(self.path, self.build())
        original = json.loads(self.path.read_text())
        cases = [[], {**original, "extra": True}, {**original, "dimension": True},
                 {**original, "entries": []}, {**original, "schema": "future"}]
        for vector in ([3, 4], [math.nan, 1], [True, 1], [1], [0, 0]):
            case = copy.deepcopy(original)
            case["entries"][0]["vector"] = vector
            cases.append(case)
        case = copy.deepcopy(original)
        case["entries"][1]["command_id"] = case["entries"][0]["command_id"]
        cases.append(case)
        case = copy.deepcopy(original)
        case["entries"][0]["command_id"] = "mods-install"
        cases.append(case)
        for document in cases:
            self.path.write_text(json.dumps(document))
            self.assert_code("AssistantIndexInvalid", lambda: self.index.load_index(
                self.path, self.catalog, self.profile))
        for raw in ('{"schema":1e999}', '{"schema":"x","schema":"y"}', '{'):
            self.path.write_text(raw)
            self.assert_code("AssistantIndexInvalid", lambda: self.index.load_index(
                self.path, self.catalog, self.profile))

    def assert_nonregular_index_rejected_promptly(self, path):
        # Isolate the actual open: a regression must fail within a bounded time,
        # rather than leave the test runner waiting for a FIFO writer forever.
        script = """
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from bruce_ai.catalog import load_catalog
from bruce_ai.config import AssistantError, AssistantProfile
from bruce_ai.index import load_index
catalog = load_catalog(Path(sys.argv[1]) / "bruce_ai_command_catalog_v1.json")
profile = AssistantProfile("http://127.0.0.1:11434", "all-minilm", "qwen3.5", 5, 500, 1000)
try:
    load_index(Path(sys.argv[2]), catalog, profile)
except AssistantError as error:
    print(str(error))
else:
    raise AssertionError("nonregular index was admitted")
"""
        tools = Path(__file__).resolve().parents[2] / "tools"
        try:
            result = subprocess.run([sys.executable, "-c", script, str(tools), str(path)],
                                    capture_output=True, text=True, timeout=3)
        except subprocess.TimeoutExpired:
            self.fail("load_index blocked while opening a nonregular index")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "AssistantIndexInvalid\n")
        self.assertEqual(result.stderr, "")

    @unittest.skipUnless(hasattr(os, "mkfifo"), "FIFOs unavailable")
    def test_load_rejects_fifo_without_waiting_for_a_writer(self):
        os.mkfifo(self.path)
        self.assert_nonregular_index_rejected_promptly(self.path)

    @unittest.skipUnless(hasattr(os, "mkfifo") and hasattr(os, "symlink"),
                         "FIFOs or symlinks unavailable")
    def test_load_rejects_symlink_to_fifo_without_waiting_for_a_writer(self):
        fifo = self.path.with_name("sensitive-fifo")
        os.mkfifo(fifo)
        self.path.symlink_to(fifo)
        self.assert_nonregular_index_rejected_promptly(self.path)

    @unittest.skipUnless(hasattr(os, "symlink"), "Symlinks unavailable")
    def test_load_rejects_symlink_even_to_valid_regular_index(self):
        self.index.write_index(self.path, self.build())
        link = self.path.with_name("sensitive-link")
        link.symlink_to(self.path)
        self.assert_code("AssistantIndexInvalid", lambda: self.index.load_index(
            link, self.catalog, self.profile))

    def test_load_fails_closed_when_secure_open_flags_are_unavailable(self):
        self.index.write_index(self.path, self.build())
        for flag in ("O_NOFOLLOW", "O_NONBLOCK"):
            with self.subTest(flag=flag), patch.object(os, flag, None, create=True):
                self.assert_code("AssistantIndexInvalid", lambda: self.index.load_index(
                    self.path, self.catalog, self.profile))

    def test_publication_is_atomic_exclusive_private_and_byte_deterministic(self):
        index = self.build()
        real_link = os.link
        def inspect_publication(source, destination, **kwargs):
            self.assertFalse(self.path.exists())
            parsed = self.index.load_index(Path(source), self.catalog, self.profile)
            self.assertEqual(parsed, index)
            return real_link(source, destination, **kwargs)
        with patch("bruce_ai.index.os.link", side_effect=inspect_publication):
            self.index.write_index(self.path, index)
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        before = self.path.read_bytes()
        self.assert_code("AssistantIndexExists", lambda: self.index.write_index(self.path, index))
        self.assertEqual(self.path.read_bytes(), before)
        other = self.path.with_name("other.json")
        self.index.write_index(other, self.build())
        self.assertEqual(other.read_bytes(), before)
        failed = self.path.with_name("failed.json")
        with patch("bruce_ai.index.os.fsync", side_effect=OSError("sensitive")):
            self.assert_code("AssistantIndexWriteFailed", lambda: self.index.write_index(failed, index))
        self.assertFalse(failed.exists())
        with patch("bruce_ai.index.os.link", side_effect=OSError("sensitive")):
            self.assert_code("AssistantIndexWriteFailed", lambda: self.index.write_index(failed, index))
        self.assertEqual(sorted(path.name for path in self.path.parent.iterdir()), ["index.json", "other.json"])
        target = self.path.with_name("target")
        target.write_text("unchanged")
        symlink = self.path.with_name("link")
        symlink.symlink_to(target)
        self.assert_code("AssistantIndexExists", lambda: self.index.write_index(symlink, index))
        self.assertEqual(target.read_text(), "unchanged")


class ProposalTests(unittest.TestCase):
    def setUp(self):
        from bruce_ai.catalog import load_catalog
        from bruce_ai import proposal
        self.proposal = proposal
        self.catalog = load_catalog(Path(__file__).resolve().parents[2] / "tools/bruce_ai_command_catalog_v1.json")
        self.directory = tempfile.TemporaryDirectory(prefix="bruce-ai-proposal-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.data = self.root / "data"
        self.data.mkdir()
        self.file = self.data / "train.csv"
        self.file.write_text("x,y\n1,2\n")
        from bruce_ai.config import AssistantConfig
        self.config = AssistantConfig("bruce-local-ai-config-v1", "default", self.root / "bruce",
                                      self.data / "index.json", (self.data,), True, {})

    def document(self, command="calculate", arguments=None, **changes):
        return {"schema": "bruce-local-ai-proposal-v1", "kind": "proposal", "command_id": command,
                "arguments": {"op": "add", "left": "1", "right": "2"} if arguments is None else arguments,
                "missing_inputs": [], "reason": "Bounded calculation", **changes}

    def parse(self, command="calculate", arguments=None, **changes):
        return self.proposal.parse_proposal(self.document(command, arguments, **changes), self.catalog)

    def argv(self, command="calculate", arguments=None, **changes):
        return self.proposal.canonical_argv(self.parse(command, arguments, **changes), self.config)

    def assert_code(self, code, operation):
        with self.assertRaises(AssistantError) as caught:
            operation()
        self.assertEqual(str(caught.exception), code)

    def test_canonical_order_and_ignored_extras_cannot_control_execution(self):
        args = {"right": "128", "modulus": "13", "op": "modpow", "left": "7",
                "output": "/tmp/stolen", "argv": ["/bin/sh"], "env": {"LD_PRELOAD": "evil"}}
        proposal = self.parse(arguments=args, executable="/bin/sh", cwd="/", shell=True)
        self.assertEqual(self.proposal.canonical_argv(proposal, self.config),
                         [str(self.config.bruce_program), "calculate", "--op", "modpow", "--left", "7",
                          "--right", "128", "--modulus", "13"])
        self.assertNotIn("output", proposal.arguments)
        with self.assertRaises(TypeError):
            proposal.arguments["left"] = "evil"
        args["left"] = "999"
        self.assertEqual(proposal.arguments["left"], "7")

    def test_candidate_response_schema_requires_real_modpow_and_reason(self):
        calculate = next(entry for entry in self.catalog if entry.command_id == "calculate")
        schema = self.proposal.response_schema_for_candidates([calculate])
        self.assertEqual(len(schema["oneOf"]), 2)
        proposal_branch = next(branch for branch in schema["oneOf"]
                               if branch["properties"]["kind"]["enum"] == ["proposal"])
        self.assertEqual(proposal_branch["properties"]["arguments"]["properties"]["op"]["enum"][-1],
                         "modpow")
        self.assertEqual(proposal_branch["properties"]["arguments"]["required"], ["op", "left", "right"])
        self.assertEqual(proposal_branch["properties"]["missing_inputs"]["maxItems"], 0)
        self.assertIn("reason", proposal_branch["required"])
        needs_branch = next(branch for branch in schema["oneOf"]
                            if branch["properties"]["kind"]["enum"] == ["needs_input"])
        self.assertEqual(needs_branch["properties"]["arguments"]["required"], [])
        self.assertEqual(needs_branch["properties"]["missing_inputs"]["minItems"], 1)

    def test_forms_and_missing_inputs_never_produce_argv(self):
        for proposal in (self.parse("research", {}, kind="needs_input", missing_inputs=["train_csv"]),
                         self.parse("research", {}), self.parse("coordinate", {"x": "1"}),
                         self.parse("grid", {"path": "1,1", "mode": "modular"}),
                         self.parse(arguments={"op": "modpow", "left": "1", "right": "2"})):
            self.assertEqual(proposal.kind, "needs_input")
            self.assertTrue(proposal.missing_inputs)
            self.assert_code("AssistantInputRequired", lambda: self.proposal.canonical_argv(proposal, self.config))
        for command in ("mods-install", "mods-activate", "mods-update", "mods-rollback", "mods-trust-activate",
                        "release-sign", "rules-activate", "/bin/sh", "calculate; touch /tmp/owned"):
            self.assert_code("AssistantProposalInvalid", lambda: self.parse(command, {}))

    def test_needs_input_with_complete_rational_arguments_still_blocks_argv(self):
        proposal = self.parse(arguments={"op": "add", "domain": "rational",
                                         "left": "1/3", "right": "1/6"},
                              kind="needs_input", missing_inputs=[])
        self.assertEqual(proposal.kind, "needs_input")
        self.assertEqual(proposal.missing_inputs, ("input",))
        self.assert_code("AssistantInputRequired",
                         lambda: self.proposal.canonical_argv(proposal, self.config))

    def test_shape_types_enums_ranges_and_shell_injection_are_rejected(self):
        for update in ({"schema": "future"}, {"kind": "run"}, {"arguments": []},
                       {"reason": 42}, {"missing_inputs": "train"}, {"missing_inputs": [False]}):
            self.assert_code("AssistantProposalInvalid", lambda: self.parse(**update))
        document = self.document()
        del document["arguments"]
        self.assert_code("AssistantProposalInvalid", lambda: self.proposal.parse_proposal(document, self.catalog))
        for args in ({"op": "exec", "left": "1", "right": "2"},
                     {"op": "add", "left": "1;touch /tmp/owned", "right": "2"},
                     {"op": "add", "left": "$(id)", "right": "2"},
                     {"op": "add", "left": "1\n", "right": "2"},
                     {"op": "add", "left": 1, "right": "2"},
                     {"op": "add", "left": "1", "right": "2", "max_bits": True},
                     {"op": "add", "left": "1", "right": "2", "max_bits": 1.0},
                     {"op": "add", "left": "1", "right": "2", "max_bits": 0},
                     {"op": "add", "left": "1", "right": "2", "max_bits": 16385},
                     {"op": "add", "left": "1", "right": "2", "domain": "int64"},
                     {"op": "add", "left": "1/2", "right": "2"},
                     {"op": "add", "left": "1", "right": "2", "modulus": "3"},
                     {"op": "modpow", "left": "1", "right": "2", "modulus": "0"},
                     {"op": "modpow", "left": "1", "right": "2", "modulus": "3", "domain": "rational"}):
            self.assert_code("AssistantProposalInvalid", lambda: self.parse(arguments=args))

    def test_coordinate_and_grid_source_bounds_and_cross_field_rules(self):
        for args in ({"z": "1", "x": "0", "y": "0"}, {"z": "18446744073709551616"},
                     {"x": "6074000999", "y": "6074000999"}, {"x": "0", "y": "6074000999"}):
            self.assert_code("AssistantProposalInvalid", lambda: self.parse("coordinate", args))
        self.assertEqual(self.argv("coordinate", {"y": "2", "x": "1"})[1:],
                         ["coordinate", "--x", "1", "--y", "2"])
        self.assertEqual(self.argv("coordinate", {"z": "18446744073709551615"})[-1], "18446744073709551615")
        for command, field in (("grid", "path"), ("grid-batch", "paths")):
            for args in ({field: "8,0"}, {field: "/".join(["0,0"] * 65)},
                         {field: "0,0", "stage": 0}, {field: "0,0", "modulus": "2"},
                         {field: "0,0", "mode": "modular", "modulus": "18446744073709551616"},
                         {field: "0,0", "rule": "scaled-exponential-v1", "stage": 58}):
                self.assert_code("AssistantProposalInvalid", lambda: self.parse(command, args))
        self.assertEqual(self.argv("grid-batch", {"paths": "0,0;1,2", "stage": 0,
                                                  "rule": "scaled-exponential-v1"})[1:],
                         ["grid-batch", "--paths", "0,0;1,2", "--rule", "scaled-exponential-v1", "--stage", "0"])

    def test_research_exact_domain_controls_match_bruce_contract(self):
        for name in ("max_bits", "max_denominator", "max_trace_bytes"):
            for domain in (None, "int64"):
                args = {"train": "train.csv", name: 8}
                if domain:
                    args["domain"] = domain
                self.assert_code("AssistantProposalInvalid", lambda: self.parse("research", args))
        for domain in ("bigint", "rational"):
            self.assertIn("--max-trace-bytes", self.argv("research", {
                "train": "train.csv", "domain": domain, "max_bits": 8,
                "max_denominator": 1, "max_trace_bytes": 256}))
        self.assertIn("--max-denominator", self.argv("research", {
            "train": "train.csv", "domain": "rational", "max_denominator": 16}))
        for args in (
                {"train": "train.csv", "domain": "bigint", "max_denominator": 2},
                {"train": "train.csv", "domain": "rational", "max_denominator": 17},
                {"train": "train.csv", "domain": "bigint", "max_trace_bytes": 255},
                {"train": "train.csv", "domain": "bigint", "max_trace": 1},
                {"train": "train.csv", "domain": "int64", "min": "-1000001"},
                {"train": "train.csv", "domain": "int64", "max": "1000001"}):
            self.assert_code("AssistantProposalInvalid", lambda args=args: self.parse("research", args))
        self.assert_code("AssistantProposalInvalid", lambda: self.parse("research", {
            "train": "train.csv", "min": "9", "max": "8"}))

    def test_existing_inputs_resolve_beneath_roots_and_output_extras_disappear(self):
        self.assertEqual(self.argv("research", {"train": "train.csv", "output": "/tmp/injected"}),
                         [str(self.config.bruce_program), "research", "--train", str(self.file)])
        for command in ("inspect-run", "replay", "assess-run", "gate-run"):
            self.assertEqual(self.argv(command, {"run": str(self.file)})[1:], [command, str(self.file)])
        self.assertEqual(self.argv("mods-verify", {"package": "train.csv", "trust_store": "train.csv"})[1:],
                         ["mods", "verify", str(self.file), "--trust-store", str(self.file)])
        self.assertEqual(self.argv("mods-trust-status", {"mod_root": str(self.data)})[1:],
                         ["mods", "trust", "status", "--mod-root", str(self.data)])
        for entry in self.catalog:
            if entry.side_effect == "fixed-demo":
                self.assertEqual(self.argv(entry.command_id, {})[1:], list(entry.argv))
        outside = self.root / "secret.csv"
        outside.write_text("secret")
        (self.data / "escape").symlink_to(outside)
        for name in ("../secret.csv", str(outside), "escape", "train.csv;id", "$(id)", "train.csv\n"):
            self.assert_code("UnsafeAssistantPath", lambda: self.argv("research", {"train": name}))
        self.assert_code("AssistantInputRequired", lambda: self.argv("research", {"train": "missing.csv"}))
        self.assert_code("AssistantInputRequired", lambda: self.argv("research", {"train": str(self.data)}))
        if hasattr(os, "mkfifo"):
            os.mkfifo(self.data / "fifo")
            self.assert_code("AssistantInputRequired", lambda: self.argv("research", {"train": "fifo"}))

    def test_absolute_alias_outside_root_cannot_enter_an_allowed_root(self):
        alias = self.root / "external-alias"
        alias.symlink_to(self.file)
        self.assert_code("UnsafeAssistantPath", lambda: self.argv("research", {"train": str(alias)}))

    def test_decimal_spelling_limits_do_not_escape_as_python_exceptions(self):
        with self.subTest("leading_zero_grid"):
            self.assertEqual(self.argv("grid-batch", {"paths": "0" * 5000 + ",0"})[-1], "0" * 5000 + ",0")
        for args in ({"op": "pow", "left": "2", "right": "-1"},
                     {"op": "pow", "left": "2", "right": "18446744073709551616"},
                     {"op": "pow", "left": "2", "right": "1/2", "domain": "rational"},
                     {"op": "divmod", "left": "2", "right": "1", "domain": "rational"},
                     {"op": "add", "left": "1/0", "right": "1", "domain": "rational"}):
            with self.subTest(arguments=args):
                self.assert_code("AssistantProposalInvalid", lambda: self.parse(arguments=args))
        for args in ({"train": "train.csv", "min": "9"}, {"train": "train.csv", "max": "-9"},
                     {"train": "train.csv", "min": "-9223372036854775809"},
                     {"train": "train.csv", "max": "9223372036854775808"}):
            with self.subTest(arguments=args):
                self.assert_code("AssistantProposalInvalid", lambda: self.parse("research", args))


class ExecutionTests(unittest.TestCase):
    def setUp(self):
        from bruce_ai import executor
        self.executor = executor
        self.directory = tempfile.TemporaryDirectory(prefix="bruce-ai-execution-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.program = self.root / "fake-bruce"
        self.record = self.root / "argv.jsonl"
        self.program.write_text("#!" + sys.executable + "\n" + '''
import json, os, pathlib, sys, time
with pathlib.Path(__file__).with_name("argv.jsonl").open("a") as record:
    record.write(json.dumps({"argv": sys.argv[1:], "env": dict(os.environ)}) + "\\n")
mode = sys.argv[1]
if mode == "timeout":
    os.write(1, b'{"partial":')
    os.write(2, b'waiting')
    time.sleep(10)
elif mode == "stdout-limit":
    while True:
        os.write(1, b'x' * 4096)
elif mode == "stderr-limit":
    os.write(1, b'{"ok":true}')
    while True:
        os.write(2, b'e' * 4096)
elif mode == "invalid":
    os.write(1, b'not json\\xff')
elif mode == "duplicate":
    os.write(1, b'{"x":1,"x":2}')
elif mode == "nonfinite":
    os.write(1, b'{"x":1e999}')
elif mode == "empty":
    pass
elif mode == "array":
    os.write(1, b'[]')
else:
    os.write(1, b' {"answer":"3"}\\n')
    os.write(2, b'raw warning\\n')
    if mode == "exit2":
        sys.exit(2)
''')
        self.program.chmod(0o700)

    def execute(self, mode="ok", confirmation="run", timeout_ms=1000):
        return self.executor.execute_confirmed([str(self.program), mode], confirmation, timeout_ms)

    def test_preview_and_every_confirmation_other_than_exact_run_start_no_process(self):
        for confirmation in ("", "Run", "RUN", "run\n", " run", "run ", "yes", None, True):
            with self.assertRaises(AssistantError) as caught:
                self.execute(confirmation=confirmation)
            self.assertEqual(caught.exception.code, "AssistantConfirmationRejected")
        self.assertFalse(self.record.exists())

    def test_confirmed_direct_argv_is_started_once_and_environment_is_fixed(self):
        marker = self.root / "owned"
        argv = [str(self.program), "ok", f";touch {marker}", "$(id)", "space in one argument"]
        with patch.dict(os.environ, {"BRUCE_ATTACK": "sensitive", "LD_PRELOAD": "/missing"}):
            result = self.executor.execute_confirmed(argv, "run", 1000)
        records = [json.loads(row) for row in self.record.read_text().splitlines()]
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["argv"], argv[1:])
        self.assertNotIn("BRUCE_ATTACK", records[0]["env"])
        self.assertNotIn("LD_PRELOAD", records[0]["env"])
        self.assertFalse(marker.exists())
        self.assertEqual(result.argv, tuple(argv))
        self.assertEqual(result.status, "completed")
        self.assertEqual(result.exit_code, 0)
        self.assertEqual(result.raw_stdout, b' {"answer":"3"}\n')
        self.assertEqual(result.raw_stderr, b'raw warning\n')
        self.assertEqual(result.result, {"answer": "3"})
        self.assertEqual(result.parse_state, "valid_json")
        self.assertEqual(result.source_result_digest, hashlib.sha256(b' {"answer":"3"}\n').hexdigest())

    def test_exit_2_keeps_raw_results_and_invalid_json_has_its_own_state(self):
        result = self.execute("exit2")
        self.assertEqual(result.exit_code, 2)
        self.assertEqual(result.status, "completed")
        self.assertEqual(result.result, {"answer": "3"})
        for mode in ("invalid", "duplicate", "nonfinite", "array"):
            result = self.execute(mode)
            self.assertEqual(result.status, "completed")
            self.assertEqual(result.exit_code, 0)
            self.assertEqual(result.parse_state, "invalid_json")
            self.assertIsNone(result.result)
            self.assertTrue(result.raw_stdout)
        self.assertEqual(self.execute("invalid").raw_stdout, b"not json\xff")
        self.assertEqual(self.execute("empty").parse_state, "empty")

    def test_timeout_and_stream_limits_are_bounded_and_keep_retained_bytes(self):
        started = time.monotonic()
        result = self.execute("timeout", timeout_ms=60)
        self.assertLess(time.monotonic() - started, 2)
        self.assertEqual(result.status, "timeout")
        self.assertIsNotNone(result.exit_code)
        self.assertEqual(result.raw_stdout, b'{"partial":')
        self.assertEqual(result.raw_stderr, b"waiting")
        self.assertEqual(result.parse_state, "incomplete")
        self.assertIsNone(result.result)
        for mode in ("stdout-limit", "stderr-limit"):
            started = time.monotonic()
            result = self.execute(mode)
            self.assertLess(time.monotonic() - started, 2)
            self.assertEqual(result.status, "output_limit")
            self.assertEqual(result.parse_state, "incomplete")
            self.assertLessEqual(len(result.raw_stdout), 1024 * 1024)
            self.assertLessEqual(len(result.raw_stderr), 1024 * 1024)
            self.assertTrue(result.stdout_truncated or result.stderr_truncated)
            self.assertEqual(result.source_result_digest, hashlib.sha256(result.raw_stdout).hexdigest())

    def test_bad_execution_parameters_and_spawn_failure_are_redacted(self):
        for timeout in (0, -1, True, 0.5):
            with self.assertRaises(AssistantError) as caught:
                self.execute(timeout_ms=timeout)
            self.assertEqual(str(caught.exception), "AssistantExecutionInvalid")
        for argv in ([], ["relative-program"], [str(self.program), None], [str(self.program), "x\x00y"]):
            with self.assertRaises(AssistantError) as caught:
                self.executor.execute_confirmed(argv, "run", 1000)
            self.assertEqual(str(caught.exception), "AssistantExecutionInvalid")
        with self.assertRaises(AssistantError) as caught:
            self.executor.execute_confirmed([str(self.root / "sensitive-missing")], "run", 1000)
        self.assertEqual(str(caught.exception), "AssistantExecutionFailed")
        self.assertFalse(self.record.exists())


class ResponseTests(unittest.TestCase):
    def setUp(self):
        self.raw = b'{ "answer": "3", "status": "bounded" }\n'
        self.result = json.loads(self.raw)
        self.digest = hashlib.sha256(self.raw).hexdigest()
        self.document = {"schema": "bruce-local-ai-answer-v1", "summary": "Three.",
                         "status": "explanation", "caveats": ["Bounded result."],
                         "source_result_digest": self.digest}
        self.profile = AssistantProfile("http://127.0.0.1:11434", "embed", "chat", 2, 100, 100)

    def render(self, document=None, failure=None):
        from bruce_ai.response import render_answer
        owner = self

        class Model:
            def chat_json(self, messages, profile):
                owner.messages = messages
                if failure:
                    if isinstance(failure, Exception):
                        raise failure
                    raise AssistantError(failure)
                return copy.deepcopy(owner.document if document is None else document)

        return render_answer(self.result, Model(), profile=self.profile,
                             raw_stdout=self.raw).to_dict()

    def test_answer_keeps_raw_digest_and_marks_all_model_content_untrusted(self):
        answer = self.render()
        self.assertEqual(answer["schema"], "bruce-local-ai-answer-v1")
        self.assertEqual(answer["source_result_digest"], self.digest)
        self.assertEqual(answer["status"], "untrusted_model_explanation")
        self.assertEqual(answer["model"], "chat")
        self.assertEqual(answer["summary"], "Three.")
        self.assertIn("Bounded result.", answer["caveats"])
        self.assertTrue(answer["caveats"])
        self.assertEqual(answer["explanation_status"], "completed")
        payload = json.loads(self.messages[-1]["content"])
        self.assertEqual(payload["bruce_result"], self.result)
        self.assertEqual(payload["source_result_digest"], self.digest)
        self.assertEqual(self.result, {"answer": "3", "status": "bounded"})

    def test_malformed_or_forged_explanation_is_bounded_failure(self):
        cases = [{}, {**self.document, "summary": 2}, {**self.document, "caveats": "proof"},
                 {**self.document, "source_result_digest": "0" * 64},
                 {**self.document, "status": "verified"},
                 {**self.document, "summary": "sensitive" * 10000},
                 {**self.document, "extra": "sensitive"}]
        for case in cases:
            with self.subTest(case=list(case)):
                answer = self.render(case)
                self.assertEqual(answer["explanation_status"], "failed")
                self.assertEqual(answer["error"], "AssistantExplanationInvalid")
                self.assertEqual(answer["source_result_digest"], self.digest)
                self.assertNotIn("sensitive", json.dumps(answer))

    def test_timeout_retains_source_digest_and_no_model_claim(self):
        answer = self.render(failure="OllamaTimeout")
        self.assertEqual(answer["explanation_status"], "failed")
        self.assertEqual(answer["error"], "OllamaTimeout")
        self.assertEqual(answer["summary"], "")
        self.assertEqual(answer["source_result_digest"], self.digest)

    def test_unexpected_model_failure_cannot_escape_or_leak_exception_text(self):
        answer = self.render(failure=RuntimeError("sensitive model exception"))
        self.assertEqual(answer["explanation_status"], "failed")
        self.assertEqual(answer["error"], "AssistantExplanationInvalid")
        self.assertEqual(answer["source_result_digest"], self.digest)
        self.assertNotIn("sensitive", json.dumps(answer))

    def test_transcript_publication_is_atomic_and_failed_link_leaves_no_artifact(self):
        from bruce_ai.response import write_transcript
        with tempfile.TemporaryDirectory(prefix="bruce-ai-transcript-") as directory:
            target = Path(directory) / "result.json"
            document = {"schema": "bruce-local-ai-transcript-v1", "question": "one"}
            original_link = os.link

            def publish(source, destination, **kwargs):
                self.assertFalse(target.exists())
                self.assertEqual(json.loads(Path(source).read_bytes()), document)
                self.assertEqual(Path(source).stat().st_mode & 0o777, 0o600)
                return original_link(source, destination, **kwargs)

            with patch("bruce_ai.response.os.link", side_effect=publish):
                write_transcript(target, document)
            self.assertEqual(json.loads(target.read_text()), document)
            failed = Path(directory) / "failed.json"
            with patch("bruce_ai.response.os.link", side_effect=OSError("sensitive")):
                with self.assertRaises(AssistantError) as caught:
                    write_transcript(failed, document)
            self.assertEqual(str(caught.exception), "AssistantTranscriptWriteFailed")
            self.assertEqual(sorted(path.name for path in Path(directory).iterdir()), ["result.json"])
            alias = Path(directory) / "alias.json"
            alias.symlink_to(target)
            with self.assertRaises(AssistantError) as caught:
                write_transcript(alias, document)
            self.assertEqual(str(caught.exception), "AssistantTranscriptExists")
            self.assertTrue(alias.is_symlink())


class CLITests(unittest.TestCase):
    def setUp(self):
        ConfigTests.setUp(self)
        self.cli = Path(__file__).resolve().parents[2] / "tools/bruce_ai.py"
        self.record = self.root / "executed.json"
        self.raw = b'{ "answer": "3", "status": "bounded" }\n'
        self.program.write_text(
            f"#!{sys.executable}\nimport pathlib,sys,json\n"
            f"pathlib.Path({str(self.record)!r}).write_text(json.dumps(sys.argv[1:]))\n"
            f"sys.stdout.buffer.write({self.raw!r})\n", encoding="utf-8")
        self.requests = []
        self.answer_mode = "ok"
        self.explanation_started = threading.Event()
        self.release_explanation = threading.Event()
        self.addCleanup(self.release_explanation.set)
        self.proposal = {"schema": "bruce-local-ai-proposal-v1", "kind": "proposal",
                         "command_id": "calculate", "arguments": {"op": "add", "left": "1", "right": "2"},
                         "missing_inputs": [], "reason": "Add."}
        owner = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                document = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                owner.requests.append((self.path, document))
                if self.path == "/api/show":
                    response = {"details": {"format": "gguf"},
                                "model_info": {"general.architecture": "bert"}}
                elif self.path == "/api/embed":
                    response = {"embeddings": [[1, 0]]}
                else:
                    payload = json.loads(document["messages"][-1]["content"])
                    if "bruce_result" not in payload:
                        content = owner.proposal
                    else:
                        if owner.answer_mode == "interrupt":
                            owner.explanation_started.set()
                            owner.release_explanation.wait(3)
                        if owner.answer_mode == "timeout":
                            time.sleep(0.4)
                        content = {"schema": "bruce-local-ai-answer-v1", "summary": "Three.",
                                   "status": "explanation", "caveats": ["Bounded."],
                                   "source_result_digest": payload["source_result_digest"]}
                        if owner.answer_mode == "invalid":
                            content = {"secret": "sensitive"}
                    response = {"message": {"role": "assistant", "content": json.dumps(content)},
                                "done": True, "done_reason": "stop"}
                raw = json.dumps(response).encode()
                try:
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(raw)))
                    self.end_headers()
                    self.wfile.write(raw)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def log_message(self, *args):
                pass

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)
        self.document["profiles"]["default"]["ollama_url"] = f"http://127.0.0.1:{self.server.server_port}"
        self.document["profiles"]["default"]["request_timeout_ms"] = 150
        self.path.write_text(json.dumps(self.document))

    def run_cli(self, *arguments, confirmation="", config=True):
        command = [sys.executable, str(self.cli), *arguments]
        if config:
            command += ["--config", str(self.path)]
        result = subprocess.run(command, input=confirmation, text=True, capture_output=True, timeout=5)
        # Every emitted line must be a JSON object, even on parser failures.
        for stream in (result.stdout, result.stderr):
            for line in stream.splitlines():
                self.assertIsInstance(json.loads(line), dict)
        return result

    def build(self):
        result = self.run_cli("index", "build")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_config_check_only_metadata_and_index_build_is_explicit_exclusive(self):
        result = self.run_cli("config", "check")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([path for path, _ in self.requests], ["/api/show", "/api/show"])
        self.assertFalse((self.data / "index.json").exists())
        self.build()
        before = (self.data / "index.json").read_bytes()
        result = self.run_cli("index", "build")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantIndexExists")
        self.assertEqual((self.data / "index.json").read_bytes(), before)
        self.assertFalse(self.record.exists())

    def test_ask_preview_is_flushed_before_confirmation_and_exact_argv_runs_once(self):
        self.build()
        process = subprocess.Popen([sys.executable, str(self.cli), "ask", "--question", "one plus two",
                                    "--config", str(self.path)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        preview = json.loads(process.stdout.readline())
        self.assertEqual(preview["schema"], "bruce-local-ai-preview-v1")
        self.assertEqual(preview["side_effect"], "compute")
        self.assertEqual(preview["argv"], [str(self.program), "calculate", "--op", "add", "--left", "1", "--right", "2"])
        self.assertFalse(self.record.exists())
        stdout, stderr = process.communicate("run\n", timeout=5)
        self.assertEqual(process.returncode, 0, stderr)
        result = json.loads(stdout)
        self.assertEqual(json.loads(self.record.read_text()), preview["argv"][1:])
        self.assertEqual(result["bruce"]["result"], {"answer": "3", "status": "bounded"})
        self.assertEqual(base64.b64decode(result["bruce"]["raw_stdout_base64"]), self.raw)
        self.assertEqual(result["bruce"]["source_result_digest"], hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(result["answer"]["status"], "untrusted_model_explanation")
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ["bruce", "config.json", "data", "executed.json"])
        self.assertEqual(sorted(p.name for p in self.data.iterdir()), ["index.json"])

    def test_all_nonliteral_confirmations_and_missing_input_never_launch(self):
        self.build()
        for text in ("", "yes\n", "RUN\n", " run\n", "run \n", "run\r\n"):
            with self.subTest(text=text):
                result = self.run_cli("ask", "--question", "add", confirmation=text)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(json.loads(result.stderr)["error"], "AssistantConfirmationRejected")
                self.assertFalse(self.record.exists())
        self.proposal["arguments"] = {}
        result = self.run_cli("ask", "--question", "add", confirmation="run\n")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stdout)["argv"], [])
        self.assertFalse(self.record.exists())

    def test_proposal_prompt_defines_complete_and_missing_input_kinds(self):
        self.build()
        self.proposal["arguments"] = {"op": "add", "domain": "rational",
                                      "left": "1/3", "right": "1/6"}
        result = self.run_cli("ask", "--question",
                              "Berechne 1/3 plus 1/6 exakt als rationale Zahl.")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantConfirmationRejected")
        request = next(document for route, document in self.requests if route == "/api/chat")
        prompt = request["messages"][0]["content"]
        self.assertEqual(request["messages"][0]["role"], "system")
        for contract in (
                'Use kind="proposal" and missing_inputs=[] when all required values are supplied.',
                'Use kind="needs_input" only when at least one required named field is missing.',
                'For needs_input, command_id must still name the selected candidate command',
                'missing_inputs must list the actual missing field names, never an empty list.',
                'requires op="modpow" and its positive modulus',
                'domain="bigint" with max_denominator other than 1',
                'Always include the required short reason field.'):
            with self.subTest(contract=contract):
                self.assertIn(contract, prompt)
        self.assertFalse(self.record.exists())

    def test_modpow_uses_candidate_schema_and_runs_with_exact_token(self):
        self.build()
        self.proposal["arguments"] = {"op": "modpow", "left": "2", "right": "10",
                                      "modulus": "1000", "domain": "bigint"}
        self.proposal["reason"] = "Modular power."
        result = self.run_cli("ask", "--question", "Berechne 2 hoch 10 modulo 1000 exakt.",
                              confirmation="run\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.record.read_text()),
                         ["calculate", "--op", "modpow", "--left", "2", "--right", "10",
                          "--domain", "bigint", "--modulus", "1000"])
        request = next(document for route, document in self.requests if route == "/api/chat")
        self.assertIsInstance(request["format"], dict)
        calculate = next(branch for branch in request["format"]["oneOf"]
                         if branch["properties"]["command_id"]["enum"] == ["calculate"]
                         and branch["properties"]["kind"]["enum"] == ["proposal"])
        self.assertIn("modpow", calculate["properties"]["arguments"]["properties"]["op"]["enum"])
        self.assertIn("reason", calculate["required"])

    def test_needs_input_with_complete_rational_arguments_never_launches(self):
        self.build()
        self.proposal.update(kind="needs_input", missing_inputs=[], arguments={
            "op": "add", "domain": "rational", "left": "1/3", "right": "1/6"})
        result = self.run_cli("ask", "--question",
                              "Berechne 1/3 plus 1/6 exakt als rationale Zahl.", confirmation="run\n")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantInputRequired")
        self.assertEqual(json.loads(result.stdout)["argv"], [])
        self.assertEqual(json.loads(result.stdout)["missing_inputs"], ["input"])
        self.assertFalse(self.record.exists())
        self.assertEqual(sum(route == "/api/chat" for route, _ in self.requests), 1)

    def test_missing_in_root_file_preview_names_field_and_blocks_execution(self):
        self.document["profiles"]["default"]["top_k"] = 64
        self.path.write_text(json.dumps(self.document))
        self.build()
        self.proposal.update(command_id="research", reason="Research the supplied CSV.")
        for path in ("missing.csv", str(self.data / "missing.csv")):
            with self.subTest(path=path):
                self.requests.clear()
                self.proposal["arguments"] = {"train": path}
                result = self.run_cli("ask", "--question", "research", confirmation="run\n")
                self.assertEqual(result.returncode, 2)
                self.assertEqual(json.loads(result.stderr), {"error": "AssistantInputRequired"})
                preview = json.loads(result.stdout)
                self.assertEqual(preview["schema"], "bruce-local-ai-preview-v1")
                self.assertEqual(preview["argv"], [])
                self.assertFalse(self.record.exists())
                self.assertFalse((self.data / "missing.csv").exists())
                self.assertEqual(sum(route == "/api/chat" for route, _ in self.requests), 1)
                self.assertEqual(preview["missing_inputs"], ["train"])

    def test_explain_reads_exact_existing_json_without_index_or_bruce_execution(self):
        source = self.data / "result.json"
        source.write_bytes(self.raw)
        result = self.run_cli("explain", "--input", str(source))
        self.assertEqual(result.returncode, 0, result.stderr)
        output = json.loads(result.stdout)
        self.assertEqual(output["bruce"]["source_result_digest"], hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(output["bruce"]["result"], json.loads(self.raw))
        self.assertFalse(self.record.exists())
        self.assertEqual([path for path, _ in self.requests], ["/api/show", "/api/chat"])
        for raw in (b'{"x":1,"x":2}', b'[]', b'{"x":NaN}', b'\xff'):
            source.write_bytes(raw)
            result = self.run_cli("explain", "--input", str(source))
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stderr)["error"], "AssistantResultInvalid")
        self.assertFalse(self.record.exists())

    def test_response_failure_does_not_hide_successful_bruce_or_log_automatically(self):
        self.build()
        for mode, error in (("timeout", "OllamaTimeout"), ("invalid", "AssistantExplanationInvalid")):
            self.answer_mode = mode
            result = self.run_cli("ask", "--question", "add", confirmation="run\n")
            self.assertEqual(result.returncode, 2)
            output = json.loads(result.stdout.splitlines()[-1])
            self.assertEqual(output["bruce"]["exit_code"], 0)
            self.assertEqual(output["bruce"]["result"]["answer"], "3")
            self.assertEqual(output["answer"]["explanation_status"], "failed")
            self.assertEqual(output["answer"]["error"], error)
            self.assertNotIn("sensitive", result.stdout + result.stderr)
        self.assertEqual(sorted(p.name for p in self.data.iterdir()), ["index.json"])

    def test_interrupted_explanation_emits_completed_bruce_and_opt_in_transcript(self):
        self.build()
        self.answer_mode = "interrupt"
        self.document["profiles"]["default"]["request_timeout_ms"] = 2000
        self.path.write_text(json.dumps(self.document))
        target = self.data / "transcript.json"
        process = subprocess.Popen(
            [sys.executable, str(self.cli), "ask", "--question", "add",
             "--config", str(self.path), "--transcript-out", str(target)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        preview = json.loads(process.stdout.readline())
        process.stdin.write("run\n")
        process.stdin.flush()
        self.assertTrue(self.explanation_started.wait(3), "explanation did not start")
        process.send_signal(signal.SIGINT)
        stdout, stderr = process.communicate(timeout=5)
        self.release_explanation.set()
        self.assertEqual(process.returncode, 2)
        records = [json.loads(line) for line in stdout.splitlines()]
        self.assertEqual(len(records), 1, "completed BRUCE result must still be emitted")
        output = records[0]
        self.assertEqual(preview["schema"], "bruce-local-ai-preview-v1")
        self.assertEqual(output["schema"], "bruce-local-ai-result-v1")
        self.assertTrue(output["bruce"]["authoritative"])
        self.assertEqual(output["bruce"]["status"], "completed")
        self.assertEqual(output["bruce"]["exit_code"], 0)
        self.assertEqual(output["bruce"]["result"], {"answer": "3", "status": "bounded"})
        self.assertEqual(base64.b64decode(output["bruce"]["raw_stdout_base64"]), self.raw)
        digest = hashlib.sha256(self.raw).hexdigest()
        self.assertEqual(output["bruce"]["source_result_digest"], digest)
        self.assertEqual(output["answer"]["source_result_digest"], digest)
        self.assertEqual(output["answer"]["explanation_status"], "interrupted")
        self.assertEqual(output["answer"]["error"], "AssistantInterrupted")
        self.assertEqual(output["answer"]["summary"], "")
        self.assertEqual(json.loads(stderr), {"error": "AssistantInterrupted"})
        self.assertEqual(json.loads(target.read_text())["result"], output)
        self.assertEqual(sorted(p.name for p in self.data.iterdir()), ["index.json", "transcript.json"])

    def test_interrupt_before_confirmation_still_cancels_without_execution(self):
        self.build()
        process = subprocess.Popen(
            [sys.executable, str(self.cli), "ask", "--question", "add", "--config", str(self.path)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        self.assertEqual(json.loads(process.stdout.readline())["schema"], "bruce-local-ai-preview-v1")
        process.send_signal(signal.SIGINT)
        stdout, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 2)
        self.assertEqual(stdout, "")
        self.assertEqual(json.loads(stderr), {"error": "AssistantInterrupted"})
        self.assertFalse(self.record.exists())

    def test_transcript_is_opt_in_versioned_private_and_never_overwrites(self):
        self.build()
        target = self.data / "transcript.json"
        result = self.run_cli("ask", "--question", "add", "--transcript-out", str(target), confirmation="run\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        transcript = json.loads(target.read_text())
        self.assertEqual(transcript["schema"], "bruce-local-ai-transcript-v1")
        self.assertEqual(transcript["question"], "add")
        self.assertEqual(transcript["profile"], "default")
        self.assertEqual(transcript["result"], json.loads(result.stdout.splitlines()[-1]))
        self.assertEqual(len(transcript["catalog_digest"]), 64)
        self.assertEqual(len(transcript["index_digest"]), 64)
        self.assertEqual(target.stat().st_mode & 0o777, 0o600)
        original = target.read_bytes()
        result = self.run_cli("ask", "--question", "add", "--transcript-out", str(target), confirmation="run\n")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantTranscriptExists")
        self.assertEqual(target.read_bytes(), original)
        self.assertFalse(any(p.name.startswith(".bruce-ai-transcript-") for p in self.data.iterdir()))
        rejected = self.data / "rejected.json"
        result = self.run_cli("ask", "--question", "add", "--transcript-out", str(rejected), confirmation="no\n")
        self.assertFalse(rejected.exists())

    def test_parser_requires_config_and_rejects_unknown_duplicate_or_irrelevant_flags(self):
        commands = [("index", "build"), ("config", "check"), ("ask", "--question", "add"),
                    ("explain", "--input", str(self.data / "result.json"))]
        for command in commands:
            with self.subTest(command=command):
                for extra, config in (((), False), (("--unknown", "sensitive"), True),
                                      (("--config", str(self.path)), True),
                                      (("--profile", "default", "--profile=alternate"), True)):
                    result = self.run_cli(*command, *extra, config=config)
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(json.loads(result.stderr), {"error": "InvalidAssistantArguments"})
                    self.assertEqual(result.stdout, "")
        for arguments in (("ask", "--question", "a", "--question=b"),
                          ("ask", "--question", "a", "--input", "x"),
                          ("explain", "--input", "x", "--input", "y"),
                          ("explain", "--input", "x", "--transcript-out", "y"),
                          ("index", "build", "--question", "a"),
                          ("config", "check", "--question", "a"),
                          ("ask", "--question", "a", "--transcript-out", "x", "--transcript-out=y"),
                          ("ask", "--question", "a", "--yes"), ("ask",), ("explain",),
                          ("ask", "--question", ""), ("ask", "--question", "a", "--conf", "x")):
            result = self.run_cli(*arguments)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stderr), {"error": "InvalidAssistantArguments"})
        self.assertEqual(self.requests, [])
        self.assertFalse(self.record.exists())

    def test_profile_override_and_flags_before_command_do_not_rewrite_configuration(self):
        self.document["profiles"]["alternate"]["ollama_url"] = self.document["profiles"]["default"]["ollama_url"]
        self.path.write_text(json.dumps(self.document))
        before = self.path.read_bytes()
        result = self.run_cli("--profile=alternate", "index", "build")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads((self.data / "index.json").read_text())["profile_name"], "alternate")
        self.assertEqual(self.path.read_bytes(), before)
        result = self.run_cli("--config=" + str(self.path), "--profile", "alternate", "ask",
                              "--question=add", confirmation="run\n", config=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout.splitlines()[-1])["answer"]["model"], "other-chat:latest")
        self.assertEqual(self.path.read_bytes(), before)

    def test_stale_index_and_query_dimensions_block_chat_and_execution(self):
        self.build()
        self.requests.clear()
        source = self.data / "index.json"
        document = json.loads(source.read_text())
        document["embedding_model"] = "different-model"
        source.write_text(json.dumps(document))
        result = self.run_cli("ask", "--question", "add", confirmation="run\n")
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantIndexStale")
        self.assertEqual(self.requests, [])
        document["embedding_model"] = "all-minilm"
        document["dimension"] = 3
        for entry in document["entries"]:
            entry["vector"] = [1, 0, 0]
        source.write_text(json.dumps(document))
        result = self.run_cli("ask", "--question", "add", confirmation="run\n")
        self.assertEqual(json.loads(result.stderr)["error"], "AssistantIndexStale")
        self.assertEqual([path for path, _ in self.requests], ["/api/show", "/api/embed"])
        self.assertFalse(self.record.exists())

    def test_explain_rejects_fifo_symlink_oversize_and_missing_source_before_http(self):
        source = self.data / "result.json"
        source.write_bytes(self.raw)
        alias = self.data / "alias.json"
        alias.symlink_to(source)
        fifo = self.data / "fifo"
        os.mkfifo(fifo)
        huge = self.data / "huge.json"
        huge.write_bytes(b" " * (1024 * 1024 + 1))
        for path in (alias, fifo, huge, self.data / "missing"):
            result = self.run_cli("explain", "--input", str(path))
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stderr)["error"], "AssistantResultInvalid")
        self.assertEqual(self.requests, [])
        self.assertFalse(self.record.exists())

    def test_failed_and_invalid_bruce_results_retain_exact_streams_without_transcripts(self):
        self.build()
        target = self.data / "transcript.json"
        self.program.write_text(f"#!{sys.executable}\nimport sys\nsys.stdout.buffer.write({self.raw!r})\n"
                                "sys.stderr.buffer.write(b'failure\\xff')\nsys.exit(2)\n")
        result = self.run_cli("ask", "--question", "add", "--transcript-out", str(target), confirmation="run\n")
        self.assertEqual(result.returncode, 2)
        output = json.loads(result.stdout.splitlines()[-1])
        self.assertEqual(output["bruce"]["exit_code"], 2)
        self.assertEqual(output["bruce"]["result"]["answer"], "3")
        self.assertEqual(base64.b64decode(output["bruce"]["raw_stderr_base64"]), b"failure\xff")
        self.assertFalse(target.exists())
        self.requests.clear()
        self.program.write_text(f"#!{sys.executable}\nimport sys\nsys.stdout.buffer.write(b'bad\\xff')\n")
        result = self.run_cli("ask", "--question", "add", "--transcript-out", str(target), confirmation="run\n")
        output = json.loads(result.stdout.splitlines()[-1])
        self.assertEqual(output["bruce"]["parse_state"], "invalid_json")
        self.assertEqual(base64.b64decode(output["bruce"]["raw_stdout_base64"]), b"bad\xff")
        self.assertIsNone(output["answer"])
        self.assertEqual(sum(path == "/api/chat" for path, _ in self.requests), 1)
        self.assertFalse(target.exists())


class CTestDocsTests(unittest.TestCase):
    """Catch an unregistered workflow or an unusable published config example."""

    root = Path(__file__).resolve().parents[2]

    def test_ctest_registers_mock_workflow_without_real_binary_or_endpoint(self):
        cmake = (self.root / "CMakeLists.txt").read_text(encoding="utf-8")
        registrations = re.findall(
            r"add_test\(NAME\s+bruce_local_ai_workflows\s+COMMAND\s+([^)]*)\)", cmake)
        self.assertEqual(len(registrations), 1, "register the local mock workflow once")
        self.assertEqual(registrations[0].split(), [
            "${Python3_EXECUTABLE}",
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_bruce_ai.py"])

    def test_handbook_config_loads_and_profile_override_preserves_file(self):
        handbook = self.root / "docs/cli/local-ollama-assistant.md"
        self.assertTrue(handbook.is_file(), "publish the local assistant handbook")
        examples = re.findall(r"```json\n(.*?)\n```", handbook.read_text(encoding="utf-8"), re.S)
        self.assertTrue(examples, "include a complete JSON configuration example")
        document = json.loads(examples[0])
        with tempfile.TemporaryDirectory(prefix="bruce-ai-handbook-") as directory:
            root = Path(directory)
            program = root / "bruce"
            program.write_text("fixture; never executed", encoding="utf-8")
            program.chmod(0o700)
            # Substitute only installation-specific paths, keeping the published schema.
            document["bruce_program"] = str(program)
            document["allowed_roots"] = [str(root)]
            document["index_path"] = str(root / "index.json")
            config_path = root / "config.json"
            raw = json.dumps(document).encode()
            config_path.write_bytes(raw)
            with patch("socket.socket", side_effect=AssertionError("docs must stay offline")), \
                    patch("subprocess.Popen", side_effect=AssertionError("no process")):
                config = load_config(config_path)
                self.assertEqual(config.profile.embedding_model, "all-minilm")
                self.assertEqual(config.profile.response_model, "qwen3.5")
                self.assertTrue(config.require_confirmation)
                self.assertGreaterEqual(len(config.profiles), 2)
                alternate = next(name for name in config.profiles if name != config.active_profile)
                self.assertEqual(load_config(config_path, alternate).active_profile, alternate)
            self.assertEqual(config_path.read_bytes(), raw)

    def test_handbook_is_in_offline_document_link_inputs(self):
        handbook = self.root / "docs/cli/local-ollama-assistant.md"
        self.assertTrue(handbook.is_file(), "handbook must be scanned under docs/")
        result = subprocess.run([sys.executable, str(self.root / "scripts/check_local_doc_links.py")],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("broken=0", result.stdout)


class StarterScriptTests(unittest.TestCase):
    """The convenience script must not introduce another execution surface."""

    def setUp(self):
        self.root = Path(__file__).resolve().parents[2]
        self.starter = self.root / "scripts" / "bruce-local-ai.sh"

    def run_starter(self, *arguments):
        return subprocess.run([str(self.starter), *arguments], text=True,
                              capture_output=True, timeout=5)

    def test_starter_exposes_only_safe_assistant_operations(self):
        help_result = self.run_starter("--help")
        self.assertEqual(help_result.returncode, 0, help_result.stderr)
        self.assertIn("config-check CONFIG", help_result.stdout)
        self.assertIn("index-build CONFIG", help_result.stdout)
        self.assertIn("ask CONFIG QUESTION", help_result.stdout)
        self.assertIn("explain CONFIG RESULT", help_result.stdout)
        rejected = self.run_starter("install", "/tmp/config.json")
        self.assertEqual(rejected.returncode, 64)
        self.assertIn("No model download", rejected.stderr)

    def test_starter_forwards_only_the_existing_assistant_cli_contract(self):
        result = self.run_starter("config-check", "/missing/config.json", "default")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr), {"error": "InvalidAssistantConfig"})

    def test_published_config_template_is_complete_and_needs_only_local_paths(self):
        template = self.root / "examples" / "bruce-local-ai-config.example.json"
        self.assertTrue(template.is_file(), "publish a copyable local-only configuration template")
        document = json.loads(template.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory(prefix="bruce-ai-template-") as directory:
            root = Path(directory)
            program = root / "bruce"
            program.write_text("fixture; never executed", encoding="utf-8")
            program.chmod(0o700)
            document["bruce_program"] = str(program)
            document["index_path"] = str(root / "index.json")
            document["allowed_roots"] = [str(root)]
            path = root / "config.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            config = load_config(path)
            self.assertTrue(config.require_confirmation)
            self.assertEqual(config.profile.ollama_url, "http://127.0.0.1:11434")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--case", action="append", choices=("config", "ollama", "catalog", "index", "proposal", "execution", "response", "cli", "ctest-docs", "starter"))
    args = parser.parse_args()
    cases = {"config": ConfigTests, "ollama": OllamaTests, "catalog": CatalogTests, "index": IndexTests,
             "proposal": ProposalTests, "execution": ExecutionTests, "response": ResponseTests, "cli": CLITests,
             "ctest-docs": CTestDocsTests, "starter": StarterScriptTests}
    suite = unittest.TestSuite()
    for name in args.case or cases:
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(cases[name]))
    return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
