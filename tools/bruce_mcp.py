#!/usr/bin/env python3
"""Local stdio boundary for a deliberately small BRUCE MCP surface."""
import hashlib
import json
import os
import selectors
import subprocess
import sys
import time
from collections.abc import Mapping
from dataclasses import dataclass
from typing import TextIO

from bruce_mcp_trace import (ADAPTER_VERSION, TraceConfigurationError, TraceWriteError,
                             TraceWriter, writer_from_environment)

BRUCE = os.environ.get("BRUCE_MCP_BRUCE", "bruce")
ASSET_ROOT = os.path.realpath(os.environ.get("BRUCE_MCP_ASSET_ROOT", os.getcwd()))
MAX_MESSAGE_BYTES = 16 * 1024
MAX_RESPONSE_TEXT_BYTES = 64 * 1024
MAX_SUBPROCESS_OUTPUT_BYTES = 64 * 1024
COMMAND_TIMEOUT_SECONDS = 10
LATEST_PROTOCOL = "2025-06-18"
LEGACY_PROTOCOL = "2024-11-05"
negotiated_protocol = LATEST_PROTOCOL

DEMOS = {
    "dag_scheduler_v1_mixed": ["dag-scheduler-v1", "demo", "mixed"],
    "dag_scheduler_v1_budget": ["dag-scheduler-v1", "demo", "budget"],
    "dag_scheduler_v1_cancelled": ["dag-scheduler-v1", "demo", "cancelled"],
    "dag_scheduler_v2_flow": ["dag-scheduler-v2", "demo", "flow"],
}
CALCULATE_OPERATIONS = ["add", "sub", "mul", "div", "divmod", "pow", "modpow"]
ASSET_COMMANDS = {
    "bruce_inspect_run": "inspect-run",
    "bruce_replay_run": "replay",
    "bruce_assess_run": "assess-run",
    "bruce_gate_run": "gate-run",
}

def asset(name):
    if not isinstance(name, str) or not name or "/" in name or "\\" in name or name in {".", ".."}:
        raise ValueError("InvalidAssetName")
    candidate = os.path.realpath(os.path.join(ASSET_ROOT, name))
    if os.path.commonpath([ASSET_ROOT, candidate]) != ASSET_ROOT or not os.path.isfile(candidate):
        raise ValueError("UnknownAsset")
    return candidate

OBJECT_OUTPUT_SCHEMA = {"type": "object"}
ASSET_NAME_SCHEMA = {"type": "string", "minLength": 1, "pattern": "^(?!\\.{1,2}$)[^/\\\\]+$"}
CALCULATE_OUTPUT_SCHEMA = {
    "type": "object", "required": ["domain", "operation", "max_bits"], "additionalProperties": False,
    "properties": {
        "domain": {"enum": ["bigint", "rational"]},
        "operation": {"enum": CALCULATE_OPERATIONS},
        "max_bits": {"type": "integer", "minimum": 1, "maximum": 16384},
        "value": {"type": "string"}, "quotient": {"type": "string"}, "remainder": {"type": "string"},
    },
    "oneOf": [
        {"properties": {"operation": {"enum": [op for op in CALCULATE_OPERATIONS if op != "divmod"]}},
         "required": ["value"]},
        {"properties": {"operation": {"const": "divmod"}}, "required": ["quotient", "remainder"]},
    ],
}
ASSET_INPUT_SCHEMA = {"type": "object", "required": ["asset"], "additionalProperties": False,
                      "properties": {"asset": ASSET_NAME_SCHEMA}}
