"""Finite local cosine index with explicit freshness and exclusive publication."""

from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import tempfile

from .catalog import CatalogEntry
from .config import AssistantError, AssistantProfile, strict_json_loads


SCHEMA = "bruce-ai-command-index-v2"
MAX_DIMENSION = 65536
MAX_INDEX_BYTES = 16 * 1024 * 1024


def _invalid():
    raise AssistantError("AssistantIndexInvalid")


@dataclass(frozen=True)
class IndexEntry:
    command_id: str
    vector: tuple[float, ...]


@dataclass(frozen=True)
class IndexDocument:
    schema: str
    catalog_digest: str
    profile_name: str
    embedding_model: str
    dimension: int
    entries: tuple[IndexEntry, ...]

    def to_dict(self) -> dict:
        return {"schema": self.schema, "catalog_digest": self.catalog_digest,
                "profile_name": self.profile_name, "embedding_model": self.embedding_model,
                "dimension": self.dimension,
                "entries": [{"command_id": item.command_id, "vector": list(item.vector)}
                            for item in sorted(self.entries, key=lambda item: item.command_id)]}


@dataclass(frozen=True)
class RankedEntry:
    command_id: str
    score: float


def _finite_vector(vector):
    if not isinstance(vector, (list, tuple)) or not 1 <= len(vector) <= MAX_DIMENSION:
        _invalid()
    result = []
    for number in vector:
        if type(number) not in (int, float):
            _invalid()
        try:
            number = float(number)
        except (OverflowError, ValueError):
            _invalid()
        if not math.isfinite(number):
            _invalid()
        result.append(number)
    return result


def normalize(vector) -> tuple[float, ...]:
    """Scale before L2 norm so finite extreme magnitudes do not overflow."""
    values = _finite_vector(vector)
    scale = max(abs(value) for value in values)
    if scale == 0:
        _invalid()
    scaled = [value / scale for value in values]
    norm = math.sqrt(math.fsum(value * value for value in scaled))
    return tuple(value / norm for value in scaled)


def catalog_digest(catalog: list[CatalogEntry]) -> str:
    if (not isinstance(catalog, (list, tuple)) or not catalog
            or any(not isinstance(entry, CatalogEntry) for entry in catalog)
            or len({entry.command_id for entry in catalog}) != len(catalog)):
        _invalid()
    document = {"schema": "bruce-ai-command-catalog-v1",
                "entries": [entry.to_dict() for entry in sorted(catalog, key=lambda item: item.command_id)]}
    try:
        raw = json.dumps(document, sort_keys=True, ensure_ascii=False, allow_nan=False,
                         separators=(",", ":")).encode("utf-8")
    except (TypeError, ValueError, UnicodeError, RecursionError):
        raise AssistantError("AssistantIndexInvalid") from None
    return hashlib.sha256(raw).hexdigest()


def _check_shape(index):
    if isinstance(index, IndexDocument) and index.schema == "bruce-ai-command-index-v1":
        raise AssistantError("AssistantIndexStale")
    if (not isinstance(index, IndexDocument) or index.schema != SCHEMA
            or not isinstance(index.catalog_digest, str)
            or re.fullmatch(r"[0-9a-f]{64}", index.catalog_digest) is None
            or not isinstance(index.profile_name, str) or not index.profile_name.strip()
            or len(index.profile_name) > 256
            or not isinstance(index.embedding_model, str) or not index.embedding_model
            or len(index.embedding_model) > 256
            or type(index.dimension) is not int or not 1 <= index.dimension <= MAX_DIMENSION
            or not isinstance(index.entries, tuple) or not 1 <= len(index.entries) <= 24):
        _invalid()
    seen = set()
    for item in index.entries:
        if (not isinstance(item, IndexEntry) or not isinstance(item.command_id, str)
                or re.fullmatch(r"[a-z0-9][a-z0-9-]{0,127}", item.command_id) is None
                or item.command_id in seen):
            _invalid()
        vector = _finite_vector(item.vector)
        if (len(vector) != index.dimension
                or not math.isclose(math.hypot(*vector), 1.0, rel_tol=1e-12, abs_tol=1e-12)):
            _invalid()
        seen.add(item.command_id)


def build_index(catalog, profile: AssistantProfile, client, *, profile_name="default") -> IndexDocument:
    digest = catalog_digest(catalog)
    entries = []
    dimension = None
    for entry in sorted(catalog, key=lambda item: item.command_id):
        vector = normalize(client.embed(entry.embedding_text, profile))
        if dimension is not None and len(vector) != dimension:
            _invalid()
        dimension = len(vector)
        entries.append(IndexEntry(entry.command_id, vector))
    index = IndexDocument(SCHEMA, digest, profile_name, profile.embedding_model, dimension, tuple(entries))
    _check_shape(index)
    return index


