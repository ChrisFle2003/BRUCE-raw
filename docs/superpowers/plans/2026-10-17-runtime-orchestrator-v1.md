# Runtime Orchestrator V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` (recommended) or
> `superpowers:executing-plans` to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one Linux-local, in-memory controller that admits, runs,
cancels and reports multiple already-described V6 DAG runs without changing
their canonical artifacts, records or replay.

**Architecture:** The controller owns only queue state and declared
reservations.  A prepared job delegates its complete execution to the existing
V6 host API in a joinable controller-owned thread; V6 remains the only owner of
its attempts, sidecar, workers, FD 6, remote restore and record publication.
The public surface is one foreground, line-oriented local CLI host so a single
process can receive `submit`, `status` and `cancel` while jobs run.  It is not a
socket service, daemon, process launcher or persistence format.

**Tech Stack:** C++20; Linux; CMake/CTest; Python integration tests; existing
Logic-Mod Artifact DAG V6 and supervision-sidecar APIs.

**Spec:** `docs/superpowers/specs/2026-10-17-runtime-orchestrator-v1-design.md`

## Locked V1 decisions

- The local input form is `bruce runtime-orchestrator-v1 serve` with bounded
  canonical JSON objects, one command per stdin line and one JSON reply per
  stdout line.  `submit`, `status` and `cancel` are commands of that one
  foreground host, not separate processes; stdin EOF stops accepting work and
  drains active runs before exit.
- `submit` requires all finite V6 admission values: `max_parallel_nodes`,
  `max_total_wall_ms`, `max_total_payload_bytes` and
  `max_started_workers`.  They are copied unchanged into V6.  Optional
  `cancel_after_ms` and `remote_restore` retain their existing V6 meaning, but
  do not create a new global resource category.  There are no orchestrator
  defaults and no changed V6 CLI defaults.
- The serving command requires all five finite host limits explicitly:
  `max_queued_runs`, `max_active_runs`, `max_reserved_parallel_nodes`,
  `max_reserved_started_workers`, and `max_reserved_payload_bytes`.
  The plan deliberately introduces no capacity promise or undocumented
  “reasonable” default.  A later deployment calibration may choose values.
- A V6 private host-cancel latch is the only new V6 seam.  It is empty for all
  existing V6 callers and never appears in a CLI, Spec, artifact, record,
  replay or sidecar.  V6 observes it in the same polling/admission locations
  as its existing host cancel path and performs its own drain/escalation.
- Job plans are prepared and ownership-checked before their queue position is
  issued.  The host retains the prepared V6 registry/session state until its
  run thread joins.  Status stores only opaque IDs and bounded metadata; it
  never returns submitted filesystem paths, URL, PID, FD, frame, payload,
  signature, credential or raw telemetry.
- V1 is in-memory only.  It neither writes a queue journal nor reimports
  files, resumes jobs, repairs sidecars/records, rolls back artifacts or
  claims recovery after the process dies.

## Global Constraints

- Preserve V2–V6 Spec digest, DAG ID, result artifacts, successful canonical
  record bytes and replay behavior exactly.  Replay never loads the
  orchestrator, starts a worker, creates a sidecar or contacts remote restore.
- The orchestrator starts no worker, signals no PID/PGID, opens no worker FD,
  and reads no V6 frame.  It requests cancellation only through the private
  V6 latch and releases an admission only after the V6 call returns.
- FIFO uses `(enqueue_seq, run_id)`.  A non-fitting head blocks younger jobs;
  only a reservation exceeding a single host maximum becomes `BLOCKED`.
- A queue cancel performs no V6 call or store/record/report mutation.  An
  active cancel latches once, transitions through `DRAINING`, joins V6, then
  frees every reservation exactly once.
- Host/global reservations are admission accounting, not CPU, cgroup, RAM,
  process, I/O or network enforcement.  Do not add `RLIMIT_AS` values or call
  them a physical memory limit.
- Treat all input as one effective UID.  Reject unsafe/non-owner plan files
  and report roots before enqueue.  Do not add HTTP, RPC, Unix sockets,
  Cockpit, multiple users, priorities, retries, backoff, preemption, aging or
  recovery.
- Build in a fresh native `/tmp` directory.  Generated CLI artifacts may be
  changed only by `docs/cli/verify_examples.py` in a complete temporary copy.

## File Map

