# Common Runtime Host V2 / Stage 3A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a small Common Runtime Host V2 layer that supervises one local Worker Boundary V1 attempt, records the existing JSONL lifecycle, exposes controlled cancellation, and is exercised through the existing V5 single-node path.

**Architecture:** Host V2 is an additive application-layer facade over `worker_v1::start_attempt`, `poll_attempt`, `finish_attempt`, `request_cancel`, and `WorkerRunLog`. It owns host-controlled run context and a movable opaque run handle; the worker request and frame encoding remain unchanged. The V5 CLI keeps its public syntax and uses the new facade only for the Stage 3A single-node/single-worker path, while existing multi-node, payload, flow, handoff, and retry paths remain on their current implementation until a later stage.

**Tech Stack:** C++20, CMake/CTest, existing Linux `fork`/pipes/`RLIMIT_AS`/watchdog supervisor, existing JSONL run log, Python integration tests.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-host-v2-stage3-worker-design.md`

## Global Constraints

- Keep C++20 and existing CMake/CTest conventions.
- Do not change Worker Boundary V1 frame encoding, `WorkerRequest`, `WorkerResult`, or V2/V3/V4 CLI output contracts.
- Stage 3A starts one Worker Child per Attempt; the supervisor may also start its internal watchdog helper process.
- The host context, executable path, run root, budget, and fixed `grace_ms == 1000` are host-controlled; users cannot provide a worker executable, PID, signal, or free path through the worker request.
- `RLIMIT_AS` and peak telemetry are per observed Worker PID; do not claim an aggregated process-group memory limit or an exact descendant peak.
- Public successful output contains only validated status, result code, usage, digest, size, and logical reference; never raw payload bytes, stderr, PIDs, or absolute paths.
- Retry remains opt-in through the existing V5 CLI option and is not implicit in the Host V2 API.
- JSONL records use the existing `worker-run-log-v1` writer and inspector; no hand-edited generated CLI artifacts.
- Do not add networking, GPU execution, dynamic code plugins, external providers, free-form language, general proving, or multi-node orchestration in Stage 3A. :codex-annotation{index="1"}
- Run focused tests and required build checks; document known Cockpit socket and unusable-Git limitations instead of hiding them.

## Review Focus

- Invalid host context or budget mismatch must fail before a Worker Child starts; Task 1 test `rejects_invalid_context_without_starting_child`.
- A Worker Result with mismatched `output_artifact_ref`, digest, or size must never produce `ARTIFACT_ACCEPTED`; Task 1 test `rejects_noncanonical_ready_projection`.
- A cooperative and an ignored cancellation must both terminate through the host path with the fixed 1000-ms grace; Task 1 tests `cooperative_cancel_lifecycle` and `ignored_cancel_is_escalated`.
- A watchdog timeout or address-space crash must retain the conservative existing failure classification and clean the process group; Task 1 test `timeout_and_process_cleanup`.
- V5's existing public report and multi-node behavior must remain unchanged while the single-node path uses Host V2; Task 2 integration test `single_node_host_v2_preserves_v5_contract`.

---

### Task 1: Implement Common Runtime Host V2 single-worker facade

**Files:**
- Create: `include/bruce/application/common_runtime_host_v2.hpp`
- Create: `src/application/common_runtime_host_v2.cpp`
- Create: `tests/unit/common_runtime_host_v2_tests.cpp`
- Modify: `CMakeLists.txt: core source list and BUILD_TESTING unit target block`
- Test fixture: reuse the self-spawning worker-child pattern from `tests/unit/worker_supervisor_v1_tests.cpp`

**Interfaces:**
- Consumes: `worker_v1::WorkerRequest`, `worker_v1::Budget`, `worker_v1::WorkerResult`, `worker_v1::WorkerAttemptHandle`, `WorkerRunLog`, and the existing abort/supervisor contracts.
- Produces:
  - `struct CommonRuntimeHostV2Request { worker_v1::WorkerRequest worker_request; }`
  - `struct CommonRuntimeHostV2Context { std::filesystem::path run_root; std::filesystem::path worker_executable; worker_v1::Budget budget; std::uint64_t grace_ms{1000}; }`
  - movable opaque `CommonRuntimeHostV2Run`
  - `CommonRuntimeHostV2Outcome { worker_v1::WorkerResult result; int host_exit_code; }`
  - `start_single_worker_v2(request, context)`
  - `poll_single_worker_v2(run)`
  - `finish_single_worker_v2(run)`
  - `request_cancel_v2(run)`

- [ ] **Step 1: Write failing contract tests** in `tests/unit/common_runtime_host_v2_tests.cpp`. The fixture must support a valid READY result, cooperative CANCEL, ignored CANCEL/timeout, malformed result, and a child that writes its PID. Assert the exact lifecycle and result classifications named in Review Focus.
- [ ] **Step 2: Run the new unit target before implementation.** Run `ctest --test-dir /tmp/bruce-stage3a-build -R '^common_runtime_host_v2_contracts$' --output-on-failure`. Expected: target is not yet configured or tests fail because the API is absent.
- [ ] **Step 3: Add the public Host V2 declarations.** Keep the run handle opaque and movable, prohibit copying, and expose only the request/context/outcome plus start/poll/finish/cancel operations. Include `<filesystem>`, `<optional>`, `<cstdint>`, and the existing Worker V1 header.
- [ ] **Step 4: Implement the Host V2 state machine.** Validate the request/context before spawning: exact budget equality, `grace_ms == 1000`, canonical host executable, owner-only run root, and valid Worker V1 request. Reuse the existing Supervisor API without changing its frame or budget semantics.
- [ ] **Step 5: Implement host-owned lifecycle logging.** Create or open `run/control/run-log.jsonl`, append `RUN_CREATED`, `ATTEMPT_STARTED`, and `STATUS(RUNNING)` in order, then append `PROCESS_EXITED`, `RESULT_RECEIVED`, optional `ARTIFACT_ACCEPTED`, and `RUN_FINISHED` exactly once. Continue the existing monotonic sequence and never duplicate an already-existing event.
- [ ] **Step 6: Implement canonical result projection.** Accept `READY` only when the Supervisor has already validated `output_artifact_ref.reference == "result"`, matching size, matching SHA-256, and matching `payload_digest`. Map the validated metadata to the Host V2 outcome; map non-ready results to a null/empty public artifact view and host exit 1.
- [ ] **Step 7: Implement cancellation and polling.** `request_cancel_v2` is idempotent, appends one `CANCEL_REQUESTED` event, and delegates to `worker_v1::request_cancel`. Polling must preserve the existing late-result ordering and finish the JSONL run exactly once.
- [ ] **Step 8: Run focused unit tests.** Run `cmake --build /tmp/bruce-stage3a-build --target bruce_common_runtime_host_v2_tests` and `ctest --test-dir /tmp/bruce-stage3a-build -R '^(common_runtime_host_v2_contracts|worker_supervisor_v1_contracts|worker_run_log_v1_contracts)$' --output-on-failure`. Expected: all selected tests pass.
- [ ] **Step 9: Self-review and report.** Check no Worker V1 or V2/V3/V4 contract changed, run `git diff --check` if Git metadata is available, and write `.superpowers/sdd/<plan>/task-1-report.md`.

### Task 2: Route V5 single-node/single-worker execution through Host V2

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp: execute path and single-node state handling`
- Modify: `CMakeLists.txt: add the Stage 3A integration test`
- Create: `tests/integration/test_common_runtime_host_v2_stage3.py`

