# DAG-Scheduler V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a bounded, serially deterministic, adapter-neutral DAG scheduler with fixed executable reference demos.

**Architecture:** `dag_scheduler_v1` is a standalone `bruce_core` component. It validates a fixed callback registry and DAG, executes one lexicographically selected ready callback at a time, then publishes sorted receipts and a chronological logical trace. The CLI owns only three fixed in-memory scenarios and has no free graph or callback surface.

**Tech Stack:** C++20, CMake/Ninja/CTest, Python `unittest`, existing CLI JSON helper and documentation generator.

**Spec:** `docs/superpowers/specs/2026-10-13-dag-scheduler-v1-design.md`

## Global Constraints

- Add no dependency and do not modify Phase-DAG V2/V3, Cockpit, Signed-Affine or Toy-Relation product contracts.
- Execute exactly one callback at a time; no thread, physical wave or parallelism API.
- Validate a sorted unique fixed callback registry; permit 1..64 nodes, 0..15 dependencies per node, and start budget 1..64.
- Start the lexicographically smallest ready node and expose inputs only from direct `READY` predecessors sorted by producer ID.
- Settle `BLOCKED` transitively before applying cancellation or budget; terminal receipts are node-ID sorted, trace is chronological and ends `COMPLETED`.
- A callback exception and every invalid Spec/result publish no partial `DagSchedulerRunV1`; callback `failed` produces a normal terminal receipt.
- The unconditional CLI accepts only `dag-scheduler-v1 demo mixed|budget|cancelled`; no free files, callbacks, budgets or parameters.
- Protect generated CLI artifacts: use `docs/cli/verify_examples.py` only in a complete native temporary copy.
- Use `scripts/with-host-deps.sh`; because the workspace path contains spaces and Git metadata is unavailable, retain native `/tmp` build evidence plus snapshots/reversible diffs/whitespace checks instead of commits.

## Review Focus

- A ready successor must preempt an already-ready later independent node when lexicographically smaller; Task 1’s dynamic-ready contract asserts this exact trace.
- A `FAILED` predecessor must transitively block all descendants but not unrelated sources; Task 1 asserts callback counts as well as receipts.
- Cancellation and start-budget exhaustion must never invoke a remaining callback; Task 1 checks a counter and distinct terminal codes.
- Registry and callback-result validation must happen before a partial run exists; Task 1 tests malformed catalogs/results and exception propagation.
- CLI reports must retain receipt sorting but chronological trace, including empty `COMPLETED.node_id`; Task 2 compares complete decoded JSON and raw bytes.

---

### Task 1: Adapter-Neutral Scheduler Core

**Files:**
- Create: `include/bruce/dag_scheduler/dag_scheduler_v1.hpp`
- Create: `src/dag_scheduler/dag_scheduler_v1.cpp`
- Create: `tests/unit/dag_scheduler_v1_tests.cpp`
- Modify: `CMakeLists.txt` (add core source and `dag_scheduler_v1_contracts` target)

**Interfaces:**
- Produces all exact public types and functions from Design §2, including `validate_dag_scheduler_spec_v1(const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1)` and `execute_dag_scheduler_v1(const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1, const DagSchedulerControlV1& = {})`.
- Consumed by Task 2 through the fixed callback registry and `DagSchedulerRunV1` only.

- [ ] **Step 1: Write the failing unit contracts**

Create the unit executable in the existing function-table style. Use real callbacks with counters and test exact projections for: lexicographic dynamic-ready ordering; sorted direct inputs; `mixed` failure/blocking/unrelated progress; budget; cancellation; every invalid graph/registry/result class; callback exception; repeated determinism; and a 64-node bounded chain. Assert no callback ran for `BLOCKED`/`CANCELLED` nodes.

- [ ] **Step 2: Configure and prove RED in a native temporary source copy**

Copy the source tree to a path without spaces under `/tmp`, configure with the dependency wrapper and build the new target. Expected: missing header/source/target or missing public symbols. Retain the original workspace configuration failure separately; do not mistake it for feature RED evidence.

- [ ] **Step 3: Implement the exact core contract**

Implement the types in the new header and the serial scheduler in the new source. Use a sorted ready set and per-node terminal state; repeatedly settle all newly blockable nodes before control/budget checks. Validate callback result codes, retain only successful direct predecessor inputs, sort receipts only after execution, and append `COMPLETED` last. Register source and CTest target with `-Wall -Wextra -Werror`.

- [ ] **Step 4: Prove GREEN and run adjacent scheduler regressions**

Build and run `dag_scheduler_v1_contracts`, then run it with Phase-DAG V2/V3 contracts. Expected: all selected tests pass and no V2/V3 file changes.

- [ ] **Step 5: Create the non-Git recovery checkpoint**