| File | Change | Responsibility |
| --- | --- | --- |
| `include/bruce/mod/logic_mod_artifact_dag_v6.hpp`, `src/mod/logic_mod_artifact_dag_v6.cpp` | modify | Optional private-to-host cancellation latch, retaining V6 defaults and record/replay bytes. |
| `include/bruce/mod/logic_mod_artifact_dag_v6_plan.hpp`, `src/mod/logic_mod_artifact_dag_v6_plan.cpp` | create | Prepare a bounded, ownership-checked V6 invocation and retain its registries/sessions until `run()`. |
| `include/bruce/runtime/runtime_orchestrator_v1.hpp`, `src/runtime/runtime_orchestrator_v1.cpp` | create | State machine, FIFO queue, atomic reservation accounting, admission, cancellation, drain and redacted status. |
| `apps/runtime_orchestrator_v1_cli.hpp`, `apps/bruce_cli.cpp` | create/modify | Strict foreground `serve` protocol, dispatch and help; no direct worker entry point. |
| `CMakeLists.txt` | modify | Sources, CTest targets and static fixtures. |
| `tests/unit/logic_mod_artifact_dag_v5_tests.cpp` | modify | Add V6 external-latch regression under the existing V6 CMake target. |
| `tests/unit/runtime_orchestrator_v1_tests.cpp` | create | State, FIFO, reservation, cancel/drain, no-leak and compatibility contract tests. |
| `tests/integration/test_runtime_orchestrator_v1_cli.py` | create | Real foreground protocol, redaction, cancellation, host-exit and V6 workflow tests. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/architecture/03-blueprint-consolidation.md`, `docs/superpowers/specs/2026-10-17-runtime-orchestrator-v1-design.md` | modify | Verified public contract and actual limits only after green evidence. |
| `docs/superpowers/sdd/runtime-orchestrator-v1/task-6-report.md` | create | Reproducible evidence, environment limits and final review result. |

## Review Focus

- Task 1: a supplied latch cancelled before the first V6 wave causes no worker
  start or record publication, while an unset latch is byte-compatible.
- Task 2: overflow, duplicate opaque IDs and a malformed/foreign/symlink plan
  are rejected without consuming `enqueue_seq` or creating a sidecar.
- Task 3: a fitting younger job never overtakes a valid non-fitting FIFO head;
  an impossible head becomes `BLOCKED` and unblocks the next job.
- Task 4: cancel racing completion cannot free a reservation twice, relabel an
  external V6 cancel as host cancellation, or start a later wave.
- Tasks 5–6: public output and error objects reveal no paths, PID/PGID, URL,
  credential, payload/frame, signature or raw sidecar telemetry; host exit
  claims only V6 cleanup, never recovery.

---

### Task 1: Add the V6 host-cancel seam without changing V6 contracts

**Files:**
- Modify: `include/bruce/mod/logic_mod_artifact_dag_v6.hpp`,
  `src/mod/logic_mod_artifact_dag_v6.cpp`, `tests/unit/logic_mod_artifact_dag_v5_tests.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces `LogicModArtifactDagControlV6::host_cancel_latch` as an optional
  `std::shared_ptr<const std::atomic_bool>` (empty by default).
- V6 observes `true` before a wave starts and while active attempts are
  polled; it returns its existing `DAG_HOST_CANCELLED` outcome only after its
  own attempt drain.  This field is host-library-only and does not serialize.

- [ ] **Step 1: Write failing V6 latch tests.**

  Add tests for a pre-latched run (no child, store, record or report) and a
  latch raised during the timed fixture (V6 reaps started work, admits no next
  wave, preserves a result accepted before observation).  Compare default V6
  record bytes against the current fixture baseline.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-orchestrator-task1 -R 'logic_mod_artifact_dag_v[2-6]_contracts' --output-on-failure`

  Expected: the new latch cases fail before the field/observations exist;
  legacy V2–V5 groups pass.

- [ ] **Step 3: Implement only the optional latch observation.**

  Check it at V6's existing pre-wave and active polling boundaries.  Reuse
  the existing V6 cancel/drain code; do not signal, add a timer, alter
  `cancel_after_ms`, write the latch into the sidecar, or change V2–V6
  record/read/replay functions.

- [ ] **Step 4: GREEN and independent V6 compatibility review.**

  Run: `cmake -G Ninja -S . -B /tmp/bruce-orchestrator-task1 -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bruce-orchestrator-task1 --target logic_mod_artifact_dag_v2_tests logic_mod_artifact_dag_v3_tests logic_mod_artifact_dag_v4_tests logic_mod_artifact_dag_v5_tests logic_mod_artifact_dag_v6_tests && ctest --test-dir /tmp/bruce-orchestrator-task1 -R 'logic_mod_artifact_dag_v[2-6]_contracts' --output-on-failure`

  Expected: selected groups pass.  A fresh reviewer confirms a null latch is
  behaviorally/byte compatible and the only child owner remains V6.

### Task 2: Prepare bounded V6 jobs and implement the pure queue state machine

**Files:**
- Create: `include/bruce/mod/logic_mod_artifact_dag_v6_plan.hpp`,
  `src/mod/logic_mod_artifact_dag_v6_plan.cpp`,
  `include/bruce/runtime/runtime_orchestrator_v1.hpp`,
  `src/runtime/runtime_orchestrator_v1.cpp`,
  `tests/unit/runtime_orchestrator_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `LogicModArtifactDagV6PlanRequest` contains the existing V6 paths/control
  and no worker handles.  `prepare_logic_mod_artifact_dag_v6_plan(...)`
  performs bounded parsing, effective-UID owner-only/no-symlink checks and
  creates a move-only plan owning the V6 registry/session lifetimes;
  `plan.run(latch)` delegates once to `run_logic_mod_artifact_dag_v6`.
