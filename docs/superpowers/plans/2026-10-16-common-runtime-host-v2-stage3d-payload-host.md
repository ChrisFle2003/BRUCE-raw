# Common Runtime Host V2 / Stage 3D Payload Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the existing local V5 payload route onto `CommonRuntimeHostV2Batch` while preserving Artifact Store V1, Payload V13/V14, public CLI and report contracts.

**Architecture:** The V5 scheduler remains responsible for DAG readiness, provenance, input materialization, deadlines, store publication, retry policy and public reports. The Batch Host becomes the sole owner of worker start/poll/cancel/reap and private JSONL lifecycle. A small host contract extension carries absolute deadlines, typed post-READY rejection and scheduler failures without inventing worker attempts.

**Tech Stack:** C++20, CMake/Ninja, worker V1 supervisor and JSONL RunLog, Artifact Store V1, Python integration tests, CTest.

**Spec:** `docs/superpowers/specs/2026-10-16-common-runtime-host-v2-stage3d-payload-host-design.md`

## Global Constraints

- Keep `bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE` and existing option names unchanged.
- Keep retry range `0..3`, workers range `1..4`, and existing budget defaults/maxima unchanged.
- Retry only `PROCESS_CRASHED`, `TIMEOUT` and `PROTOCOL_ERROR` after dispatch; preflight/store/provenance/digest/limit failures are not retryable unless the spec explicitly names post-READY `TIMEOUT`.
- Preserve Payload V13, External Inputs V14, Artifact Store V1, Worker V1 frames, public V5 reports and canonical receipt order.
- The scheduler must never pass store credentials or DAG semantics into the Batch Host.
- No worker attempt may be invented for a preflight failure; no successor starts before accepted manifest publication.
- No automatic deletion or rollback of immutable Store objects after a failed acceptance or cancel.
- Stream, Flow, Handoff, network, GPU, plugins, free language, general proving and broad runtime orchestration remain outside this stage and remain DOKUMENTIERT/KONZEPT. :codex-annotation{index="1"}
- Generated CLI artifacts may only be checked through `docs/cli/verify_examples.py` in a complete temporary copy.
- Use the repository's native CMake/CTest flow; do not install packages or change CI configuration.

## Review Focus

- A payload preparation failure before dispatch must produce `RUN_FINISHED:FAILED` without an invented attempt; test in Task 3.
- A canonical READY rejected by a store/digest/publication condition must lose artifact metadata and remain inspector-valid; test in Tasks 1–3.
- A deadline must include preparation, worker execution and publication without being reset at worker start; test in Tasks 1 and 3.
- Cancel during materialization, active worker, pending READY or immediately before acceptance must prevent acceptance and successor starts; test in Task 3.
- A retry must use a new private staging directory and accepted provenance must reference only the successful final attempt; test in Task 3.

---

### Task 1: Extend Batch Host lifecycle contracts

**Files:**
- Modify: `include/bruce/application/common_runtime_host_v2_batch.hpp`
- Modify: `src/application/common_runtime_host_v2_batch.cpp`
- Test: `tests/unit/common_runtime_host_v2_batch_tests.cpp`
- Modify: `CMakeLists.txt` only if a new focused contract target is required

**Interfaces:**
- Consumes: existing `CommonRuntimeHostV2Batch`, `WorkerRequest`, `WorkerResult`, `record_attempt_retry_v2`, `finish_batch_v2`.
- Produces: `CommonRuntimeHostV2AttemptOptions` with optional absolute `steady_clock::time_point deadline`; `CommonRuntimeHostV2RejectionReason` with `OUTPUT_LIMIT_EXCEEDED`, `TIMEOUT`, `ARTIFACT_REFERENCE_INVALID`, `ARTIFACT_MANIFEST_INVALID`, `ARTIFACT_DIGEST_MISMATCH`, `ARTIFACT_STORE_VIOLATION`; overloaded `start_batch_attempt_v2(batch, request, attempt_options)`; `record_attempt_rejection_v2(batch, attempt_id, reason)`; `record_scheduler_failure_v2(batch)`; existing overloads remain source-compatible.