def validate_index(index, catalog, profile: AssistantProfile, query_dimension=None, *, profile_name=None):
    """Admission gate to use before selecting candidates or asking a response model."""
    _check_shape(index)
    if query_dimension is not None and (type(query_dimension) is not int or query_dimension <= 0):
        _invalid()
    if (index.catalog_digest != catalog_digest(catalog)
            or index.embedding_model != profile.embedding_model
            or (query_dimension is not None and index.dimension != query_dimension)
            or (profile_name is not None and index.profile_name != profile_name)):
        raise AssistantError("AssistantIndexStale")
    if {entry.command_id for entry in index.entries} != {entry.command_id for entry in catalog}:
        _invalid()
    return index


def load_index(path: Path, catalog, profile: AssistantProfile, query_dimension=None, *, profile_name=None) -> IndexDocument:
    try:
        # A pathname precheck races replacement. Open without following the
        # final symlink or waiting for a FIFO writer, then admit that descriptor.
        # Platforms without both safeguards cannot safely load an index.
        nofollow = getattr(os, "O_NOFOLLOW", None)
        nonblock = getattr(os, "O_NONBLOCK", None)
        if not nofollow or not nonblock:
            _invalid()
        descriptor = os.open(path, os.O_RDONLY | nofollow | nonblock)
        try:
            if not stat.S_ISREG(os.fstat(descriptor).st_mode):
                _invalid()
            with os.fdopen(descriptor, "rb", closefd=False) as source:
                raw = source.read(MAX_INDEX_BYTES + 1)
        finally:
            os.close(descriptor)
        if len(raw) > MAX_INDEX_BYTES:
            _invalid()
        document = strict_json_loads(raw.decode("utf-8"))
        if (not isinstance(document, dict) or set(document) != set(IndexDocument.__dataclass_fields__)
                or not isinstance(document["entries"], list)):
            _invalid()
        entries = []
        for item in document["entries"]:
            if (not isinstance(item, dict) or set(item) != {"command_id", "vector"}
                    or not isinstance(item["vector"], list)):
                _invalid()
            entries.append(IndexEntry(item["command_id"], tuple(item["vector"])))
        index = IndexDocument(document["schema"], document["catalog_digest"], document["profile_name"],
                              document["embedding_model"], document["dimension"], tuple(entries))
    except FileNotFoundError:
        raise AssistantError("AssistantIndexMissing") from None
    except (OSError, UnicodeError, ValueError, TypeError, RecursionError):
        raise AssistantError("AssistantIndexInvalid") from None
    return validate_index(index, catalog, profile, query_dimension, profile_name=profile_name)


def write_index(path: Path, index: IndexDocument):
    """Write privately beside the target, fsync, then publish without replacement.

    A hard link is the atomic no-clobber commit: the destination is never
    visible with partial bytes, and an existing file or symlink wins the race.
    Only the exclusively created staging file is removed on failure.
    """
    _check_shape(index)
    try:
        raw = (json.dumps(index.to_dict(), sort_keys=True, ensure_ascii=False,
                          allow_nan=False, separators=(",", ":")) + "\n").encode("utf-8")
    except (TypeError, ValueError, UnicodeError, RecursionError):
        raise AssistantError("AssistantIndexInvalid") from None
    if len(raw) > MAX_INDEX_BYTES:
        _invalid()
    staging = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".bruce-ai-index-", dir=path.parent,
                                         mode="wb", delete=False) as output:
            staging = Path(output.name)
            output.write(raw)
            output.flush()
            os.fsync(output.fileno())
        os.link(staging, path, follow_symlinks=False)
    except FileExistsError:
        raise AssistantError("AssistantIndexExists") from None
    except OSError:
        raise AssistantError("AssistantIndexWriteFailed") from None
    finally:
        if staging is not None:
            try:
                staging.unlink()
            except OSError:
                # Never remove the destination after successful publication.
                # A failed staging cleanup does not invalidate committed bytes.
                pass


def rank(index: IndexDocument, query_vector, top_k: int) -> list[RankedEntry]:
    _check_shape(index)
    if type(top_k) is not int or top_k <= 0:
        _invalid()
    query = normalize(query_vector)
    if len(query) != index.dimension:
        raise AssistantError("AssistantIndexStale")
    scored = [RankedEntry(entry.command_id,
                          max(-1.0, min(1.0, math.fsum(left * right for left, right in zip(entry.vector, query)))))
              for entry in index.entries]
    return sorted(scored, key=lambda item: (-item.score, item.command_id))[:top_k]
