# BRUCE MCP stdio adapter

**Status:** IMPLEMENTIERT UND BELEGT — lokaler stdio-Adapter, geprüft durch
`bruce_mcp_workflows` in CTest. Eine externe oder Netzwerk-Exponierung bleibt
eine offene Entscheidung.

## Goal

Provide a local stdio MCP adapter that exposes a deliberately small, typed
subset of BRUCE without opening a network listener or accepting arbitrary
shell command lines.

## Boundary

The adapter is a separate local process. It communicates with its MCP client
only through standard input/output and invokes the configured `bruce` binary
with fixed argument schemas. It does not call the OpenAI API and requires no
API key.

## Exposed tools

| Tool | BRUCE operation | Inputs |
|---|---|---|
| `bruce_calculate` | `calculate` | `op`, string operands, optional `domain`, `max_bits` (1..16384), and a positive decimal-string `modulus` required only for bigint `modpow` |
| `bruce_research` | `research` | approved training and holdout CSV asset names; fixed internal research budgets |
| `bruce_inspect_run` | `inspect-run` | approved run asset name |
| `bruce_replay_run` | `replay` | approved run asset name |
| `bruce_assess_run` | `assess-run` | approved run asset name |
| `bruce_gate_run` | `gate-run` | approved run asset name |
| `bruce_demo` | fixed DAG/reference demo | enumerated demo identifier only |

With negotiated MCP protocol `2025-06-18`, every successful tool call retains
the BRUCE JSON text in MCP `content` and also returns the parsed JSON object in
`structuredContent`; tool listings include `outputSchema`. A client requesting
the supported legacy protocol `2024-11-05` receives the compatible text content
without those newer fields. A successful subprocess response that is not one
strict JSON object, including non-finite JSON constants, is reported as a tool
error instead of being represented as structured data. BRUCE command failures
remain text tool errors with `isError: true`.

## Security rules

- stdio only; no HTTP listener, socket, browser, or remote-store access.
- Every file input is an asset name resolved below one configured workspace
  root; absolute paths, `..`, symlinks escaping that root, and arbitrary CLI
  argument vectors are rejected.
- The adapter exposes no publication, mod lifecycle, worker, Cockpit, or
  artifact-store tools in this version.
- JSON-RPC/MCP messages, tool parameters, subprocess output, and errors have
  explicit byte limits and timeouts.
- BRUCE JSON is returned as text and structured data; a BRUCE success never
  represents a general mathematical proof.

## Verification

`tests/integration/test_bruce_mcp.py` covers MCP initialization and tool
listing, all exposed operation classes, structured success results, calculation
option and cross-field validation, current/legacy protocol negotiation,
initialized-notification handling, stable malformed-request errors, rejected
path traversal and unknown tools, invalid/non-finite success JSON, and
subprocess-output limiting. The adapter implements a ten-second timeout and
keeps successful stdout separate from failed-command stderr; those two branches
are source-backed but do not currently have dedicated integration cases.

## Local launch

The adapter is a local process, not a BRUCE subcommand and not a network
service. Start it with an explicitly selected BRUCE executable and an approved
asset directory:

```sh
BRUCE_MCP_BRUCE=/absolute/path/to/bruce \
BRUCE_MCP_ASSET_ROOT=/absolute/path/to/approved-assets \
python3 tools/bruce_mcp.py
```

Its exposed tools are fixed: exact calculation, bounded research over two
approved CSV files, inspection/replay/assessment/gating of approved run files,
and four named DAG-scheduler demos. It accepts neither arbitrary shell commands
nor arbitrary file paths.
