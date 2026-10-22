# Local security boundaries

**IMPLEMENTIERT UND BELEGT:** This document records current source-backed and
test-backed boundaries for local inspection and artifact workflows. It is not
an authentication, authorization, or internet-exposure design.

## Scope

BRUCE accepts explicitly named command arguments and files. It does not expose
a general shell, a general filesystem browser, or a dynamic executable-plugin
loader. A successful command does not authorize later commands or a different
caller.

## MCP

The optional MCP inspector is a local, read-only boundary. Its command is
`bruce mcp [--port 1..65535]`; the service binds only to `127.0.0.1`.
It accepts the fixed inspection routes documented in the CLI manual and does
not create research runs, jobs, imports, persistent uploads, executable
plugins, or filesystem browsing routes.

Inspection responses expose selected metadata only. Raw request, context,
output, evidence, and payload bytes are not returned by the documented
inspection API. This filtering is not encryption: local bundles can contain
confidential bytes and remain protected by their storage location and operating
system permissions.

## Artifact and remote-store paths

Artifact-store and payload commands use explicit local paths and bounded
formats. Store implementations validate identifiers, sizes, and selected
filesystem entries before publication or resolution. Remote-store variants are
controlled command paths, not a general network proxy: callers provide the
documented endpoint and artifact inputs; BRUCE does not derive shell commands
from those values.

No BRUCE command may translate model, user, bundle, or remote response text
into an arbitrary subprocess invocation. Worker process boundaries remain
fixed implementation paths with explicit arguments and recorded contracts.

## Installed Mods

**IMPLEMENTIERT UND BELEGT:** the durable mod lifecycle is scoped to the
invoking local Linux user. It uses an explicitly named private root and trust
store; it creates no account, privileged helper, background service, global
installation, remote repository, or multi-user authorization layer. Built-in
IDs cannot be mutated through it.

Package inspection accepts only bounded regular files without following a
final symlink. Logic packages contain one signed worker, but the builder never
compiles or invokes it. Installation stores it owner-only; a fresh binding
rechecks and pins exact bytes before the existing memfd/rlimit/Seccomp worker
supervisor starts. Package text, routes, and worker output never become shell
commands. Updates retain older versions and require explicit activation for a
rollback. See the [Local Mod Lifecycle V2 contract](../contracts/10-local-mod-lifecycle-v2.md).

## MCP or external service integration

**IMPLEMENTIERT UND BELEGT:** `tools/bruce_mcp.py` provides a separate local
stdio MCP adapter for a deliberately small typed BRUCE surface. It has no HTTP
listener, socket, browser, remote-store, publication, worker, MCP Inspector, or
artifact-store tool. It allows only fixed argument schemas, simple asset names
below `BRUCE_MCP_ASSET_ROOT`, four enumerated in-memory demos, exact arithmetic,
fixed-budget research, and read-only inspect/replay/assess/gate operations.
Requests are limited to 16 KiB, combined subprocess output and returned text to
64 KiB, and subprocesses to ten seconds. Under MCP `2025-06-18`, successful
strict BRUCE JSON remains in text `content` and is additionally returned as an
object in `structuredContent`; negotiated `2024-11-05` clients keep the former
text-only representation. The adapter never accepts a shell command line or
arbitrary CLI argument vector. Its CTest coverage is `bruce_mcp_workflows`.
It is not the optional built-in `bruce mcp` loopback HTTP inspector, and neither
interface installs, updates, activates, disables, or executes installed Mods.

### Opt-in stdio audit trace

**IMPLEMENTIERT UND BELEGT:** the stdio adapter version
`0.3` can record a local `bruce-mcp-trace-v1` JSONL trace. Set both
`BRUCE_MCP_RUN_ID` and `BRUCE_MCP_TRACE_PATH`; with neither set it opens no
trace. The run ID is 1–128 ASCII characters from `[A-Za-z0-9._-]`, a caller
label, not author identity. The path must be absolute and normalized, its
existing parent directories real and free of symlinks. No directories are
created. Existing targets, including symlinks, are rejected. Creation is
exclusive, append-only through the opened descriptor, with mode `0600`;
the writer checks the regular file and sets `0600` explicitly.

