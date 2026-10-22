# Scaled Exponential Grid Stage V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing `scaled-exponential-v1` Grid rule accept an optional explicit stage `0..57`, with canonical cache and replay evidence while retaining every implicit V1 byte path.

**Architecture:** An absent optional stage preserves the current leaf-`y` evaluation and V1 evidence verbatim. A present stage produces a resolved `grid-spec-v2`, new explicit-only Grid/Batch/Resume bundle headers, and a cache identity that includes the selected stage. The CLI introduces `--stage` only on `grid` and `grid-batch`; Resume derives it from the replayed parent.

**Tech Stack:** C++20, existing BRUCE Grid/replay/cache code, CMake, CTest, Python CLI integration tests.

**Spec:** `docs/superpowers/specs/2026-10-16-scaled-exponential-grid-stage-v1-design.md`

## Global Constraints

- Use C++20 and existing CMake/CTest layouts; install no dependencies.
- Only `scaled-exponential-v1` accepts optional `--stage 0..57`; an absent stage means leaf-`y`.
- The local Grid coordinates remain `0..7`; a path stays an address, not a stage encoding.
- Exact honors `max_bits`; Metric retains its 64-bit evaluation semantics; Modular uses the existing nonzero modulus contract.
- `(0,0)` plus `--zero-zero stop` remains address-based and precedes evaluation.
- Implicit Stage-less Grid/Batch/Resume output, keys, traces, V1/V2 existing bundle formats and CLI behavior stay byte-compatible.
- Explicit Stage evidence uses exactly Grid V2, Batch V3/V4, and Resume V2 headers; do not repurpose an existing header.
- Do not hand-edit `docs/cli/source-index.md`, `docs/cli/examples.json`, or `docs/cli/validation.md`; verify them only in a complete temporary source copy.
- Git metadata is unavailable in this checkout: record failed Git checks and do not claim commits or a Git diff range.

## Review Focus

- Explicit stage 57 must succeed at 64 bits and fail at 63 bits; Task 1 pins both limits.
- An explicit stage must not bypass the address `(0,0)` stop sentinel; Task 1 pins stopped versus non-stopped stage-zero paths.
- Stage must distinguish cache entries for identical paths; Task 2 pins separate keys, misses and values.
- Each new bundle header must require canonical stage fields, while old headers retain old parsing; Tasks 1 and 2 pin both paths.
- Resume must inherit an explicit parent stage without a CLI override; Task 2 pins partial Batch, Resume and replay.

---

## File structure

| File | Responsibility |
| --- | --- |
| `include/bruce/application/grid.hpp` | Optional explicit stage on `GridTask`/result data. |
| `include/bruce/application/grid_spec.hpp` | Grid spec V1/V2 selection and canonical Stage constants. |
| `src/application/grid_spec.cpp` | Validate stage/rule combination and render/cache the selected spec version. |
| `src/application/grid.cpp` | Resolve stage, select Grid V1/V2 payload/header, and replay each version. |
| `src/application/grid_batch.cpp` | Preserve explicit Stage in Batch V3/V4 evidence and replay. |
| `src/application/grid_resume.cpp` | Select/parse Resume V1/V2 according to embedded parent evidence. |
| `src/application/application_replay.cpp` | Dispatch new Grid/Batch/Resume headers. |
| `apps/bruce_cli.cpp` | Parse restricted `--stage` for Grid and Batch. |
| `tests/unit/grid_tests.cpp` | Core stage, limits, sentinel and single replay contracts. |
| `tests/unit/grid_cache_tests.cpp` | Cache/Batch/Resume versioned stage contracts. |
| `tests/integration/test_cli.py` | Public option, errors and compatibility. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/75-current-architecture-and-milestones.md` | Final user contract and evidence-labeled architecture summary. |

### Task 1: Explicit-stage Grid V2 core and single replay

**Files:**
- Modify: `include/bruce/application/grid.hpp`
- Modify: `include/bruce/application/grid_spec.hpp`
- Modify: `src/application/grid_spec.cpp`
- Modify: `src/application/grid.cpp`
- Modify: `tests/unit/grid_tests.cpp`

**Interfaces:**
- Consumes: existing `GridTask`, `GridSpec`, `evaluate_grid`, cache-key rendering and `power`/`modular_power` contracts.
- Produces: `std::optional<std::uint64_t> GridTask::stage` and `GridSpec::stage`; V2 rendering/key/payload behavior only when the optional stage is present.

- [ ] **Step 1: Write the failing core-stage tests**

Add `grid_contracts` assertions for an explicit scaled task at path `2,6`, stage `0`, `7`, and `57`; assert values `0`, `12700`, and `14411518807585587100`, Metric bit length `64` at stage 57, modular stage-57 result `28` with modulus `128`, and exact stage-57 rejection at `max_bits=63`. Assert stage 58 and a stage on `leaf-local-power` reject with the specified errors. Assert `(0,0)` with stop is still stopped despite explicit stage, while another stage-zero address evaluates.

Add exact byte assertions that an absent-stage legacy run still renders/packs V1 exactly. Add V2 replay, rehashed stage mutation rejection and V1 legacy replay assertions.

- [ ] **Step 2: Run the core test to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_tests && ctest --test-dir /tmp/bruce-build -R '^grid_contracts$' --output-on-failure`