**Interfaces:**
- Consumes: Task 1 Host V2 API and lifecycle result; existing V5 parsed node/spec/report helpers.
- Produces: existing `bruce dag-scheduler-v5 execute SPEC --output BUNDLE` behavior for the Stage 3A route, with no new public command and no changed V5 JSON schema.

- [ ] **Step 1: Write the failing integration test** `single_node_host_v2_preserves_v5_contract`. Execute a one-node literal spec with default options and assert exit 0, existing `dag-scheduler-v5-cli-v1` report keys, no raw payload bytes, a canonical digest/reference, and the exact Host V2 JSONL lifecycle.
- [ ] **Step 2: Add failure-path integration cases.** Cover a one-node timeout with `--max-wall-ms 1`, an external `dag-scheduler-v5 cancel BUNDLE` request against a long-running run, and a malformed/digest-rejected result if the existing executable fixture can produce one. Assert `TIMEOUT`/`CANCELLED`, no artifact acceptance on failure, and process cleanup.
- [ ] **Step 3: Run the integration test before routing.** Run `python3 tests/integration/test_common_runtime_host_v2_stage3.py /tmp/bruce-stage3a-build/bruce`. Expected: the test fails because V5 still uses its direct per-node Supervisor path.
- [ ] **Step 4: Add a narrow single-node gate in V5.** Select Host V2 only when the route is ordinary `execute`, the parsed spec has exactly one node, `--workers 1`, and no payload/stream/flow/handoff extension. Preserve the current path for multi-node, payload, flow, handoff, and explicit retry runs until later orchestration work.
- [ ] **Step 5: Adapt V5 setup and report generation.** Prepare the same owner-only run root and request input, let Host V2 own the single-node lifecycle log, and project its validated outcome into the existing V5 single-node report/history format. Do not add public fields or raw local paths.
- [ ] **Step 6: Preserve cancel semantics.** Ensure the existing V5 cancel marker is observed by the V5 host loop and translated to `request_cancel_v2`; keep the current metadata-only cancel report unchanged.
- [ ] **Step 7: Run focused V5 verification.** Run `cmake --build /tmp/bruce-stage3a-build --target bruce` and `python3 tests/integration/test_common_runtime_host_v2_stage3.py /tmp/bruce-stage3a-build/bruce`, then rerun `python3 tests/integration/test_dag_scheduler_v5_cli.py /tmp/bruce-stage3a-build/bruce`. Expected: the new Stage 3A cases and existing V5 CLI cases pass.
- [ ] **Step 8: Self-review and report.** Confirm multi-node/payload/flow/handoff code paths remain byte-compatible, inspect the run log for monotonic seq/fsync-backed records, and write the Task 2 report.

