"""Private local JSONL traces for the BRUCE stdio MCP boundary."""
import hashlib
import json
import os
import re
import stat
import time
from collections.abc import Callable, Mapping

TRACE_VERSION = "bruce-mcp-trace-v1"
ADAPTER_VERSION = "0.3"
_RUN_ID = re.compile(r"[A-Za-z0-9._-]{1,128}", re.ASCII)
_HASH = re.compile(r"[0-9a-f]{64}", re.ASCII)
_ZERO_HASH = "0" * 64


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False, allow_nan=False)


def record_hash(record_without_hash: dict) -> str:
    return hashlib.sha256(canonical_json(record_without_hash).encode("utf-8")).hexdigest()


class TraceConfigurationError(Exception):
    def __init__(self):
        super().__init__("TraceConfigurationError")


class TraceWriteError(Exception):
    def __init__(self):
        super().__init__("TraceWriteError")


def _create_trace(path: os.PathLike[str] | str) -> int:
    """Pin each real parent directory before exclusively creating the leaf."""
    parent_fd = None
    target_fd = None
    try:
        raw = os.fspath(path)
        if (not isinstance(raw, str) or not os.path.isabs(raw)
                or os.path.normpath(raw) != raw or raw.startswith("//")):
            raise ValueError
        parent, leaf = os.path.split(raw)
        if not leaf or os.path.realpath(parent, strict=True) != parent:
            raise ValueError
        flags = os.O_RDONLY | os.O_DIRECTORY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0)
        parent_fd = os.open("/", flags)
        for component in parent.split("/")[1:]:
            if not component:
                continue
            next_fd = os.open(component, flags, dir_fd=parent_fd)
            os.close(parent_fd)
            parent_fd = next_fd
        target_fd = os.open(leaf, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_APPEND
                            | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_CLOEXEC", 0),
                            0o600, dir_fd=parent_fd)
        if not stat.S_ISREG(os.fstat(target_fd).st_mode):
            raise ValueError
        os.fchmod(target_fd, 0o600)
        return target_fd
    except (OSError, ValueError, TypeError):
        if target_fd is not None:
            os.close(target_fd)
        raise TraceConfigurationError() from None
    finally:
        if parent_fd is not None:
            os.close(parent_fd)


class TraceWriter:
    def __init__(self, path: os.PathLike[str] | str, run_id: str, *,
                 wall_clock_ns: Callable[[], int] = time.time_ns,
                 monotonic_ns: Callable[[], int] = time.monotonic_ns):
        if not isinstance(run_id, str) or _RUN_ID.fullmatch(run_id) is None:
            raise TraceConfigurationError()
        self._fd = _create_trace(path)
        self._run_id = run_id
        self._wall_clock_ns = wall_clock_ns
        self._sequence = 0
        self._previous_hash = _ZERO_HASH
        self._message_count = 0
        self._committed_size = 0
        self._failed = False
        self._append("header", adapter_name="bruce-mcp", adapter_version=ADAPTER_VERSION,
                     created_unix_ns=wall_clock_ns())

    def _release(self) -> None:
        fd, self._fd = self._fd, None
        if fd is not None:
            try:
                os.close(fd)
            except OSError:
                self._failed = True

    def _append(self, record_type: str, **fields: object) -> None:
        if self._failed or self._fd is None:
            raise TraceWriteError()
        record = {"trace_version": TRACE_VERSION, "run_id": self._run_id,
                  "record_type": record_type, "sequence": self._sequence,
                  "previous_hash": self._previous_hash, **fields}
        try:
            record["record_hash"] = record_hash(record)
            data = (canonical_json(record) + "\n").encode("utf-8")
            offset = 0
            while offset < len(data):
                try:
                    written = os.write(self._fd, data[offset:])
                except InterruptedError:
                    continue
                if written <= 0:
                    raise OSError
                offset += written
            while True:
                try:
                    os.fsync(self._fd)
                    break
                except InterruptedError:
                    continue
        except (OSError, ValueError, TypeError, OverflowError, RecursionError):
            self._failed = True
            if record_type == "footer":
                # A fully written but unsynced footer must not look complete.
                # Recovery remains best effort if the storage itself has failed.
                try:
                    os.ftruncate(self._fd, self._committed_size)
                    os.fsync(self._fd)
                except OSError:
                    pass
            self._release()
            raise TraceWriteError() from None
        self._previous_hash = record["record_hash"]
        self._sequence += 1
        self._committed_size += len(data)

    def append_exchange(self, *, request_text: str | None, request_bytes: int,
                        request_sha256: str, request: object | None,
                        response_text: str | None, response: dict | None, outcome: str,
                        received_unix_ns: int, completed_unix_ns: int,
                        duration_monotonic_ns: int) -> None:
        try:
            canonical_json(request).encode("utf-8")
        except (ValueError, OverflowError, RecursionError):
            # Parsed JSON may contain infinity or lone surrogates. Preserve
            # its raw text and digest without coercing the structured value.
            request = None
        self._append("exchange", request_text=request_text, request_bytes=request_bytes,
                     request_sha256=request_sha256, request=request, response_text=response_text,
                     response=response, outcome=outcome, received_unix_ns=received_unix_ns,
                     completed_unix_ns=completed_unix_ns, duration_monotonic_ns=duration_monotonic_ns)
        self._message_count += 1

    def close(self) -> None:
        if self._failed:
            raise TraceWriteError()
        if self._fd is None:
            return
        self._append("footer", message_count=self._message_count,
                     completed_unix_ns=self._wall_clock_ns())
        self._release()
        if self._failed:
            raise TraceWriteError()