Expected: compile failure because `GridTask::stage`/`GridSpec::stage` and V2 behavior do not exist, or a new assertion failure.

- [ ] **Step 3: Implement optional-stage spec resolution and V2 single evidence**

Add optional `std::uint64_t stage` after existing fields to preserve aggregate initialization order. `grid_spec` must permit it only for `scaled_exponential_rule` and only through 57. Add helpers that select `grid-spec-v1`/`grid-spec-v2` and resolve leaf-`y` only when stage is absent.

Keep every V1 serializer byte-identical. For explicit stage, render canonical `stage_source explicit` and `stage N`, select `BRUCE_GRID_BUNDLE_V2`, emit explicit-stage JSON/trace metadata, and accept/replay V1 and V2 separately. V2 parsing must demand the two fields before reconstructing and comparing the complete payload.

- [ ] **Step 4: Run the focused core verification**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_tests && ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_replay_workflows)$' --output-on-failure`

Expected: both pass with no skipped tests.

### Task 2: Batch, Resume, dispatcher and CLI Stage transport

**Files:**
- Modify: `src/application/grid_batch.cpp`
- Modify: `src/application/grid_resume.cpp`
- Modify: `src/application/application_replay.cpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `tests/unit/grid_cache_tests.cpp`
- Modify: `tests/integration/test_cli.py`

**Interfaces:**
- Consumes: Task 1 optional stage, version selection and V2 single-Grid replay.
- Produces: Batch V3/V4, Resume V2, central replay dispatch for all new headers, and `--stage` on the two permitted CLI routes.

- [ ] **Step 1: Write failing Batch/CLI tests**

In `grid_cache_tests.cpp`, run equal paths under explicit stages 6 and 57; assert distinct cache digests/misses and values. Assert complete and partial explicit batches use V3/V4 respectively, carry canonical `stage_source`/`stage`, replay, reject rehashed stage mutations, and resume to V2 while preserving stage and cache reuse. Assert a legacy Batch/Resume remains on V1/V2 existing headers and replays unchanged.

In `test_cli.py`, assert `grid --path 2,6 --rule scaled-exponential-v1 --stage 57` reports the reference value; Batch accepts one shared stage; Resume retains it; `--stage 58` and `--rule leaf-local-power --stage 3` emit controlled errors and no export. Pin an omitted-stage legacy call.

- [ ] **Step 2: Run focused tests to establish RED**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_cache_tests bruce && ctest --test-dir /tmp/bruce-build -R '^grid_cache_contracts$' --output-on-failure`

Then run the new focused Python methods directly from `tests/integration/test_cli.py`.

Expected: compile failures or assertions because Batch/Resume/header dispatch/CLI do not transport `stage`.

