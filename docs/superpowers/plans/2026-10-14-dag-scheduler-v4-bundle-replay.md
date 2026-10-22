# DAG-Scheduler V4 Bundle and Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export bounded V3 scheduler runs as canonical local bundles and verify them through read-only inspect/replay routes.

**Architecture:** Add a V4 library codec over existing V2 run types and the fixed V3 callback catalog. V4 CLI owns exclusive atomic publication and metadata rendering; decoder/replay validates bytes, reconstructs V2 data, reexecutes the fixed callbacks, and demands canonical equality.

**Tech Stack:** C++20, V2 scheduler core, V3 fixed callback registry, LocalFiles, SHA-256, CMake/CTest, Python integration tests.

**Spec:** `docs/superpowers/specs/2026-10-14-dag-scheduler-v4-bundle-replay-design.md`

## Global Constraints

- Do not alter V2/V3 public behavior or V3 execute bytes.
- V4 bundle header is `BRUCE_DAG_SCHEDULER_RUN_V4`; maximum file size is 5 MiB.
- Bundle raw request/payload bytes are confidential local data; all V4 stdout reports remain metadata-only.
- Decoder performs bounded length/count validation before allocations and rejects trailing/noncanonical bytes.
- V4 uses only fixed `literal-v1`, `append-v1`, `join-v1`; no arbitrary callbacks, persistence service, network, Cockpit, parallelism or migration.
- `execute` uses exclusive atomic output publication; `inspect`/`replay` never write.
- User selected Subagent-driven execution after plan approval.

## Review Focus

- Corrupt lengths can cause allocations before limits; Task 1 owns truncated/oversized/count mutation tests.
- Bundle validity without replay equality can hide altered receipts; Task 1 requires reexecution and full run comparison.
- Reports can accidentally expose embedded bytes; Task 2 forbids payload/request/hex text in all routes.
- Existing target overwrite can destroy evidence; Task 2 tests an existing sentinel output remains unchanged.
- V3 must remain unchanged; Task 2 pins its existing process output and Task 3 includes V3 regressions.

---

### Task 1: V4 Core Bundle Codec and Replay Contracts

**Files:**
- Create: `include/bruce/dag_scheduler/dag_scheduler_v4.hpp`
- Create: `src/dag_scheduler/dag_scheduler_v4.cpp`
- Create: `tests/unit/dag_scheduler_v4_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `std::string pack_dag_scheduler_run_v4(const DagSchedulerRunV2&)` and `DagSchedulerRunV2 replay_dag_scheduler_run_v4(std::string_view, DagSchedulerCallbackRegistryV2)`.

- [ ] Write failing unit tests for canonical pack/replay, byte equality, 5 MiB/field/count boundaries, and each mutation class from the spec.
- [ ] Run the new V4 unit CTest RED.
- [ ] Implement bounded length-prefixed codec, header/digest verifier, canonical decoder and replay equality.
- [ ] Run V4/V2 unit contracts GREEN.

### Task 2: V4 CLI and Real-binary Bundle Workflows

**Files:**
- Create: `apps/dag_scheduler_v4_cli.hpp`
- Create: `tests/integration/test_dag_scheduler_v4_cli.py`
- Modify: `apps/bruce_cli.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: V4 codec and fixed V3 parser/callback registry.
- Produces exact routes `execute SPEC --output BUNDLE`, `inspect BUNDLE`, `replay BUNDLE`.

- [ ] Write failing process tests for export, exclusive output, metadata-only reports, inspect/replay equality, tampering, exact usage and V3 byte stability.
- [ ] Prove RED against the absent V4 route.
- [ ] Implement V4 CLI, atomic output through LocalFiles, and V4 metadata renderers without raw bytes.
- [ ] Run V4 plus V2/V3 focused standard/OFF workflows GREEN.

### Task 3: Documentation and Full Acceptance

**Files:**
- Modify: `docs/cli/README.md`, `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/architecture/03-blueprint-consolidation.md`
- Modify: `tests/integration/test_cli_docs.py`

- [ ] Add failing documentation contract tests for V4 routes, raw-byte boundary, replay meaning and non-goals.
- [ ] Document only implemented V4 behavior and explicit bundle confidentiality.
- [ ] Run docs test, native verifier, link check, full native build/CTest and isolate the known socket environment result.

## Plan Self-Review

All V4 Spec requirements map to one task; codec and CLI boundaries are separate; every new route begins with a red real-behavior test; V2/V3 remain explicit regression targets.