- `RuntimeOrchestratorLimitsV1` contains exactly the five explicit host
  limits.  `RuntimeOrchestratorV1` exposes `submit(prepared_plan,
  reservation)`, `cancel(run_id)`, `poll()`, `status(run_id)` and
  `statuses()`; its test seam accepts a run callable but production uses the
  prepared V6 plan.
- Status uses only `run_id`, `enqueue_seq`, state, closed reason, monotone
  submit/start/finish sequence numbers, reservation, V6 `accepted`/status/DAG
  ID and an opaque sidecar reference token.

- [ ] **Step 1: Write failing model and preparation tests.**

  Pin `QUEUED`, `ADMITTED`, `STARTING`, `RUNNING`, `DRAINING`, `SUCCEEDED`,
  `FAILED`, `BLOCKED` and `CANCELLED`; reject a bad value, unsafe plan input,
  duplicate generated ID or queue-full submission before sequence allocation.
  Assert prepared sessions live through one V6 call, but paths and plan bytes
  never occur in status.  Exercise arithmetic overflow for every cumulative
  reservation.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-orchestrator-task2 -R runtime_orchestrator_v1_contracts --output-on-failure`

  Expected: target/API absent.

- [ ] **Step 3: Implement immutable preparation and state-only storage.**

  Reuse the V6 CLI's package/trust/session assembly by moving it into the
  plan adapter; make the V6 CLI consume that adapter so it keeps current
  behavior.  Generate opaque run IDs internally, retain the full prepared
  plan privately, and expose no path-bearing field from status.  Validate
  limits/reservations with checked addition before state mutation.

- [ ] **Step 4: GREEN and state-security review.**

  Run: `cmake --build /tmp/bruce-orchestrator-task2 --target runtime_orchestrator_v1_tests bruce logic_mod_artifact_dag_v6_tests && ctest --test-dir /tmp/bruce-orchestrator-task2 -R 'runtime_orchestrator_v1_contracts|logic_mod_artifact_dag_v6_contracts|logic_mod_artifact_dag_v6_cli_workflows' --output-on-failure`

  Expected: selected tests pass.  Reviewer checks lifetime, checked counters,
  state legality and no queue admission before validation.

### Task 3: Implement atomic FIFO admission and reservation release

**Files:**
- Modify: `include/bruce/runtime/runtime_orchestrator_v1.hpp`,
  `src/runtime/runtime_orchestrator_v1.cpp`,
  `tests/unit/runtime_orchestrator_v1_tests.cpp`

**Interfaces:**
- Consumes Task 2 reservations and state.
- Produces `poll()` admission that considers exactly the oldest queued job;
  it atomically reserves active-run, parallel-node, worker and payload slots,
  or leaves the head queued.  A reservation exceeding one host maximum becomes
  `BLOCKED` with `RESERVATION_EXCEEDS_HOST` and releases nothing.

- [ ] **Step 1: Write failing FIFO/admission tests.**

  Submit three controllable jobs; assert start order equals `enqueue_seq`.
  Make the head valid but non-fitting and assert a younger smaller job cannot
  start.  Make the head individually impossible and assert it becomes blocked
  before its callable/V6/store path runs, then the next job progresses.
  Use concurrent submit/poll calls to prove every cumulative counter stays at
  or below its host limit and no job starts twice.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-orchestrator-task3 -R runtime_orchestrator_v1_contracts --output-on-failure`

  Expected: FIFO/admission assertions fail.

- [ ] **Step 3: Implement one lock-protected admission transition.**

  Keep queue order in `enqueue_seq` order.  Evaluate individual impossibility
  before fit; never backfill.  Under the same mutex reserve all four active
  values and change the job to `ADMITTED`, then launch outside the mutex.
  Release all reservations exactly once only from the joined terminal path.

