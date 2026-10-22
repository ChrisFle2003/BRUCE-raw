# Logic-Mod Artifact DAG V6 Supervision and Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run V6 DAG nodes through host-owned Linux attempts and write a
private, bounded diagnostic sidecar without changing V2–V5 records or replay.

**Architecture:** `worker_v1::WorkerAttemptHandle` cannot be reused directly:
it launches a framed `worker-v1 child`, whereas Logic-Mod execution freshly
authorizes a static memfd executable, installs a seccomp notification gate,
and may hand it FD 6. Extract that existing Logic-Mod preparation into an
internal helper and add a move-only Logic-Mod attempt around the same child
boundary. The V6 DAG loop owns the handles, polls/cancels them, and writes
diagnostics. Canonical V2 records remain the only DAG truth.

**Tech Stack:** C++20; Linux `fork`, `setpgid`, `prctl`, `wait4`, `RLIMIT_AS`;
OpenSSL; CMake/CTest; Python integration tests.

**Spec:** `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v6-supervision-telemetry-design.md`

## Global Constraints

- V6 is Linux-only and only its `run` path changes. V2–V5 commands, specs,
  DAG IDs, record bytes, response frames, FD-6, authorization and replay are
  unchanged.
- The settled escalation is fixed and host-only: after observing cancel, wait
  **1000 ms** without inventing a Logic-Mod cancel protocol, then `SIGTERM`
  for at most **50 ms**, then `SIGKILL`. No CLI, Spec or worker can vary it.
  This matches `src/worker/worker_supervisor_v1.cpp`'s timing without adding a
  worker control frame.
- Existing `LogicModBudgetsV1` per-node limits remain authoritative; V4 totals
  remain admission limits, not retroactive kill limits.
- Classify an ambiguous `RLIMIT_AS` result as `PROCESS_CRASHED`. Emit
  `MEMORY_LIMIT` only after a separate implementation proves the exact Linux
  evidence required by the V6 spec.
- V6 `run` requires `--report REPORT_ROOT`. It creates exactly
  `REPORT_ROOT.run/control/supervision-v6.jsonl`, with `REPORT_ROOT.run` and
  `control` mode `0700`, log mode `0600`. This is a dedicated Logic-Mod trace:
  `dag-scheduler-v5`'s `WorkerRunLog` has another scheduler contract and there
  is no Logic-Mod DAG V5 run root to extend. Replay accepts no report/live
  options and neither reads nor creates a sidecar.
- The worker sees no report path/FD, URL, store path, credential, PID or PGID.
  The sidecar contains no path, PID/PGID, command line, raw frame, payload,
  signature, URL, truststore data or secret.
- Writer and reader use descriptor-relative `openat` plus `O_NOFOLLOW`; all
  directories/files must be effective-user-owned, owner-only, regular and
  single-link. Entries are bounded canonical JSONL with global increasing seq.
- Build into a fresh `/tmp` directory. The project wrapper needs a native
  path. Generated CLI artifacts are changed only by `docs/cli/verify_examples.py`.

## File Map

| File | Change | Responsibility |
| --- | --- | --- |
| `include/bruce/mod/logic_mod_worker_v1.hpp`, `src/mod/logic_mod_worker_v1.cpp` | modify | Extract shared authorization, sealed FDs, seccomp gate and response validation while preserving synchronous V1 bytes. |
| `include/bruce/mod/logic_mod_worker_attempt_v6.hpp`, `src/mod/logic_mod_worker_attempt_v6.cpp` | create | Move-only Logic-Mod attempt, process group/PDEATHSIG, poll/cancel/reap and host observation. |
| `include/bruce/mod/logic_mod_supervision_log_v6.hpp`, `src/mod/logic_mod_supervision_log_v6.cpp` | create | Private V6 sidecar schema, descriptor-relative writer and validator. |
| `include/bruce/mod/logic_mod_artifact_dag_v6.hpp`, `src/mod/logic_mod_artifact_dag_v6.cpp` | create | V6 controls, V4-compatible waves, V5 restore, attempt scheduling and telemetry. |
| `apps/logic_mod_artifact_dag_v6_cli.hpp`, `apps/bruce_cli.cpp` | create/modify | Strict V6 CLI, help and dispatch. |
| `CMakeLists.txt` | modify | Core sources, fixtures, unit and integration test registration. |
| `tests/unit/logic_mod_worker_attempt_v6_tests.cpp` | create | Linux attempt lifecycle, group cleanup, limits and observation tests. |
| `tests/unit/logic_mod_supervision_log_v6_tests.cpp` | create | Exact JSONL and hostile sidecar filesystem/parser tests. |
| `tests/unit/logic_mod_artifact_dag_v6_tests.cpp` | create | V6 wave/cancel/terminal/record compatibility tests. |
| `tests/integration/test_logic_mod_artifact_dag_v6_cli.py` | create | Public CLI, report permission and real static-worker workflows. |
| CLI/format/blueprint/V6-spec docs | modify | Verified contract and evidence only. |

