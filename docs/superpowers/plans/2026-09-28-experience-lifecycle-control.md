# Experience Lifecycle Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add opt-in `transfer-checkpoints-v1` control to `bruce experience revoke` and `bruce experience replace` while preserving every legacy invocation byte-for-byte.

**Architecture:** Keep the existing legacy branch for each action intact. When either transfer option is present, create one existing `TransferControl`, route all CLI file reads, `execute_experience`, `revoke_experience`/`replace_experience` and `LocalFiles::publish_new` through its `ReplayCheckpoint`, then wrap only successful native reports with `controlled-transfer-v1`. Optional checkpoint parameters preserve existing domain callers and no snapshot format changes are needed.

**Tech Stack:** C++20, existing `TransferControl`/`ReplayCheckpoint` and `LocalFiles` adapters, Python `unittest` CLI integration tests, CMake/CTest, Markdown documentation.

**Spec:** `docs/superpowers/specs/2026-09-28-experience-lifecycle-control-design.md`

## Global Constraints

- Scope is exactly `experience revoke` and `experience replace`; `inspect`, add/plan/apply, compatibility, sessions, gates, and rule administration remain unchanged.
- Reuse `--max-transfer-checkpoints N`, `--transfer-cancellation none|checkpoints`, `transfer-checkpoints-v1`, and `controlled-transfer-v1` exactly as existing controlled transfers do.
- A legacy invocation without either option must retain its existing arguments, stdout success bytes, stderr error contract, exit code, and snapshot bytes.
- A controlled success contains the unchanged native report as `result`; a control failure is exit 2 JSON on stderr with no stdout report or output publication.
- Signal cancellation takes precedence over budget exhaustion at a shared checkpoint; counters are per process invocation and never persisted.
- Do not change Experience snapshot/bundle formats or add CMake registrations: the two existing Python test files are already CTest targets.
- This workspace has no Git metadata; do not add commit steps or fabricate commits.

## Review Focus

1. `--transfer-cancellation none` alone must still select the controlled branch and success mantle for both actions — Task 1 adds explicit success checks.
2. `--max-transfer-checkpoints 0` must fail before opening an optional missing store or a replace source — Task 1 adds missing-path assertions for both read positions.
3. An exact observed budget must succeed and one fewer must leave neither final nor `.tmp.*` output — Task 1 establishes this separately for Revoke and Replace.
4. A pre-existing target must remain byte-identical and have no temporary sibling after a controlled export failure — Task 1 covers it for both actions.
5. `experience inspect` must continue to reject the transfer options rather than silently gaining a controller — Task 1 retains the existing `Usage: bruce experience inspect SNAPSHOT` assertion.

---

## File Structure

- `apps/bruce_cli.cpp` — accepts the two existing flags for lifecycle actions and selects controlled versus legacy I/O/reporting paths.
- `tests/integration/test_experience.py` — Revoke lifecycle controller acceptance, zero-budget ordering, cancel, and command-scope regression coverage.
- `tests/integration/test_experience_replace.py` — Replace lifecycle controller acceptance, source-read ordering, and publication coverage.
- `docs/cli/transfer-control.md` — public list of controlled commands and lifecycle read/export semantics.
- `docs/cli/experience-transfer.md` — documents the lifecycle extension without rewriting the add-transfer plan contract.
- `docs/contracts/06-control-and-publication.md` — updates the control-boundary matrix to distinguish opted-in and legacy lifecycle calls.

### Task 1: Specify lifecycle controller behavior in CLI integration tests

**Files:**
- Modify: `tests/integration/test_experience.py:125-218`
- Modify: `tests/integration/test_experience_replace.py:1-74`

**Interfaces:**
- Consumes: existing CLI spelling `bruce experience revoke|replace`, `controlled-transfer-v1`, and JSON error convention.
- Produces: failing regression tests that Task 2 satisfies; no production API.

- [ ] **Step 1: Write failing Revoke controller tests in `tests/integration/test_experience.py`**

Add `test_controlled_revoke_preserves_bytes_and_prevents_publication`. Build a normal store/record, run plain Revoke and a high-budget controlled Revoke, and assert `contract == 'controlled-transfer-v1'`, `result == plain`, and identical snapshot bytes. Repeat with `--transfer-cancellation none`; use the reported `usage['checkpoints']` for exact-budget success and one-less failure. Assert the failed target and `target.name + '.tmp.*'` do not exist. Add `--max-transfer-checkpoints 0` cases with an absent `--store` and without `--store`, each expecting `TransferCheckpointBudgetExceeded` and no publication. Retain the existing FIFO/SIGTERM synchronization pattern to assert `TransferCancelled` during a controlled store read, with stderr-only JSON and no output.

- [ ] **Step 2: Write failing Replace controller tests in `tests/integration/test_experience_replace.py`**

Add `test_controlled_replace_preserves_bytes_and_prevents_publication`. Start from `initial()`, use its active record ID and a different eligible `source()`, then compare plain and high-budget controlled Replace report/snapshot bytes. Assert the `none` cancellation mode wraps an otherwise identical native result. Use the observed count for exact and one-less budgets; assert no target or temp siblings after exhaustion. Add `--max-transfer-checkpoints 0` with a missing `--from-run` and with a missing optional `--store`, both expecting `TransferCheckpointBudgetExceeded` before file-open errors. Add a controlled existing-output case asserting `ExportExistsOrUnavailable`, preserved `b'keep'`, and no temporary sibling.

- [ ] **Step 3: Change the former option-rejection regression in `tests/integration/test_experience.py`**