The header identifies the adapter/version, run ID and creation time. Every
stdin line produces one exchange with wall-clock timestamps, monotonic
duration, request length/digest, request text/parsed JSON, and the exact
selected response text/JSON, or null response fields for notifications.
The existing 16384-byte size decision counts the supplied line terminator.
For recorded `request_text`, `request_bytes` and `request_sha256`, exactly one
LF or CRLF terminator is removed. Subprocess stdin preserves CRLF; the trace
does not claim original bytes before Python text decoding. Oversized input
keeps only request length/digest and the full `MessageTooLarge` response.
Regular EOF adds a footer with the message count and final chain hash.

For in-limit input, structured `request` is null if parsing fails or the
parsed value cannot be represented by the trace's canonical UTF-8 JSON.
Examples are infinity produced by `1e400` and escaped lone UTF-16 surrogates.
The complete `request_text`, byte count and digest remain recorded; the value
is not coerced into another structured value. Existing parsing and MCP
responses are preserved, and subsequent messages and the EOF footer continue
to be captured. Consumers needing those values must use the retained raw
text. The verifier accepts this null representation without reparsing it.

Each record is synchronized with `fsync` before the selected response is sent
to stdout. This establishes the selected payload, not client receipt.
Invalid/partial environment configuration or unsafe file creation fails before
stdin with `BruceMcpTraceConfigurationError` on stderr and exit 2. Write/sync
or response output failure stops the adapter with
`BruceMcpTraceWriteError` and exit 3, without sending a further response.
Aborts can leave an incomplete trace; a partially written record can instead
be invalid. Footer rollback after storage failure is best effort.

Verify locally without replaying tools or reading configured assets:

```sh
python3 tools/verify_bruce_mcp_trace.py /absolute/private/session.jsonl
```

The single JSON result reports `VALID` for a complete consistent chain,
`INCOMPLETE` for a valid prefix without a footer, `TAMPERED` for chain,
ordering, ID or count contradictions, and `INVALID_FORMAT` for malformed or
unsupported records. Only `VALID` exits 0; the other statuses exit 1.

Trace data is confidential: request/response bodies may themselves contain
local filenames, inputs and results. There is no automatic redaction or
encryption. Protect the directory and file as run artifacts. The adapter
does not add environment variables, configured executable/asset-root/trace
paths, subprocess arguments or internal Python exception details; data already
in a request or response remains recorded.

The SHA-256 chain checks consistency, not author identity and not a mathematical proof.
It is not a digital signature, authenticated user claim, remote audit service,
or proof of client receipt. A local user can rewrite the whole trace and
recompute all hashes; an independently retained expected final hash or original
file is needed to compare against that rewrite. The trace does not extend the
separate `bruce mcp` HTTP inspector or its metadata-only response contract.
See the [trace format and tests](../superpowers/specs/2026-10-18-bruce-mcp-audit-trace-design.md).

**OFFENE ENTSCHEIDUNG:** Exposing BRUCE through an MCP server reachable beyond the
local stdio client, an API server, a reverse proxy, or a non-loopback listener
requires a separate design and threat model. That design must at least define
authentication, authorization by operation and path, tenant/workspace
isolation, request and response size limits, timeouts, rate limiting, audit
logging, secret handling, and an incident-response path.

Until such a design is implemented and verified, external exposure is outside
the supported security boundary. An integration layer should offer typed,
bounded operations such as replay inspection or a selected research task; it
must not forward arbitrary CLI argument vectors, arbitrary filesystem paths,
or arbitrary process commands.

## Evidence

- `apps/bruce_cli.cpp` — fixed command dispatch and structured error output.
- `include/bruce/io/local_files.hpp` — bounded local file and publication API.
- `include/bruce/artifact/artifact_store_v1.hpp` — descriptor-relative,
  bounded artifact-store operations.
- `docs/cli/commands.md` — current MCP and artifact command contracts.
- `tests/mods/mcp/` and `tests/integration/test_artifact_store_v*.py` —
  contract evidence, subject to host capabilities such as local test sockets.
