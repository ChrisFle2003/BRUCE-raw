# Worker Abort Control V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an owner-only V5 host control path that lets Bruce observe an explicit cancellation request and terminate all active local worker attempts while preserving existing worker, V2, V3 and V4 contracts.

**Architecture:** A new `dag-scheduler-v5 cancel BUNDLE` command atomically creates a fixed marker under the private `BUNDLE.run/control` directory. The existing V5 host loop polls that marker, logs `CANCEL_REQUESTED`, stops new dispatches, calls the existing `request_cancel` API for every live handle, and emits a metadata-only `CANCELLED` run/report. The existing worker frame, supervisor grace/escalation and result types remain the enforcement boundary.

**Tech Stack:** C++20, existing `bruce_core`, Linux/POSIX descriptor-relative file APIs, existing `WorkerRunLog`, `WorkerRunInspectorV1`, V5 CLI, CMake/CTest and Python `unittest` workflows.

**Spec:** `docs/superpowers/specs/2026-10-15-worker-abort-control-v1-design.md`

## Global Constraints

- The public command is exactly `bruce dag-scheduler-v5 cancel BUNDLE`.
- The marker path is `BUNDLE.run/control/cancel.request` and its exact content is `BRUCE_WORKER_CANCEL_V1\n`.
- Marker creation uses owner-only descriptor-relative `O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC`, mode `0600`, followed by `fsync`.
- The existing `worker_v1::request_cancel` API remains the only worker-abort primitive; no direct public PID or signal interface is added.
- The supervisor grace remains fixed at `1000 ms`; its existing 50-ms TERM-to-KILL escalation and process-group cleanup remain authoritative.
- A host cancellation stops all new starts, marks unstarted nodes `CANCELLED`, cancels every active handle and never retries a cancelled attempt.
- Active attempts write exactly one `CANCEL_REQUESTED` event; cancelled attempts never write `ARTIFACT_ACCEPTED` or artifact metadata.
- Existing successful and ordinary-failure report bytes, V2/V3/V4 APIs and worker frame bytes remain unchanged.
- No payload bytes, local paths, PIDs, command lines, tokens or raw stderr enter public output or control records.
- Linux process control and descriptor semantics are the supported V1 platform; no portability layer is introduced.
- Generated CLI files are updated only through `docs/cli/verify_examples.py` in a complete temporary copy.

## Review Focus

- A second `cancel` invocation must be idempotent and must not append a second control marker; cover in Task 1.
- A symlink, hardlink, unsafe permission or altered marker must fail closed without signaling any worker; cover in Task 1 and Task 2.
- A cancellation arriving while several workers are live must reach every handle in one scheduler iteration and must not start a queued dependent; cover in Task 3.
- A cooperative CANCEL response and an ignored CANCEL must both end in `CANCELLED`, with the fixed grace and complete reaping; cover in Task 4.
- Cancellation racing with receipt acceptance must have a defined parent-side linearization and leave the inspector/report contract consistent; cover in Task 3 and Task 5.

## File Map

**Create:**

- `include/bruce/worker/worker_abort_control_v1.hpp` — marker protocol and host-side request/probe result types.
- `src/worker/worker_abort_control_v1.cpp` — secure descriptor-relative marker creation and validation.
- `tests/unit/worker_abort_control_v1_tests.cpp` — marker permissions, idempotence, malformed-control and path-safety contracts.
- `tests/integration/test_dag_scheduler_v5_abort.py` — real-binary cancellation workflows and multi-worker behavior.

**Modify:**

- `CMakeLists.txt` — add the abort-control source, unit target and Python workflow.
- `apps/dag_scheduler_v5_cli.hpp` — parse/dispatch `cancel`, poll the marker in V5 execution, cancel active handles, mark unstarted nodes and render cancellation status.
- `tests/unit/dag_scheduler_v5_cli_tests.cpp` — exact cancel syntax/output and parser contracts.
- `tests/unit/worker_run_inspector_v1_tests.cpp` — accepted `CANCEL_REQUESTED` → `CANCELLED` event traces and report status binding.
- `tests/integration/test_dag_scheduler_v5_cli.py` — preserve existing execute behavior and add cancellation assertions where the shared fixture is appropriate.
- `docs/cli/commands.md` — public command, marker behavior, statuses and safety boundaries.
- `docs/cli/formats.md` — V5 cancellation report and JSONL event contract.
- `docs/cli/README.md` — link the command section if the existing navigation requires it.
- Generated CLI artifacts only through `docs/cli/verify_examples.py` in a temporary copy.