- [ ] **Step 1: Write failing Batch Host contract tests**
  - Add tests for an attempt deadline expiring before start, during polling and after a READY result is projected.
  - Assert timeout projection is `FAILED/TIMEOUT`, preserves usage, cannot accept an artifact and can enter the existing retry decision only where the scheduler requests it.
  - Add tests for every typed rejection reason, unknown reason, repeated decision, rejection after acceptance and rejection after cancel.
  - Add a scheduler-failure test proving `finish_batch_v2` emits `RUN_FINISHED:FAILED` with zero attempts and no fabricated lifecycle events.

- [ ] **Step 2: Run the focused contract target and record RED behavior**
  - Run: `ctest --test-dir /tmp/bruce-stage3d-build -R '^common_runtime_host_v2_batch_contracts$' --output-on-failure`
  - Expected: new tests fail because the overloads/reasons/scheduler-failure projection do not exist.

- [ ] **Step 3: Implement deadline and finalization contracts**
  - Store the optional absolute deadline per active attempt; check it before start and during `poll_batch_v2` without resetting the caller's deadline.
  - Use the existing supervisor cancellation/reap path for an active timeout and project `TIMEOUT`, not `CANCELLED`.
  - Add `record_scheduler_failure_v2` as an idempotent final failure marker; it must not create an attempt or override an already observed owner cancel.

- [ ] **Step 4: Implement typed post-READY rejection**
  - Validate that the attempt is completed, canonical READY, undecided and unaccepted.
  - Append the narrow rejection decision with its reason, clear effective artifact metadata, preserve measured usage, and prevent later acceptance.
  - For `TIMEOUT`, append the rejection event while leaving the completed attempt explicitly retry-pending; `record_attempt_retry_v2` then closes that history before a new attempt starts. All other payload rejection reasons become terminal immediately and cannot be reopened.

- [ ] **Step 5: Run the focused contract target GREEN**
  - Run: `cmake --build /tmp/bruce-stage3d-build -j2 && ctest --test-dir /tmp/bruce-stage3d-build -R '^common_runtime_host_v2_batch_contracts$' --output-on-failure`
  - Expected: all existing Stage 3A/3B/3C Batch Host tests plus new deadline/rejection/scheduler-failure tests pass.

- [ ] **Step 6: Review the task result**
  - Record exact test counts and any environment limitation in `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3d/task-1-report.md`.
  - Do not modify V5 routing in this task.

### Task 2: Extend RunLog and Inspector for payload decisions

**Files:**
- Modify: `src/worker/worker_run_log_v1.cpp` and its public declarations if event validation is declared elsewhere
- Modify: `src/worker/worker_run_inspector_v1.cpp`
- Modify: `tests/unit/worker_run_inspector_v1_tests.cpp`
- Modify: `tests/unit/worker_run_log_v1_tests.cpp` if present, otherwise add the smallest existing contract target
- Modify: `CMakeLists.txt` only when adding a focused target

**Interfaces:**
- Consumes: Task 1's typed rejection names and Batch Host event emission.
- Produces: private JSONL validation for `RESULT_REJECTED` with the six allowed reasons; effective FAILED/TIMEOUT projection with no artifact metadata; valid zero-attempt scheduler failure; one post-result Cancel decision; rejection/acceptance/cancel exclusivity.

- [ ] **Step 1: Write failing Inspector/RunLog tests**
  - Add valid logs for post-READY `RESULT_REJECTED/TIMEOUT`, each terminal Artifact Store reason, and scheduler failure without attempts.
  - Add invalid logs for unknown reasons, metadata-bearing rejection, rejection after acceptance, duplicate rejection, acceptance after rejection, and malformed zero-attempt completion.
  - Cross-check parsed attempt status, failure code, result code, usage and artifact fields against the expected V5 report projection.

- [ ] **Step 2: Run focused RunLog/Inspector targets and record RED behavior**
  - Run: `ctest --test-dir /tmp/bruce-stage3d-build -R 'worker_run_(log|inspector)_v1_contracts' --output-on-failure`
  - Expected: new event/reason cases fail while existing Stage 3C post-result Cancel tests remain green.

