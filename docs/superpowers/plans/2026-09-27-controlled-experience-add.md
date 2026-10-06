# Controlled `experience add` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add optional transfer-style checkpoint and signal control to `bruce experience add` without changing existing invocations, snapshot bytes, or unrelated administrative commands.

**Architecture:** Reuse the existing CLI-local `TransferControl`, `LocalFiles::read`, `execute_experience(..., ReplayCheckpoint)` and `LocalFiles::publish_new` chain. Only the `experience add` branch of `apps/bruce_cli.cpp` gains the two existing transfer options and conditionally wraps successful output in `controlled-transfer-v1`; `revoke`, `replace`, `inspect`, rule commands, sessions and gate commands remain untouched.

**Tech Stack:** C++20, CMake, Ninja, GMP/GMPXX, OpenSSL Crypto, Python 3 `unittest` integration tests.

**Spec:** `docs/superpowers/specs/2026-09-27-experience-add-control-design.md`

## Global Constraints

- Add no packages and change no mathematical algorithm, bundle format, rule activation flow or existing controller semantics.
- `experience add` without either new option must preserve its stdout JSON, stderr JSON, exit status and output bytes.
- The only new accepted options are `--max-transfer-checkpoints N` and `--transfer-cancellation none|checkpoints`, and only for `experience add`.
- Controlled checkpoints retain `TransferControl::visit()` order: signal, budget, counter increment.
- A detected control error before publication exits 2 and leaves no destination or temporary export residue.
- The current GVFS source path contains a colon and cannot directly provide a valid `PATH`/`PKG_CONFIG_LIBDIR` to `with-host-deps.sh`; use a fresh minimal copy under `/tmp` for full build verification.
- This directory is not a Git repository. Do not invent commits; record the absent Git status in the handoff instead.

## Review Focus

- No-control compatibility: an old `experience add` call must retain byte-identical output and its unwrapped legacy report — Task 1.
- Exact versus one-short budgets: the reported exact count succeeds; one fewer checkpoint publishes nothing — Task 1.
- Signal while a FIFO-backed source is waiting: `SIGTERM` must yield `TransferCancelled` and no target — Task 1.
- Existing destination: even under a sufficient control budget, exclusive publication must preserve its old contents — Task 1.
- Invalid or absent inputs with a zero budget: the first control point must win before attempting the missing read — Task 1.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `apps/bruce_cli.cpp` | Parse the two options only for `experience add`, construct the existing controller, propagate its callback across all selected boundaries, and conditionally render the existing transfer result wrapper. |
| `tests/integration/test_experience.py` | End-to-end compatibility, budget, signal, invalid-input and publication assertions for direct `experience add`. |
| `docs/cli/commands.md` | User-facing syntax, scope and result/error contract for the new optional controls. |
| `docs/cli/control-overview.md` | Add direct `experience add` to the controlled-path matrix while retaining its distinct scope. |
| `docs/contracts/06-control-and-publication.md` | Update the inventory of actually controlled paths and explicitly preserve all exclusions. |

### Task 1: Direct-Experience CLI Contract Test

**Files:**

- Modify: `tests/integration/test_experience.py`

**Interfaces:**

- Consumes: Existing `ExperienceTests.call(*args, ok=True)`, `source(k)`, and temporary `root` helpers.
- Produces: `ExperienceTests.test_controlled_add_preserves_legacy_bytes_and_prevents_publication`, which fails until Task 2 accepts and applies the new options.

- [ ] **Step 1: Add the failing direct-add integration test**

Add `test_controlled_add_preserves_legacy_bytes_and_prevents_publication`. It must:

```python
plain = self.call('experience', 'add', '--from-run', source, '--output', plain_path)
bounded = self.call('experience', 'add', '--from-run', source,
                    '--max-transfer-checkpoints', 1_000_000,
                    '--transfer-cancellation', 'checkpoints', '--output', bounded_path)
self.assertEqual(bounded['contract'], 'controlled-transfer-v1')
self.assertEqual(bounded['result'], plain)
self.assertEqual(bounded_path.read_bytes(), plain_path.read_bytes())
```

Extend the same test with a nonempty prior store and a different eligible source, then assert byte equality for an exact budget, no target or `*.tmp.*` after budget `0` and after `used - 1`, a zero-budget missing-source error of `TransferCheckpointBudgetExceeded`, and preservation of an already-existing target. Reuse the FIFO/SIGTERM synchronization pattern from `tests/integration/test_experience_transfer.py` for `experience add`; require exit 2, `TransferCancelled`, no target and no temporary residue.

- [ ] **Step 2: Run the focused test to verify it fails for the intended reason**

Run from a fresh `/tmp` source copy with a freshly built `bruce` binary:

```bash
python3 tests/integration/test_experience.py ./build-audit/bruce
```

Expected: failure because the current `experience add` option parser rejects `--max-transfer-checkpoints` or `--transfer-cancellation` as unknown options. Do not accept an unrelated failure.

### Task 2: Additive CLI Control Propagation

**Files:**

