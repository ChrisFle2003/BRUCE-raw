# Grid Stage Sweep V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded, replayable `grid-stage-sweep` command that makes every scaled-exponential stage and diagnostic artifact inspectable.

**Architecture:** A new application-layer Sweep model drives existing explicit-stage Grid evaluations through one fresh GridCache. Its versioned bundle stores compact records by default and full per-stage traces only with `--debug`; central replay dispatch recreates and compares the complete run. Existing Grid, Batch and Resume formats remain untouched.

**Tech Stack:** C++20, existing Grid/cache/replay code, CMake, CTest and Python CLI workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-grid-stage-sweep-v1-design.md`

## Global Constraints

- The command fixes the rule to `scaled-exponential-v1`, stages to a contiguous range `0..57`, and local coordinates to their existing `0..7` address domain.
- Sweep V1 has no resume, stage list, repeated stage or cross-run cache persistence.
- Exact stops with retained prior records at its first `IntegerBitLimit`; Metric and Modular retain current Grid semantics.
- `--debug` controls only trace retention/serialization, never values, order or cache identity.
- The only new evidence header is `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1`; do not alter legacy Grid/Batch/Resume bytes.
- Do not manually edit generated CLI artifacts; use the verifier in a complete temporary copy only.
- Git metadata remains unavailable; record that limitation without claiming commits/diffs.

## Review Focus

- Sweep stage 57 at 63 bits must retain records through 56 and stop exactly once with `IntegerBitLimit`; Task 1 pins it.
- `--debug` on/off must preserve compact records and cache keys while changing only traces/evidence; Task 1 pins it.
- Path `(0,0)` must evaluate all requested stages rather than silently stop; Task 1 pins it.
- Rehashed record, cache-statistic or trace mutations must fail canonical replay; Task 2 pins each class.
- Existing Grid/Batch/Resume outputs must remain byte-identical; Task 2 pins representative paths.

---

## File structure

| File | Responsibility |
| --- | --- |
| `include/bruce/application/grid_stage_sweep.hpp` | Public Sweep task/run/record interface. |
| `src/application/grid_stage_sweep.cpp` | Range validation, evaluation, debug rendering, bundle and replay. |
| `include/bruce/application/application_task.hpp` | Add Sweep to the application-run variant if required by central replay. |
| `src/application/application_replay.cpp` | Dispatch Sweep header and assess its status. |
| `apps/bruce_cli.cpp` | Parse/render `grid-stage-sweep` and advertise it in help. |
| `CMakeLists.txt` | Compile the new application source and register tests only if existing source lists require it. |
| `tests/unit/grid_stage_sweep_tests.cpp` | Core range, mode, debug, cache, stop and replay contracts. |
| `tests/integration/test_cli.py` | Public CLI, error, export and replay workflows. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/75-current-architecture-and-milestones.md` | Evidence-labeled final contract. |

### Task 1: Sweep core, debug records and canonical replay

**Files:**
- Create: `include/bruce/application/grid_stage_sweep.hpp`
- Create: `src/application/grid_stage_sweep.cpp`
- Modify: `CMakeLists.txt` if the source list is explicit
- Create: `tests/unit/grid_stage_sweep_tests.cpp`

**Interfaces:**
- Consumes: `GridTask`, `GridCache`, `grid_spec`, `render_grid`, bundle SHA-256 and replay checkpoint contracts.
- Produces: `GridStageSweepTask`, `GridStageSweepRun`, `evaluate_grid_stage_sweep`, `render_grid_stage_sweep`, `pack_grid_stage_sweep`, and `replay_grid_stage_sweep`.

- [ ] **Step 1: Write failing core contracts**

Add standalone unit assertions for Exact `0..7` values, Exact `0..57` at 63 bits retaining 57 processed records for stages 0..56 and stopping at 57, Metric/Modular stage 57, invalid reversed/out-of-range ranges, and `(0,0)` evaluation. Assert debug-off records have no traces, debug-on records do, while values/key digests/cache events match. Add complete/partial replay and rehashed stage/value/cache/trace mutation rejection.