TOOLS = [{"name": "bruce_calculate", "description": "Run bounded exact BRUCE arithmetic.",
          "inputSchema": {"type": "object", "required": ["op", "left", "right"],
          "additionalProperties": False,
          "properties": {"op": {"enum": CALCULATE_OPERATIONS},
          "left": {"type": "string", "minLength": 1}, "right": {"type": "string", "minLength": 1},
          "domain": {"type": "string", "enum": ["bigint", "rational"], "default": "bigint"},
          "max_bits": {"type": "integer", "minimum": 1, "maximum": 16384, "default": 4096},
          "modulus": {"type": "string", "pattern": "^0*[1-9][0-9]*$"}},
          "allOf": [
              {"if": {"properties": {"op": {"const": "modpow"}}},
               "then": {"required": ["modulus"], "properties": {"domain": {"const": "bigint"}}},
               "else": {"not": {"required": ["modulus"]}}},
              {"if": {"properties": {"op": {"const": "divmod"}}},
               "then": {"properties": {"domain": {"const": "bigint"}}}}
          ]}, "outputSchema": CALCULATE_OUTPUT_SCHEMA},
         {"name":"bruce_inspect_run","description":"Inspect an approved BRUCE run asset.","inputSchema":ASSET_INPUT_SCHEMA,"outputSchema":OBJECT_OUTPUT_SCHEMA},
         {"name":"bruce_replay_run","description":"Replay an approved BRUCE run asset.","inputSchema":ASSET_INPUT_SCHEMA,"outputSchema":OBJECT_OUTPUT_SCHEMA},
         {"name":"bruce_assess_run","description":"Assess an approved BRUCE run asset.","inputSchema":ASSET_INPUT_SCHEMA,"outputSchema":OBJECT_OUTPUT_SCHEMA},
         {"name":"bruce_gate_run","description":"Evaluate application gates for an approved BRUCE run asset.","inputSchema":ASSET_INPUT_SCHEMA,"outputSchema":OBJECT_OUTPUT_SCHEMA},
         {"name":"bruce_research","description":"Run bounded research on approved CSV assets.","inputSchema":{"type":"object","required":["train","holdout"],"additionalProperties":False,"properties":{"train":ASSET_NAME_SCHEMA,"holdout":ASSET_NAME_SCHEMA}},"outputSchema":OBJECT_OUTPUT_SCHEMA},
         {"name":"bruce_demo","description":"Run one enumerated BRUCE demo.","inputSchema":{"type":"object","required":["demo"],"additionalProperties":False,"properties":{"demo":{"enum":list(DEMOS)}}},"outputSchema":OBJECT_OUTPUT_SCHEMA}]

def has_exact_keys(arguments, required, optional=()):
    return isinstance(arguments, dict) and set(arguments) >= set(required) and set(arguments) <= set(required) | set(optional)

def calculate_command(arguments):
    if not has_exact_keys(arguments, {"op", "left", "right"}, {"domain", "max_bits", "modulus"}):
        return None
    if (arguments["op"] not in CALCULATE_OPERATIONS or not isinstance(arguments["left"], str)
            or not isinstance(arguments["right"], str) or not arguments["left"] or not arguments["right"]):
        return None
    if "domain" in arguments and (not isinstance(arguments["domain"], str) or arguments["domain"] not in {"bigint", "rational"}):
        return None
    if "max_bits" in arguments and (type(arguments["max_bits"]) is not int or not 1 <= arguments["max_bits"] <= 16384):
        return None
    if "modulus" in arguments and not isinstance(arguments["modulus"], str):
        return None
    domain = arguments.get("domain", "bigint")
    if arguments["op"] == "modpow":
        modulus = arguments.get("modulus")
        if domain != "bigint" or not modulus or not all("0" <= char <= "9" for char in modulus) or not any(char != "0" for char in modulus):
            return None
    elif "modulus" in arguments:
        return None
    if arguments["op"] == "divmod" and domain != "bigint":
        return None
    command = [BRUCE, "calculate", "--op", arguments["op"], "--left", arguments["left"], "--right", arguments["right"]]
    for key, option in (("domain", "--domain"), ("max_bits", "--max-bits"), ("modulus", "--modulus")):
        if key in arguments:
            command.extend([option, str(arguments[key])])
    return command

def tools_for_protocol():
    if negotiated_protocol != LEGACY_PROTOCOL:
        return TOOLS
    return [{key: value for key, value in tool.items() if key != "outputSchema"} for tool in TOOLS]

@dataclass(frozen=True)
class MessageResult:
    request: object | None
    response: dict | None
    response_text: str | None
    outcome: str


def make_reply(identifier: object, result: object | None = None,
               error: str | None = None, code: int = -32602) -> dict:
    out = {"jsonrpc": "2.0", "id": identifier}
    out["result" if error is None else "error"] = result if error is None else {"code": code, "message": error}
    return out


def serialize_reply(response: dict) -> str:
    return json.dumps(response, separators=(",", ":"), allow_nan=False)