- Modify: `apps/bruce_cli.cpp:477-497`
- Test: `tests/integration/test_experience.py`

**Interfaces:**

- Consumes: `TransferControl`, `transfer_checkpoint(const Options&, TransferControl&)`, `controlled_transfer_report(...)`, `LocalFiles::read(...)`, `LocalFiles::publish_new(...)`, and `execute_experience(const ExperienceTask&, const ReplayCheckpoint&)`.
- Produces: `experience add` accepts exactly the two new options and emits `controlled-transfer-v1` only when one was specified.

- [ ] **Step 1: Implement the smallest `experience add`-only branch**

Keep `inspect`, `revoke`, and `replace` behavior and their accepted-option sets byte-for-byte unchanged. Split the existing shared `add/revoke/replace` branch so only `add` accepts:

```cpp
{"--store", "--from-run", "--output",
 "--max-transfer-checkpoints", "--transfer-cancellation"}
```

For `add`, construct `TransferControl control`, obtain `ReplayCheckpoint checkpoint = transfer_checkpoint(args, control)`, read optional `--store` and required `--from-run` through `LocalFiles::read(..., experience_byte_limit, checkpoint)`, run `execute_experience(ExperienceAddTask{...}, checkpoint)`, and export through `LocalFiles::publish_new(..., checkpoint)`. Render through `controlled_transfer_report(result.report, control, bool(checkpoint))`.

Do not add a new controller type, do not modify `ExperienceTask`, and do not use `export_exclusive` for this controlled branch because it has no supplied callback argument.

- [ ] **Step 2: Run the focused integration test to verify it passes**

Run:

```bash
python3 tests/integration/test_experience.py ./build-audit/bruce
```

Expected: all existing experience tests plus the new direct-add control test pass. In particular, a controlled successful snapshot is byte-identical to the legacy snapshot, while zero/short/signal paths create no export.

- [ ] **Step 3: Run directly related regression groups**

Run:

```bash
ctest --test-dir build-audit --output-on-failure -R '^(experience_workflows|experience_transfer_workflows|local_files_contracts|export_control)$'
```

Expected: four groups pass. This confirms that the new direct-add path did not alter existing planned/apply transfers or local publication guarantees.

### Task 3: Contract Documentation and Whole-Repository Verification

**Files:**

- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/control-overview.md`
- Modify: `docs/contracts/06-control-and-publication.md`

**Interfaces:**

- Consumes: Task 2's exact option scope, success wrapper and failure behavior.
- Produces: A consistent public contract that describes direct controlled `experience add` without implying control for the excluded experience actions or rule commands.

- [ ] **Step 1: Update the three documents with the implemented, narrow scope**

In `commands.md`, extend only the `experience add` syntax and explain optional transfer controls, wrapper behavior and unchanged legacy path. In `control-overview.md`, distinguish direct controlled add from planned/apply transfers. In `06-control-and-publication.md`, add direct add to the controlled-path matrix and preserve the statement that revoke, replace, inspect and rule management have no common CLI control account.

- [ ] **Step 2: Validate documentation references and examples**

Run from the temporary build copy:

```bash
python3 docs/cli/verify_examples.py ./build-audit/bruce
```

Expected: the documentation verifier accepts the revised command reference, option list and examples.

- [ ] **Step 3: Rebuild and run the complete suite from a fresh temporary copy**

Copy the required source inputs, examples and `build-host-deps` to a new `/tmp/bruce-experience-add-XXXXXX` directory. Then run:

```bash
./scripts/with-host-deps.sh cmake -S . -B build-audit -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
./scripts/with-host-deps.sh cmake --build build-audit --parallel 2
./scripts/with-host-deps.sh ctest --test-dir build-audit --output-on-failure --parallel 2
./scripts/with-host-deps.sh python3 scripts/verify_control_flow.py build-audit/bruce --report /tmp/bruce-experience-add-control-flow.json
```

Expected: configure and build exit 0; all registered CTest groups pass; the control-flow report returns `accepted: true`. The report is evidence for the pre-existing planned/apply paths and must not be described as full direct-add coverage; Task 1 is the dedicated new direct-add coverage.

- [ ] **Step 4: Record delivery limits instead of committing**

Run `git status --short`. Expected: `fatal: not a git repository`. Report changed files, test commands and their output in the handoff; do not fabricate a commit, branch, diff or remote delivery claim.

## Plan Self-Review

- **Spec coverage:** Tasks 1–2 cover the optional options, callback boundaries, error priority, compatibility and rollback. Task 3 covers the stated documentation and whole-suite evidence requirements.
- **Type consistency:** The plan uses existing names and signatures: `TransferControl`, `ReplayCheckpoint`, `LocalFiles::read`, `LocalFiles::publish_new`, and `execute_experience`.
- **Review focus:** Every listed high-risk input is pinned to Task 1's integration test.
- **Scope:** No task changes a bundle format, an application task variant, a rule command, a mathematical algorithm or a second administrative action.
- **Git constraint:** The plan explicitly replaces unavailable commit steps with evidence-based handoff.