- [ ] **Step 3: Add narrow event validation**
  - Whitelist only the six Stage 3D rejection reasons and require `RESULT_REJECTED` to follow an unaccepted canonical READY result.
  - Clear effective reference/digest/size after rejection and retain usage; reject all duplicate or post-acceptance decisions.
  - Keep Worker V1 frame validation and public report parsing unchanged.

- [ ] **Step 4: Validate scheduler-failure and Cancel projections**
  - Permit `RUN_CREATED` → `RUN_FINISHED:FAILED` with no attempts only for the explicit scheduler-failure path.
  - Keep post-result Cancel restricted to one unaccepted result with no artifact acceptance.

- [ ] **Step 5: Run the focused targets GREEN**
  - Run: `cmake --build /tmp/bruce-stage3d-build -j2 && ctest --test-dir /tmp/bruce-stage3d-build -R 'worker_run_(log|inspector)_v1_contracts' --output-on-failure`
  - Expected: all new and existing RunLog/Inspector tests pass.

- [ ] **Step 6: Record the contract review**
  - Write `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3d/task-2-report.md` with event ordering, rejection guards and test counts.

### Task 3: Migrate the V5 Payload route to Batch Host

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp` around `execute`, `execute_payload`, `prepare_payload_inputs`, `publish_payload_output`, `make_request` and the existing payload scheduling loop
- Test: `tests/integration/test_common_runtime_host_v2_stage3d.py`
- Create: `tests/integration/common_runtime_host_v2_stage3d_fixture.cpp` only if existing fixtures cannot exercise real Append/Join payloads
- Modify: `CMakeLists.txt` to register the Stage 3D workflow

**Interfaces:**
- Consumes: Task 1 Host APIs and Task 2 RunLog/Inspector contract; existing Artifact Store V1 and Payload V13/V14 helpers.
- Produces: payload-route gate that uses Batch Host for local V13/V14 execution with `retry=0..3`, workers `1..4`, shared deadlines, typed rejection and scheduler-failure projection; unchanged public CLI/report schemas.

- [ ] **Step 1: Write failing Stage 3D integration tests**
  - Add real payload chain and fan-in scenarios for workers 1 and 2, asserting exact bytes, manifest provenance, accepted predecessor selection and canonical receipt order.
  - Add producer crash-then-success retry, exhaustion with BLOCKED successor, nonretryable digest/manifest/input-limit failures, preflight failure with zero worker attempts, and receipt-budget/store-publication rejection.
  - Add cancel tests before dispatch, during materialization, during active worker, between retries, after READY projection and immediately before acceptance; assert no late `ARTIFACT_ACCEPTED` or successor start.
  - Run `bruce dag-scheduler-v5 payload ... inspect` for every success/failure fixture and assert report/log/history consistency.

- [ ] **Step 2: Run the new Stage 3D workflow to capture RED behavior**
  - Run: `python3 tests/integration/test_common_runtime_host_v2_stage3d.py /tmp/bruce-stage3d-build/bruce`
  - Expected: the new Batch Host payload tests fail because the payload route still uses its legacy lifecycle.

- [ ] **Step 3: Add the payload-route gate**
  - Route only local V13/V14 payload execution without stream/flow/handoff extensions to the new Batch Host scheduler path; leave excluded routes on their current implementation.
  - Preserve existing parsing, Store opening, External-Inputs sidecar validation, `prepare_payload_inputs`, `make_request`, `publish_payload_output`, report rendering and receipt order.

- [ ] **Step 4: Integrate preparation and absolute deadline**
  - Create a deadline before payload preparation for each attempt; pass the remaining absolute deadline to the Batch Host.
  - Recheck owner cancel and deadline after materialization and before worker start. On preflight failure call `record_scheduler_failure_v2`/node failure projection without inventing an attempt.
  - Recreate private input staging for every retry and preserve request-plus-input budget accounting.

- [ ] **Step 5: Integrate result publication and decisions**
  - On Batch Host completion, keep canonical receipt buffering. Before acceptance, recheck cancel/deadline, validate READY, enforce receipt budget, publish the output manifest, then call Host acceptance.
  - Map publication, digest, manifest and store violations to the typed rejection API; map retryable post-dispatch TIMEOUT to the existing explicit retry policy.
  - Start dependents only after successful publication and Host acceptance; never pass Store credentials into the worker.

- [ ] **Step 6: Run Stage 3D integration GREEN**
  - Run: `cmake --build /tmp/bruce-stage3d-build -j2 && ctest --test-dir /tmp/bruce-stage3d-build -R '^common_runtime_host_v2_stage3d_workflows$' --output-on-failure`
  - Expected: all new payload chain, retry, deadline, rejection, cancel and inspector cases pass.

- [ ] **Step 7: Run compatibility regressions**
  - Run the existing V5 payload, payload-inspector, Artifact Store V1, resolve, External Inputs, Stream, Flow and Handoff tests plus Stage 3A/3B/3C tests.
  - Expected: existing public report and CLI contracts remain green; excluded routes are unchanged.

- [ ] **Step 8: Record the implementation review**
  - Write `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3d/task-3-report.md` with exact workflow counts, retry/cancel evidence and any known limitations.

### Task 4: Documentation and final verification

**Files:**
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/implementation/75-current-architecture-and-milestones.md`
- Modify: `docs/cli/commands.md` and/or `docs/cli/formats.md` only where the existing payload lifecycle contract needs an explicit Batch Host note
- Modify: `docs/superpowers/specs/2026-10-16-common-runtime-host-v2-stage3d-payload-host-design.md` status only after verification
- Create: `runs/common-runtime-host-v2-stage3d/reports/task-1.md`, `task-2.md`, `task-3.md`, `final-review.md`
- Modify: `.superpowers/sdd/2026-10-16-common-runtime-host-v2-stage3d/progress.md`

