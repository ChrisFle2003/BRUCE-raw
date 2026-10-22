"""Validate model suggestions and construct catalog-only, deterministic argv."""

from dataclasses import dataclass
from pathlib import Path
import re
from types import MappingProxyType
from typing import Mapping

from .catalog import CatalogEntry, _ROUTES, _SIMPLE_ARGUMENTS
from .config import AssistantError


class AssistantInputRequired(AssistantError):
    """Carry missing catalog field names without exposing input paths."""

    def __init__(self, missing_inputs):
        self.missing_inputs = tuple(missing_inputs)
        super().__init__("AssistantInputRequired")


@dataclass(frozen=True)
class Proposal:
    schema: str
    kind: str
    command_id: str
    arguments: Mapping
    missing_inputs: tuple[str, ...]
    reason: str
    entry: CatalogEntry


def _invalid():
    raise AssistantError("AssistantProposalInvalid")


def response_schema_for_candidates(candidates) -> dict:
    """Return the exact Ollama structured-output contract for this shortlist.

    The model still has no authority: ``parse_proposal`` below repeats every
    validation at the execution boundary.  This schema only keeps a compliant
    local model from having to infer enum spellings and required envelope
    fields from prose alone.
    """
    branches = []
    for entry in candidates:
        properties = _plain_schema(entry.arguments["properties"])
        required_arguments = list(entry.arguments["required"])
        for kind in ("proposal", "needs_input"):
            arguments = {"type": "object", "additionalProperties": False,
                         "required": required_arguments if kind == "proposal" else [],
                         "properties": properties}
            branches.append({
                "type": "object", "additionalProperties": False,
                "required": ["schema", "kind", "command_id", "arguments", "missing_inputs", "reason"],
                "properties": {
                    "schema": {"type": "string", "enum": ["bruce-local-ai-proposal-v1"]},
                    "kind": {"type": "string", "enum": [kind]},
                    "command_id": {"type": "string", "enum": [entry.command_id]},
                    "arguments": arguments,
                    "missing_inputs": {"type": "array", "items": {"type": "string", "minLength": 1,
                                                                         "maxLength": 128},
                                       "maxItems": 0 if kind == "proposal" else 64,
                                       **({"minItems": 1} if kind == "needs_input" else {})},
                    "reason": {"type": "string", "maxLength": 4096},
                },
            })
    return {"oneOf": branches}


