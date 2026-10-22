# Scaled Exponential Grid Rule V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a bounded, replayable Grid rule that evaluates `h(stage)=100*(2^stage-1)` while preserving all existing default Grid behavior.

**Architecture:** Extend the existing `GridSpec`/`GridTask` rule tag, then dispatch one fixed implementation in the Grid application layer. The cache key, single Grid bundle, batch bundle and resume path carry the selected rule so that values under different rules never alias and replay remains canonical. The CLI selects from a closed rule set; NanoCell and the free expression grammar remain untouched.

**Tech Stack:** C++20, existing BRUCE exact/modular arithmetic, CMake, CTest, Python CLI integration tests.

**Spec:** `docs/superpowers/specs/2026-10-16-scaled-exponential-grid-rule-v1-design.md`

## Global Constraints

- Use C++20 and existing CMake/CTest layouts; install no dependencies.
- V1 is exactly `100*(2^stage-1)`, with base `2`, scale `100`, `uint64` output and closed rule selection.
- The new rule maps the leaf coordinate `y` to `stage`; local coordinates remain `0..7`.
- Existing default `LEAF_LOCAL_POWER_V1` output, bundles, cache behavior, batch/resume behavior and public CLI syntax remain byte-compatible when `--rule` is omitted.
- Exact must reject overflow through the existing bit-limit contract. Metric retains its existing meaning: evaluate safely within the `uint64` rule domain and report a bit length independently of the Exact-mode display limit. Modular must use correct modular subtraction/multiplication without overflow.
- `--zero-zero stop` stays address-based and rule-independent.
- Do not hand-edit generated CLI artifacts `docs/cli/source-index.md`, `docs/cli/examples.json` or `docs/cli/validation.md`; run the verifier only in a complete temporary source copy.
- Git metadata is known unusable in this checkout; record failed Git checks and do not claim commits.

## Review Focus

- Rule omission must continue to produce legacy Grid output and bundle bytes; Task 1 unit and Task 2 CLI regression pin this.
- The same path under the two rules must have separate cache keys and separate Batch cache entries; Task 2 covers this.
- Stage `0` must return `0`, while address `(0,0)` with `--zero-zero stop` must still stop rather than evaluate; Task 1 covers both.
- Modular arithmetic must handle a modulus smaller than `100` and avoid unsigned underflow after `2^stage mod M - 1`; Task 1 uses modulus `17` and `1`.
- Manipulated rule identifiers in individual, batch and resumed evidence must fail replay rather than silently selecting the default rule; Tasks 1 and 2 cover these paths.

---

## File structure

| File | Responsibility |
|---|---|
| `include/bruce/application/grid.hpp` | Carry the rule identifier in `GridTask` and `GridRun` data used by evaluation/replay. |
| `include/bruce/application/grid_spec.hpp` | Define canonical fixed rule IDs and store the selected rule in `GridSpec`. |
| `src/application/grid_spec.cpp` | Validate/render/cache-key the selected rule. |
| `src/application/grid.cpp` | Dispatch the two fixed rules; render and replay the selected rule. |
| `src/application/grid_batch.cpp` | Preserve the dynamic rule through batch evidence/replay/cache/resume inputs. |
| `apps/bruce_cli.cpp` | Parse closed `--rule` option for `grid` and `grid-batch`. |
| `tests/unit/grid_tests.cpp` | Verify core exact/metric/modular/sentinel/replay contracts. |
| `tests/unit/grid_cache_tests.cpp` | Verify cache-key separation and batch/replay compatibility. |
| `tests/integration/test_cli.py` | Verify public CLI selection, legacy default behavior and rejection. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/75-current-architecture-and-milestones.md` | Document only the final implemented contract and its scope. |

### Task 1: Rule contract, evaluator and single-run replay

**Files:**
- Modify: `include/bruce/application/grid.hpp`
- Modify: `include/bruce/application/grid_spec.hpp`
- Modify: `src/application/grid_spec.cpp`
- Modify: `src/application/grid.cpp`
- Modify: `tests/unit/grid_tests.cpp`

**Interfaces:**
- Consumes: existing `power`, `modular_power`, Grid path parsing and Grid bundle digest/replay.
- Produces: `GridSpec::leaf_local_power_rule`, `GridSpec::scaled_exponential_rule`, `GridTask::rule`, and `GridSpec::rule`; existing `evaluate_grid`, `render_grid`, `pack_grid`, `replay_grid` continue with the selected rule.

- [ ] **Step 1: Write failing Grid contract tests**

Add focused assertions to `tests/unit/grid_tests.cpp` for:

```cpp
GridTask scaled{.path="2,3", .rule=GridSpec::scaled_exponential_rule};
require(evaluate_grid(scaled).value == 700, "Scaled exponential stage 3 differs");
```

Cover stages `0` and `7`, metric bit length, modular values for moduli `17` and `1`, an Exact-mode too-small `max_bits`, `zero_zero="stop"`, unknown rule rejection, single-bundle replay and a rehashed rule-field mutation rejection.

- [ ] **Step 2: Run the Grid contract test to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_tests && ctest --test-dir /tmp/bruce-build -R '^grid_contracts$' --output-on-failure`

