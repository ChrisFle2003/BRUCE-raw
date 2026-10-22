# DAG-Scheduler V3 Inspector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a read-only `inspect` route for validated V3 DAG Specs, exposing deterministic static order and metadata without scheduling work.

**Architecture:** Reuse V3 `parse_spec()` and V2 structural validation. Add a pure local Kahn-order helper plus a dedicated inspector renderer; it must not call V3 callbacks or `execute_dag_scheduler_v2`. Extend the existing V3 integration/unit tests and documentation contracts.

**Tech Stack:** C++20, existing V3 CLI adapter/V2 DAG types, CMake/CTest, Python `unittest`, Markdown.

**Spec:** `docs/superpowers/specs/2026-10-13-dag-scheduler-v3-inspect-design.md`

## Global Constraints

- Do not modify V2 headers/sources or change successful V3 `execute` bytes.
- Public routes are exactly `bruce dag-scheduler-v3 execute SPEC` and `bruce dag-scheduler-v3 inspect SPEC`; invalid routes use the exact combined usage JSON specified by the Spec.
- Inspector always uses `parse_spec()`, performs no callback invocation and never calls `execute_dag_scheduler_v2`.
- The report is `dag-scheduler-v3-inspect-v1` and contains only the Spec digest, IDs, callbacks, dependency IDs, request sizes/digests, budgets and planned node order; no raw request/payload bytes or request hex.
- Retain all existing V3 format boundaries including the 65,536-byte canonical input limit and the permanent 65,537-byte regression.
- Do not hand-edit generated CLI artifacts. Preserve execute, V1/V2 and Phase-DAG behavior; Inspector works with NanoCell/Cockpit OFF.
- User-selected execution method is Subagent-driven after plan approval.

## Review Focus

- A seemingly read-only path may accidentally call `execute`; Task 1 tests callback-free planning through a sentinel callback and code review checks the call graph.
- A plan can silently diverge from V2 order; Task 1 asserts a fan-in/fan-out Kahn fixture and V3’s reference DAG order.
- Inspector serialization can leak source hex even when execution does not; Task 2 process tests forbid raw bytes and request-hex text.
- A new `inspect` route can weaken execute’s exact usage/output contract; Task 2 repeats execute byte-stability and route errors.
- Input failures must yield no partial JSON; Task 2 reuses invalid header, cycle and 65.537-byte specs against inspect.

---

## File Structure

| File | Responsibility |
|---|---|
| `apps/dag_scheduler_v3_cli.hpp` | Pure planned-order helper, inspector renderer and exact CLI route selection. |
| `tests/unit/dag_scheduler_v3_cli_tests.cpp` | Callback-free planner and metadata-only renderer contracts. |
| `tests/integration/test_dag_scheduler_v3_cli.py` | Real-binary inspect contract, error boundary and execute regression. |
| `tests/integration/test_cli_docs.py` | Pins documented Inspector behavior to public contract. |
| `docs/cli/README.md` | Lists V3 inspect capability and scope. |
| `docs/cli/commands.md` | Documents syntax, result contract and non-execution boundary. |
| `docs/cli/formats.md` | Connects the existing V3 Spec grammar to both execute and inspect. |
| `docs/architecture/03-blueprint-consolidation.md` | Records inspector as V3 adapter functionality, not a new scheduler. |

### Task 1: Pure Inspector Planner and C++ Contract

**Files:**
- Modify: `apps/dag_scheduler_v3_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v3_cli_tests.cpp`

**Interfaces:**
- Consumes: validated `DagSchedulerSpecV2` from `ParsedDagSchedulerSpecV3`.
- Produces: `planned_start_order(const DagSchedulerSpecV2&) -> std::vector<std::string>` and `render_inspect(const ParsedDagSchedulerSpecV3&) -> std::string` in `dag_scheduler_v3_cli_detail`.

- [ ] **Step 1: Write failing C++ planner/renderer tests**

Add tests that construct a valid V3 Spec, call `parse_spec`, then require the exact static order `a-source`, `b-transform`, `c-independent`, `d-join`; require inspector JSON metadata and absence of decoded/raw hex. Add a branch/fan-in fixture proving lexicographically smallest ready-node selection. Add a sentinel callback registry assertion or equivalent direct call-graph assertion showing that the planner has no callback entry point.