def writer_from_environment(environ: Mapping[str, str]) -> TraceWriter | None:
    has_id = "BRUCE_MCP_RUN_ID" in environ
    has_path = "BRUCE_MCP_TRACE_PATH" in environ
    if not has_id and not has_path:
        return None
    if not has_id or not has_path:
        raise TraceConfigurationError()
    return TraceWriter(environ["BRUCE_MCP_TRACE_PATH"], environ["BRUCE_MCP_RUN_ID"])


def _reject_constant(_value):
    raise ValueError


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError
        result[key] = value
    return result


def _natural(value):
    return type(value) is int and value >= 0


def _valid_schema(record):
    if not isinstance(record, dict):
        return False
    common = {"trace_version", "run_id", "record_type", "sequence", "previous_hash", "record_hash"}
    kind = record.get("record_type")
    if not isinstance(kind, str):
        return False
    fields = {
        "header": {"adapter_name", "adapter_version", "created_unix_ns"},
        "exchange": {"request_text", "request_bytes", "request_sha256", "request", "response_text",
                     "response", "outcome", "received_unix_ns", "completed_unix_ns", "duration_monotonic_ns"},
        "footer": {"message_count", "completed_unix_ns"},
    }
    if kind not in fields or set(record) != common | fields[kind]:
        return False
    if (record["trace_version"] != TRACE_VERSION or not isinstance(record["run_id"], str)
            or not _RUN_ID.fullmatch(record["run_id"]) or not _natural(record["sequence"])):
        return False
    for key in ("previous_hash", "record_hash"):
        if not isinstance(record[key], str) or not _HASH.fullmatch(record[key]):
            return False
    if kind == "header":
        return (record["adapter_name"] == "bruce-mcp" and record["adapter_version"] == ADAPTER_VERSION
                and _natural(record["created_unix_ns"]))
    if kind == "footer":
        return _natural(record["message_count"]) and _natural(record["completed_unix_ns"])
    return (all(_natural(record[key]) for key in ("request_bytes", "received_unix_ns", "completed_unix_ns", "duration_monotonic_ns"))
            and isinstance(record["request_sha256"], str) and _HASH.fullmatch(record["request_sha256"]) is not None
            and (record["request_text"] is None or isinstance(record["request_text"], str))
            and (record["response_text"] is None or isinstance(record["response_text"], str))
            and (record["response"] is None or isinstance(record["response"], dict))
            and isinstance(record["outcome"], str)
            and record["outcome"] in {"response", "notification", "message_too_large"})


def _verification_result(status, reason, *, run_id=None, message_count=None, final_hash=None):
    return {"status": status, "trace_version": TRACE_VERSION if run_id is not None else None,
            "run_id": run_id, "message_count": message_count, "final_hash": final_hash, "reason": reason}


def verify_trace(path: os.PathLike[str] | str) -> dict[str, object]:
    """Read only the named trace; classify format before chain contradictions."""
    try:
        with open(path, "r", encoding="utf-8", newline="") as source:
            lines = source.readlines()
        records = []
        for line in lines:
            if not line.endswith("\n"):
                raise ValueError
            record = json.loads(line, parse_constant=_reject_constant, object_pairs_hook=_unique_object)
            if not _valid_schema(record) or canonical_json(record) + "\n" != line:
                raise ValueError
            records.append(record)
        if not records:
            raise ValueError
    except (OSError, ValueError, TypeError, OverflowError, RecursionError):
        return _verification_result("INVALID_FORMAT", "InvalidTraceFormat")

    run_id = records[0]["run_id"]
    previous = _ZERO_HASH
    count = 0
    footer = False
    for sequence, record in enumerate(records):
        if (record["sequence"] != sequence or record["run_id"] != run_id
                or record["previous_hash"] != previous
                or record["record_hash"] != record_hash({key: value for key, value in record.items() if key != "record_hash"})):
            return _verification_result("TAMPERED", "TraceChainMismatch", run_id=run_id, message_count=count)
        kind = record["record_type"]
        if (footer or (sequence == 0 and kind != "header") or (sequence > 0 and kind == "header")
                or (kind == "footer" and record["message_count"] != count)):
            return _verification_result("TAMPERED", "TraceOrderOrCountMismatch", run_id=run_id, message_count=count)
        if kind == "exchange":
            count += 1
        footer = kind == "footer"
        previous = record["record_hash"]
    return _verification_result("VALID" if footer else "INCOMPLETE", "TraceComplete" if footer else "MissingFooter",
                                run_id=run_id, message_count=count, final_hash=previous)
