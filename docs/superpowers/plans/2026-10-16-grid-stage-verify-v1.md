# Grid Stage Verify V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a replayable verifier that proves the fixed scaled-exponential rule across the implemented finite Stage domain `0..57`.

**Architecture:** A new application run binds a complete replayed Sweep V1 source, validates its exact domain/mode, compares every value with the fixed formula and records either domain verification or first refutation. It uses an independent V1 header and integrates with generic replay/inspection without altering Sweep or legacy formats.

**Tech Stack:** C++20, existing Sweep/replay/evidence code, CMake, CTest, Python CLI tests.

**Spec:** `docs/superpowers/specs/2026-10-16-grid-stage-verify-v1-design.md`

## Global Constraints

- Only full `0..57`, completed, Exact or Metric Sweep V1 sources are admissible.
- The rule/formula and finite claim scope are fixed; no formula search, free rules or theorem-prover claim.
- Invalid source produces no partial verification run; refutation is a valid replayable result.
- Header is exactly `BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1`; existing bytes stay unchanged.
- Git unavailable; generated docs only through temporary-copy verifier.

## Review Focus

- Metric results must be compared with expected bit lengths, Exact with expected values; Task 1 pins both.
- A validly rehashed but altered source must become REFUTED or fail source replay; Task 1 pins both branches.
- Incomplete/modular/range-mismatched sources must fail before any verify bundle exists; Task 1 pins them.
- Generic replay/inspect/assessment must state finite scope only; Task 2 pins it.
- Existing Sweep/Grid/Batch/Resume bytes remain unchanged; Task 2 pins representative compatibility.

### Task 1: Verification core and evidence replay

**Files:** Create `include/bruce/application/grid_stage_verify.hpp`, `src/application/grid_stage_verify.cpp`, `tests/unit/grid_stage_verify_tests.cpp`; modify CMake only as required.

**Interfaces:** Consumes Sweep V1 APIs; produces verify task/run, evaluate/render/pack/replay APIs.

- [ ] Write failing tests for Exact/Metric success, first refutation, invalid modular/incomplete/range sources, bound source digest, success/refutation replay and rehashed mutations.
- [ ] Run target/CTest RED; expected absent interface/target.
- [ ] Implement source replay/validation, fixed formula comparison, `VERIFIED_ON_DOMAIN`/`REFUTED`, V1 payload header and full-payload replay.
- [ ] Run focused core GREEN.

### Task 2: CLI, application integration and compatibility

**Files:** Modify application task/replay integration as required, `apps/bruce_cli.cpp`, CLI/unit tests.

- [ ] Add failing tests for `grid-stage-verify`, generic replay/inspect/assessment, help and controlled invalid-source errors.
- [ ] Witness RED.
- [ ] Add only closed `--from-sweep`/output command, generic dispatch and finite claim scope.
- [ ] Run focused GREEN and representative legacy byte comparisons.

### Task 3: Documentation, final verification and review

**Files:** Modify commands/formats/architecture/spec status; create `runs/grid-stage-verify-v1/reports/final-review.md`.

- [ ] Document finite-domain meaning, source limits, refutation and non-goals.
- [ ] Run native build, relevant tests, links, Git limitation checks and temporary-copy CLI verifier with hashes.
- [ ] Record evidence; update status only after success; require fresh whole-change review.

## Plan self-review

- Core owns source validation/formula/evidence; integration owns public route; final task owns docs/evidence.
- Each implementation task starts RED and has focused GREEN.
- Every review-focus risk has a named owning task.