Save a source snapshot, manifest, reversible unified diff and added/removed-line whitespace check under `runs/dag-scheduler-v1-sdd/`.

### Task 2: Fixed CLI Demos and Real-Binary Workflows

**Files:**
- Create: `apps/dag_scheduler_v1_cli.hpp`
- Create: `tests/integration/test_dag_scheduler_v1_cli.py`
- Modify: `apps/bruce_cli.cpp`
- Modify: `CMakeLists.txt` (register `dag_scheduler_v1_cli_workflows`)

**Interfaces:**
- Consumes: Task 1’s scheduler types/functions.
- Produces: `int dag_scheduler_v1_cli(int argc, char** argv)` and stdout JSON contract `dag-scheduler-v1-cli-v1` for exactly `mixed`, `budget`, and `cancelled`.

- [ ] **Step 1: Write failing real-binary workflow tests**

Model the Python workflow on the existing fixed-demo suites. Assert all three full reports, including canonical receipt sorting, chronological trace, mixed `FAILED`/`BLOCKED`/`READY`, the two cancellation codes, no-start callback effects encoded in receipts, final newlines, repeated raw-byte stability, exact help lines, and every invalid tuple class from Review Focus.

- [ ] **Step 2: Prove RED against the current binary**

Build `bruce` in the native copy and run the new workflow. Expected: command absent or complete schema missing; retain the genuine failure output.

- [ ] **Step 3: Implement fixed catalogs, scenarios, renderer and route**

Create the inline adapter with only the three specified callback catalogs/specs. For cancellation, use a local flag set by the first callback and read by `DagSchedulerControlV1`. Render deterministic JSON metadata with receipts sorted by ID and trace in recorded order. Add unconditional main route before optional modules and CTest registration.

- [ ] **Step 4: Prove default and disabled-module GREEN**

Run direct workflow and CTest in a default native build, then in a fresh build with NanoCell and Cockpit disabled. Require all tests pass and all three raw reports byte-identical between builds.

- [ ] **Step 5: Run fixed-demo regressions and checkpoint**

Run new scheduler core/CLI plus existing V2/V3, Signed-Affine-Relation and Toy-Relation focused CTests. Save snapshot, reversible diff, byte comparisons and whitespace check.

### Task 3: Documentation and Generated CLI Evidence

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `tests/integration/test_cli_docs.py`
- Generated only through verifier: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: Task 2’s exact command, JSON contract, scenarios and boundaries.
- Produces: documentation that labels only V1’s serial local reference as **IMPLEMENTIERT UND BELEGT** and preserves broader scheduler/runtime work as **DOKUMENTIERT / KONZEPT**.

- [ ] **Step 1: Write the failing documentation contract**

Add a dedicated method isolating the `dag-scheduler-v1` manual section. Require all commands, contract version, serial Kahn order, callbacks, receipts/trace distinction, mixed/budget/cancelled terminal codes, exit behavior, no free files/parameters, no V2/V3 modification, no threads/GPU/network/bundle/replay/general runtime claim, real workflow path and generator coverage caveat.

- [ ] **Step 2: Prove genuine RED with the binary-first invocation convention**

Run `test_cli_docs.py` with the native binary as `argv[1]` and the unittest selector afterwards, matching the existing harness. Expected: the dedicated manual contracts are absent and fail.

- [ ] **Step 3: Author documentation and regenerate protected outputs correctly**

Update only scoped authored documents. In a complete native temporary copy run the unchanged verifier through the wrapper; copy back only its generated outputs, record whether examples execute scheduler demos, and accurately document any coverage limit.

- [ ] **Step 4: Prove GREEN and focused acceptance**

Run all documentation methods, generator-backed verification, local Markdown-link checker, and focused scheduler/V2/V3/Bridge CLI CTests. Expected: all selected checks pass.

- [ ] **Step 5: Run final full verification and save recovery evidence**

Run the AGENTS.md configure/build/CTest sequence in a fresh native temporary build. Report any socket-only Cockpit sandbox limit separately, run a controlled permitted rerun if required, and save final reversible diff, reverse dry run, manifest and whitespace check.

## Plan Self-Review

- **Spec coverage:** Task 1 owns every public type, serial selection, validation, terminal state, no-partial and trace requirement. Task 2 owns each fixed scenario and all public behavior. Task 3 owns only truthful documentation and generator handling.
- **Step scan:** Every task has test-first RED, explicit implementation work, GREEN proof and non-Git recovery evidence.
- **Type consistency:** Tasks 2/3 use the exact `DagScheduler*V1` names, CLI command, JSON contract and terminal codes from Task 1/spec.
- **Review focus coverage:** Dynamic order, transitive blocking, no-start counts, invalid registry/results and report ordering map directly to Task 1/2 tests.
- **Proportion:** Three independent, reviewable deliverables implement one bounded subsystem without altering existing Phase-DAG contracts.