- [ ] **Step 2: Run the unit target to prove RED**

Run: `ctest --test-dir /tmp/bruce-dag-scheduler-v3-inspect -R '^dag_scheduler_v3_cli_contracts$' --output-on-failure`

Expected: FAIL because inspector interfaces do not exist.

- [ ] **Step 3: Implement pure `planned_start_order` and `render_inspect`**

Use only node IDs/dependencies and a sorted ready set. Validate first through existing V2 validation and return every node exactly once. Render `contract`, `action`, `spec_version`, `spec_digest`, `scheduler_version`, `dag_id`, both budgets, `nodes` in canonical Spec order and `planned_start_order`. For each node render only `node_id`, `callback_id`, `dependencies`, `request_size`, `request_digest`.

- [ ] **Step 4: Run the C++ unit contract green**

Run the same CTest selector. Expected: PASS.

### Task 2: CLI Route and Real-binary Inspector Contract

**Files:**
- Modify: `apps/dag_scheduler_v3_cli.hpp`
- Modify: `tests/integration/test_dag_scheduler_v3_cli.py`

**Interfaces:**
- Consumes: Task 1’s `render_inspect` and existing parse contract.
- Produces: exact `bruce dag-scheduler-v3 inspect SPEC` behavior, while preserving `execute` bytes.

- [ ] **Step 1: Write failing process tests**

Add a real-binary `inspect` helper and assert the four-node fixture’s full JSON object, order, digest, node metadata and absence of raw data/source hex. Test repeated byte identity, empty/NUL requests, invalid header, cycle, and canonical 65.537-byte file with empty stdout on error. Update usage tests to require `Usage: bruce dag-scheduler-v3 execute|inspect SPEC`; retain an execute byte-stability assertion.

- [ ] **Step 2: Prove process RED**

Run: `ctest --test-dir /tmp/bruce-dag-scheduler-v3-inspect -R '^dag_scheduler_v3_cli_workflows$' --output-on-failure`

Expected: FAIL because `inspect` is not accepted.

- [ ] **Step 3: Wire `inspect` without touching execution**

Accept only `execute` or `inspect` with one non-option path. For `inspect`, parse once and print `render_inspect`; for `execute`, preserve current parse/execute/render path and output. Invalid route errors must be compact and exact.

- [ ] **Step 4: Run focused green verification**

Run V3 unit/process and V2 unit/process tests in standard and NanoCell/Cockpit-OFF native builds. Expected: all selected tests pass.

### Task 3: Documentation, Verifier and Full Acceptance

**Files:**
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `tests/integration/test_cli_docs.py`

**Interfaces:**
- Consumes: final route and report behavior from Tasks 1–2.
- Produces: documentation marked **IMPLEMENTIERT UND BELEGT** with a matching documentation test.

- [ ] **Step 1: Add a failing V3 Inspector documentation test**

Require the V3 section to contain exact inspect syntax/usage, contract, static Kahn order, metadata-only fields, raw-data boundary, no callback execution and all stated non-goals.

- [ ] **Step 2: Document implemented behavior only**

Update manuals and blueprint with Inspector semantics. State clearly that it validates and plans but cannot predict callback outcomes, payload sizes, receipt budgets or cancellation effects. Preserve Phase-DAG wording and all unrelated contracts.

- [ ] **Step 3: Run documentation and generator verification**

Run the documentation test, then the existing verifier only in a full native temporary copy. Retain generated changes only if produced by that verifier; document if it does not execute inspector.

- [ ] **Step 4: Full acceptance**

Run the prescribed native configure/build/CTest sequence, report pass/fail/skip counts and isolate the known sandbox Cockpit socket result. Verify local Markdown links and whitespace. Git is unavailable: report this rather than fabricating a commit.

## Plan Self-Review

- **Spec coverage:** Tasks 1–2 implement all runtime rules; Task 3 covers documentation, generator limits and full verification.
- **Type consistency:** Task 1 produces the only two new internal interfaces consumed by Task 2.
- **Review focus:** Each listed risk has a named owner test in Task 1 or 2.
- **Scope:** Three reviewable tasks add one read-only route without extending V2, persistence or execution capabilities.
