# Language-Core Phase Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a deterministic German mini-language that produces a visible, replayable draft for one existing rational portfolio task and runs it only through the existing checked Core session.

**Architecture:** A new `language` library owns normalization, interpretation, draft selection and canonical language bundles. A small application adapter turns only an understood or explicitly selected draft into the existing `ExactResearchTask` and delegates execution to `execute_checked_application`; it records an outer linear phase trace without changing `ApplicationTask`, `ExecutionSession`, core budgets, mathematical algorithms, rules or gates.

**Tech Stack:** C++20, existing CMake/Ninja build, GMP/GMPXX, OpenSSL SHA-256, Python `unittest` integration tests.

**Spec:** `docs/superpowers/specs/2026-09-27-language-core-phase-slice-design.md`

## Global Constraints

- The only supported V1 semantic intent is rational portfolio research with exactly two explicitly passed local inputs. Its unique form is `Untersuche @train mit @holdout auf rationale Formeln.`; the `@left und @right` form exists solely to expose two explicit alternatives before selection.
- Natural-language text never supplies filesystem paths, output paths, shell arguments, rules, gate choices or budgets.
- `language interpret` writes a draft only; `language select` selects an existing alternative only; only `language execute` may start the Core.
- V1 maps to `ExactResearchTask{domain="rational", strategy="portfolio-rational"}` and must use `execute_checked_application`.
- `AMBIGUOUS` and `UNSUPPORTED` never execute the Core. Selection is explicit and bound to a canonical draft.
- A `LanguageRunBundleV1` preserves raw/normalized text, grammar version, bound input bytes and digests, alternatives, selection, native Core-bundle bytes/digest, outer phases and the deterministic explanation.
- Language rendering only repeats structured Core fields; it cannot create truth claims or open Rule-, Learn- or Publish-Gates.
- No LLM, network, database, GUI, NanoCell, parallel execution, new universal budget, `ApplicationTask` variant or mathematical strategy is part of this plan.
- Existing CLI calls, native bundles, output bytes, error contracts and rule flows remain unchanged.
- The source directory is not a Git repository. Do not create a commit or claim one; record `git status --short` failure in the handoff.
- The GVFS path contains a colon; build and test from a fresh `/tmp` source copy using `scripts/with-host-deps.sh` and Ninja.

## Review Focus

- Invalid UTF-8, unsupported text, missing aliases and duplicate aliases must return a language error/draft without invoking or exporting a Core run — Task 1 and Task 3.
- An ambiguous phrase must preserve both canonical alternatives; `language execute` must reject it until `language select` creates a bound selection — Task 1 and Task 3.
- A selected draft with changed source bytes, changed grammar version, forged digest or unknown alternative must fail before Core execution — Task 2 and Task 3.
- A native Core `INCOMPLETE`, budget error or replay failure must retain its existing status/error semantics; the language renderer may not promote it — Task 2 and Task 3.
- Replaying a language run must reproduce the chosen task, embedded native bundle and explanation only under the original grammar and input bytes — Task 2 and Task 3.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `include/bruce/language/language.hpp` | Public V1 request, interpretation, draft, phase-run and canonical bundle interfaces. |
| `src/language/language.cpp` | Deterministic normalization, V1 phrase grammar, alternative selection, canonical digest-wrapped draft/run serialization and replay checks. |
| `include/bruce/application/language_run.hpp` | Application-only bridge from a bound language draft to an existing checked `ExactResearchTask`. |
| `src/application/language_run.cpp` | Executes selected draft, preserves native `ExecutionSession`, builds phase trace and template explanation. |
| `include/bruce/application/exact_research.hpp`, `src/application/exact_research.cpp` | Expose byte-based exact-sample parsing so the bridge can parse already bound input bytes without rereading paths. |
| `apps/bruce_cli.cpp` | New isolated `language interpret/select/execute/replay` command branch. |
| `tests/unit/language_tests.cpp` | Grammar, normalization, selection, canonical serialization, tampering and no-Core-start unit coverage. |
| `tests/unit/language_run_tests.cpp` | Core bridge, phase trace, native result preservation, template rendering and replay coverage. |
| `tests/integration/test_language.py` | Process-level CLI, exclusive files, ambiguity, invalid input, budget/error and replay cases. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/cli/control-overview.md`, `docs/cli/verify_examples.py` | User contract, bundle format, scope boundary and generated option/example verification. |
| `CMakeLists.txt` | Adds language sources and both unit test executables to `bruce_core`/CTest. |

### Task 1: Deterministic language drafts and canonical selection

**Files:**
- Create: `include/bruce/language/language.hpp`
- Create: `src/language/language.cpp`
- Create: `tests/unit/language_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `enum class LanguageInterpretationStatus { understood, ambiguous, unsupported }`.
- Produces `struct LanguageInputV1 { std::string alias, bytes, digest; }`, `struct LanguageAlternativeV1 { std::string id, rule; }`, `struct LanguageDraftV1`, and `struct LanguageRunBundleV1` as declared in `language.hpp`.
- Produces `LanguageDraftV1 interpret_language(LanguageRequestV1)`; `LanguageDraftV1 select_language_alternative(const LanguageDraftV1&, std::string_view id)`; `std::string pack_language_draft(const LanguageDraftV1&)`; and `LanguageDraftV1 unpack_language_draft(std::string_view)`.
- Task 2 consumes an `understood` draft or an `ambiguous` draft with exactly one selected `LanguageAlternativeV1`.

