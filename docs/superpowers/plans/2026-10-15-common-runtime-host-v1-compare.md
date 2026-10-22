# Common Runtime Host V1 `compare` Session Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the smallest Stage-2 Common Runtime Host V1 adapter by delegating an explicit `compare --compatibility-family session` invocation exactly once to the existing Session execution path while preserving all legacy behavior.

**Architecture:** The new host is a bounded in-process adapter in `bruce_core`. It validates one non-serialized request containing the `compare` entrypoint and canonical Session descriptor, then forwards control to a caller-supplied callback. The CLI uses it only for the explicit `compare` family confirmation; the existing `execute_cli`, ApplicationTask, Session budget, rendering, and export paths remain the owners of execution semantics.

**Tech Stack:** C++20, existing `bruce::Error`, `std::function`, CMake/CTest, Python `unittest`, existing CLI and Session contracts.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-host-v1-compare-design.md`

## Global Constraints

- Support exactly `entrypoint == "compare"`, Session family, and legacy route `execute-cli`.
- Validate the canonical descriptor before invoking the delegate.
- Invoke a valid delegate exactly once.
- Do not aggregate Session, Gate, Transfer, or Management budgets.
- Preserve callback exception type and message without translation.
- Preserve legacy `compare` stdout, stderr, exit code, and optional bundle bytes.
- Do not add a top-level command, descriptor output, bundle field, or report field.
- Do not modify V2, V3, V4, V5, Phase-DAG, Artifact Store, or worker contracts.
- Do not introduce a scheduler, worker, process, network, GPU, plugin, external-store, or Cockpit path.
- Mark documentation as `IMPLEMENTIERT UND BELEGT` only after implementation and verification succeed.

## Review Focus

- A wrong entrypoint must fail before the callback runs; Task 1 tests no side effect.
- A mutated Session descriptor must fail before delegation; Task 1 tests every binding field.
- A delegate exception must retain its original type/message; Task 1 tests pass-through.
- Explicit and legacy `compare` output and bundle bytes must match; Task 2 tests both forms.
- The host must not silently become a generic runtime; Task 2 tests non-target routes and Task 3 checks the diff for scope creep.

## File Map

Create:

- `include/bruce/application/common_runtime_host_v1.hpp` — request, delegate type, and host API.
- `src/application/common_runtime_host_v1.cpp` — bounded request validation and exactly-once delegation.
- `tests/unit/common_runtime_host_v1_tests.cpp` — host contract and exception pass-through tests.
- `tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py` — CLI byte-equality and negative workflow tests.

Modify:

- `apps/bruce_cli.cpp` — use the host only for explicit `compare --compatibility-family session`.
- `CMakeLists.txt` — source, unit target, and CTest integration registration.
- `docs/contracts/06-control-and-publication.md` — record the Stage-2 probe after verification.
- `docs/implementation/184-control-contract-matrix-usage.md` — record focused evidence after verification.

Generated CLI artifacts are not expected to change because the public syntax
already documents `--compatibility-family session`. If the verifier detects a
required change, regenerate `docs/cli/source-index.md`, `docs/cli/examples.json`,
and `docs/cli/validation.md` only through `docs/cli/verify_examples.py` in a
complete temporary copy, then copy the verified outputs back.

## Task 1: Freeze the Host API and Contract Tests

**Files:**

- Create: `include/bruce/application/common_runtime_host_v1.hpp`
- Create: `src/application/common_runtime_host_v1.cpp`
- Create: `tests/unit/common_runtime_host_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

- `struct bruce::CommonRuntimeHostV1Request { std::string entrypoint; CommonRuntimeCompatibilityV1 compatibility; };`
- `using bruce::CommonRuntimeHostV1Delegate = std::function<void()>;`
- `void validate_common_runtime_host_v1(const CommonRuntimeHostV1Request&);`
- `void delegate_common_runtime_host_v1(const CommonRuntimeHostV1Request&, CommonRuntimeHostV1Delegate);`

- [ ] **Step 1: Write failing unit tests** for canonical `compare`/Session acceptance and exactly-once delegate invocation.

  Test names should cover `delegates_once`, `rejects_wrong_entrypoint_without_call`, `rejects_wrong_family_without_call`, `rejects_mutated_descriptor_without_call`, `rejects_empty_delegate`, `preserves_delegate_error`, and `does_not_create_budget_or_output_state`.

- [ ] **Step 2: Register the focused unit target and run it before implementation.**

  Run:

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_host_v1_contracts$' --output-on-failure
  ```

  Expected: the new target is not yet implemented and the focused test fails to build or execute.

- [ ] **Step 3: Implement the bounded host API.**

  Validate the entrypoint, resolve the family through
  `control_family_compatibility_v1("session")`, reject a mutated known-family
  descriptor with the existing compatibility mismatch error, and reject an
  empty callback. Invoke the callback once after validation. Do not catch
  callback exceptions.
  Keep all validation and errors in the new host source; do not add budget or
  output fields to the request.

- [ ] **Step 4: Add the source and unit target to CMake and rerun the focused test.**

  Expected: the host unit target passes and reports all contract assertions.

## Task 2: Integrate the Host with the `compare` CLI Probe

**Files:**

- Modify: `apps/bruce_cli.cpp:730-740`
- Create: `tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Consume `validate_session_compatibility` behavior only through the new host
  request for the explicit `compare` option.
