# Worker Boundary V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the first local Bruce-supervised worker boundary behind a new experimental V5 DAG CLI while preserving all V2/V3/V4 contracts.

**Architecture:** Bruce remains the supervisor and starts one built-in child process per worker attempt. The parent validates framed request/result traffic, enforces budgets and cancellation, stores attempt artifacts under a private run directory, and writes an append-only JSONL control log. V5 reuses the existing V3 spec format and fixed adapters, but schedules ready nodes through the worker boundary with `--workers 1..4`.

**Tech Stack:** C++20, existing `bruce_core` library, POSIX/Linux process APIs (`fork`, `exec`, `poll`, `wait4`, `setrlimit`, process groups), OpenSSL SHA-256 already used by the repository, Python `unittest` integration tests, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-15-worker-boundary-v1-design.md`

## Global Constraints

- V2, V3 and V4 public contracts and emitted bytes remain unchanged.
- Worker results contain only status, result metadata, digest and reference; no payload bytes cross the result frame.
- Retry occurs only when the user supplied `--retry N`; `0 <= N <= 3`, default `0`.
- Hard budgets are wall time, input bytes, output bytes, operations and process memory.
- V1 grace time is a fixed `1000 ms`; it is not a CLI option.
- `--workers` defaults to `1` and has an absolute maximum of `4`.
- The worker executable is Bruce's own internal `worker-v1 child` mode; no user executable, plugin or network target is accepted.
- Paths are relative to a private run tree; absolute paths, `..` and escaping symlinks are rejected.
- The first handoff record is Bruce-owned append-only `run/control/run-log.jsonl`; workers never write it.
- Raw payloads, local paths, command lines, tokens and unfiltered stderr stay out of public reports and JSONL events.
- Linux process-limit and process-group behavior is explicit; portability beyond Linux is out of scope for V1.

## Review Focus

- A truncated frame or false `payload-bytes` value must fail closed without an unbounded allocation; test in Task 1.
- A JSONL log containing a duplicate/non-monotone sequence or digest/size mismatch must invalidate the run; test in Task 2.
- A worker that ignores cancellation must be terminated as a process group after exactly the fixed grace period; test in Task 3.
- A retryable process failure must not replay a semantic `FAILED`, `BLOCKED`, `CANCELLED` or digest-mismatch result; test in Task 4.
- `--workers N` must never start a dependent node early or exceed four live children, while `--workers 1` remains deterministic; test in Task 5.

## File Map

**Create:**

- `include/bruce/worker/worker_boundary_v1.hpp` — versioned frame, request/result, budget, status and error types.
- `src/worker/worker_boundary_v1.cpp` — bounded frame encoding/decoding and contract validation.
- `include/bruce/worker/worker_run_log_v1.hpp` — JSONL event and append-only log API.
- `src/worker/worker_run_log_v1.cpp` — canonical event rendering, sequence validation and safe log append.
- `include/bruce/worker/worker_supervisor_v1.hpp` — parent-side attempt execution API.
- `src/worker/worker_supervisor_v1.cpp` — Linux child creation, pipes, process groups, budgets, cancellation and reaping.
- `apps/dag_scheduler_v5_cli.hpp` — V5 argument parsing, internal child route and bounded DAG orchestration.
- `tests/unit/worker_boundary_v1_tests.cpp` — frame, budget and result-contract tests.
- `tests/unit/worker_run_log_v1_tests.cpp` — JSONL event and path/sequence tests.
- `tests/unit/worker_supervisor_v1_tests.cpp` — deterministic helper-process lifecycle tests.
- `tests/unit/dag_scheduler_v5_cli_tests.cpp` — V5 parser, defaults and scheduler contract tests.
- `tests/integration/test_dag_scheduler_v5_cli.py` — end-to-end CLI, retries, cancellation, limits and multi-worker workflows.

**Modify:**

- `CMakeLists.txt` — add worker sources, unit targets and the V5 Python CTest workflow.
- `apps/bruce_cli.cpp` — include V5 CLI, advertise its public route, dispatch `dag-scheduler-v5` and keep `worker-v1 child` internal.
- `docs/cli/commands.md` — document the experimental V5 command and JSONL/attempt behavior with the repository evidence labels.
- `docs/cli/formats.md` — document the exact V5 invocation, reused V3 spec input, run tree and metadata-only result contract.

## Task 1: Freeze the Worker Contract and Frame Codec

**Files:**

- Create: `include/bruce/worker/worker_boundary_v1.hpp`
- Create: `src/worker/worker_boundary_v1.cpp`
- Create: `tests/unit/worker_boundary_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Produces `bruce::worker_v1::FrameType`, `Frame`, `Budget`, `ArtifactRef`, `WorkerRequest`, `WorkerResult`, `FailureCode` and `encode_frame`/`decode_frame` functions.
- `encode_frame(FrameType, std::string_view) -> std::string` emits the exact `BRUCE_WORKER_FRAME_V1` header and SHA-256 metadata.
- `decode_frame(std::string_view, std::size_t max_payload_bytes) -> Frame` rejects unknown types, duplicate/missing headers, invalid decimal values, wrong digest, trailing bytes and oversized payloads.