- [ ] **Step 3: Implement explicit Stage transport**

Parse `--stage` exclusively for `grid` and `grid-batch`, using a bounded helper that normalizes malformed or out-of-range public input to `InvalidGridStage`. Leave `grid-resume` option-free.

Serialize explicit Batch specs with `grid-spec-v2` and fields in canonical order; choose V3 for complete and V4 for partial evidence, while retaining existing V1/V2 serializers exactly for absent stage. Add Resume V2 when its replayed parent is explicit and preserve V1 for legacy parents. Extend `application_replay.cpp` and all specific replayers to dispatch/accept only their documented header set.

- [ ] **Step 4: Run integration compatibility verification**

Run: `cmake --build /tmp/bruce-build --target bruce_grid_cache_tests bruce && ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_cache_contracts|grid_cache_workflows|grid_partial_workflows|grid_replay_workflows|grid_resume_workflows|cli_workflows)$' --output-on-failure`

Expected: all selected suites pass with no skips. Independently compare representative stage-less Grid, Batch and Resume stdout/bundle bytes with the Task-1 baseline binary.

### Task 3: Documentation, final verification and independent review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md` if it describes affected headers or payloads
- Modify: `docs/implementation/75-current-architecture-and-milestones.md`
- Modify: `docs/superpowers/specs/2026-10-16-scaled-exponential-grid-stage-v1-design.md` status only after verification
- Create: `runs/scaled-exponential-grid-stage-v1/reports/final-review.md`

**Interfaces:**
- Consumes: completed Task 1–2 versioned feature and test evidence.
- Produces: evidence-labeled public documentation and final review material.

- [ ] **Step 1: Document the implemented narrow contract**

Document `--stage 0..57` only for `scaled-exponential-v1`, leaf-y fallback, shared Batch stage, Stage/rule errors, V1 legacy compatibility, V2/V3/V4/V2 explicit headers, and Resume inheritance. State that paths remain local addresses and the change does not add coordinate expansion, free formulas or physical modeling.

- [ ] **Step 2: Run final relevant verification**

Run native configure/build and then:

```sh
ctest --test-dir /tmp/bruce-build -R '^(grid_contracts|grid_cache_contracts|grid_cache_workflows|grid_partial_workflows|grid_budget_workflows|grid_axis_workflows|grid_research_workflows|grid_replay_workflows|grid_resume_workflows|grid_experience_workflows|grid_experience_budget_workflows|cli_workflows)$' --output-on-failure
```

Run `python3 scripts/check_local_doc_links.py`, `git status --short`, and `git diff --check`. In a complete temporary source copy, hash the original generated CLI artifacts before/after and run `docs/cli/verify_examples.py`; never edit generated files by hand.

- [ ] **Step 3: Record evidence and update status**

Write exact commands, exit codes, CTest totals, byte-compatibility outcomes, verifier/hashes, link result and Git limitation to the final report. Mark the design **IMPLEMENTIERT UND BELEGT** only if all required work passes; otherwise retain a conservative status.

- [ ] **Step 4: Independent whole-change review**

Use a fresh reviewer to inspect limits, header/version routing, absent-stage compatibility, cache/replay binding, documentation and evidence. Require PASS before declaring this scope complete.

## Plan self-review

- **Spec coverage:** Task 1 implements optional-stage validation, evaluation and V2 single evidence. Task 2 transports it through Batch, Resume, central replay and CLI with explicit headers. Task 3 documents and independently verifies the scope.
- **Step scan:** Each implementation task begins with concrete failing values and replay tests before production code, then has a focused GREEN command.
- **Type consistency:** Task 1 defines the optional stage fields and version behavior consumed by Task 2; only Task 2 creates Batch/Resume/dispatcher support.
- **Review focus:** The five listed high-risk cases are pinned respectively in Task 1 or Task 2 tests.
- **Proportion:** Three tasks cover one existing Grid rule's bounded input extension without expanding coordinates, arithmetic domain, plugin model or scheduler scope.