def _plain_schema(value):
    """Turn the immutable catalog schema back into JSON-compatible values."""
    if isinstance(value, Mapping):
        return {key: _plain_schema(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [_plain_schema(item) for item in value]
    return value


def _validate_value(value, rule):
    if rule["type"] == "integer":
        if type(value) is not int or not rule["minimum"] <= value <= rule["maximum"]:
            _invalid()
    else:
        if type(value) is not str:
            _invalid()
        if "enum" in rule:
            if value not in rule["enum"]:
                _invalid()
        elif (not rule["minLength"] <= len(value) <= rule["maxLength"]
              or ("pattern" in rule and re.fullmatch(rule["pattern"], value) is None)):
            _invalid()


def _integer_key(text):
    # Compare arbitrary catalog-sized signed decimals without interpreter
    # decimal conversion limits or model-influenced global settings.
    negative = text.startswith("-")
    digits = text.lstrip("-").lstrip("0") or "0"
    if negative and digits != "0":
        return (-1, -len(digits), tuple(-ord(c) for c in digits))
    return (0, len(digits), tuple(ord(c) for c in digits))


def _cross_fields(command, args, missing):
    if command == "calculate":
        domain = args.get("domain", "bigint")
        if domain == "bigint" and any("/" in args.get(name, "") for name in ("left", "right")):
            _invalid()
        if any("/" in args.get(name, "") and not args[name].split("/")[1].strip("0")
               for name in ("left", "right")):
            _invalid()
        if args.get("op") in ("pow", "modpow") and "right" in args:
            exponent = args["right"].lstrip("0") or "0"
            if not exponent.isascii() or not exponent.isdecimal() or _integer_key(exponent) > _integer_key(str(2**64 - 1)):
                _invalid()
        if args.get("op") == "divmod" and domain != "bigint":
            _invalid()
        if "modulus" in args and args.get("op") != "modpow":
            _invalid()
        if args.get("op") == "modpow":
            if domain != "bigint":
                _invalid()
            if "modulus" not in args:
                missing.append("modulus")
    elif command == "coordinate":
        if "z" in args:
            if "x" in args or "y" in args:
                _invalid()
        else:
            missing.extend(name for name in ("x", "y") if name not in args)
        if any(int(value) > 2**64 - 1 for value in args.values()):
            _invalid()
        if "x" in args and "y" in args:
            x, y = int(args["x"]), int(args["y"])
            if (x + y) * (x + y + 1) // 2 + y > 2**64 - 1:
                _invalid()
    elif command in ("grid", "grid-batch"):
        if "stage" in args and args.get("rule", "leaf-local-power") != "scaled-exponential-v1":
            _invalid()
        if args.get("mode", "exact") == "modular":
            if "modulus" not in args:
                missing.append("modulus")
            elif int(args["modulus"]) > 2**64 - 1:
                _invalid()
        elif "modulus" in args:
            _invalid()
        field = "path" if command == "grid" else "paths"
        if field in args:
            paths = args[field].split(";")
            if len(paths) > 1024:
                _invalid()
            for path in paths:
                steps = path.split("/")
                if len(steps) > 64 or any(len(n.lstrip("0")) > 1 or int(n.lstrip("0") or "0") > 7
                                         for step in steps for n in step.split(",")):
                    _invalid()
    elif command == "research":
        domain = args.get("domain", "int64")
        if domain == "int64" and any(
                name in args for name in ("max_bits", "max_denominator", "max_trace_bytes")):
            _invalid()
        if domain == "bigint" and args.get("max_denominator", 1) != 1:
            _invalid()
        minimum, maximum = args.get("min", "-8"), args.get("max", "8")
        if _integer_key(minimum) > _integer_key(maximum):
            _invalid()
        if domain == "int64" and (
                _integer_key(minimum) < _integer_key("-1000000")
                or _integer_key(maximum) > _integer_key("1000000")):
            _invalid()


def parse_proposal(model_json: dict, catalog) -> Proposal:
    """Ignore model extras, but validate every recognized field before use."""
    fields = {"schema", "kind", "command_id", "arguments", "missing_inputs", "reason"}
    if (type(model_json) is not dict or not fields <= model_json.keys()
            or model_json["schema"] != "bruce-local-ai-proposal-v1"
            or model_json["kind"] not in ("proposal", "needs_input")
            or type(model_json["command_id"]) is not str
            or type(model_json["arguments"]) is not dict
            or type(model_json["reason"]) is not str or len(model_json["reason"]) > 4096
            or type(model_json["missing_inputs"]) is not list
            or len(model_json["missing_inputs"]) > 64
            or any(type(item) is not str or not 1 <= len(item) <= 128
                   for item in model_json["missing_inputs"])):
        _invalid()
    command = model_json["command_id"]
    entry = next((item for item in catalog if item.command_id == command), None)
    if entry is None or command not in _ROUTES or entry.argv != _ROUTES[command][0]:
        _invalid()
    properties = entry.arguments["properties"]
    args = {name: value for name, value in model_json["arguments"].items() if name in properties}
    for name, value in args.items():
        _validate_value(value, properties[name])
    missing = list(dict.fromkeys(model_json["missing_inputs"]))
    missing.extend(name for name in entry.arguments["required"] if name not in args and name not in missing)
    _cross_fields(command, args, missing)
    kind = "needs_input" if missing or model_json["kind"] == "needs_input" else "proposal"
    if kind == "needs_input" and not missing:
        missing.append("input")
    return Proposal(model_json["schema"], kind, command, MappingProxyType(args),
                    tuple(dict.fromkeys(missing)), model_json["reason"], entry)


def _input_path(value, rule, roots, field):
    if any(c in value for c in "\x00\r\n;$`|&<>") or ".." in Path(value).parts:
        raise AssistantError("UnsafeAssistantPath")
    path = Path(value)
    if path.is_absolute() and not any(path == root or root in path.parents for root in roots):
        raise AssistantError("UnsafeAssistantPath")
    candidates = (path,) if path.is_absolute() else tuple(root / path for root in roots)
    found = []
    for candidate in candidates:
        try:
            resolved = candidate.resolve()
            if not any(resolved == root or root in resolved.parents for root in roots):
                raise AssistantError("UnsafeAssistantPath")
            valid = resolved.is_dir() if rule["format"] == "input-directory" else resolved.is_file()
            if valid and resolved not in found:
                found.append(resolved)
        except (OSError, RuntimeError, ValueError):
            raise AssistantError("UnsafeAssistantPath") from None
    if len(found) > 1:
        raise AssistantError("UnsafeAssistantPath")
    if not found:
        raise AssistantInputRequired((field,))
    return str(found[0])


def canonical_argv(proposal: Proposal, config) -> list[str]:
    """Resolve inputs at the preview boundary, never accepting model argv."""
    # Revalidate even a replaced/dataclass-constructed proposal at the boundary.
    checked = parse_proposal({"schema": proposal.schema, "kind": proposal.kind,
                              "command_id": proposal.command_id, "arguments": dict(proposal.arguments),
                              "missing_inputs": list(proposal.missing_inputs), "reason": proposal.reason},
                             [proposal.entry])
    if checked.kind != "proposal":
        raise AssistantInputRequired(checked.missing_inputs)
    properties = checked.entry.arguments["properties"]
    order = _SIMPLE_ARGUMENTS.get(checked.command_id, "mod_root package trust_store").split()
    argv = [str(config.bruce_program), *checked.entry.argv]
    for name in order:
        if name not in checked.arguments:
            continue
        value = checked.arguments[name]
        if "format" in properties[name]:
            value = _input_path(value, properties[name], config.allowed_roots, name)
        positional = name == "run" or (name == "package" and checked.command_id in ("mods-inspect", "mods-verify"))
        if not positional:
            argv.append("--" + name.replace("_", "-"))
        argv.append(str(value))
    return argv
