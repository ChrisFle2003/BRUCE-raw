# Common Runtime Host V1 `grid-batch` Session Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing Common Runtime Host V1 adapter so an explicit `grid-batch --compatibility-family session` invocation delegates exactly once while preserving every legacy batch result, cache, query-budget, replay, error, and export contract.

**Architecture:** Keep one in-process Host V1 API. Extend its accepted entrypoint set to `compare`, `grid`, and `grid-batch`; do not add a route-specific host type. The CLI validates the canonical Session descriptor immediately after `options(...)`, then passes the existing task construction and `execute_cli` call through one callback. The host owns no path, cache, query, Session, rendering, replay, or publication semantics.

**Tech Stack:** C++20, existing `bruce::Error`, `std::function`, CMake/CTest, Python `unittest`, existing `GridBatchTask`, `GridCache`, `ExecutionSession`, and CLI contracts.

**Spec:** `docs/superpowers/specs/2026-10-15-common-runtime-host-v1-grid-batch-design.md`

## Global Constraints

- Support exactly `entrypoint == "compare"`, `"grid"`, or `"grid-batch"`, Session family, and legacy route `execute-cli`.
- Reuse `CommonRuntimeHostV1Request` and `CommonRuntimeHostV1Delegate`; do not add a route-specific grid-batch host type.
- Validate the canonical descriptor immediately after `options(...)` and before cache, query, path, signal, or task processing.
- Invoke a valid delegate exactly once and preserve callback exception type and message.
- Preserve legacy grid-batch stdout, stderr, exit code, complete bundle bytes, partial bundle bytes, Session versions, cache statistics, query-boundary behavior, and error envelopes.
- Preserve exact, modular, and metric modes, existing path and cache limits, replay, cancellation, serialization cancellation, and exclusive export.
- Do not aggregate Session, Gate, Transfer, or Management budgets.
- Do not add a command, descriptor output, bundle field, report field, scheduler, worker, process, network, GPU, plugin, external-store, or Cockpit path.
- Keep host delegation limited to `compare`, `grid`, and `grid-batch`; do not alter `grid-resume`, `research`, `compatibility`, or replay routes.
- Keep this plan and its implementation evidence separate from Stage 3/Common Runtime V2 claims.
- Mark the result **IMPLEMENTIERT UND BELEGT** only after implementation and verification succeed.

## Review Focus

- A canonical `grid-batch` callback must run exactly once; the unit test pins the count.
- A wrong or unknown family with missing, malformed, or over-limit paths must fail at the host boundary before route processing; the integration test pins precedence.
- Cache and query-boundary options must remain route-owned; the integration test exercises hits, limits, and partial execution.
- Complete and partial output bundles must remain byte-identical; the integration test compares temporary files.
- Adding `grid-batch` must not broaden Host V1 to `grid-resume`, research, compatibility, replay, or a unified runtime; boundary tests and review inspect the allow-list.

## File Map

Modify:

- `include/bruce/application/common_runtime_host_v1.hpp` — update the entrypoint contract comment to include `grid-batch`; keep the public types and signatures unchanged.
- `src/application/common_runtime_host_v1.cpp` — accept exactly `compare`, `grid`, and `grid-batch`, then retain canonical Session descriptor validation and exactly-once delegation.
- `tests/unit/common_runtime_host_v1_tests.cpp` — add canonical `grid-batch` acceptance and all-route boundary assertions while retaining compare/grid coverage.
- `apps/bruce_cli.cpp` — route explicit `grid-batch --compatibility-family session` through the existing Host V1 callback; leave the no-option path direct.
- `CMakeLists.txt` — register the new Python integration test.
- `docs/contracts/06-control-and-publication.md` — record the verified third bounded Host V1 probe.
- `docs/implementation/184-control-contract-matrix-usage.md` — record focused evidence after verification.

Create:

- `tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py` — byte-preservation, cache/query, replay, negative, and non-target boundary tests.
- `runs/common-runtime-host-v1-grid-batch.md` — reproducible implementation and verification evidence after all checks pass.

Generated CLI artifacts are not expected to change because the public
`grid-batch` syntax already contains `--compatibility-family session`. If the
verifier detects a required change, regenerate `docs/cli/source-index.md`,
`docs/cli/examples.json`, and `docs/cli/validation.md` only through
`docs/cli/verify_examples.py` in a complete temporary repository copy.

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
  `entrypoint` exactly `compare`, `grid`, or `grid-batch` and the canonical
  Session descriptor.