## Review Focus

- Task 2: symlink/FIFO/hardlink/foreign-owner/non-owner-mode report targets fail before `fork`.
- Task 3: a result after observed cancellation is `CANCELLED`; a prior accepted result remains `READY_ACCEPTED`.
- Task 1: parent death between `fork` and `exec` leaves no executable worker or descendant group.
- Task 1: unavailable Procfs emits `UNAVAILABLE`, never `0`, and does not kill a valid worker.
- Tasks 3–4: replay starts no child, contacts no remote and does not touch a sidecar; V2–V5 bytes are exact.

---

### Task 1: Extract the Logic-Mod child boundary and add supervised attempts

**Files:**
- Modify: `include/bruce/mod/logic_mod_worker_v1.hpp`, `src/mod/logic_mod_worker_v1.cpp`, `CMakeLists.txt`
- Create: `include/bruce/mod/logic_mod_worker_attempt_v6.hpp`, `src/mod/logic_mod_worker_attempt_v6.cpp`, `tests/unit/logic_mod_worker_attempt_v6_tests.cpp`
- Test: `tests/unit/logic_mod_worker_v1_tests.cpp`, `tests/unit/logic_mod_artifact_dag_v[2-5]_tests.cpp`

**Interfaces:**
- Consumes: existing registry binding, `LogicModPayloadV1` FD 6, static sealed memfd, seccomp exec gate and `LogicModBudgetsV1`.
- Produces: move-only internal `LogicModPreparedWorkerV6` and
  `LogicModWorkerAttemptV6`; `start_logic_mod_worker_attempt_v6`,
  `poll_logic_mod_worker_attempt_v6`, `cancel_logic_mod_worker_attempt_v6`,
  `finish_logic_mod_worker_attempt_v6`. The outcome has `LogicModRunV1`, one
  V6 terminal reason, elapsed time, optional peak bytes and availability
  `OBSERVED|UNAVAILABLE|NOT_APPLICABLE`.