def bounded_text(text):
    encoded = text.encode("utf-8")
    if len(encoded) <= MAX_RESPONSE_TEXT_BYTES:
        return text
    marker = "\n[BRUCE MCP response truncated]"
    return encoded[:MAX_RESPONSE_TEXT_BYTES - len(marker.encode("utf-8"))].decode("utf-8", errors="ignore") + marker

def reject_json_constant(_value):
    raise ValueError("InvalidJSON")

def successful_tool_result(stdout):
    try:
        structured = json.loads(stdout, parse_constant=reject_json_constant)
    except (TypeError, ValueError):
        return {"content": [{"type": "text", "text": "BRUCE returned invalid JSON"}], "isError": True}
    if not isinstance(structured, dict):
        return {"content": [{"type": "text", "text": "BRUCE returned invalid JSON"}], "isError": True}
    result = {"content": [{"type": "text", "text": bounded_text(stdout)}], "isError": False}
    if negotiated_protocol != LEGACY_PROTOCOL:
        result["structuredContent"] = structured
    return result

def run_bounded(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ, "stdout")
    selector.register(process.stderr, selectors.EVENT_READ, "stderr")
    output = {"stdout": bytearray(), "stderr": bytearray()}
    remaining = MAX_SUBPROCESS_OUTPUT_BYTES
    deadline = time.monotonic() + COMMAND_TIMEOUT_SECONDS
    try:
        while selector.get_map():
            wait = deadline - time.monotonic()
            if wait <= 0:
                process.kill()
                process.wait()
                raise subprocess.TimeoutExpired(command, COMMAND_TIMEOUT_SECONDS)
            events = selector.select(wait)
            if not events:
                continue
            for key, _ in events:
                chunk = os.read(key.fileobj.fileno(), min(8192, remaining + 1))
                if not chunk:
                    selector.unregister(key.fileobj)
                    continue
                if len(chunk) > remaining:
                    process.kill()
                    process.wait()
                    return None
                output[key.data].extend(chunk)
                remaining -= len(chunk)
        return process.wait(), output["stdout"].decode("utf-8", errors="replace"), output["stderr"].decode("utf-8", errors="replace")
    finally:
        selector.close()

def handle_message(message_text: str) -> MessageResult:
    global negotiated_protocol
    request = None

    def reply(identifier, result=None, error=None, code=-32602):
        response = make_reply(identifier, result, error, code)
        return MessageResult(request, response, serialize_reply(response), "response")

    try:
        request = json.loads(message_text, parse_constant=reject_json_constant)
    except (json.JSONDecodeError, ValueError):
        return reply(None, error="ParseError", code=-32700)
    if not isinstance(request, dict) or request.get("jsonrpc") != "2.0" or not isinstance(request.get("method"), str):
        return reply(None, error="InvalidRequest", code=-32600)
    method, ident = request["method"], request.get("id")
    notification = "id" not in request
    try:
        if method == "notifications/initialized":
            return MessageResult(request, None, None, "notification")
        if notification:
            return MessageResult(request, None, None, "notification")
        if method == "initialize":
            params = request.get("params", {})
            if not isinstance(params, dict):
                return reply(ident, error="InvalidParams")
            requested_protocol = params.get("protocolVersion", LATEST_PROTOCOL)
            if not isinstance(requested_protocol, str):
                return reply(ident, error="InvalidParams")
            negotiated_protocol = requested_protocol if requested_protocol in {LATEST_PROTOCOL, LEGACY_PROTOCOL} else LATEST_PROTOCOL
            return reply(ident, {"protocolVersion": negotiated_protocol, "capabilities": {"tools": {}},
                                "serverInfo": {"name": "bruce-mcp", "version": ADAPTER_VERSION}})
        elif method == "tools/list":
            return reply(ident, {"tools": tools_for_protocol()})
        elif method == "tools/call":
            params = request.get("params")
            if not isinstance(params, dict):
                return reply(ident, error="InvalidParams")
            args, name = params.get("arguments", {}), params.get("name")
            if not isinstance(name, str) or not isinstance(args, dict):
                raise ValueError("InvalidToolArguments")
            command = calculate_command(args) if name == "bruce_calculate" else None
            if name in ASSET_COMMANDS and has_exact_keys(args, {"asset"}) and isinstance(args["asset"], str): command=[BRUCE,ASSET_COMMANDS[name],asset(args["asset"])]
            elif name=="bruce_research" and has_exact_keys(args,{"train","holdout"}) and isinstance(args["train"],str) and isinstance(args["holdout"],str): command=[BRUCE,"research","--train",asset(args["train"]),"--holdout",asset(args["holdout"]),"--max-candidates","1000","--max-operations","10000","--max-trace","1000","--max-results","4"]
            elif name=="bruce_demo" and has_exact_keys(args,{"demo"}) and isinstance(args["demo"],str) and args["demo"] in DEMOS: command=[BRUCE,*DEMOS[args["demo"]]]
            elif command is None: raise ValueError("InvalidToolArguments")
            run = run_bounded(command)
            if run is None:
                return reply(ident, {"content": [{"type": "text", "text": "BRUCE output limit exceeded"}], "isError": True})
            else:
                returncode, stdout, stderr = run
                result = successful_tool_result(stdout) if returncode == 0 else {"content": [{"type": "text", "text": bounded_text(stderr)}], "isError": True}
                return reply(ident, result)
        else:
            return reply(ident, error="MethodNotFound", code=-32601)
    except subprocess.TimeoutExpired:
        return reply(ident, {"content": [{"type": "text", "text": "BRUCE operation timed out"}], "isError": True})
    except ValueError as exc:
        message = str(exc) if str(exc) in {"InvalidAssetName", "UnknownAsset", "InvalidToolArguments"} else "InvalidToolArguments"
        return reply(ident, error=message)
    except Exception:
        return reply(ident, error="InternalError", code=-32603)