Expected: compile failure because `GridTask` has no `rule`, or a failing scaled-exponential assertion.

- [ ] **Step 3: Implement canonical rule selection and evaluation**

Add the two canonical identifiers and a default `leaf_local_power_rule` to `GridSpec`/`GridTask`. Make `grid_spec`, `render_grid_spec`, `grid_cache_key`, Grid payload/render/replay preserve and validate the chosen rule.

Implement an internal bounded helper for `scaled_exponential_v1(stage, max_bits)` that computes `100*(2^stage-1)` with `power(2, stage, ...)` plus checked subtraction/multiplication. Implement a modular counterpart with modular subtraction and `__uint128_t` multiplication. Dispatch by closed rule tag; unknown tags throw `InvalidGridRule`.

- [ ] **Step 4: Run the core Grid verification**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_tests && ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_replay_workflows)$' --output-on-failure`

Expected: both tests pass with no skipped tests.

- [ ] **Step 5: Record Git limitation instead of committing**

Run: `git status --short` and `git diff --check`.

Expected: current checkout reports unusable Git metadata; record this in the task report and do not claim a commit.

### Task 2: Batch, cache, resume and public CLI

**Files:**
- Modify: `src/application/grid_batch.cpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `tests/unit/grid_cache_tests.cpp`
- Modify: `tests/integration/test_cli.py`
- Modify only if required by existing bundle parsing: `src/application/grid_resume.cpp`

**Interfaces:**
- Consumes: Task 1 dynamic `GridSpec::rule` and unchanged `GridCache`/Batch/Resume public function signatures.
- Produces: `bruce grid --rule ...` and `bruce grid-batch --rule ...`; batch evidence stores/replays dynamic `value_rule`; cache identity includes it.

- [ ] **Step 1: Write failing cache, batch and CLI tests**

In `tests/unit/grid_cache_tests.cpp`, evaluate the same path once under each rule and assert two misses, distinct cache key digests and expected values (`64` versus `6300` for path `2,6`). Add batch replay mutation coverage for the rule field.

In `tests/integration/test_cli.py`, add a case asserting:

```python
result = self.call('grid', '--path', '2,3', '--rule', 'scaled-exponential-v1')
self.assertEqual(result['value'], '700')
```

Also assert the omitted rule still returns the legacy value, `grid-batch` accepts the same rule, and an invalid value returns a JSON error.

- [ ] **Step 2: Run focused tests to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_cache_tests bruce && ctest --test-dir /tmp/bruce-build -R '^(grid_cache_workflows|grid_replay_workflows)$' --output-on-failure`

Expected: new tests fail because Batch evidence and CLI do not yet propagate `rule`.

- [ ] **Step 3: Propagate the selected rule through Batch and CLI**