- [ ] **Step 1: Write the failing grammar and selection unit tests**

Add tests that prove all of the following before implementation:

```cpp
const auto understood=interpret_language(request(" Untersuche @train mit @holdout auf rationale Formeln. ", train, holdout));
require(understood.status==LanguageInterpretationStatus::understood,"expected understood");
require(understood.normalized_text=="untersuche @train mit @holdout auf rationale formeln.","normalization changed");
require(understood.alternatives.size()==1,"expected one task");

const auto ambiguous=interpret_language(request("Untersuche @left und @right auf rationale Formeln.", left, right));
require(ambiguous.status==LanguageInterpretationStatus::ambiguous && ambiguous.alternatives.size()==2,"alternatives lost");
require(select_language_alternative(ambiguous,"train=left;holdout=right").selected_id=="train=left;holdout=right","selection lost");
```

Also assert idempotent normalization; unsupported text; malformed UTF-8; missing/duplicate aliases; an unknown alternative; digest tampering; and that packing then unpacking a draft is byte-canonical.

- [ ] **Step 2: Run the new unit test to verify it fails**

Run: `ctest --test-dir build-audit --output-on-failure -R '^language_contracts$'`

Expected: failure because the target and language interfaces do not yet exist.

- [ ] **Step 3: Implement `language.hpp` and `language.cpp`**

Use `sha256` and the existing canonical text-with-digest style; define headers `BRUCE_LANGUAGE_DRAFT_V1` and `BRUCE_LANGUAGE_RUN_V1`, length-prefix all free-text/byte fields, reject trailing data, duplicate aliases and noncanonical re-encoding. Normalize only ASCII case and surrounding/inter-token ASCII whitespace in V1; validate UTF-8 before interpreting and preserve the original raw text separately.

Implement exactly two grammatical productions:

- `untersuche @train mit @holdout auf rationale formeln.` → one `rational-portfolio-v1` alternative;
- `untersuche @left und @right auf rationale formeln.` → two ordered alternatives, `train=left;holdout=right` and `train=right;holdout=left`.

All other normalized forms return `unsupported`; they never infer an alias or a task. `select_language_alternative` must accept only an existing alternative of an `ambiguous` draft and return a new canonical draft with `selected_id` bound into its digest.

- [ ] **Step 4: Run the unit test to verify it passes**

Run: `ctest --test-dir build-audit --output-on-failure -R '^language_contracts$'`

Expected: PASS. The test must demonstrate that an ambiguous draft is data, not a silent task choice.

- [ ] **Step 5: Record the completed task without committing**

Record the changed files and test result in the implementation handoff. `git status --short` is expected to fail because this checkout has no Git metadata.

### Task 2: Checked Core bridge, outer phase trace and language-run replay

**Files:**
- Create: `include/bruce/application/language_run.hpp`
- Create: `src/application/language_run.cpp`
- Modify: `include/bruce/application/exact_research.hpp`
- Modify: `src/application/exact_research.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/language_run_tests.cpp`