- [ ] **Step 1: Write failing unit tests** for valid request/result frame round trips, embedded NUL/newline payloads, wrong digest, truncated body, unknown type, duplicate header and payload-limit rejection. Assert the exact five-part framing contract and that result models have no payload-byte field.
- [ ] **Step 2: Run the focused unit target** with `ctest --test-dir /tmp/bruce-build -R worker_boundary_v1_contracts --output-on-failure`; verify the new tests fail before implementation.
- [ ] **Step 3: Implement the types, constants and bounded codec** in the two worker-boundary files. Use the existing `bruce::sha256`, `bruce::Error` and C++20 spans/strings; validate all numeric ranges before allocation.
- [ ] **Step 4: Add the unit target** linked to `bruce_core` with the repository's `-Wall -Wextra -Werror` flags, then rerun the focused test and expect PASS.
- [ ] **Step 5: Run the existing V2/V3/V4 unit targets** and confirm no public contract output changes.

## Task 2: Add the Bruce-Owned JSONL Handoff Log

**Files:**

- Create: `include/bruce/worker/worker_run_log_v1.hpp`
- Create: `src/worker/worker_run_log_v1.cpp`
- Create: `tests/unit/worker_run_log_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Produces `WorkerLogEvent` with `schema`, optional `run_id`/`attempt_id`, `seq`, `ts_unix_ms`, `event`, `status`, optional digest/reference/size/usage fields.
- Produces `WorkerRunLog::create(path, run_id)` and `WorkerRunLog::append(const WorkerLogEvent&)`.
- `append` is the only write path, requires strictly increasing sequence numbers, emits one compact JSON object plus LF, and never emits raw payload bytes, absolute paths, command lines or stderr.

- [ ] **Step 1: Write failing tests** for canonical event rendering, exact sequence monotonicity, allowed event names, rejection of unknown events, digest/size mismatch, append-only newline behavior and rejection of an attempt path outside the run root.
- [ ] **Step 2: Run `worker_run_log_v1_contracts`** and verify the tests fail.
- [ ] **Step 3: Implement canonical JSON escaping with existing `json_string`, event validation, parent-owned append mode and private-run path checks. Treat a partial or malformed existing line as `PROTOCOL_ERROR`.
- [ ] **Step 4: Register and run the unit target; expect PASS.** Confirm the log contains metadata only and remains readable line by line.
- [ ] **Step 5: Add a regression assertion** that V2/V3/V4 reports do not gain JSONL fields or paths.

## Task 3: Implement the Linux Supervisor and Child Lifecycle

**Files:**

- Create: `include/bruce/worker/worker_supervisor_v1.hpp`
- Create: `src/worker/worker_supervisor_v1.cpp`
- Create: `tests/unit/worker_supervisor_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Produces `SupervisorOptions { executable, run_root, budgets, grace_ms=1000 }`, `WorkerAttemptHandle start_attempt(const WorkerRequest&, const SupervisorOptions&)`, `std::optional<WorkerAttemptOutcome> poll_attempt(WorkerAttemptHandle&)`, `WorkerAttemptOutcome finish_attempt(WorkerAttemptHandle&)` and `void request_cancel(WorkerAttemptHandle&)`.
- `start_attempt` creates the attempt directory, starts Bruce's child mode with pipes, applies `RLIMIT_AS`, creates a process group and closes unrelated descriptors. `poll_attempt`/`finish_attempt` enforce monotonic wall/input/output/operation/memory limits, read one validated result frame, write no public payload and reap every child.
- `request_cancel` sends the cooperative frame, waits the fixed `1000 ms` grace and escalates the complete process group with SIGTERM/SIGKILL.