Remove only the loop that expects `UnknownOption: --max-transfer-checkpoints` for Revoke and Replace. Keep the `experience inspect SNAPSHOT --max-transfer-checkpoints 1` invocation and its exact usage-error assertion.

- [ ] **Step 4: Run the focused tests to verify they fail before production changes**

Run: `cmake --build build --target bruce -j2 && ctest --test-dir build --output-on-failure -R '^(experience_workflows|experience_replace_workflows)$'`

Expected: FAIL because Revoke and Replace still reject `--max-transfer-checkpoints` as an unknown option; `inspect` continues to pass its rejection assertion.

### Task 2: Route lifecycle CLI actions through the existing transfer controller

**Files:**
- Modify: `apps/bruce_cli.cpp:597-606`
- Test: `tests/integration/test_experience.py:125-218`
- Test: `tests/integration/test_experience_replace.py:1-74`

**Interfaces:**
- Consumes: `transfer_checkpoint(const Options&, TransferControl&) -> ReplayCheckpoint`, `controlled_transfer_report(const std::string&, const TransferControl&, bool) -> std::string`, `LocalFiles::read(path, experience_byte_limit, checkpoint)`, `LocalFiles::publish_new(path, bytes, checkpoint)`, and `execute_experience(const ExperienceTask&, const ReplayCheckpoint&)`.
- Produces: controlled Revoke/Replace CLI behavior using no new public C++ type or snapshot version.

- [ ] **Step 1: Extend the allowed option sets for both lifecycle actions**

In the `action=="revoke" || action=="replace"` branch, add exactly `--max-transfer-checkpoints` and `--transfer-cancellation` to each current `options(...)` allowlist. Keep required `--id`, `--reason`, `--from-run` for Replace, and `--output` validation unchanged.

- [ ] **Step 2: Implement the controlled Revoke/Replace branch in `apps/bruce_cli.cpp`**

Construct `TransferControl control; const auto checkpoint=transfer_checkpoint(args, control);`. When `checkpoint` is nonempty, read optional `--store` with `LocalFiles::read(..., experience_byte_limit, checkpoint)`, read Replace `--from-run` via the same adapter, construct the same `ExperienceRevokeTask` or `ExperienceReplaceTask`, call `execute_experience(task, checkpoint)`, and call `LocalFiles::publish_new(required(args,"--output"), result.snapshot, checkpoint)`. Print `controlled_transfer_report(result.report, control, true)` followed by one newline only after successful publication.

- [ ] **Step 3: Preserve the current legacy branch exactly**

When `checkpoint` is empty, retain `read_bounded`, the current task construction, `execute_experience(task)`, `export_exclusive`, and the existing direct native report output. Do not route a legacy call through `LocalFiles` or add a new wrapper.

- [ ] **Step 4: Run focused lifecycle tests to verify they pass**

Run: `cmake --build build --target bruce -j2 && ctest --test-dir build --output-on-failure -R '^(experience_workflows|experience_replace_workflows)$'`

Expected: PASS; controlled successes are wrapped, exact budgets succeed, exhausted/cancelled calls use stderr exit 2 with no publication, and `inspect` still rejects the flags.

### Task 3: Publish the updated public control boundary

**Files:**
- Modify: `docs/cli/transfer-control.md:7-30,65-76`
- Modify: `docs/cli/experience-transfer.md:100-106`
- Modify: `docs/contracts/06-control-and-publication.md:20-21`
- Test: `tests/integration/test_experience.py:125-218`
- Test: `tests/integration/test_experience_replace.py:1-74`

**Interfaces:**
- Consumes: the completed Task 2 CLI contract and `controlled-transfer-v1` documentation convention.
- Produces: documentation that names exactly the two new opt-in lifecycle commands and preserves the legacy/control boundary.

- [ ] **Step 1: Update `docs/cli/transfer-control.md`**

Change the command count from five to seven, add `experience revoke` and `experience replace` to the list and examples, and state that controlled Revoke reads an optional base while controlled Replace reads its optional base and source bundle before the shared Experience execution and exclusive export. Keep the generic counter/error ordering and wrapper JSON unchanged.

- [ ] **Step 2: Update `docs/cli/experience-transfer.md` without broadening its plan format**

Keep the document’s plan/add transfer contract scoped to add. In its additional-control section, state that the two lifecycle mutations now accept the same two flags and use `controlled-transfer-v1`; identify Revoke’s optional base and Replace’s optional base plus source bundle. Keep the explicit non-goal of changing plan formats or rule activation.

- [ ] **Step 3: Correct the lifecycle rows in `docs/contracts/06-control-and-publication.md`**

Move opted-in Revoke/Replace into the `transfer-checkpoints-v1` row with the controlled I/O, Experience execution, and publication scope. Leave no-option Revoke/Replace and `inspect` in the legacy row; remove the stale claim that all Revoke/Replace calls lack a transfer controller.

- [ ] **Step 4: Verify documentation examples and the full suite**

Run: `python3 docs/cli/verify_examples.py build/bruce && ctest --test-dir build --output-on-failure`

Expected: the documentation verifier reports no broken examples/links, and every registered CTest passes.

## Final Verification

- [ ] **Step 1: Re-read the completed diff against the spec**

Confirm every controlled success uses the existing wrapper, every legacy call stays on the original I/O path, only Revoke/Replace gain the flags, and no persisted format/version changes appear.

- [ ] **Step 2: Run final clean checks**

Run: `cmake --build build --target bruce -j2 && python3 docs/cli/verify_examples.py build/bruce && ctest --test-dir build --output-on-failure`

Expected: build succeeds, documentation verification succeeds, and all CTest tests pass.
