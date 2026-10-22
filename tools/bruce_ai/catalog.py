"""Versioned source-backed V1 routes, with immutable named-argument schemas."""

from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
import re
from types import MappingProxyType

from .config import AssistantError, strict_json_loads


# These exact prefixes are the authority boundary, independent of file content.
# No install/activation/update/rollback/trust transition/release/rules route is
# admitted, even if a supplied catalog describes it as read-only.
_SIMPLE_ARGUMENTS = {
    "calculate": "op left right domain max_bits modulus",
    "coordinate": "x y z",
    "grid": "path mode rule stage modulus max_bits zero_zero",
    "grid-batch": "paths mode rule stage modulus max_bits zero_zero cache_bytes max_query_steps",
    "grid-resume": "from_run additional_queries",
    "compare": "left_numerator left_denominator right_numerator right_denominator domain_policy max_bits max_operations",
    "research": "train holdout domain min max max_bits max_denominator max_candidates max_operations max_trace max_results max_trace_bytes",
    "inspect-run": "run", "replay": "run", "assess-run": "run", "gate-run": "run",
}
_ROUTES = {name: ((name,), frozenset(fields.split()),
                  "read" if name in {"inspect-run", "assess-run", "gate-run"} else "compute")
           for name, fields in _SIMPLE_ARGUMENTS.items()}
for _family, _scenarios in (
    ("signed-affine-dag-v1", ("merge", "cancellation")),
    ("signed-affine-relation-v1", ("merge",)),
    ("toy-relation-v1", ("collision", "noninvertible")),
    ("dag-scheduler-v1", ("mixed", "budget", "cancelled")),
    ("dag-scheduler-v2", ("flow",)),
):
    for _scenario in _scenarios:
        _ROUTES[f"{_family}-{_scenario}"] = ((_family, "demo", _scenario), frozenset(), "fixed-demo")
_ROUTES.update({
    "mods-list": (("mods", "list"), frozenset({"mod_root"}), "read"),
    "mods-inspect": (("mods", "inspect"), frozenset({"package"}), "read"),
    "mods-verify": (("mods", "verify"), frozenset({"package", "trust_store"}), "read"),
    "mods-trust-status": (("mods", "trust", "status"), frozenset({"mod_root"}), "read"),
})


def _invalid():
    raise AssistantError("AssistantCatalogInvalid")


def _freeze(value):
    if isinstance(value, dict):
        return MappingProxyType({key: _freeze(item) for key, item in value.items()})
    if isinstance(value, list):
        return tuple(_freeze(item) for item in value)
    return value


def _plain(value):
    if isinstance(value, Mapping):
        return {key: _plain(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [_plain(item) for item in value]
    return value


@dataclass(frozen=True)
class CatalogEntry:
    command_id: str
    synopsis: str
    syntax: str
    argv: tuple[str, ...]
    arguments: Mapping
    side_effect: str
    sources: tuple[str, ...]

    def to_dict(self) -> dict:
        return {"command_id": self.command_id, "synopsis": self.synopsis,
                "syntax": self.syntax, "argv": list(self.argv),
                "arguments": _plain(self.arguments), "side_effect": self.side_effect,
                "sources": list(self.sources)}

    @property
    def embedding_text(self) -> str:
        # V2 routing embeds only human catalog wording. Technical records stay
        # authoritative for proposal validation, not semantic similarity.
        return self.synopsis


def _validate_arguments(schema, allowed):
    if (not isinstance(schema, dict)
            or set(schema) != {"type", "additionalProperties", "required", "properties"}
            or schema["type"] != "object" or schema["additionalProperties"] is not False
            or not isinstance(schema["required"], list)
            or any(not isinstance(key, str) for key in schema["required"])
            or len(set(schema["required"])) != len(schema["required"])
            or not isinstance(schema["properties"], dict)
            or set(schema["properties"]) != allowed
            or not set(schema["required"]) <= allowed):
        _invalid()
    for rule in schema["properties"].values():
        if not isinstance(rule, dict) or rule.get("type") not in ("integer", "string"):
            _invalid()
        if rule["type"] == "integer":
            if (set(rule) != {"type", "minimum", "maximum"}
                    or type(rule["minimum"]) is not int or type(rule["maximum"]) is not int
                    or not 0 <= rule["minimum"] <= rule["maximum"] <= 1048576):
                _invalid()
        elif "enum" in rule:
            if (set(rule) != {"type", "enum"} or not isinstance(rule["enum"], list)
                    or not rule["enum"] or any(not isinstance(item, str) or not item for item in rule["enum"])
                    or len(set(rule["enum"])) != len(rule["enum"])):
                _invalid()
        elif (not {"type", "minLength", "maxLength"} <= set(rule)
                or not set(rule) <= {"type", "minLength", "maxLength", "pattern", "format"}
                or type(rule["minLength"]) is not int or type(rule["maxLength"]) is not int
                or not 1 <= rule["minLength"] <= rule["maxLength"] <= 65536
                or ("format" in rule and rule["format"] not in {"input-file", "input-directory"})):
            _invalid()
        if "pattern" in rule:
            if not isinstance(rule["pattern"], str) or len(rule["pattern"]) > 512:
                _invalid()
            try:
                re.compile(rule["pattern"])
            except re.error:
                _invalid()


def load_catalog(path: Path) -> list[CatalogEntry]:
    try:
        with path.open("rb") as source:
            raw = source.read(256 * 1024 + 1)
        if len(raw) > 256 * 1024:
            _invalid()
        document = strict_json_loads(raw.decode("utf-8"))
        if (not isinstance(document, dict) or set(document) != {"schema", "entries"}
                or document["schema"] != "bruce-ai-command-catalog-v1"
                or not isinstance(document["entries"], list) or not document["entries"]
                or len(document["entries"]) > len(_ROUTES)):
            _invalid()
        entries = []
        seen = set()
        for item in document["entries"]:
            if (not isinstance(item, dict) or set(item) != set(CatalogEntry.__dataclass_fields__)
                    or not isinstance(item["command_id"], str)
                    or item["command_id"] not in _ROUTES or item["command_id"] in seen):
                _invalid()
            argv, allowed, effect = _ROUTES[item["command_id"]]
            if item["argv"] != list(argv) or item["side_effect"] != effect:
                _invalid()
            for key in ("synopsis", "syntax"):
                if not isinstance(item[key], str) or not item[key].strip() or len(item[key]) > 4096:
                    _invalid()
            if (not isinstance(item["sources"], list) or not item["sources"]
                    or any(not isinstance(ref, str) or len(ref) > 256
                           or not ref.startswith(("apps/", "tests/"))
                           or ".." in Path(ref).parts for ref in item["sources"])):
                _invalid()
            _validate_arguments(item["arguments"], allowed)
            entries.append(CatalogEntry(item["command_id"], item["synopsis"], item["syntax"], argv,
                                        _freeze(item["arguments"]), effect, tuple(item["sources"])))
            seen.add(item["command_id"])
        return sorted(entries, key=lambda entry: entry.command_id)
    except (OSError, UnicodeError, ValueError, TypeError, RecursionError):
        raise AssistantError("AssistantCatalogInvalid") from None