- [ ] **Step 1: Write failing lifecycle tests.**

  Add static fixture modes for valid result, indefinite pause, cooperative
  cancel, SIGTERM-ignoring descendant, pre-exec failure, crash and allocation
  pressure. Assert exactly one reap; no surviving group; deadline gives
  `WALL_TIME_LIMIT`; cancel follows 1000/50 ms and gives `HOST_CANCELLED`.
  Assert a normal worker has only established descriptors/optional FD 6 and
  cannot observe report/store/network descriptors.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-v6-task1 -R 'logic_mod_worker_attempt_v6_contracts|logic_mod_artifact_dag_v[2-5]_contracts|logic_mod_worker_v1' --output-on-failure`

  Expected: the new target fails because API/target do not exist; the legacy
  baseline groups pass.

- [ ] **Step 3: Extract preparation while retaining V1.**

  Move the shared resolve/bind/seal/gate setup out of
  `run_logic_mod_worker_v1(...)`; keep that function as its synchronous
  adapter. Add accepted, rejected and FD-6 golden assertions showing
  `logic_mod_run_record_v1` bytes remain identical.

- [ ] **Step 4: Implement the attempt as the sole V6 process owner.**

  Child: own process group, `PR_SET_PDEATHSIG(SIGKILL)`, immediate parent
  recheck, `RLIMIT_AS` before exec and existing seccomp execution path. Parent:
  retain expected group, monotonic poll, `waitid`/`wait4`, `/proc/<pid>/status`
  `VmHWM` and post-reap `ru_maxrss`; signal only `-expected_pgid`. Failed group
  binding/watchdog/reap cleans the direct child and returns
  `SUPERVISION_UNAVAILABLE`, never a guessed-PID signal. Destruction performs
  bounded cleanup. Allocation failure without new specified evidence remains
  `PROCESS_CRASHED`.

- [ ] **Step 5: GREEN, evidence and fresh review.**

  Run: `cmake -G Ninja -S . -B /tmp/bruce-v6-task1 -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bruce-v6-task1 --target logic_mod_worker_attempt_v6_tests bruce_logic_mod_worker_v1_tests logic_mod_artifact_dag_v5_tests && ctest --test-dir /tmp/bruce-v6-task1 -R 'logic_mod_worker_attempt_v6_contracts|logic_mod_worker_v1_contracts|logic_mod_artifact_dag_v[2-5]_contracts' --output-on-failure`

  Expected: selected groups pass. A fresh reviewer checks parent-death race,
  process-group identity, descriptor closure, cleanup/reap uniqueness and V1
  byte preservation. Save output in `runs/logic-mod-artifact-dag-v6/evidence/task-1-*.log`.

### Task 2: Implement the owner-only V6 sidecar

**Files:**
- Create: `include/bruce/mod/logic_mod_supervision_log_v6.hpp`, `src/mod/logic_mod_supervision_log_v6.cpp`, `tests/unit/logic_mod_supervision_log_v6_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 outcome and V6 report root.
- Produces: `LogicModSupervisionLogV6::create(report_root, run_id)`,
  `append(SupervisionEventV6)` and existing-line validation. Events contain
  schema, opaque run/attempt/node ID, seq, event, status, terminal reason,
  elapsed/budget/usage numbers and peak availability only.

- [ ] **Step 1: Write failing writer/parser tests.**

  Pin canonical bytes for `ATTEMPT_STARTED`, `CANCEL_SENT`, `TERM_SENT`,
  `KILL_SENT`, `REAPED` and terminal event. Test increasing global seq,
  bounded files, required/forbidden fields, `UNAVAILABLE` omitting peak value,
  plus symlink/FIFO/directory/hardlink/truncated JSON/unknown field/foreign
  owner/permissive mode rejection before write.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-v6-task2 -R logic_mod_supervision_log_v6_contracts --output-on-failure`

  Expected: FAIL until target and writer exist.

- [ ] **Step 3: Implement the private log.**

  Follow defensive patterns from `WorkerRunLog::append` and
  `worker_abort_control_v1.cpp`, but do not reuse their schema/path. Build
  `REPORT_ROOT.run/control` through checked descriptors; use `openat`, lock,
  validate all existing canonical lines, append one bounded LF record, fsync
  the file and directory. The reader rejects partial/unknown records and is
  diagnostic-only.

- [ ] **Step 4: GREEN and fresh filesystem-security review.**

  Run: `cmake --build /tmp/bruce-v6-task2 --target logic_mod_supervision_log_v6_tests && ctest --test-dir /tmp/bruce-v6-task2 -R logic_mod_supervision_log_v6_contracts --output-on-failure`

  Expected: PASS. Reviewer confirms forbidden data cannot serialize and unsafe
  targets fail before worker start. Preserve red/green/review evidence.

### Task 3: Add V6 waves, classification and byte compatibility

**Files:**
- Create: `include/bruce/mod/logic_mod_artifact_dag_v6.hpp`, `src/mod/logic_mod_artifact_dag_v6.cpp`, `tests/unit/logic_mod_artifact_dag_v6_tests.cpp`
- Modify: `CMakeLists.txt`; refactor `src/mod/logic_mod_artifact_dag_v4.cpp` and `src/mod/logic_mod_artifact_dag_execution.hpp` only if sharing preflight/topology/input preparation preserves V4 behavior.

**Interfaces:**
- Consumes: V4 controls, V5 predecessor restore, Task 1 attempts and Task 2 log.
- Produces: `LogicModArtifactDagControlV6 : LogicModArtifactDagControlV5` with required `std::filesystem::path report_root`; V6 run/record/read/replay functions whose record functions delegate to V2-compatible bytes.

- [ ] **Step 1: Write failing V6 core tests.**

  Test accepted node, timeout, crash, start failure, worker failure/block,
  input/output limit, valid `cancel_after_ms`, parallel roots and accepted-then-
  cancel. Every started node has exactly one `REAPED` event/reason. Observed
  cancel prevents later waves and rejects unaccepted ready races, but earlier
  verified/published READY stays accepted.

  Create successful V2/V3/V4/V5 runs and compare V6 default DAG ID and exact
  `logic_mod_artifact_dag_record_v2` bytes. Assert all V2–V5 readers accept
  them. Under SIGCHLD/HTTPS counters, prove V6 replay does not spawn, contact
  remote or create/read sidecar.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-v6-task3 -R 'logic_mod_artifact_dag_v6_contracts|logic_mod_artifact_dag_v[2-5]_contracts' --output-on-failure`

  Expected: V6 fails before implementation, legacy groups pass.