def process_stream(input_stream: TextIO, output_stream: TextIO, error_stream: TextIO,
                   trace_writer: TraceWriter | None = None) -> int:
    global negotiated_protocol
    negotiated_protocol = LATEST_PROTOCOL
    for line in input_stream:
        if trace_writer is not None:
            received_unix_ns, started_monotonic_ns = time.time_ns(), time.monotonic_ns()
        if len(line.encode("utf-8")) > MAX_MESSAGE_BYTES:
            response = make_reply(None, error="MessageTooLarge")
            result = MessageResult(None, response, serialize_reply(response), "message_too_large")
        else:
            result = handle_message(line)
        if trace_writer is not None:
            # Preserve the original line-size decision above; remove only its
            # observed terminator from the text recorded at the MCP boundary.
            request_text = line[:-2] if line.endswith("\r\n") else line.removesuffix("\n")
            request_data = request_text.encode("utf-8")
            trace_writer.append_exchange(
                request_text=None if result.outcome == "message_too_large" else request_text,
                request_bytes=len(request_data),
                request_sha256=hashlib.sha256(request_data).hexdigest(), request=result.request,
                response_text=result.response_text, response=result.response, outcome=result.outcome,
                received_unix_ns=received_unix_ns, completed_unix_ns=time.time_ns(),
                duration_monotonic_ns=time.monotonic_ns() - started_monotonic_ns)
        if result.response_text is not None:
            try:
                # The synchronized exchange proves the selected payload, not
                # client receipt. Output failure must leave the trace footerless.
                print(result.response_text, file=output_stream, flush=True)
            except (OSError, UnicodeError, ValueError):
                if trace_writer is None:
                    raise
                raise TraceWriteError() from None
    if trace_writer is not None:
        trace_writer.close()
    return 0


def main(environ: Mapping[str, str] = os.environ) -> int:
    try:
        trace_writer = writer_from_environment(environ)
        return process_stream(sys.stdin, sys.stdout, sys.stderr, trace_writer)
    except TraceConfigurationError:
        print("BruceMcpTraceConfigurationError", file=sys.stderr, flush=True)
        return 2
    except TraceWriteError:
        print("BruceMcpTraceWriteError", file=sys.stderr, flush=True)
        return 3


if __name__ == "__main__":
    exit_code = main()
    if exit_code == 3:
        # A failed flush can leave buffered output pending. Do not retry it at
        # interpreter shutdown and replace the stable trace error with details.
        sys.stdout = None
    raise SystemExit(exit_code)