- [ ] **Step 4: GREEN and concurrency review.**

  Run: `cmake --build /tmp/bruce-orchestrator-task3 --target runtime_orchestrator_v1_tests && ctest --test-dir /tmp/bruce-orchestrator-task3 -R runtime_orchestrator_v1_contracts --output-on-failure`

  Expected: PASS.  Reviewer stresses checked arithmetic, lock boundaries,
  FIFO head blocking and reservation release ownership.

### Task 4: Connect V6 execution, active cancel and complete drain

**Files:**
- Modify: `include/bruce/runtime/runtime_orchestrator_v1.hpp`,
  `src/runtime/runtime_orchestrator_v1.cpp`, `tests/unit/runtime_orchestrator_v1_tests.cpp`

**Interfaces:**
- Consumes the V6 latch of Task 1 and admitted prepared plans of Task 2.
- `cancel(run_id)` turns `QUEUED` directly into `CANCELLED` with
  `CANCELLED_QUEUED`; for active states it latches once, sets `DRAINING` and
  returns while V6 drains.  The join path maps accepted V6 completion to
  `SUCCEEDED`; ordinary V6 failures to `FAILED`; a V6 cancel without an
  orchestrator latch to `FAILED/V6_CANCELLED_EXTERNALLY`; and a latched,
  confirmed V6 cancellation to `CANCELLED`.

- [ ] **Step 1: Write failing active-cancel tests.**

  Use the timed V6 worker and a barrier-controlled fake runner.  Assert queue
  cancel makes zero V6 calls, sidecars, records, artifacts or workers.  Assert
  active cancel calls only the latch, holds reservations while `DRAINING`,
  publishes `CANCELLED` only after V6 returns/reaps, releases once, and does
  not admit another job until then.  Cover cancel-versus-success and an
  independent V6 `DAG_HOST_CANCELLED` result.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-orchestrator-task4 -R 'runtime_orchestrator_v1_contracts|logic_mod_artifact_dag_v6_contracts' --output-on-failure`

  Expected: cancel/drain checks fail before the run-thread and join logic.

- [ ] **Step 3: Implement join-owned lifecycle transitions.**

  Run each admitted plan on one orchestrator-owned `std::jthread`; it calls no
  worker API other than V6.  Latch cancellation under the controller lock,
  join outside it, then use one terminalization function to record the reason,
  finish sequence and release.  Do not signal, shorten V6 grace, inspect a
  sidecar for authority, retry or roll back existing output.

- [ ] **Step 4: GREEN and independent cancellation review.**

  Run: `cmake --build /tmp/bruce-orchestrator-task4 --target runtime_orchestrator_v1_tests logic_mod_artifact_dag_v6_tests && ctest --test-dir /tmp/bruce-orchestrator-task4 -R 'runtime_orchestrator_v1_contracts|logic_mod_artifact_dag_v6_contracts' --output-on-failure`

  Expected: PASS.  Reviewer checks latch precedence, join-before-release,
  no new work after observed cancellation and exception-safe thread cleanup.

### Task 5: Add the strict foreground CLI and read-only inspection

**Files:**
- Create: `apps/runtime_orchestrator_v1_cli.hpp`,
  `tests/integration/test_runtime_orchestrator_v1_cli.py`
- Modify: `apps/bruce_cli.cpp`, `CMakeLists.txt`,
  `tests/unit/runtime_orchestrator_v1_tests.cpp`

**Interfaces:**
- Command: `bruce runtime-orchestrator-v1 serve --max-queued-runs N
  --max-active-runs N --max-reserved-parallel-nodes N
  --max-reserved-started-workers N --max-reserved-payload-bytes N`.
- Stdin commands are canonical bounded JSON objects with `action` equal to
  `submit`, `status` or `cancel`.  A submit uses exactly the Task 2 V6 plan
  fields and all four reservation fields; status/cancel use only opaque
  `run_id`.  Each line gets one canonical JSON result or a closed error object.

- [ ] **Step 1: Write failing public protocol tests.**

  Run a foreground host through pipes.  Test malformed/oversized/duplicate
  fields, unknown action, missing limits, queue-full rejection, FIFO status,
  cancel by opaque ID, and EOF drain.  Verify every status/error reply lacks
  forbidden path/PID/PGID/URL/credential/payload/frame/signature/telemetry
  strings.  Use a real timed V6 job to prove a public active cancel drains and
  produces only the documented V6 sidecar.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-orchestrator-task5 -R runtime_orchestrator_v1_cli_workflows --output-on-failure`

  Expected: FAIL before command/help/parser exists.