- [ ] **Step 3: Implement the V6 scheduling loop.**

  Reuse V4 validation/stable waves/preflight and V5 host-only predecessor
  restore. Replace only V4's `std::async` worker section with handle polling.
  Validate report root before any start; emit `ATTEMPT_STARTED` after parent/
  child handshake. On V4 `cancel_after_ms`, cancel every active handle in one
  iteration, log actions, reap all, publish only pre-cancel accepted results,
  and mark later/queued work `CANCELLED`. Map exactly one terminal reason;
  ambiguous memory endings are `PROCESS_CRASHED`. Do not put diagnostics in
  V2 records or V6 standard JSON.

- [ ] **Step 4: GREEN, byte proof and fresh review.**

  Run: `cmake -G Ninja -S . -B /tmp/bruce-v6-task3 -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bruce-v6-task3 --target logic_mod_artifact_dag_v6_tests logic_mod_artifact_dag_v2_tests logic_mod_artifact_dag_v3_tests logic_mod_artifact_dag_v4_tests logic_mod_artifact_dag_v5_tests && ctest --test-dir /tmp/bruce-v6-task3 -R 'logic_mod_artifact_dag_v[2-6]_contracts' --output-on-failure`

  Expected: all groups pass. Reviewer checks cancellation/result ordering,
  handle ownership, remote boundary and recorded byte hashes. Preserve output.

### Task 4: Expose strict V6 CLI and prove real workflows

**Files:**
- Create: `apps/logic_mod_artifact_dag_v6_cli.hpp`, `tests/integration/test_logic_mod_artifact_dag_v6_cli.py`
- Modify: `apps/bruce_cli.cpp`, `CMakeLists.txt`, `tests/unit/logic_mod_artifact_dag_v6_tests.cpp`

**Interfaces:**
- Consumes: Task 3 and unchanged V2 Spec parser.
- Produces: `bruce logic-mod-artifact-dag-v6 run SPEC --store STORE --record RECORD --report REPORT [--remote-restore HTTPS_BASE] [V4 RUN OPTIONS]`; `replay` has only the six fixed V2 arguments.

- [ ] **Step 1: Write failing public CLI tests.**

  Test unknown dispatch, exactly-one required `--report`, replay rejecting all
  V6 options before opening FIFO spec, strict V4 numeric spelling/ranges, and
  unsafe report paths causing zero worker forks. Test report path/modes/content
  and no forbidden strings; real timeout/cancel leaves no descendant; loopback
  TLS V5 restore happens only in run; replay is offline.

- [ ] **Step 2: Prove RED.**

  Run: `ctest --test-dir /tmp/bruce-v6-task4 -R logic_mod_artifact_dag_v6_cli_workflows --output-on-failure`

  Expected: FAIL before dispatch/parser exists.

