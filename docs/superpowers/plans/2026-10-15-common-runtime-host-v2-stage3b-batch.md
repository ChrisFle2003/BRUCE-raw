# Common Runtime Host V2 / Stage 3B Batch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route ordinary multi-node V5 `execute` runs through one Bruce-controlled Common Runtime Host V2 batch while preserving existing V5, Worker V1, payload, flow, handoff and retry contracts.

**Architecture:** Add a `CommonRuntimeHostV2Batch` application facade with one shared run logger, one host-owned sequence counter, and one internal supervisor handle per active Worker V1 attempt. Reuse the existing V5 DAG scheduler for parsing, dependency gating, canonical receipt order and blocked propagation; the batch owns process lifecycle, cancellation ordering, lifecycle events and validated metadata projection.

**Tech Stack:** C++20, existing Worker V1 supervisor and JSONL run log, CMake/CTest, Python integration tests, Linux process supervision.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-host-v2-stage3b-batch-design.md`

## Global Constraints

- Keep C++20 and existing CMake/CTest conventions.
- Stage 3B applies only to ordinary local V5 `execute` with multiple nodes, `--retry 0`, and no payload, stream, flow or handoff extension.
- Do not change Worker V1 frames, Worker V1 result fields, V5 public syntax, V5 report schema, V2/V3/V4 behavior or payload/flow/handoff paths.
- The host owns one run logger and one monotonic JSONL sequence per batch; never open multiple independent Host V2 loggers for one run root.
- `--workers` remains 1..4 and limits active worker attempts; `max-started-nodes` remains a separate total budget.
- Budgets and fixed `grace_ms == 1000` retain Stage 3A semantics.
- RLIMIT_AS and peak telemetry remain per observed Worker PID; do not claim aggregate process-group memory accounting.
- After cancel is observed, log `CANCEL_REQUESTED` before polling or taking over any result; a late READY becomes CANCELLED and cannot be accepted.
- Public output remains metadata-only; no PID, absolute path, stderr or raw payload bytes may enter the V5 report.
- No automatic retries, external payload stores, GPU/network execution, dynamic plugins, free language, general proving or broad runtime orchestration in this stage. :codex-annotation{index="1"}
- Do not hand-edit generated CLI artifacts; use the repository verifier in a complete temporary copy.
- Git metadata is unavailable in this checkout; preserve file-based review reports and record the limitation instead of claiming commits.

## Review Focus

- Two workers finishing out of order must still produce canonical receipt order and one shared monotonic run log; test `shared_log_and_canonical_receipt_order`.
- A third start request must be refused while `max_workers == 2` attempts are active; test `active_worker_limit_is_host_enforced`.
- A cancel marker observed between scheduling and polling must prevent late READY acceptance and all new starts; test `cancel_linearization_blocks_late_ready_and_pending_nodes`.
- A rejected result or output-limit failure must finish with no `ARTIFACT_ACCEPTED` for that attempt and must block descendants; test `rejected_attempt_blocks_descendants`.
- The batch must reap every Worker and watchdog before `RUN_FINISHED`; test `finish_reaps_all_attempts` and the Stage 3B process integration.

---

### Task 1: Implement the shared Common Runtime Host V2 batch facade

**Files:**
- Create: `include/bruce/application/common_runtime_host_v2_batch.hpp`
- Create: `src/application/common_runtime_host_v2_batch.cpp`
- Create: `tests/unit/common_runtime_host_v2_batch_tests.cpp`
- Modify: `CMakeLists.txt` to add the source and `common_runtime_host_v2_batch_contracts` target

**Interfaces:**
- Consumes: `worker_v1::WorkerRequest`, `worker_v1::Budget`, `worker_v1::WorkerResult`, `WorkerAttemptHandle`, `WorkerRunLog`, abort control and supervisor operations.
- Produces:
  - `struct CommonRuntimeHostV2BatchContext { std::filesystem::path run_root; std::filesystem::path worker_executable; worker_v1::Budget budget; std::uint64_t max_workers; std::uint64_t grace_ms{1000}; };`
  - movable, non-copyable opaque `CommonRuntimeHostV2Batch`
  - `struct CommonRuntimeHostV2BatchResult { std::string attempt_id; worker_v1::WorkerResult result; };`
  - `struct CommonRuntimeHostV2BatchOutcome { worker_v1::WorkerStatus status; int host_exit_code; };`
  - `[[nodiscard]] CommonRuntimeHostV2Batch start_batch_v2(std::string run_id, const CommonRuntimeHostV2BatchContext& context)`
  - `void start_batch_attempt_v2(CommonRuntimeHostV2Batch& batch, const worker_v1::WorkerRequest& worker_request)`
  - `[[nodiscard]] std::vector<CommonRuntimeHostV2BatchResult> poll_batch_v2(CommonRuntimeHostV2Batch& batch)`
  - `void record_attempt_acceptance_v2(CommonRuntimeHostV2Batch& batch, std::string_view attempt_id)`
  - `void request_cancel_batch_v2(CommonRuntimeHostV2Batch& batch)`
  - `[[nodiscard]] CommonRuntimeHostV2BatchOutcome finish_batch_v2(CommonRuntimeHostV2Batch& batch)`

- [ ] **Step 1: Write failing contract tests.** Add fixtures for two valid workers, out-of-order completion, active-worker overflow, cancel-before-poll, invalid result projection, rejected result, and process cleanup. Assert one `RUN_CREATED`, one `RUN_FINISHED`, monotonic sequence values, per-attempt lifecycle events, no post-cancel `ATTEMPT_STARTED`, and no artifact acceptance after cancellation.

- [ ] **Step 2: Run the new unit target before implementation.** Run `ctest --test-dir /tmp/bruce-stage3a-build -R '^common_runtime_host_v2_batch_contracts$' --output-on-failure`. Expected: target is absent or tests fail because the API is not implemented.

- [ ] **Step 3: Add the opaque batch declarations.** Keep the batch movable and non-copyable. Keep PIDs, supervisor handles, logger state and result-takeover state private. Expose only the context, per-attempt result, lifecycle operations and host exit outcome.

- [ ] **Step 4: Implement batch context validation and shared lifecycle.** Validate canonical Bruce executable, owner-only run root, exact budget equality, `max_workers` in 1..4 and `grace_ms == 1000` before any spawn. Create one `WorkerRunLog`, append `RUN_CREATED` once, and allocate one monotonic sequence counter for all attempts.

- [ ] **Step 5: Implement attempt start and bounded polling.** `start_batch_attempt_v2` rejects duplicate attempt IDs, rejects a full active set, appends `ATTEMPT_STARTED` and `STATUS`, then starts the existing Worker V1 supervisor. `poll_batch_v2` polls all active handles, reaps completed attempts, validates canonical READY metadata and appends `PROCESS_EXITED`/`RESULT_RECEIVED` exactly once per attempt.

- [ ] **Step 6: Implement acceptance and cancellation ordering.** `record_attempt_acceptance_v2` may append `ARTIFACT_ACCEPTED` only for a validated READY result and only once. `request_cancel_batch_v2` appends one `CANCEL_REQUESTED` per live attempt before calling supervisor cancellation. Polling after that marker maps READY to CANCELLED and clears public artifact metadata. No attempt may start after cancellation is logged.

- [ ] **Step 7: Implement batch finish and cleanup.** `finish_batch_v2` cancels or finishes all remaining handles, waits through the fixed grace behavior, reaps Workers and watchdogs, appends exactly one `RUN_FINISHED`, and returns a derived READY/FAILED/CANCELLED host outcome. It must reject unfinished result decisions rather than silently accepting them.

- [ ] **Step 8: Run focused unit tests and existing Host V2 contracts.** Run `cmake --build /tmp/bruce-stage3a-build --target bruce_common_runtime_host_v2_batch_tests` and `ctest --test-dir /tmp/bruce-stage3a-build -R '^(common_runtime_host_v2_batch_contracts|common_runtime_host_v2_contracts|worker_supervisor_v1_contracts|worker_run_log_v1_contracts)$' --output-on-failure`. Expected: all selected tests pass.

- [ ] **Step 9: Self-review and report.** Confirm no Worker V1 or V5 report contract changed, run `git diff --check` if metadata is usable, and write `.superpowers/sdd/2026-10-15-common-runtime-host-v2-stage3b/task-1-report.md`.

### Task 2: Route ordinary multi-node V5 execute through the batch facade

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp` in the ordinary execute branch and active-attempt polling loop
- Create: `tests/integration/test_common_runtime_host_v2_stage3b.py`
- Modify: `CMakeLists.txt` to register `common_runtime_host_v2_stage3b_workflows`

