# Common Runtime Host V1 `grid` Session Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing Common Runtime Host V1 adapter so an explicit `grid --compatibility-family session` invocation delegates exactly once while preserving every legacy grid result, error, budget, replay, and export contract.

**Architecture:** Keep one in-process Host V1 API. The existing request already carries an entrypoint string and a canonical control-family descriptor, so the validator will accept exactly `compare` and `grid`; the CLI will pass `grid` through the same callback boundary. The grid task, Session execution, rendering, and export remain owned by the existing `execute_cli` path.

**Tech Stack:** C++20, existing `bruce::Error`, `std::function`, CMake/CTest, Python `unittest`, existing GridTask and ExecutionSession contracts.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-host-v1-grid-design.md`

## Global Constraints

- Support exactly `entrypoint == "compare"` or `entrypoint == "grid"`, Session family, and legacy route `execute-cli`.
- Reuse `CommonRuntimeHostV1Request` and `CommonRuntimeHostV1Delegate`; do not add a route-specific grid host type.
- Validate the canonical descriptor before invoking the delegate and before grid route processing.
- Invoke a valid delegate exactly once and preserve callback exception type and message.
- Preserve legacy grid stdout, stderr, exit code, optional bundle bytes, Session versions, and error envelopes.
- Preserve exact, modular, and metric modes, existing limits, replay, cancellation, serialization, `INCOMPLETE`, and export behavior.
- Do not aggregate Session, Gate, Transfer, or Management budgets.
- Do not add a command, descriptor output, bundle field, report field, scheduler, worker, process, network, GPU, plugin, external-store, or Cockpit path.
- Keep host delegation limited to `compare` and `grid`; do not alter `grid-batch`, `grid-resume`, `research`, `compatibility`, or replay routes.
- Keep this plan and the implementation evidence separate from Stage-3/Common Runtime V2 claims.

## Review Focus

- A valid `grid` callback must run exactly once; the unit test pins the count.
- A wrong entrypoint or mutated Session descriptor must fail before callback side effects; the unit test mutates each descriptor field.
- An explicit host invocation must preserve optional output bundle bytes; the integration test compares temporary files byte-for-byte.
- Grid route validation errors must remain unchanged; the integration test compares an existing error path.
- A `grid` host extension must not broaden the host to other routes; the integration test checks non-target route boundaries.

## File Map

Modify:

- `include/bruce/application/common_runtime_host_v1.hpp` — update the entrypoint comment/API contract to allow `compare` and `grid` without adding fields.
- `src/application/common_runtime_host_v1.cpp` — accept exactly the two Stage-2 entrypoints and retain canonical Session descriptor validation.
- `tests/unit/common_runtime_host_v1_tests.cpp` — add the `grid` acceptance and boundary assertions while retaining compare coverage.
- `apps/bruce_cli.cpp` — route explicit `grid --compatibility-family session` through the existing Host V1 callback; leave the no-option path direct.
- `CMakeLists.txt` — register the new Python integration test.

Create:

- `tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py` — byte-preservation, negative, replay/budget, mode, and non-target route checks.
- `runs/common-runtime-host-v1-grid.md` — reproducible implementation and verification evidence after code and tests pass.

Generated CLI artifacts are not expected to change: the `grid` syntax already
contains `--compatibility-family`. If the verifier detects a required change,
regenerate `docs/cli/source-index.md`, `docs/cli/examples.json`, and
`docs/cli/validation.md` only through `docs/cli/verify_examples.py` in a
complete temporary repository copy.

## Task 1: Extend the Host Contract

**Files:**

- Modify: `src/application/common_runtime_host_v1.cpp`
- Modify: `include/bruce/application/common_runtime_host_v1.hpp`
- Modify: `tests/unit/common_runtime_host_v1_tests.cpp`

**Interfaces:**

- Consume: `CommonRuntimeHostV1Request`, `CommonRuntimeHostV1Delegate`,
  `control_family_compatibility_v1`, and
  `require_control_family_compatibility_v1`.
- Produce: unchanged public function signatures; valid requests have
  `entrypoint` exactly `compare` or `grid` and the canonical Session
  descriptor.

- [ ] **Step 1: Add failing unit assertions for the `grid` entrypoint.**

  Add a canonical `grid` request and assert that the delegate count becomes
  exactly one. Add an unsupported entrypoint assertion and retain the existing
  `compare` exactly-once assertion.

- [ ] **Step 2: Add unit assertions for grid host boundaries.**

  Assert that Gate, Transfer, and Management descriptors fail for `grid`, each
  Session descriptor field mutation fails, an empty delegate fails, and a
  delegate exception preserves its type and message. Assert that the request
  remains free of budget/output/bundle state.

- [ ] **Step 3: Run the focused unit target before changing the validator.**

  Run:

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_host_v1_contracts$' --output-on-failure
  ```

  Expected: the new `grid` acceptance assertion fails while the existing
  compare assertions remain green.

- [ ] **Step 4: Implement the additive validator.**

  In `src/application/common_runtime_host_v1.cpp`, accept only
  `request.entrypoint == "compare" || request.entrypoint == "grid"` before
  applying the existing canonical Session descriptor checks. Preserve
  `CommonRuntimeHostV1EntrypointMismatch`,
  `CommonRuntimeCompatibilityV1FamilyMismatch`, and
  `EmptyCommonRuntimeHostV1Delegate` exactly. Do not catch callback exceptions.