- [ ] **Step 3: Implement only the public adapter.**

  Follow V5's strict pair parser: reject duplicates, whitespace, leading zeroes
  and replay options before I/O; use the same session/spec assembly. Add help
  and dispatch. Render `contract:"logic-mod-artifact-dag-v6"` plus existing
  metadata only—never PID, reason, peak, payload or report path.

- [ ] **Step 4: GREEN and end-to-end reviewer gate.**

  Run: `cmake -G Ninja -S . -B /tmp/bruce-v6-task4 -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/bruce-v6-task4 --target bruce logic_mod_artifact_dag_v6_tests && ctest --test-dir /tmp/bruce-v6-task4 -R 'logic_mod_artifact_dag_v[2-6]_(contracts|cli_workflows)' --output-on-failure`

  Expected: selected groups pass. Review parser-before-I/O, JSON secrecy and
  actual modes. Keep integration logs and cleanup probe.

### Task 5: Documentation, evidence and whole-change review

**Files:**
- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/architecture/03-blueprint-consolidation.md`, `docs/superpowers/specs/2026-10-17-logic-mod-artifact-dag-v6-supervision-telemetry-design.md`
- Create: `docs/superpowers/sdd/logic-mod-artifact-dag-v6-supervision-telemetry/task-5-report.md`

**Interfaces:**
- Consumes: reviewed green Task 1–4 evidence.
- Produces: verified implementation documentation and reproducible report, never manually generated CLI artifacts.

- [ ] **Step 1: Add documentation checks before changing status.**

  Require wording for 1000/50 ms, `--report`, Linux-only scope, V2–V5 byte/
  replay preservation, diagnostic-only telemetry, `PROCESS_CRASHED` fallback
  and all explicit non-goals. Do not claim `MEMORY_LIMIT` without Task 1
  evidence.

- [ ] **Step 2: Update verified docs only after all green evidence exists.**

  Mark the V6 spec **IMPLEMENTIERT UND BELEGT** only if Task 1–4 proof is
  attached; otherwise use **IMPLEMENTIERT, ABER NICHT AKTUELL VERIFIZIERT** and
  name the missing verification.

- [ ] **Step 3: Run final verification.**

  Run: `python3 docs/cli/verify_examples.py /tmp/bruce-v6-final/bruce`

  Run: `python3 - <<'PY'
from pathlib import Path
import re
broken=[]
for page in Path('docs').glob('**/*.md'):
    for target in re.findall(r'\]\(([^)#]+)(?:#[^)]+)?\)', page.read_text(encoding='utf-8')):
        if '://' not in target and not (page.parent/target).resolve().exists(): broken.append((str(page),target))
print(broken)
raise SystemExit(bool(broken))
PY`

  Run: `ctest --test-dir /tmp/bruce-v6-final --output-on-failure`

  Expected: verifier changes only its three generated CLI artifacts in an
  isolated copy; link result is `[]`; full suite passes except separately
  recorded sandbox-denied socket cases. Record command, exit code, counts and
  environmental limitation.

- [ ] **Step 4: Independent whole-change review.**

  Re-run hostile sidecar, cancellation race, descendant cleanup and V2–V5
  byte fixtures. Save findings/resolutions in the SDD report. Use the project
  review flow; do not rely on unavailable Git metadata.

## Plan Self-Review

Task 1 covers process ownership, Linux limits, PID/group safety and telemetry
sources. Task 2 covers private sidecar integrity. Task 3 covers V4/V5
composition, cancellation, taxonomy and compatibility. Task 4 covers public
input handling. Task 5 covers evidence and documentation. GPU/network workers,
cgroups, global process accounting, retry, operation counters, sidecar repair/
streaming and PID-based public cancellation remain out of scope. The only
planning decision previously open is now fixed: V6 uses 1000 ms/50 ms grace.

## Execution Handoff

The user has already selected subagent-driven development. Implement one task
at a time with a fresh implementer and reviewer, saved RED/GREEN command
evidence, and no next task until the preceding review has no unresolved finding.