- [ ] **Step 1: Write failing lifecycle tests** using a controlled helper child mode for READY, malformed frame, crash, timeout, ignored CANCEL and output-limit cases. Assert error-code separation, no orphan process and recorded peak-memory diagnostics.
- [ ] **Step 2: Run `worker_supervisor_v1_contracts`** and verify the tests fail.
- [ ] **Step 3: Implement POSIX pipes, `fork`/`exec`, process-group setup, `setrlimit(RLIMIT_AS)`, nonblocking/poll reads, monotonic deadlines, fixed-grace cancellation and `wait4`/Procfs peak collection. Map failures only to the versioned error codes from the spec.
- [ ] **Step 4: Run the focused supervisor tests on Linux** and expect PASS; skip only platform-unavailable tests with an explicit CTest skip reason.
- [ ] **Step 5: Verify repository whitespace checks when Git metadata is available and ensure no test writes outside its temporary directory.

## Task 4: Wire the Internal Worker Child and Single-Node V5 Execution

**Files:**

- Create: `apps/dag_scheduler_v5_cli.hpp`
- Create: `tests/unit/dag_scheduler_v5_cli_tests.cpp`
- Modify: `apps/bruce_cli.cpp`, `CMakeLists.txt`

**Interfaces:**

- Public route: `bruce dag-scheduler-v5 execute SPEC --output BUNDLE` with the options defined in the spec.
- Internal route: `bruce worker-v1 child` reads one framed `WorkerRequest`, resolves only parent-approved relative input references, invokes the fixed existing V3 adapter registry (`literal-v1`, `append-v1`, `join-v1`), writes one output artifact in the attempt directory and returns only a framed `WorkerResult`.
- V5 single-node execution translates a parsed V3 node into `WorkerRequest`, runs it through `run_attempt`, validates the output size/digest and emits a metadata-only JSON report plus an accepted final reference.

- [ ] **Step 1: Write failing parser and single-node tests** for exact usage, defaults, every maximum, invalid zero/negative/over-maximum values, internal child rejection from public help, successful literal/append execution, output digest mismatch and no raw payload in stdout.
- [ ] **Step 2: Run `dag_scheduler_v5_cli_contracts`** and verify the tests fail.
- [ ] **Step 3: Implement the CLI parser by reusing `dag_scheduler_v3_cli_detail::parse_spec`; add `--retry`, all five budget options and `--workers`, with defaults `0`, `30000`, `65536`, `65536`, `1000000`, `268435456`, `1` and maxima `3`, `600000`, `1048576`, `5242880`, `10000000`, `2147483648`, `4`.
- [ ] **Step 4: Implement the internal child route and single-node parent path.** Store output bytes only in the private attempt file, send digest/reference metadata through the result frame, and append `RUN_CREATED`, `ATTEMPT_STARTED`, `RESULT_RECEIVED`, `ARTIFACT_ACCEPTED` and `RUN_FINISHED` events.
- [ ] **Step 5: Run the focused target and assert PASS; verify existing CLI help and V2/V3/V4 command output are byte-identical.