**Interfaces:**
- Consumes `LanguageDraftV1` and its bound `LanguageInputV1` bytes from Task 1.
- Produces `std::vector<ExactSample> parse_exact_samples(std::string_view bytes, const NumberDomain&)`; `LanguageRunV1 execute_language_draft(const LanguageDraftV1&)`; `std::string render_language_run(const LanguageRunV1&)`; `std::string pack_language_run(const LanguageRunV1&)`; and `LanguageRunV1 replay_language_run(std::string_view)`.
- `LanguageRunV1` contains its `LanguageDraftV1`, native `ExecutionSession`, outer sequence `RECEIVED, INTERPRETED, CONFIRMED, CORE_EXECUTED, CORE_REPLAYED, EXPLAINED, COMPLETED`, and deterministic explanation.
- Task 3 uses these APIs without rebuilding a Core task from raw CLI arguments.

- [ ] **Step 1: Write failing Core-bridge and replay tests**

Write direct C++ tests asserting:

```cpp
const auto run=execute_language_draft(selected_or_understood_draft);
require(run.session.phases.current()==ExecutionPhase::ready,"native replay was skipped");
require(run.phases==expected_language_phases,"outer phases changed");
require(render_language_run(run).find(render_execution_session(run.session))!=std::string::npos,"renderer changed Core evidence");
require(replay_language_run(pack_language_run(run)).explanation==run.explanation,"language replay changed explanation");
```

Add cases for a draft without a selection, changed input bytes, changed grammar version, a Core budget that yields `INCOMPLETE`, and a corrupt embedded Core bundle. Assert no case changes `claim_scope`, `general_proof`, `learn_gate` or `publish_gate` from the embedded structured Core result.

- [ ] **Step 2: Run the bridge test to verify it fails**

Run: `ctest --test-dir build-audit --output-on-failure -R '^language_run_contracts$'`

Expected: failure because no language-to-core bridge or language-run replay exists.

- [ ] **Step 3: Refactor exact sample reading and implement the bridge**

Extract the current CSV body of `read_exact_samples(path, domain)` into `parse_exact_samples(bytes, domain)`. Keep `read_exact_samples` as a compatibility wrapper that reads at its existing limit and calls the new parser; existing CLI behavior and errors must remain unchanged.

`execute_language_draft` must reject unsupported or unresolved ambiguous drafts before constructing a task. For a valid selected/understood draft, parse the already bound `train` and `holdout` bytes, construct only `ExactResearchTask` with `domain="rational"`, `strategy="portfolio-rational"` and its existing defaults/budget fields stored in the draft, then call `execute_checked_application`. Build the outer phases only around that call. Pack native `session.bundle` bytes and their digest into `BRUCE_LANGUAGE_RUN_V1`; replay must validate the outer digest, draft grammar/input digests, embedded native bundle and `replay_application` equivalence before rendering the stored deterministic explanation.

- [ ] **Step 4: Run focused compatibility tests**

Run:

```bash
ctest --test-dir build-audit --output-on-failure -R '^(language_run_contracts|application_task_adapters|execution_session_states|exact_domain_workflows|rational_portfolio_workflows)$'
```

Expected: all groups PASS. The language run is new evidence around the existing Core, not a modified Core result.

- [ ] **Step 5: Record the completed task without committing**

Record the changed files and focused verification. Do not create a commit in this non-Git checkout.

### Task 3: Explicit CLI workflow, documentation and end-to-end acceptance