## Task 1: Implement the Secure Abort Marker Core

**Files:**

- Create: `include/bruce/worker/worker_abort_control_v1.hpp`
- Create: `src/worker/worker_abort_control_v1.cpp`
- Create: `tests/unit/worker_abort_control_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Produce a fixed `kMarker = "BRUCE_WORKER_CANCEL_V1\\n"`, `enum class AbortRequestResult { REQUESTED, ALREADY_REQUESTED }` and `enum class AbortProbeResult { NOT_REQUESTED, REQUESTED, INVALID }`.
- Produce `request_abort(const std::filesystem::path& run_root) -> AbortRequestResult` for atomic marker creation; unavailable or unsafe roots raise the repository's controlled protocol error.
- Produce `probe_abort(const std::filesystem::path& run_root) -> AbortProbeResult`; malformed state returns `INVALID` and must never be reported as `NOT_REQUESTED`.

- [ ] **Step 1: Write failing unit tests** for a valid owner-only run tree, first request returning `REQUESTED`, second request returning `ALREADY_REQUESTED`, exact bytes/mode/size, missing root, symlinked root/control/marker, wrong mode, hardlink marker, altered bytes and extra bytes.
- [ ] **Step 2: Run the focused target** with `ctest --test-dir /tmp/bruce-v14-core-build -R worker_abort_control_v1_contracts --output-on-failure`; verify the new target fails before implementation.
- [ ] **Step 3: Implement descriptor-relative validation and creation.** Open each path component with `O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC`, verify owner/mode/hardlink invariants, create with `O_EXCL`, write the exact marker, `fsync` the file and close all descriptors on every error path.
- [ ] **Step 4: Register and run the unit target** with the project warning flags; expect PASS and verify tests write only inside a temporary root.
- [ ] **Step 5: Run existing worker boundary/log/inspector targets** to demonstrate the new library does not alter their contracts.

## Task 2: Add the Public `cancel` CLI Route

**Files:**

- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v5_cli_tests.cpp`
- Modify: `CMakeLists.txt` only if a separate CLI test target is needed.

**Interfaces:**

- Parse exactly `cancel BUNDLE`; reject options, `.run` targets, empty/dash-leading paths and extra arguments.
- Render `dag-scheduler-v5-cancel-v1` metadata-only results for `REQUESTED`, `ALREADY_REQUESTED` and `ALREADY_FINISHED`; a final `RUN_FINISHED` event takes precedence over an existing marker.
- Map missing/unsafe/invalid targets to `CANCEL_TARGET_UNAVAILABLE` with exit 1 and syntax errors to the existing compact protocol error with exit 2.

- [ ] **Step 1: Write failing parser/CLI unit tests** for exact syntax, all three successful statuses, unavailable target, no path/PID leakage and unchanged existing V5 usage.
- [ ] **Step 2: Run `ctest --test-dir /tmp/bruce-v14-core-build -R dag_scheduler_v5_cli_contracts --output-on-failure`** and verify the new assertions fail.
- [ ] **Step 3: Implement the route.** Normalize the BUNDLE path without following symlinks, derive the `.run` root, inspect only the private log/marker state needed for the status, and delegate marker creation to the core API.
- [ ] **Step 4: Run the focused CLI target** and assert byte-exact canonical JSON plus exit codes.
- [ ] **Step 5: Run existing V5 execute/inspect/publish parser tests** and confirm they remain unchanged.

## Task 3: Integrate Host Polling and Scheduler Cancellation

**Files:**

- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v5_cli_tests.cpp`
- Modify: `tests/integration/test_dag_scheduler_v5_abort.py`

**Interfaces:**

- Add a local V5 execution state flag for the observed host cancellation and one per-state cancellation-log guard.
- Poll `probe_abort(root)` before dispatch and while processing live attempts.
- For active attempts append `CANCEL_REQUESTED` exactly once, call `request_cancel`, and prevent all new starts. Mark unstarted states `CANCELLED` with null attempt/reference/digest values.

- [ ] **Step 1: Add failing scheduler tests** for one active worker, multiple active workers, queued independent nodes, dependent nodes, `--retry`, and cancellation before the first node starts.
- [ ] **Step 2: Add failing real-binary workflows** that create a controllable long-running helper/run, invoke the new CLI command from a second process, and inspect stdout, bundle, run-log and process termination.
- [ ] **Step 3: Implement the polling linearization.** Observe the marker before starting ready work; if seen, freeze dispatch, log active attempts, request cancellation for all handles, and normalize any result observed after the request but before receipt commit to `CANCELLED`.
- [ ] **Step 4: Preserve the existing normal scheduler path.** Keep retry and dependency release byte-identical when the marker is absent; use the existing `request_cancel` call rather than duplicating signal logic.
- [ ] **Step 5: Run focused unit and workflow tests** and verify no dependent worker starts after cancellation and no new attempt is created.

## Task 4: Freeze Grace, Exit, Log and Inspector Semantics

**Files:**

- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/worker_supervisor_v1_tests.cpp` only for a focused regression if current coverage does not pin the exact host-triggered path.
- Modify: `tests/unit/worker_run_inspector_v1_tests.cpp`
- Modify: `tests/integration/test_dag_scheduler_v5_abort.py`

**Interfaces:**

- Keep `request_cancel`'s existing fixed 1000-ms grace, 50-ms TERM-to-KILL escalation, process-group cleanup and reaping as the only enforcement behavior.
- Emit `CANCEL_REQUESTED`, `PROCESS_EXITED`, `RESULT_RECEIVED` with no artifact fields for cancelled attempts, then `RUN_FINISHED` with `CANCELLED`.
- Render a single- or multi-node V5 report with top-level/receipt `CANCELLED`; return exit 1. Preserve exact report keys for existing READY/FAILED runs.

- [ ] **Step 1: Add failing inspector tests** for valid cancellation event ordering, no `ARTIFACT_ACCEPTED`, cancelled attempts with empty artifact metadata, unstarted cancelled receipts and report/run-status equality.
- [ ] **Step 2: Add failing lifecycle assertions** for cooperative cancellation, ignored CANCEL, descendant cleanup, watchdog disarming and no retry after cancellation.
- [ ] **Step 3: Implement status/report/log binding** without adding payload bytes, paths or a new worker frame. Ensure the marker remains readable for diagnosis and `inspect BUNDLE` remains read-only.
- [ ] **Step 4: Run worker supervisor, run-log, run-inspector and V5 focused groups**; expect PASS and capture elapsed grace evidence without asserting scheduler wall-clock precision beyond the bounded contract.

## Task 5: Documentation, Generated CLI Verification and Full Validation

**Files:**

- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/cli/README.md` as needed.
- Modify generated `docs/cli/source-index.md`, `docs/cli/examples.json` and `docs/cli/validation.md` only through `docs/cli/verify_examples.py` in a temporary complete copy.
- Create: `runs/bruce-worker-abort-control-v1-<date>.md` for reproducible evidence.

- [ ] **Step 1: Document the route as IMPLEMENTIERT UND BELEGT only after code/tests pass.** Mark external stores, plugins, network/GPU, free language, proving and global orchestration as out of scope.
- [ ] **Step 2: Run the prescribed native configure/build/CTest path** in a path accepted by `scripts/with-host-deps.sh`; if the wrapper is unavailable in the current path, use the established native `/tmp` build and record that limitation.
- [ ] **Step 3: Run focused abort, worker, inspector, V5 execute/inspect/payload/flow and relevant V2/V3/V4 groups.** Run `docs/cli/verify_examples.py` only in its required temporary copy.
- [ ] **Step 4: Run `git diff --check`, local Markdown-link validation, secret scan and Git status where metadata is available.** Do not modify unrelated existing changes or generated files by hand.
- [ ] **Step 5: Record exact commands, exit codes, pass/skip counts and the known Cockpit socket PermissionError separately from feature results.**

## Verification Commands

```sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R '^(worker_abort_control_v1_contracts|worker_boundary_v1_contracts|worker_run_log_v1_contracts|worker_supervisor_v1_contracts|worker_run_inspector_v1_contracts|dag_scheduler_v5_cli_contracts|dag_scheduler_v5_cli_workflows|dag_scheduler_v5_inspector_workflows)$' --output-on-failure
python3 scripts/check_local_doc_links.py
```

Expected feature evidence is all focused tests passing, with any unrelated
Cockpit socket sandbox failure reported separately. A full CTest pass is not
claimed unless every test, including the environment-sensitive Cockpit test,
passes.