- [ ] **Step 3: Implement parser-before-access and redacted renderer.**

  Reject invalid command lines before opening submitted files.  Drive
  `poll()` between nonblocking stdin reads and after each command.  Keep
  command JSON bounded and canonical; render only Task 2 status fields plus
  the opaque sidecar reference.  Status never validates, opens, repairs or
  follows the sidecar; it is read-only controller metadata.

- [ ] **Step 4: GREEN and protocol/security review.**

  Run: `cmake --build /tmp/bruce-orchestrator-task5 --target bruce runtime_orchestrator_v1_tests && ctest --test-dir /tmp/bruce-orchestrator-task5 -R 'runtime_orchestrator_v1_(contracts|cli_workflows)|logic_mod_artifact_dag_v6_(contracts|cli_workflows)' --output-on-failure`

  Expected: selected groups pass.  Reviewer verifies line framing, parser
  ordering, redaction, no direct worker surface and EOF/drain behavior.

### Task 6: Document limits, verify host-exit boundary and complete review

**Files:**
- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`,
  `docs/architecture/03-blueprint-consolidation.md`,
  `docs/superpowers/specs/2026-10-17-runtime-orchestrator-v1-design.md`
- Create: `docs/superpowers/sdd/runtime-orchestrator-v1/task-6-report.md`

**Interfaces:**
- Consumes reviewed green Tasks 1–5.
- Produces only verified documentation: V1 foreground/in-memory scope,
  required explicit limits, FIFO/head blocking, reservation versus hard
  resource boundary, cancel/drain behavior, redacted status and no recovery.

- [ ] **Step 1: Write the host-exit and sidecar-boundary tests.**

  Terminate a foreground host during a timed V6 job and assert only the
  existing V6 child cleanup evidence; do not assert queue reconstruction or
  record rollback.  Corrupt, truncate, chmod or symlink a V6 sidecar after a
  run and prove status/terminal result does not change, repair it or start a
  worker.  Run legacy V2–V6 record/replay byte compatibility again.

- [ ] **Step 2: Run the RED/GREEN acceptance matrix.**

  Run: `cmake -G Ninja -S . -B /tmp/bruce-orchestrator-final -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bruce-orchestrator-final --target bruce runtime_orchestrator_v1_tests logic_mod_artifact_dag_v2_tests logic_mod_artifact_dag_v3_tests logic_mod_artifact_dag_v4_tests logic_mod_artifact_dag_v5_tests logic_mod_artifact_dag_v6_tests && ctest --test-dir /tmp/bruce-orchestrator-final -R 'runtime_orchestrator_v1_(contracts|cli_workflows)|logic_mod_artifact_dag_v[2-6]_(contracts|cli_workflows)' --output-on-failure`

  Expected: all selected groups pass.  Record an AF_INET sandbox denial as an
  environment limitation, not a successful TLS proof.

- [ ] **Step 3: Update docs only from the green evidence.**

  Mark the spec as **IMPLEMENTIERT UND BELEGT** only after Tasks 1–5 and this
  matrix pass.  State explicitly that no persistence/recovery, service API,
  fairness beyond FIFO, global CPU/RAM enforcement or automatic retry exists.
  Run the CLI generator only in a complete temporary copy; do not edit its
  generated artifacts by hand.

- [ ] **Step 4: Link check and independent whole-change review.**

  Run: `python3 scripts/check_local_doc_links.py`

  Expected: no broken local Markdown links.  A fresh whole-change reviewer
  checks state transitions, reservations, V6 isolation/compatibility,
  cancellation races, status secrecy, host-exit wording and evidence before
  the final report is written.

## Self-Review

- **Spec coverage:** Tasks 2–5 cover the local host, queue, FIFO,
  reservations, cancel/drain, status and single-user limits.  Task 6 covers
  host exit, sidecar non-authority, documentation and final acceptance.
- **Intentional scope:** The plan does not add a recovery file, socket/HTTP
  API, scheduler priority, cgroup integration or a new canonical record.
- **Type consistency:** Task 1 provides the latch used by Task 4; Task 2
  owns prepared V6 plan lifetimes used by Tasks 3–5; only Task 2 status types
  feed the public renderer.
- **Review focus coverage:** latch/compatibility is Task 1; hostile input and
  redaction are Tasks 2/5; FIFO/reservations are Task 3; cancellation is Task
  4; host exit and sidecars are Task 6.

## Execution Handoff

This plan should be executed subagent-driven: each state/concurrency task has
a separate correctness boundary, and cancellation or reservation mistakes
would otherwise leak authority across V6 runs.  Every task needs a fresh
implementation review; Task 6 adds a whole-change review.
