"""Strict immutable configuration and redacted assistant errors."""

from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import re
from types import MappingProxyType
from typing import Mapping
from urllib.parse import urlsplit


class AssistantError(Exception):
    """A fixed public error code, without underlying exception text."""

    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def _invalid():
    raise AssistantError("InvalidAssistantConfig")


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def _no_constant(value):
    raise ValueError("nonfinite JSON constant")


def strict_json_loads(raw):
    """Reject ambiguous duplicate keys and non-standard nonfinite constants."""
    def finite_float(value):
        result = float(value)
        if not math.isfinite(result):
            raise ValueError("nonfinite JSON number")
        return result

    return json.loads(raw, object_pairs_hook=_unique_object, parse_constant=_no_constant,
                      parse_float=finite_float)


def validate_ollama_url(value: str):
    """Only literal loopback HTTP origins with explicit ports are accepted."""
    if not isinstance(value, str) or any(c.isspace() for c in value):
        _invalid()
    try:
        parsed = urlsplit(value)
        port = parsed.port
        if (parsed.scheme != "http" or parsed.hostname not in ("127.0.0.1", "::1")
                or parsed.username is not None or parsed.password is not None
                or parsed.path not in ("", "/") or parsed.query or parsed.fragment
                or "?" in value or "#" in value
                or port is None or not 1 <= port <= 65535):
            _invalid()
        # Require the canonical host syntax; no aliases or URL parser tricks.
        host = "[::1]" if parsed.hostname == "::1" else "127.0.0.1"
        if parsed.netloc != f"{host}:{port}":
            _invalid()
    except (ValueError, TypeError):
        _invalid()
    return parsed.hostname, port


@dataclass(frozen=True)
class AssistantProfile:
    ollama_url: str
    embedding_model: str
    response_model: str
    top_k: int
    connect_timeout_ms: int
    request_timeout_ms: int

    def __post_init__(self):
        validate_ollama_url(self.ollama_url)
        for model in (self.embedding_model, self.response_model):
            if (not isinstance(model, str) or len(model) > 256
                    or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._:/-]*", model) is None):
                _invalid()
            tag = model.rsplit(":", 1)[-1].casefold() if ":" in model else ""
            if tag == "cloud" or tag.endswith("-cloud"):
                _invalid()
        for limit in (self.top_k, self.connect_timeout_ms, self.request_timeout_ms):
            if type(limit) is not int or limit <= 0:
                _invalid()


@dataclass(frozen=True)
class AssistantConfig:
    schema: str
    active_profile: str
    bruce_program: Path
    index_path: Path
    allowed_roots: tuple[Path, ...]
    require_confirmation: bool
    profiles: Mapping[str, AssistantProfile]

    @property
    def profile(self) -> AssistantProfile:
        return self.profiles[self.active_profile]


def _absolute_path(value):
    if not isinstance(value, str) or not value or "\x00" in value:
        raise AssistantError("UnsafeAssistantPath")
    path = Path(value)
    if not path.is_absolute():
        raise AssistantError("UnsafeAssistantPath")
    return path


def _resolve(path):
    try:
        return path.resolve()
    except (OSError, RuntimeError, ValueError):
        raise AssistantError("UnsafeAssistantPath") from None


def _inside(path, root):
    return path == root or root in path.parents


def load_config(path: Path, profile_override: str | None = None) -> AssistantConfig:
    """Read explicit JSON, validating every profile and path before any I/O client."""
    try:
        with path.open("rb") as source:
            raw = source.read(1024 * 1024 + 1)
        if len(raw) > 1024 * 1024:
            _invalid()
        document = strict_json_loads(raw.decode("utf-8"))
    except (OSError, UnicodeError, ValueError, RecursionError):
        raise AssistantError("InvalidAssistantConfig") from None
    fields = {"schema", "active_profile", "bruce_program", "index_path",
              "allowed_roots", "require_confirmation", "profiles"}
    if (not isinstance(document, dict) or set(document) != fields
            or document["schema"] != "bruce-local-ai-config-v1"
            or document["require_confirmation"] is not True
            or not isinstance(document["active_profile"], str)
            or not document["active_profile"]):
        _invalid()
    profiles = document["profiles"]
    if not isinstance(profiles, dict) or not profiles:
        _invalid()
    parsed_profiles = {}
    profile_fields = set(AssistantProfile.__dataclass_fields__)
    for name, profile in profiles.items():
        if (not isinstance(name, str) or not name.strip() or not isinstance(profile, dict)
                or set(profile) != profile_fields):
            _invalid()
        parsed_profiles[name] = AssistantProfile(**profile)
    selected = document["active_profile"] if profile_override is None else profile_override
    if (not isinstance(selected, str) or selected not in parsed_profiles
            or document["active_profile"] not in parsed_profiles):
        raise AssistantError("UnknownAssistantProfile")

    roots = document["allowed_roots"]
    if not isinstance(roots, list) or not roots:
        raise AssistantError("UnsafeAssistantPath")
    lexical_roots = tuple(_absolute_path(root) for root in roots)
    canonical_roots = tuple(_resolve(root) for root in lexical_roots)
    if any(not root.is_dir() for root in canonical_roots):
        raise AssistantError("UnsafeAssistantPath")
    paths = []
    for field in ("bruce_program", "index_path"):
        original = _absolute_path(document[field])
        canonical = _resolve(original)
        for lexical_root, canonical_root in zip(lexical_roots, canonical_roots):
            if _inside(original, lexical_root) and not _inside(canonical, canonical_root):
                raise AssistantError("UnsafeAssistantPath")
        paths.append(canonical)
    program, index = paths
    if (not program.is_file() or not os.access(program, os.X_OK)
            or not index.parent.is_dir() or (index.exists() and not index.is_file())):
        raise AssistantError("UnsafeAssistantPath")
    return AssistantConfig(document["schema"], selected, program, index,
                           canonical_roots, True, MappingProxyType(parsed_profiles))
