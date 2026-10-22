"""Explicit confirmation and direct execution with bounded retained streams."""

from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import selectors
import signal
import subprocess
import time

from .config import AssistantError, strict_json_loads


MAX_STDOUT_BYTES = 1024 * 1024
MAX_STDERR_BYTES = 256 * 1024
MAX_EXECUTION_MS = 120000


@dataclass(frozen=True)
class BruceProcessResult:
    argv: tuple[str, ...]
    raw_stdout: bytes
    raw_stderr: bytes
    exit_code: int
    status: str
    parse_state: str
    result: dict | None
    source_result_digest: str
    stdout_truncated: bool
    stderr_truncated: bool


def _kill_group(process):
    # The child has a private session, so timeout also closes pipes inherited
    # by its descendants without touching the assistant's own process group.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def execute_confirmed(argv: list[str], confirmation: str, timeout_ms: int) -> BruceProcessResult:
    """Only the exact literal `run` authorizes this already canonical vector."""
    if type(confirmation) is not str or confirmation != "run":
        raise AssistantError("AssistantConfirmationRejected")
    if (type(argv) is not list or not argv or len(argv) > 128
            or any(type(value) is not str or not value or "\x00" in value or len(value) > 65536 for value in argv)
            or not Path(argv[0]).is_absolute()
            or sum(len(value) for value in argv) > 256 * 1024
            or type(timeout_ms) is not int or timeout_ms <= 0):
        raise AssistantError("AssistantExecutionInvalid")
    vector = tuple(argv)
    try:
        process = subprocess.Popen(vector, shell=False, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"},
                                   start_new_session=True)
    except (OSError, ValueError):
        raise AssistantError("AssistantExecutionFailed") from None
    buffers = {"stdout": bytearray(), "stderr": bytearray()}
    limits = {"stdout": MAX_STDOUT_BYTES, "stderr": MAX_STDERR_BYTES}
    truncated = {"stdout": False, "stderr": False}
    status = "completed"
    deadline = time.monotonic() + min(timeout_ms, MAX_EXECUTION_MS) / 1000
    try:
        with selectors.DefaultSelector() as selector:
            for name, stream in (("stdout", process.stdout), ("stderr", process.stderr)):
                os.set_blocking(stream.fileno(), False)
                selector.register(stream, selectors.EVENT_READ, name)
            while selector.get_map():
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    if status != "completed":
                        break
                    status = "timeout"
                    _kill_group(process)
                    deadline = time.monotonic() + 0.2
                    continue
                for key, _ in selector.select(remaining):
                    try:
                        chunk = os.read(key.fileobj.fileno(), 4096)
                    except BlockingIOError:
                        continue
                    if not chunk:
                        selector.unregister(key.fileobj)
                        continue
                    name = key.data
                    available = limits[name] - len(buffers[name])
                    buffers[name].extend(chunk[:available])
                    if len(chunk) > available:
                        truncated[name] = True
                        if status == "completed":
                            status = "output_limit"
                            _kill_group(process)
                            deadline = time.monotonic() + 0.2
            # Closing both streams does not mean a process has terminated.
            if status == "completed":
                try:
                    process.wait(timeout=max(0, deadline - time.monotonic()))
                except subprocess.TimeoutExpired:
                    status = "timeout"
                    _kill_group(process)
            else:
                _kill_group(process)
            process.wait(timeout=1)
    except (OSError, ValueError, subprocess.SubprocessError):
        raise AssistantError("AssistantExecutionFailed") from None
    finally:
        if process.poll() is None:
            _kill_group(process)
            process.wait()
        process.stdout.close()
        process.stderr.close()
    stdout, stderr = bytes(buffers["stdout"]), bytes(buffers["stderr"])
    parsed = None
    parse_state = "incomplete" if status != "completed" else "empty"
    if status == "completed" and stdout:
        try:
            parsed = strict_json_loads(stdout.decode("utf-8"))
            if type(parsed) is not dict:
                raise ValueError("not an object")
            parse_state = "valid_json"
        except (UnicodeError, ValueError, RecursionError):
            parsed = None
            parse_state = "invalid_json"
    return BruceProcessResult(vector, stdout, stderr, process.returncode, status, parse_state, parsed,
                              hashlib.sha256(stdout).hexdigest(), truncated["stdout"], truncated["stderr"])