**Interfaces:**
- Consumes: Task 1–3 implementation and reports.
- Produces: evidence-labeled architecture status, final verification report and completed SDD ledger.

- [ ] **Step 1: Update architecture and CLI documentation**
  - Mark only the migrated local Payload V13/V14 route as IMPLEMENTIERT UND BELEGT.
  - State that external services, network/GPU workers, free plugins, free language, general proving and broad orchestration remain DOKUMENTIERT/KONZEPT. :codex-annotation{index="1"}
  - Document the private `RESULT_REJECTED` reasons, deadline scope, scheduler-failure projection and no-rollback Store behavior.

- [ ] **Step 2: Run the required verification set**
  - Native configure/build with `cmake -G Ninja -S . -B /tmp/bruce-stage3d-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release` and `cmake --build /tmp/bruce-stage3d-build`.
  - Focused CTest for Host, RunLog, Inspector, V5 CLI, Stage 3A/3B/3C, Stage 3D, Payload, Artifact Store, External Inputs, Stream, Flow and Handoff.
  - Repeat the Stage 3D workflow at least five times for race-sensitive paths.
  - Run `git diff --check` and Git status attempt; record unusable metadata without claiming commits.
  - Run local Markdown-link checks. Use the CLI verifier only in a complete temporary copy and compare original generated artifact hashes.

- [ ] **Step 3: Write the final review report**
  - Record exact commands, exit codes, test counts, warnings, known host-deps/Git/full-suite limitations and independent reviewer verdict.
  - Mark the Stage 3D spec IMPLEMENTIERT UND BELEGT only when implementation and evidence are complete.

- [ ] **Step 4: Whole-branch review**
  - Have a fresh reviewer inspect the implementation, reports, docs and excluded routes; require PASS before claiming completion.

- [ ] **Step 5: Close the SDD ledger**
  - Update progress with each task's implementer/reviewer, exact evidence and final Stage 3D status.

## Plan Self-Review

- Spec coverage: payload migration, deadline, typed rejection, scheduler failure, retry, cancel, store publication, inspector, compatibility and documentation are covered by Tasks 1–4.
- Type consistency: Task 1 produces the deadline/rejection/scheduler-failure APIs consumed by Tasks 2–3; Task 2 validates their JSONL effects; Task 3 owns scheduler integration; Task 4 owns status/documentation.
- Review focus: all five high-risk input classes have explicit tests in Tasks 1–3.
- Scope: external services, GPU/network execution, plugins, free language, proving and broad orchestration remain excluded.
- No placeholders or unresolved implementation choices remain except the exact internal C++ overload spelling, which Task 1 must preserve through source-compatible defaults and tests.