**Interfaces:**
- Consumes: Task 1 `CommonRuntimeHostV2Batch` API; existing V5 `V5NodeState`, ready-node set, successor map, receipt order and report renderer.
- Produces: the existing V5 multi-node report and lifecycle behavior for ordinary multi-node, `retry=0`, no-payload/flow/handoff `execute` runs, with the common Host V2 batch owning worker lifecycle.

- [ ] **Step 1: Write failing integration tests.** Add a spec with two independent roots and a dependent join. Assert `--workers 2`, canonical receipt order, join start only after both accepted roots, metadata-only report, and one shared monotonic JSONL sequence.

- [ ] **Step 2: Add cancel, limit and failure cases.** Cover `--workers 1` determinism, active-worker limit, `max-started-nodes`, external cancel while two workers run, no starts after cancel, late READY rejection, transitive BLOCKED descendants and process cleanup.

- [ ] **Step 3: Run the integration test before routing.** Run `python3 tests/integration/test_common_runtime_host_v2_stage3b.py /tmp/bruce-stage3a-build/bruce`. Expected: the new assertions fail or observe the old direct-supervisor lifecycle before the route is changed.

- [ ] **Step 4: Add a narrow V5 gate.** Select the batch facade only for ordinary `execute` with multiple nodes, `retry == 0`, and no payload, stream, flow or handoff extension. Preserve the existing direct scheduler for the excluded paths and preserve the Stage 3A single-node gate.

