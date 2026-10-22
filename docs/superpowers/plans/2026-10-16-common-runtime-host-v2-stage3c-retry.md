# Common Runtime Host V2 / Stage 3C Retry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add explicit V5 retry support to the Common Runtime Host V2 paths while preserving existing retry policy, attempt history, reports and excluded payload/flow/handoff routes.

**Architecture:** Keep retry classification and DAG decisions in the V5 scheduler. Extend the Batch Host with one explicit `record_attempt_retry_v2` decision so historical failed attempts no longer force a final FAILED batch status. Route ordinary local single-node retries and multi-node retries through the existing Batch Host; keep `--retry 0` behavior and all extension routes unchanged.

**Tech Stack:** C++20, existing Worker V1 supervisor and JSONL run log, CMake/CTest, Python integration tests.

**Spec:** `docs/superpowers/specs/2026-10-16-common-runtime-host-v2-stage3c-retry-design.md`

## Global Constraints

- Keep the public command `bruce dag-scheduler-v5 execute SPEC --output BUNDLE` unchanged.
- Preserve defaults and maxima: retry `0..3`, workers `1..4`, and all five existing worker budgets.
- Retry only `PROCESS_CRASHED`, `TIMEOUT` and `PROTOCOL_ERROR`.
- Never retry `CANCELLED`, `FAILED`, `BLOCKED`, digest errors, input/output/operation/memory limits or process-start failures.
- `--retry N` counts additional attempts; a node receives at most `N+1` attempts.
- `max-started-nodes` counts each node once; retries do not consume new-node budget.
- Every retry gets a new attempt ID and private attempt directory.
- Cancellation is linearized before dispatch or result takeover; no retry starts after cancel.
- A historical failed retry must not keep a later successful batch in FAILED state.
- Worker V1 frames, V5 report schema, inspector contracts, V2/V3/V4 paths and payload/flow/handoff routes remain unchanged.
- No automatic implicit retries, network/GPU execution, plugins, free language, general proving or broad orchestration. :codex-annotation{index="1"}
- Do not hand-edit generated CLI artifacts; use the CLI verifier in a complete temporary copy.
- Git metadata is unavailable; use file-based reports and do not claim commits.

## Review Focus

- A retryable failure followed by a successful attempt must produce one final READY run and exactly one `ARTIFACT_ACCEPTED`; test `retry_success_excludes_historical_failure_from_final_status`.
- Retry exhaustion must preserve every attempt in history and end with the last failure without artifact metadata; test `retry_exhaustion_preserves_history`.
- A non-retryable failure must not requeue the node; test `non_retryable_failure_does_not_start_again`.
- Cancel between attempts must prevent the next `ATTEMPT_STARTED`; test `cancel_between_retries_starts_no_new_attempt`.
- Cancel during an active retry attempt must map late READY to CANCELLED and leave no artifact; test `cancel_active_retry_has_no_late_acceptance`.

---

### Task 1: Add explicit retry decisions to the Batch Host

**Files:**
- Modify: `include/bruce/application/common_runtime_host_v2_batch.hpp`
- Modify: `src/application/common_runtime_host_v2_batch.cpp`
- Modify: `tests/unit/common_runtime_host_v2_batch_tests.cpp`

**Interfaces:**
- Consumes: existing Batch Host attempt states, canonical result validation, acceptance/rejection decisions and finish logic.
- Produces: `void record_attempt_retry_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id)`.

- [ ] **Step 1: Write failing unit tests.** Add a worker fixture that produces a retryable failure followed by READY, then assert that the first attempt is historically decided, the second attempt can be accepted, `RUN_FINISHED` is READY, and only the second attempt emits `ARTIFACT_ACCEPTED`. Add tests for retry rejection after final decision and retry idempotency/error behavior.

- [ ] **Step 2: Run the batch contract target before implementation.** Run `ctest --test-dir /tmp/bruce-stage3a-build -R '^common_runtime_host_v2_batch_contracts$' --output-on-failure`. Expected: the new tests fail because the retry decision is absent.

- [ ] **Step 3: Add the public internal declaration.** Declare `void record_attempt_retry_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id)` beside acceptance and rejection decisions. Keep the batch opaque and do not expose retry policy or PIDs.

- [ ] **Step 4: Implement retry decision state.** Mark a completed non-accepted attempt as retry-decided, prohibit later acceptance, and exclude it from final batch failure calculation. Preserve its `PROCESS_EXITED`/`RESULT_RECEIVED` events and attempt metadata. Reject retry decisions for unknown, accepted, cancelled or already-final attempts with controlled protocol behavior.

- [ ] **Step 5: Verify finalization.** Ensure `finish_batch_v2` returns READY when a later attempt was accepted, FAILED when the final node decision failed, and CANCELLED after host cancel. Ensure exactly one `RUN_FINISHED` and no artifact acceptance for historical failures.

- [ ] **Step 6: Run focused unit checks.** Run `cmake --build /tmp/bruce-stage3a-build --target bruce_common_runtime_host_v2_batch_tests` and `ctest --test-dir /tmp/bruce-stage3a-build -R '^(common_runtime_host_v2_batch_contracts|common_runtime_host_v2_contracts|worker_supervisor_v1_contracts|worker_run_log_v1_contracts)$' --output-on-failure`. Expected: all selected tests pass.

- [ ] **Step 7: Write Task 1 evidence.** Record the new retry decision and test results in `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3c/task-1-report.md`.