- [ ] **Step 5: Rerun the focused host contract test.**

  Expected: all existing and new host assertions pass, with no new output or
  budget state.

## Task 2: Integrate the `grid` CLI Probe

**Files:**

- Modify: `apps/bruce_cli.cpp` in the existing `grid` branch.
- Create: `tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Consume: the Task 1 Host V1 API and the existing `execute_cli(args,
  std::move(task))` path.
- Produce: explicit `grid --compatibility-family session` host delegation;
  no-option `grid` remains direct.

- [ ] **Step 1: Write the failing integration test.**

  Pin these test cases:

  - legacy/explicit success stdout, stderr, and exit-code equality for the
    default exact mode;
  - legacy/explicit output-bundle byte equality using temporary paths;
  - an existing grid validation error, such as `--max-bits 0`, with identical
    stderr and exit code;
  - explicit Session replay with `--verification replay` and a valid
    `--verification-cancellation phase-boundaries` combination;
  - explicit Session `--max-session-checkpoints` acceptance;
  - modular and metric mode success through the explicit host path;
  - Gate, Transfer, Management, and unknown family errors;
  - duplicate and missing `--compatibility-family` parser errors;
  - a family mismatch with a missing/invalid route input fails at the host
    boundary before the grid route side effect;
  - `grid-batch`, `grid-resume`, `research`, `compatibility`, and `replay`
    retain their existing boundaries.

- [ ] **Step 2: Register and run the new CTest before CLI integration.**

  Register:

  ```cmake
  add_test(NAME common_runtime_compatibility_v1_stage2_grid COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py
      $<TARGET_FILE:bruce>)
  ```

  Run:

  ```sh
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage2_grid$' --output-on-failure
  ```

  Expected: FAIL until the route is wired and the binary is rebuilt.

- [ ] **Step 3: Wire the explicit `grid` request after `options(...)`.**

  In `apps/bruce_cli.cpp`, keep `validate_session_compatibility(args)` out of
  the host path for this route. When the option is present, resolve its value
  with `control_family_compatibility_v1`, construct
  `CommonRuntimeHostV1Request{"grid", descriptor}`, and invoke
  `delegate_common_runtime_host_v1` with a callback containing the existing
  GridTask construction and `execute_cli` call. Keep the no-option branch's
  statements and options unchanged. The host validation must happen before
  `--path` processing and before signal registration.

- [ ] **Step 4: Build and run the focused integration test.**

  Run:

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage2_grid$' --output-on-failure
  ```

  Expected: the CTest passes and the Python suite reports all grid cases
  passing.

## Task 3: Regression and Evidence

**Files:**

- Create: `runs/common-runtime-host-v1-grid.md`

- [ ] **Step 1: Run the focused Host, Stage-2, Stage-1, and Session suites.**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^(common_runtime_host_v1_contracts|common_runtime_compatibility_v1_stage2_compare|common_runtime_compatibility_v1_stage2_grid|common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session_states|execution_session_workflows|control_contract_matrix|control_contract_matrix_unit)$' --output-on-failure
  ```

  Record exact exit code and selected test count. The new grid test must be
  included; do not claim a full repository pass from this focused selection.

- [ ] **Step 2: Run documentation and static checks.**

  ```sh
  python3 -m json.tool docs/cli/bruce-cli-reference.json >/dev/null
  python3 scripts/check_local_doc_links.py
  python3 -m py_compile tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py
  ```

  If CLI generated files changed, run the prescribed verifier only in a
  complete temporary copy and record its counts in the evidence report.

- [ ] **Step 3: Write the evidence report.**

  Record the modified source/test paths, the additive API decision, build and
  CTest commands with exit codes, Python test count, byte-equality results,
  documentation checks, secret-scan result, and any Git/sandbox limitation.
  Label the result **IMPLEMENTIERT UND BELEGT** only after all acceptance
  criteria pass. Keep the larger Common Runtime and Stage-3 claims marked as
  concept/open decision.

## Task 4: Independent Review and Final Verification

- [ ] **Step 1: Ask an independent reviewer to inspect this change.**

  The review must check the additive API decision, exact entrypoint allow-list,
  descriptor equality, callback exactly-once behavior, callback exception
  pass-through, legacy grid byte equality, no budget aggregation, no public
  output changes, and no non-target route broadening.

- [ ] **Step 2: Resolve only findings inside the grid Stage-2 scope.**

  Do not expand to grid-batch, grid-resume, a unified runtime, workers,
  Cockpit, external stores, or any Stage-3 contract during this task.

- [ ] **Step 3: Run the final direct test and repository checks.**

  ```sh
  python3 tests/integration/test_common_runtime_compatibility_v1_stage2_grid.py /tmp/bruce-v14-core-build/bruce
  python3 scripts/check_local_doc_links.py
  git diff --check
  git status --short
  ```

  If Git metadata is unavailable, record that limitation instead of claiming
  that Git checks passed.

## Verification Definition

The plan is complete only if the Host V1 unit test and the new grid
integration test pass, the existing compare Stage-2 and Stage-0/Stage-1
regressions pass, explicit and legacy grid outputs and bundles are
byte-identical, host failures occur before route side effects, and the report
labels the result as the single additional `grid` Session probe. A green grid
probe does not authorize Stage 3 or a unified Common Runtime V2.