- [ ] **Step 5: Adapt scheduler state to batch handles.** Replace only the selected path's direct `WorkerAttemptHandle` ownership with batch attempt IDs/results. Keep existing ready-node scheduling, successor gating, canonical receipt cursor, output total budget and BLOCKED propagation. Call `record_attempt_acceptance_v2` only after the existing scheduler acceptance checks succeed.

- [ ] **Step 6: Adapt cancel and terminal reporting.** Translate the existing owner-only abort marker to `request_cancel_batch_v2`, stop scheduling new nodes, convert late results to CANCELLED, and let the existing V5 renderer produce the unchanged report schema and attempt history.

- [ ] **Step 7: Run focused V5 verification.** Run `cmake --build /tmp/bruce-stage3a-build --target bruce`, the new Stage 3B integration, existing Stage 3A integration, `test_dag_scheduler_v5_cli.py`, `test_dag_scheduler_v5_abort.py`, `test_dag_scheduler_v5_inspector.py`, and the payload/flow/handoff regression tests. Expected: selected tests pass and excluded routes remain byte-compatible.

- [ ] **Step 8: Self-review and report.** Inspect lifecycle logs for one shared sequence, no duplicate terminal events and no post-cancel starts. Write `.superpowers/sdd/2026-10-15-common-runtime-host-v2-stage3b/task-2-report.md`.

### Task 3: Documentation, verification and whole-branch review

**Files:**
- Modify: `docs/architecture/03-blueprint-consolidation.md` with factual Stage 3B status only after implementation passes
- Modify: `docs/implementation/75-current-architecture-and-milestones.md` with the verified multi-node milestone only after implementation passes
- Modify: `docs/superpowers/specs/2026-10-15-common-runtime-host-v2-stage3b-batch-design.md` status after acceptance
- Create: `runs/common-runtime-host-v2-stage3b/reports/task-1.md`
- Create: `runs/common-runtime-host-v2-stage3b/reports/task-2.md`
- Create: `runs/common-runtime-host-v2-stage3b/reports/final-review.md`

**Interfaces:**
- Consumes: Task 1 and Task 2 implementation/test reports.
- Produces: evidence separating implemented Stage 3B from future retry, payload, external-store, GPU/network, plugin, language, proving and broad orchestration concepts.

- [ ] **Step 1: Reconcile documentation only after code review.** Record the exact ordinary multi-node gate, shared host lifecycle, cancel ordering and unchanged excluded paths. Do not mark retries, payload integration or broad runtime goals implemented.

- [ ] **Step 2: Run required build and focused checks.** Run the native-path configure/build when available, focused CTest for Host V2, Worker V1, V5 and Stage 3B, all Stage 3A/V5 regressions, and the relevant documentation tests.

- [ ] **Step 3: Run CLI verification in a complete temporary copy.** Use `docs/cli/verify_examples.py`; do not edit generated artifacts manually. Validate JSON and local Markdown links with the repository's existing checks.

- [ ] **Step 4: Write final evidence report.** Include exact commands, exit codes, test counts, repeat counts for cancel races, known Cockpit/Phase-DAG limitations, unusable Git metadata, and the remaining open scope.

- [ ] **Step 5: Whole-branch review.** A fresh reviewer inspects the combined Task 1–3 changes for shared-log sequencing, cancellation races, unchanged public contracts and overclaimed documentation. Mark Stage 3B complete only after PASS.

## Rollout Order

1. Task 1 must pass before Task 2 starts.
2. Task 2 must pass before Stage 3B documentation is promoted.
3. Task 3 documentation and final review must pass before declaring Stage 3B **IMPLEMENTIERT UND BELEGT**.