### Task 3: Documentation, build wiring, and final verification

**Files:**
- Modify: `docs/architecture/03-blueprint-consolidation.md` only if the current status matrix needs the Stage 3A implementation evidence
- Modify: `docs/implementation/75-current-architecture-and-milestones.md` only to record the verified Host V2 milestone
- Modify: `docs/cli/README.md` and/or `docs/cli/formats.md` only if the existing V5 contract needs a factual Stage 3A note
- Create: `runs/common-runtime-host-v2-stage3/reports/task-1.md`
- Create: `runs/common-runtime-host-v2-stage3/reports/task-2.md`
- Create: `runs/common-runtime-host-v2-stage3/reports/final-review.md`

**Interfaces:**
- Consumes: Task 1 and Task 2 implementation/test results.
- Produces: repository documentation that distinguishes IMPLEMENTIERT UND BELEGT from remaining DOKUMENTIERT/KONZEPT work.

- [ ] **Step 1: Compare documentation with implementation.** Update only factual status/evidence lines; do not promote multi-node orchestration, GPU, network, free-language, proving, plugin, or Cockpit concepts to implemented.
- [ ] **Step 2: Run the required native-path build and focused checks.**
  - `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-stage3a-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`
  - `scripts/with-host-deps.sh cmake --build /tmp/bruce-stage3a-build`
  - focused CTest selection for Host V2, Worker V1, V5 CLI, abort, and inspector tests.
  - `git diff --check` and Git status when metadata is usable; otherwise record the known limitation.
- [ ] **Step 3: Run documentation/CLI validation.** Run the existing CLI verification path without manually editing generated artifacts, validate JSON with `python3 -m json.tool` where applicable, and run the local Markdown-link checker used by the repository.
- [ ] **Step 4: Write the final evidence report.** Record exact commands, exit codes, test counts, known Cockpit socket sandbox limitation, host-wrapper limitations if present, and the distinction between implemented evidence and future concepts.
- [ ] **Step 5: Commit/task handoff.** The implementation agent(s) must commit their code using the repository's existing conventions; no push, merge, or deletion is performed by this plan.

## Rollout order

1. Task 1 must pass before Task 2 starts.
2. Task 2 must pass before Task 3 documentation is updated.
3. A whole-branch review must inspect the combined Task 1–3 changes before declaring Stage 3A implemented.

## Expected verification

At minimum, the focused acceptance set is:

```
common_runtime_host_v2_contracts
worker_supervisor_v1_contracts
worker_run_log_v1_contracts
worker_run_inspector_v1_contracts
dag_scheduler_v5_cli_workflows
dag_scheduler_v5_abort_workflows
dag_scheduler_v5_inspector_workflows
```

The full repository suite may retain the known Cockpit socket failure in this sandbox. That failure must be reported, not silently reclassified.
