# BRUCE MCP stdio adapter Implementation Plan

**Goal:** Provide a local, typed stdio MCP boundary for a safe subset of BRUCE.

**Spec:** `docs/superpowers/specs/2026-10-17-bruce-mcp-stdio-design.md`

## Tasks

1. Add a dependency-free Python stdio JSON-RPC/MCP process under `tools/` with
   initialize, tools/list, and tools/call handling plus bounded messages.
2. Add an asset-root resolver that accepts only simple asset names and rejects
   absolute paths, traversal, and escaping symlinks.
3. Implement fixed schemas for calculate, research, inspect/replay, and an
   enumerated demo; invoke BRUCE without a shell and enforce output/time limits.
4. Add Python integration tests for initialization, dispatch, malformed input,
   path rejection, and BRUCE error propagation; register them in CTest.
5. Document local launch and trust boundaries; run fresh build and relevant
   tests from a safe path.