- [ ] **Step 2: Run the core test to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_stage_sweep_tests && ctest --test-dir /tmp/bruce-build -R '^grid_stage_sweep_contracts$' --output-on-failure`

Expected: target or interface is absent.

- [ ] **Step 3: Implement the bounded Sweep V1 interfaces**

Define a task holding path, inclusive range, Grid mode/limits/modulus, cache budget and debug flag. Iterate in ascending order through explicit-stage `GridTask`s with `zero_zero="one"` and one fresh `GridCache`. On `IntegerBitLimit`, preserve prior records, set `INCOMPLETE` and record the failing stage/reason; rethrow all input-validation failures before publishing a run.

Use `BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1`, canonical fields and a full-payload replay comparison. Exclude per-record Grid traces from both render/payload unless debug is true. Do not implement resume or a free rule selector.

- [ ] **Step 4: Run focused core verification**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_stage_sweep_tests && ctest --test-dir /tmp/bruce-build -R '^grid_stage_sweep_contracts$' --output-on-failure`

Expected: pass with no skips.

### Task 2: CLI, central replay dispatch and compatibility

**Files:**
- Modify: `include/bruce/application/application_task.hpp` if variant integration is required
- Modify: `src/application/application_replay.cpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `tests/integration/test_cli.py`
- Modify: `tests/unit/grid_stage_sweep_tests.cpp`

**Interfaces:**
- Consumes: Task 1 Sweep APIs and bundle header.
- Produces: public `grid-stage-sweep`, generic `replay` support and controlled CLI errors.

- [ ] **Step 1: Write failing CLI/dispatch tests**

Add Python tests for Exact debug Sweep `0..7`, Metric/Modular, `(0,0)`, range errors, exported generic replay and rehashed-bundle replay rejection. Assert help contains the exact command and debug option. Add a unit assertion that `replay_application` recognizes the new header if the variant route is used.

- [ ] **Step 2: Run focused tests to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce && python3 tests/integration/test_cli.py /tmp/bruce-build/bruce CliTests.test_grid_stage_sweep`

Expected: `UnknownCommand` or missing dispatch/interface failure.

- [ ] **Step 3: Add the closed CLI and generic replay route**

Parse exactly `--path`, `--from-stage`, `--to-stage`, modes, limits, modulus, `--debug` and existing export/verification controls. Fix the rule internally. Render a bounded JSON report; permit normal `replay RUN` through the central dispatcher. No options for free rules, stage lists, resume or cross-run cache reuse.

- [ ] **Step 4: Verify Sweep and unchanged legacy behavior**

Run: `ctest --test-dir /tmp/bruce-build -R '^(grid_stage_sweep_contracts|grid_contracts|grid_cache_contracts|grid_replay_workflows|cli_workflows)$' --output-on-failure` plus focused CLI methods. Compare representative legacy Grid/Batch/Resume stdout and bundles against the pre-task binary.

### Task 3: Documentation, final verification and review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/implementation/75-current-architecture-and-milestones.md`
- Modify: `docs/superpowers/specs/2026-10-16-grid-stage-sweep-v1-design.md` status after verification
- Create: `runs/grid-stage-sweep-v1/reports/final-review.md`

- [ ] **Step 1: Document only the bounded debug contract**

Document the fixed rule, limits, exact partial result behavior, debug trace boundary, per-stage diagnostic fields, V1 bundle/replay, `(0,0)` behavior and non-goals.

- [ ] **Step 2: Run final relevant verification**

Run native configure/build, the relevant Grid/CLI/Sweep CTest selection, local link checker, Git status/diff checks, and the CLI verifier only in a complete temporary copy with original generated hashes captured before/after.

- [ ] **Step 3: Record evidence and independent review**

Record commands, exit codes, test totals, compatibility, hashes, links and limitations. Update the design status only after success. Use a fresh whole-change reviewer before declaring completion.

## Plan self-review

- **Spec coverage:** Task 1 owns data, limits, debug and replay; Task 2 owns public wiring; Task 3 owns documentation and final evidence.
- **Step scan:** each behavior begins with a concrete red test before the smallest production change.
- **Type consistency:** Task 1 exports every Sweep API Task 2 consumes; legacy Grid types stay unchanged.
- **Review focus:** all five high-risk inputs/conditions are assigned to named tests.
- **Proportion:** the plan adds one bounded debug command without altering existing Grid/Batch/Resume contracts.