### Task 2: Route ordinary V5 retry execution through the Batch Host

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Create: `tests/integration/test_common_runtime_host_v2_stage3c.py`
- Modify: `CMakeLists.txt` to register `common_runtime_host_v2_stage3c_workflows`

**Interfaces:**
- Consumes: Task 1 retry decision; existing V5 `retryable`, `V5NodeState`, attempt ID generation, history, ready-node scheduling, receipt renderer and inspector.
- Produces: ordinary local V5 single-node retries with `workers=1` and ordinary multi-node retries with `workers=1..4`, all using the Batch Host; `retry=0` behavior remains unchanged.

- [ ] **Step 1: Write failing integration tests.** Add cases for single-node timeout with `--retry 2`, multi-node independent roots with `--workers 2 --retry 2`, exact attempt IDs, complete history and no public path/payload leaks. Add a deterministic C++ batch fixture case for failure-then-READY success.

- [ ] **Step 2: Add retry exhaustion and non-retryable cases.** Assert timeout retries stop at `N+1`, final status uses the last failure, no artifact is accepted, and non-retryable output-limit/cancel failures do not requeue.

- [ ] **Step 3: Add cancellation cases.** Cancel after a retryable failure but before the next dispatch, and cancel while a retry attempt is active. Assert no later `ATTEMPT_STARTED`, no `ARTIFACT_ACCEPTED`, final CANCELLED status, monotonic JSONL and cleaned processes.

- [ ] **Step 4: Run the new integration test before routing changes.** Run `python3 tests/integration/test_common_runtime_host_v2_stage3c.py /tmp/bruce-stage3a-build/bruce`. Expected: retry cases fail or use the old direct-supervisor route before the gate is expanded.

- [ ] **Step 5: Extend the single-node Batch Host path.** Update `execute_single_node_v2` to loop over attempts when `options.retry > 0`, using `attempt_id_for`, `poll_batch_v2`, `record_attempt_retry_v2`, `record_attempt_acceptance_v2` and the existing single-node report/history renderer. Preserve the exact `retry=0` output path.

- [ ] **Step 6: Extend the multi-node Batch Host scheduler.** Allow the existing `execute_multi_node_v2` batch branch to run with explicit retries. After a retryable result and remaining budget, call `record_attempt_retry_v2`, keep the node nonterminal, reinsert it into `ready_nodes`, and do not increment `max-started-nodes`. For final or non-retryable results, preserve existing BLOCKED and receipt-cursor behavior.

- [ ] **Step 7: Expand the ordinary-route gate narrowly.** Select Batch Host for ordinary local execute with no payload/stream/flow/handoff when either the existing single-node path applies or the multi-node path applies. Keep payload/flow/handoff routes and any explicitly excluded legacy paths unchanged. `retry=0` must remain byte/report compatible with Stage 3A/3B.

- [ ] **Step 8: Run V5 regressions.** Build `bruce`, run Stage 3C integration repeatedly, Stage 3A/3B integrations, V5 CLI/abort/inspector, payload, payload-flow and handoff suites, plus relevant CTest targets. Expected: all selected tests pass and existing retry tests remain valid.

- [ ] **Step 9: Write Task 2 evidence.** Record exact retry histories, cancel race repetitions, regression counts and excluded paths in `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3c/task-2-report.md`.

### Task 3: Documentation, verification and final review

**Files:**
- Modify: `docs/architecture/03-blueprint-consolidation.md` only after implementation review
- Modify: `docs/implementation/75-current-architecture-and-milestones.md` only after implementation review
- Modify: `docs/superpowers/specs/2026-10-16-common-runtime-host-v2-stage3c-retry-design.md` status after acceptance
- Create: `runs/common-runtime-host-v2-stage3c/reports/task-1.md`
- Create: `runs/common-runtime-host-v2-stage3c/reports/task-2.md`
- Create: `runs/common-runtime-host-v2-stage3c/reports/final-review.md`

**Interfaces:**
- Consumes: Task 1 and Task 2 reports, existing CLI/Inspector contracts and the Stage 3A/3B evidence set.
- Produces: factual documentation separating implemented opt-in retries from future payload/external-runtime work.

- [ ] **Step 1: Reconcile documentation.** Record the exact retry gate, attempt-history behavior, finalization semantics and unchanged excluded routes. Keep future features conceptual.

- [ ] **Step 2: Run the required focused verification.** Run native configure/build when available, focused CTest, all Stage 3A/3B/3C and V5 regressions, and CLI documentation tests.

- [ ] **Step 3: Run CLI verifier and link checks.** Use `docs/cli/verify_examples.py` in a complete temporary copy and the repository's local Markdown-link checker. Do not edit generated artifacts by hand.

- [ ] **Step 4: Write final evidence.** Include exact commands, exit codes, retry counts, cancel-race repeats, known host-wrapper/Cockpit/Phase-DAG limitations and unusable Git metadata.

- [ ] **Step 5: Whole-branch review.** A fresh reviewer checks retry finalization, attempt history, cancellation ordering, unchanged public contracts and documentation status. Mark Stage 3C complete only after PASS.

## Rollout Order

1. Task 1 must pass before Task 2 starts.
2. Task 2 must pass before Stage 3C documentation is promoted.
3. Task 3 and the whole-branch review must pass before declaring Stage 3C **IMPLEMENTIERT UND BELEGT**.