- [ ] **Step 1: Add failing unit assertions for `grid-batch`.**

  Add a canonical `grid-batch` request and assert that its delegate count is
  exactly one. Retain canonical compare/grid exactly-once assertions and add
  an unsupported-entrypoint assertion proving that `grid-resume` is rejected.

- [ ] **Step 2: Add unit assertions for all accepted host boundaries.**

  For each of `compare`, `grid`, and `grid-batch`, assert that Gate, Transfer,
  and Management descriptors fail before the delegate. Mutate every Session
  descriptor field and assert `CommonRuntimeCompatibilityV1FamilyMismatch`
  before delegation. Retain empty-delegate rejection and callback exception
  pass-through tests. Keep the request free of cache, query-budget, output,
  bundle, and descriptor-rendering state.

- [ ] **Step 3: Run the focused unit target before changing the validator.**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_host_v1_contracts$' --output-on-failure
  ```

  Expected: the new `grid-batch` acceptance assertion fails with
  `CommonRuntimeHostV1EntrypointMismatch` while existing compare/grid tests
  remain green.

- [ ] **Step 4: Implement the additive validator.**

  In `src/application/common_runtime_host_v1.cpp`, accept only
  `request.entrypoint == "compare" || request.entrypoint == "grid" ||
  request.entrypoint == "grid-batch"` before applying the existing canonical
  Session descriptor checks. Preserve
  `CommonRuntimeHostV1EntrypointMismatch`,
  `CommonRuntimeCompatibilityV1FamilyMismatch`, and
  `EmptyCommonRuntimeHostV1Delegate` exactly. Do not catch callback exceptions
  and do not add route-specific fields.

- [ ] **Step 5: Rerun the focused host contract test.**

  Expected: all existing and new host assertions pass; no cache, query, budget,
  output, or bundle state is introduced into the host request.

## Task 2: Integrate the `grid-batch` CLI Probe

**Files:**

- Modify: `apps/bruce_cli.cpp` in the existing `grid-batch` branch.
- Create: `tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py`
- Modify: `CMakeLists.txt`

**Interfaces:**

- Consume: Task 1's `CommonRuntimeHostV1Request` and
  `delegate_common_runtime_host_v1` API, and the existing
  `execute_cli(args, std::move(task), {[]{ return interrupted != 0; }})` path.
- Produce: explicit Session host delegation for `grid-batch`; no-option
  `grid-batch` remains direct and unchanged.

- [ ] **Step 1: Write the failing integration test.**

  Pin these cases:

  - legacy/explicit success stdout, stderr, and exit-code equality for
    repeated paths that exercise the FIFO cache;
  - legacy/explicit complete output-bundle byte equality;
  - legacy/explicit partial output-bundle byte equality using
    `--max-query-steps 1`;
  - an existing route-owned error such as `--max-query-steps 1025`, a cache
    limit error, or malformed path, with identical stderr and exit code;
  - explicit Session replay with `--verification replay`, valid
    `--verification-cancellation grid-query-boundaries`, and a Session
    checkpoint limit;
  - `--serialization-cancellation grid-record-checkpoints` remaining route
    valid under replay;
  - exact, modular, and metric modes retaining their existing outputs;
  - wrong known families, unknown family, duplicate family, and missing family
    values retaining their existing errors;
  - a wrong-family invocation with missing or malformed `--paths` failing with
    the family error before route-specific path processing;
  - `compare`, `grid`, `grid-resume`, `research`, `compatibility`, and `replay`
    retaining their existing host/non-target boundaries.

- [ ] **Step 2: Register and run the new CTest before CLI integration.**

  Register:

  ```cmake
  add_test(NAME common_runtime_compatibility_v1_stage2_grid_batch COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py
      $<TARGET_FILE:bruce>)
  ```

  Run:

  ```sh
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage2_grid_batch$' --output-on-failure
  ```

  Expected: FAIL until the route is wired and the binary is rebuilt.

- [ ] **Step 3: Wire the host request after `options(...)`.**

  In the `grid-batch` branch, retain the existing allowed option set. Replace
  the explicit option-only `validate_session_compatibility(args)` path with a
  `run` callback containing the current `GridBatchTask` construction, path
  splitting, signal setup, and `execute_cli` call. When
  `--compatibility-family` is present, resolve it with
  `control_family_compatibility_v1`, construct
  `CommonRuntimeHostV1Request{"grid-batch", descriptor}`, and invoke
  `delegate_common_runtime_host_v1(request, run)`. Call `run()` directly when
  the option is absent. Do not parse `--paths`, cache bytes, query steps, mode,
  modulus, or install signals before host validation.

- [ ] **Step 4: Build and run the focused integration test.**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^common_runtime_compatibility_v1_stage2_grid_batch$' --output-on-failure
  ```

  Expected: the integration suite passes and explicit/legacy outputs and
  bundles compare byte-for-byte.