## Task 5: Add Retry and Bounded Multi-Worker DAG Scheduling

**Files:**

- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v5_cli_tests.cpp`
- Modify: `tests/integration/test_dag_scheduler_v5_cli.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

- The V5 scheduler maintains canonical ready/remaining-dependency sets from the V3 spec, starts at most `workers` live attempts, and only releases successors after an accepted predecessor receipt.
- `--workers 1` preserves canonical topological result/trace order. `--workers N` allows up to N independent ready nodes but excludes timing, PID and scheduling order from the semantic digest.
- `--retry N` creates distinct attempt IDs and private output directories, retries only process crash, timeout and transient transport/protocol failure, and never retries semantic failure, blocked, cancelled or digest mismatch.

- [ ] **Step 1: Add failing unit tests** for two independent roots with `--workers 1` and `--workers 2`, dependent-node gating, maximum-four rejection, retry-on-crash, no-retry-without-option, non-retryable FAILED/BLOCKED/CANCELLED and deterministic semantic receipts.
- [ ] **Step 2: Add failing Python workflows** for a multi-node V3 spec, JSONL event sequence, `--workers` defaults/maxima, retry count, timeout/cancellation and output-bundle publication.
- [ ] **Step 3: Run the focused unit and Python tests** and verify failure before implementation.
- [ ] **Step 4: Implement the bounded scheduler and retry state machine** around `run_attempt`; use one attempt log sequence, separate attempt IDs, deterministic receipt ordering and controlled cancellation of remaining live workers after a terminal run failure.
- [ ] **Step 5: Run both focused suites and expect PASS.** Assert that no dependent child starts before its input reference is accepted and no payload bytes appear in the public report or JSONL.

## Task 6: Document, Regenerate CLI Artifacts and Verify the Full Contract

**Files:**

- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`
- Modify: generated `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md` only through `docs/cli/verify_examples.py` in a temporary native copy, then copy verified generated results if required by the repository workflow.
- Modify: `runs/` only for reproducible verification evidence, never as production source.

- [ ] **Step 1: Add documentation** marked **IMPLEMENTIERT UND BELEGT** only after the code and tests pass; describe V5 as experimental, the reused V3 spec, fixed budgets, JSONL event names, `--workers`, retry rules and the fixed 1000 ms grace period.
- [ ] **Step 2: Build a native temporary copy outside the space-containing GVFS path** and run the repository-prescribed CMake configure/build/CTest commands with host dependencies.
- [ ] **Step 3: Run the focused worker/V5 tests, all existing V2/V3/V4 tests, `ctest --output-on-failure`, and `docs/cli/verify_examples.py` against the built binary.
- [ ] **Step 4: Record exact commands, exit codes, pass/skip counts and any sandbox-only socket limitations in a new `runs/bruce-worker-boundary-v1-<date>.md` evidence file.
- [ ] **Step 5: Perform final checks:** no secrets, no generated artifact hand edits, no stale open-decision wording, no public payload bytes, no V2/V3/V4 regressions, clean whitespace and an accurate Git status where Git metadata is available.

## Verification Commands

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-worker-v1-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-worker-v1-build
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-worker-v1-build -R 'worker_boundary_v1_contracts|worker_run_log_v1_contracts|worker_supervisor_v1_contracts|dag_scheduler_v5_cli_contracts|dag_scheduler_v5_cli_workflows' --output-on-failure
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-worker-v1-build --output-on-failure
python3 docs/cli/verify_examples.py /tmp/bruce-worker-v1-build/bruce
```

The last command is run from a native copy or against the native build binary; it must not be run from the current path with a colon-containing GVFS path. Existing V2/V3/V4 failures are regressions; an already-known sandbox socket failure must remain separately identified rather than hidden.