**Files:**
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_language.py`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/cli/control-overview.md`
- Modify: `docs/cli/verify_examples.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 1 draft functions and Task 2 execution/replay functions.
- Adds four CLI commands:

```text
bruce language interpret --text TEXT --train TRAIN.csv --holdout HOLDOUT.csv [--max-bits N --max-denominator N --max-candidates N --max-operations N --max-trace N --max-results N] --output DRAFT
bruce language select DRAFT --alternative ID --output SELECTED_DRAFT
bruce language execute DRAFT --output LANGUAGE_RUN
bruce language replay LANGUAGE_RUN
```

- Produces JSON contracts `language-interpretation-v1`, `language-selection-v1`, `language-execution-v1` and `language-replay-v1`; every writer uses the existing exclusive export.

- [ ] **Step 1: Write the failing process-level language workflow test**

Create `LanguageTests` with a temporary train/holdout pair and assert:

```python
draft = call('language','interpret','--text','Untersuche @train mit @holdout auf rationale Formeln.', '--train',train,'--holdout',holdout,'--output',draft_file)
assert draft['status'] == 'UNDERSTOOD'
run = call('language','execute',draft_file,'--output',run_file)
assert run['contract'] == 'language-execution-v1'
assert run['execution']['replay_verified'] is True
assert run['result']['general_proof'] is False
assert call('language','replay',run_file) == run
```

Add E2E cases for ambiguous draft rejection before `select`, explicit selection then success, unsupported and invalid-UTF-8 text with no output, modified draft/source/run bytes, existing output preservation, a Core budget yielding `INCOMPLETE`, and an existing `research` invocation whose JSON and bundle bytes remain unchanged.

- [ ] **Step 2: Run the integration test to verify it fails**

Run: `python3 tests/integration/test_language.py ./build-audit/bruce`

Expected: failure because `language` is currently an unknown command.

- [ ] **Step 3: Add the isolated CLI branch and documentation**

Add a top-level `command=="language"` branch with only the four exact actions and option sets above. `interpret` reads the two explicitly named inputs before building `LanguageRequestV1`; it never accepts arbitrary aliases, repeated inputs, free-form path extraction or a Core output option. `select` and `execute` read canonical drafts through the bounded local reader. All writers use `export_exclusive`; all error paths retain the existing JSON error envelope and exit 2.

Document the four commands, grammar, alias binding, draft/run formats, phase trace, no-LLM boundary, status wording, replay limits and the fact that NanoCell/graph phases are not yet available. Extend `verify_examples.py` with the success, ambiguity, unsupported, selection and replay examples, then regenerate `source-index.md` and `examples.json` through that verifier.

- [ ] **Step 4: Run focused end-to-end and documentation verification**

Run:

```bash
python3 tests/integration/test_language.py ./build-audit/bruce
python3 docs/cli/verify_examples.py ./build-audit/bruce
ctest --test-dir build-audit --output-on-failure -R '^(language_contracts|language_run_contracts|language_workflows|application_task_adapters|execution_session_workflows)$'
```

Expected: all PASS; the documentation verifier regenerates the options/examples with valid links, and no language case mutates active rules or claims a general proof.

- [ ] **Step 5: Rebuild and run the complete suite from a fresh temporary copy**

Copy the required CMake inputs, `apps`, `src`, `include`, `tests`, `scripts`, `docs`, `examples` and `build-host-deps` to a fresh `/tmp/bruce-language-core-XXXXXX` directory. Then run:

```bash
./scripts/with-host-deps.sh cmake -S . -B build-audit -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
./scripts/with-host-deps.sh cmake --build build-audit --parallel 16
./scripts/with-host-deps.sh ctest --test-dir build-audit --output-on-failure --parallel 8
./scripts/with-host-deps.sh python3 scripts/verify_control_flow.py build-audit/bruce --report /tmp/bruce-language-core-control-flow.json
git status --short
```

Expected: configure/build succeed; every registered CTest group passes; the existing control-flow verifier remains accepted for the pre-existing paths; `git status --short` reports that this is not a Git repository. The dedicated language tests, not the old control-flow script, are evidence for the new workflow.

## Plan Self-Review

- **Spec coverage:** Task 1 implements deterministic parsing, alternatives and canonical drafts. Task 2 binds drafts to the existing checked Core, outer phases, storage and replay. Task 3 exposes the explicit three-step user workflow plus language replay, documentation and end-to-end acceptance. NanoCell, a graph scheduler and all excluded systems have no implementation task.
- **Type consistency:** `LanguageDraftV1` is created/unpacked only in Task 1, consumed by Task 2 and passed unchanged by Task 3. `LanguageRunV1` is produced, rendered, packed and replayed only by Task 2 before CLI exposure in Task 3. Byte parsing is added beneath the established `read_exact_samples` compatibility wrapper.
- **Review focus:** Each listed failure class is pinned to a unit or integration task: invalid inputs/aliases and ambiguity in Tasks 1/3; tampering in Tasks 2/3; native Core status preservation in Tasks 2/3; replay binding in Tasks 2/3.
- **Proportion and scope:** The plan contains three independently reviewable deliverables and does not prescribe implementation bodies beyond grammar literals, canonical format headers and interface boundaries required by the specification.