## Task 3: Regression, Documentation, and Evidence

**Files:**

- Modify: `docs/contracts/06-control-and-publication.md`
- Modify: `docs/implementation/184-control-contract-matrix-usage.md`
- Create: `runs/common-runtime-host-v1-grid-batch.md`

- [ ] **Step 1: Run the focused Host, Stage-2, Stage-1, grid, cache, and Session suites.**

  ```sh
  cmake --build /tmp/bruce-v14-core-build -j2
  ctest --test-dir /tmp/bruce-v14-core-build -R '^(common_runtime_host_v1_contracts|common_runtime_compatibility_v1_stage2_compare|common_runtime_compatibility_v1_stage2_grid|common_runtime_compatibility_v1_stage2_grid_batch|common_runtime_compatibility_v1_contracts|common_runtime_matrix_binding|common_runtime_compatibility_v1_workflows|common_runtime_compatibility_v1_stage1_gate|common_runtime_compatibility_v1_stage1_management|common_runtime_compatibility_v1_stage1_transfer|common_runtime_compatibility_v1_stage1_session|execution_session_states|execution_session_workflows|grid_cache|grid_partial|grid_resume|control_contract_matrix|control_contract_matrix_unit)$' --output-on-failure
  ```

  Record the exact exit code and selected test count. This is a focused
  selection; do not claim a full repository pass from it.

- [ ] **Step 2: Run documentation and static checks.**

  ```sh
  python3 -m json.tool docs/cli/bruce-cli-reference.json >/dev/null
  python3 scripts/check_local_doc_links.py
  python3 -m py_compile tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py
  ```

  If CLI generated files changed, run the prescribed verifier only in a
  complete temporary copy and record its counts in the evidence report.

- [ ] **Step 3: Update contract documentation after tests pass.**

  Mark the third bounded Host V1 probe as **IMPLEMENTIERT UND BELEGT** and
  link the API, test, spec, plan, and evidence report. State that cache,
  query, Session, replay, and publication semantics remain route-owned. Keep
  Stage 3, workers, external stores, and the larger runtime architecture
  marked as concept or open decision.

- [ ] **Step 4: Write the reproducible evidence report.**

  Record changed paths, the additive API decision, validation ordering, build
  and CTest commands with exit codes, Python test count, complete/partial byte
  equality, documentation checks, secret-scan result, and Git/sandbox
  limitations. Label the result **IMPLEMENTIERT UND BELEGT** only after all
  acceptance criteria pass.

## Task 4: Independent Review and Final Verification

- [ ] **Step 1: Ask an independent reviewer to inspect this change.**

  The review must check the exact entrypoint allow-list, canonical descriptor
  equality, callback exactly-once behavior, callback exception pass-through,
  pre-path/pre-cache/pre-query validation, complete and partial bundle byte
  equality, no budget aggregation, no public output changes, and no
  non-target route broadening.

- [ ] **Step 2: Resolve only findings inside the grid-batch Stage-2 scope.**

  Do not expand to `grid-resume`, research, a unified runtime, workers,
  Cockpit, external stores, or Stage-3 contracts during this task.

- [ ] **Step 3: Run final direct checks.**

  ```sh
  python3 tests/integration/test_common_runtime_compatibility_v1_stage2_grid_batch.py /tmp/bruce-v14-core-build/bruce
  python3 scripts/check_local_doc_links.py
  git diff --check
  git status --short
  ```

  If Git metadata is unavailable, record that limitation instead of claiming
  that Git checks passed.

## Verification Definition

The plan is complete only if the Host V1 unit test and new grid-batch
integration test pass, the existing compare/grid and Stage-0/Stage-1
regressions pass, explicit and legacy complete and partial bundles are
byte-identical, host failures occur before route side effects, and the report
labels this as the single additional `grid-batch` Session probe. A green
`grid-batch` probe does not authorize Stage 3 or a unified Common Runtime V2.