Parse `--rule` for `grid` and `grid-batch` only, defaulting to `leaf-local-power`. Reject unknown spellings in `grid_spec`, not through a silent CLI fallback. Replace static batch `value_rule` serialization/comparison with the validated selected rule, then parse it before mode/budget fields during replay. Resume inherits its parent Batch spec without a new public option.

- [ ] **Step 4: Run compatibility verification**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_cache_tests bruce && ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_cache_workflows|grid_partial_workflows|grid_replay_workflows|grid_resume_workflows|grid_budget_workflows)$' --output-on-failure`

Expected: all selected tests pass; standard no-`--rule` workflows stay green.

- [ ] **Step 5: Check legacy output manually**

Run: `/tmp/bruce-build/bruce grid --path 2,6`

Expected: the existing `LEAF_LOCAL_POWER_V1` result remains `"value":"64"` with no new rule option required.

### Task 3: Documentation and final verification

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md` if the existing Grid JSON schema is described there
- Modify: `docs/implementation/75-current-architecture-and-milestones.md`
- Modify: `docs/superpowers/specs/2026-10-16-scaled-exponential-grid-rule-v1-design.md` status only after successful verification
- Create: `runs/scaled-exponential-grid-rule-v1/reports/final-review.md`

**Interfaces:**
- Consumes: completed Task 1–2 implementation and test evidence.
- Produces: evidence-labeled user/developer documentation and final review record.

- [ ] **Step 1: Document the narrow implemented contract**

Document `--rule leaf-local-power|scaled-exponential-v1`, the exact formula, `y` as stage, the `0..7` V1 direct Grid range, mode behavior, `uint64`/bit-limit scope and the unchanged address-based sentinel. State that the formula is an abstract scale, not a physical bubble simulator, and that NanoCell/free formulas are not expanded.

- [ ] **Step 2: Run the full relevant verification set**

Run native configure/build and then:

```sh
ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_cache_workflows|grid_partial_workflows|grid_budget_workflows|grid_axis_workflows|grid_research_workflows|grid_replay_workflows|grid_resume_workflows|grid_experience_workflows|grid_experience_budget_workflows|cli_workflows)$' --output-on-failure
```

Also run `python3 scripts/check_local_doc_links.py`, `git status --short`, and `git diff --check`. Run `docs/cli/verify_examples.py` only in a complete temporary source copy; compare original generated-artifact hashes before/after.

- [ ] **Step 3: Write final evidence report and update status**

Record exact commands, exit codes, CTest counts, documentation/link results, generated-artifact hashes and Git limitation in `runs/scaled-exponential-grid-rule-v1/reports/final-review.md`. Change the spec status to **IMPLEMENTIERT UND BELEGT** only if Task 1–2 tests and this verification succeed. Otherwise retain **VORSCHLAG** or label the exact incomplete state.

- [ ] **Step 4: Independent whole-change review**

Use a fresh reviewer to inspect the evaluator, overflow and modular arithmetic, replay/cache separation, default compatibility, documentation, and evidence report. Require PASS before declaring the feature complete.

## Plan self-review

- **Spec coverage:** Task 1 implements the fixed formula, modes, sentinel, error handling, cache key, single Grid render/replay and limits. Task 2 implements Batch/Resume propagation and CLI selection while pinning old default behavior. Task 3 covers docs, verifier, full relevant tests and independent review.
- **Step scan:** Each task begins with a specific RED assertion, then adds only the code that owns that behavior before its targeted GREEN command.
- **Type consistency:** Task 1 defines the `GridSpec::rule`/`GridTask::rule` values consumed in Task 2; no new Batch or Resume public C++ signature is introduced.
- **Review focus:** Rule omission is covered in Task 2; cache separation in Task 2; stage-zero/sentinel and modular underflow in Task 1; replay mutation in Tasks 1–2.
- **Proportion:** Three tasks cover the one rule, its transport boundaries, and final evidence without introducing free-form evaluation, BigInt Grid changes or physical modeling.