- Delegate the existing `execute_cli(args, std::move(task), ...)` call through
  `delegate_common_runtime_host_v1` when `--compatibility-family` is present.
- Keep the no-option path direct and unchanged.

- [ ] **Step 1: Write failing integration tests** for:

  - valid explicit Session compare success;
  - exact stdout/stderr/exit-code equality with legacy compare;
  - exact optional output-bundle byte equality;
  - exact equality for an existing execution error path;
  - wrong family and unknown family rejection;
  - duplicate and missing option-value parser behavior;
  - Session replay/budget options remaining valid;
  - `grid`, `research`, and `replay` retaining their current Stage-1/non-target boundaries.

- [ ] **Step 2: Run the new CTest before CLI integration.**

  Run:

  ```sh
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage2_compare$' --output-on-failure
  ```

  Expected: FAIL because the integration target is not registered or the host
  path is not yet wired.

- [ ] **Step 3: Build the canonical host request after `options(...)` and before route-specific processing.**

  For explicit `compare --compatibility-family session`, validate the request
  before constructing or executing the task, then invoke the existing
  `execute_cli` callback exactly once. Keep the legacy no-option branch direct.
  Do not move signal setup, task execution, rendering, or export into the
  host.

- [ ] **Step 4: Register the Python integration test as `common_runtime_compatibility_v1_stage2_compare`.**

  Link it against `$<TARGET_FILE:bruce>` using the repository's established
  Python CTest pattern. Rerun the focused integration test and expect PASS.

## Task 3: Regression, Documentation, and Evidence

**Files:**

- Modify: `docs/contracts/06-control-and-publication.md`
- Modify: `docs/implementation/184-control-contract-matrix-usage.md`
- Create: `runs/common-runtime-host-v1-compare.md`

- [ ] **Step 1: Run the focused Stage-2 and Stage-1 suites.**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^(common_runtime_host_v1_contracts|common_runtime_compatibility_v1_stage2_compare|common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session_states|execution_session_workflows|control_contract_matrix|control_contract_matrix_unit)$' --output-on-failure
  ```

  Expected: every selected test passes; report exact counts and exit codes.

- [ ] **Step 2: Run the local documentation checks.**

  ```sh
  python3 -m json.tool docs/cli/bruce-cli-reference.json >/dev/null
  python3 scripts/check_local_doc_links.py
  ```

  If CLI generated files changed, run the prescribed verifier only in a
  complete temporary copy and record its counts.

- [ ] **Step 3: Update the contract docs only after the tests pass.**

  Mark the `compare` host probe as `IMPLEMENTIERT UND BELEGT`, state that the
  four family contracts remain distinct, and link the new API, test, spec,
  plan, and evidence report. Keep Stage 3 and all broader runtime features
  marked as concept/open decision.

- [ ] **Step 4: Write the reproducible evidence report.**

  Record source/build paths, commands, exit codes, unit/integration counts,
  byte-equality results, documentation checks, secret-scan result, and known
  Git/sandbox limitations. Do not claim a common runtime or Stage-3 behavior.

## Task 4: Independent Review and Final Verification

- [ ] **Step 1: Ask an independent reviewer to inspect the spec, plan, host API, CLI integration, and tests.**

  The review must specifically check exactly-once delegation, descriptor
  equality, callback exception pass-through, legacy byte equality, no budget
  aggregation, no new public output, and no Stage-3 scope creep.

- [ ] **Step 2: Resolve only findings within this Stage-2 scope.**

  Do not expand to Gate, Transfer, Management, workers, Cockpit, or a unified
  runtime during this review.

- [ ] **Step 3: Perform final repository checks.**

  ```sh
  python3 tests/integration/test_common_runtime_compatibility_v1_stage2_compare.py /tmp/bruce-v14-core-build/bruce
  python3 scripts/check_local_doc_links.py
  git diff --check
  git status --short
  ```

  If Git metadata is unavailable, record that limitation rather than
  substituting an unverified claim.

## Verification Definition

The plan is complete only if the Stage-2 unit and integration tests pass, all
existing Stage-0/Stage-1 and Session regression tests pass, explicit and
legacy compare outputs and bundles are byte-identical, host failures do not
invoke the delegate, and the evidence report accurately labels the result as
the single `compare` Session probe. A green probe does not authorize Stage 3;
Stage 3 still requires a separate Common Runtime V2 contract.
