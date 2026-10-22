# Logic-Mod Artifact DAG V3 Parallel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add bounded deterministic physical waves to Logic-Mod Artifact DAG.

**Architecture:** V3 wraps the accepted V2 graph/record model with a host-only
wave scheduler. It admits at most four independent workers concurrently, then
adopts results in existing stable order; V2 behavior remains the `1` limit.

**Tech Stack:** C++20, existing Logic-Mod DAG V2/Worker V1, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v3-parallel-design.md`

## Global Constraints

- `--max-parallel-nodes` accepts only 1..4 and defaults to 1.
- The value never enters Spec digest, DAG ID, record or replay.
- A wave contains only independent ready nodes; newly ready nodes wait.
- Await/reap every started worker before error return or a new wave.
- No Worker/FD/Seccomp contract changes; no retry or general scheduler.

## Review Focus

- Equal records for limits 1..4: Task 1.
- No same-wave successor start: Task 1 worker-event test.
- Invalid CLI limit before file access: Task 2.
- Failed wave prevents future starts while reaping started workers: Task 1/2.
- Replay remains no-spawn and has no parallel option: Task 2.

### Task 1: V3 wave scheduler core

**Files:**
- Create: `include/bruce/mod/logic_mod_artifact_dag_v3.hpp`
- Create: `src/mod/logic_mod_artifact_dag_v3.cpp`
- Create: `tests/unit/logic_mod_artifact_dag_v3_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:** Define `LogicModArtifactDagControlV3 { uint32_t max_parallel_nodes=1; }` and V3 run/record/replay APIs using the V2 node model.

- [ ] Write RED tests for limits, byte-identical records, overlap, barriers, failure/reaping and replay.
- [ ] Run RED for missing target/API.
- [ ] Implement host-only waves with stable result adoption and unchanged V2 artifacts/records.
- [ ] Run focused GREEN and obtain fresh review.

### Task 2: V3 CLI and real workflows

**Files:**
- Create: `apps/logic_mod_artifact_dag_v3_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_logic_mod_artifact_dag_v3.py`
- Modify: `CMakeLists.txt`

- [ ] Write real-binary RED proving the command absent.
- [ ] Implement strict V3 syntax, optional run-only limit and V2-compatible safe Spec/record handling.
- [ ] Test invalid limits before access, overlap, deterministic outputs, failure stop and workerfree replay.
- [ ] Run focused GREEN and obtain fresh review.

### Task 3: Docs and final verification

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v3-parallel-design.md`
- Create: `runs/logic-mod-artifact-dag-v3/reports/final-review.md`

- [ ] Document verified V3 behavior only; never hand-edit generated CLI artifacts.
- [ ] Run verifier, tests, links and whitespace; obtain whole-change review.

## Plan Self-Review

Tasks separate scheduler, untrusted CLI and documentation. Every V3 Spec
requirement maps to a testable task without expanding the Worker boundary.
