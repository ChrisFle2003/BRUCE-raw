# Exact Evidence Chain V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a local, replayable `BRUCE_EVIDENCE_CHAIN_V1` envelope for one bounded Exact-Affine research run without changing historical native bundles or elevating sample evidence to proof.

**Architecture:** A new evidence module wraps an unchanged `BRUCE_EXACT_BUNDLE_V1` together with the original training and hold-out CSV bytes and fixed evidence-contract literals. Its replay parser validates framing, digest, declared technical disjointness, embedded samples, native replay and canonical result equality. A narrow `bruce evidence create|replay` CLI surface invokes this module directly; it does not use the general application dispatcher or any promotion gate.

**Tech Stack:** C++20, GMP/GMPXX, OpenSSL SHA-256, CMake/CTest, Python 3 standard-library integration tests.

**Spec:** `docs/superpowers/specs/2026-10-01-exact-evidence-chain-design.md`

## Global Constraints

- The format header is exactly `BRUCE_EVIDENCE_CHAIN_V1`; the contract is exactly `affine-evidence-chain-v1`.
- The fixed fields are exactly `declared-disjoint-v1`, `affine-inverse-v1`, and `short-circuit-training-then-holdout-v1`.
- Preserve the Exact engine's short-circuit behavior: each reached candidate is checked training-first; a contradiction stops that candidate; inverse countercheck runs only after a matching forward evaluation. Holdout begins only after training passes and when result/budget flow reaches it. `INCOMPLETE` is not coverage; no all-candidates/all-samples claim is permitted.
- Support only `ExactResearchTask` with `strategy="affine"`, `planner="enumerate"`, and domain `bigint` or `rational`; default domain is `bigint`.
- Preserve `BRUCE_EXACT_BUNDLE_V1`, existing `research`/`replay` behavior and every historical fixture byte-for-byte.
- Read each source CSV as raw bytes before parsing; each input remains capped at 1 MiB and the exported chain at `max_file_bytes`.
- The new output must retain `general_proof:false`; it must not expose or activate learn, publish, rule, Experience, or human-identity gates.
- Use existing `DuplicateOrOverlappingSampleInput` validation rather than inventing `HoldoutOverlapOrDuplicate`.
- No network I/O, cloud service, cryptographic-search claim, or change outside this bounded local research path.
- This working directory has no Git repository; skip commit steps and report the uncommitted changed files explicitly.

## Review Focus

- Rehashed but syntactically valid policy/scope/independence mutations must be rejected by replay — Task 1 unit codec tests.
- The Native Exact task and both raw CSV roles must agree semantically, not merely by digest — Task 1 unit codec tests and Task 2 workflow tampering case.
- A disjointness declaration is not external independence or a proof — Task 2 asserts literal value and `general_proof:false`.
- A result limited by candidate/operation budget must remain `INCOMPLETE` after pack/replay — Task 1 and Task 2 budget cases.
- An existing export target must remain untouched after a create/replay failure — Task 2 integration tests.

---

## File Structure

| File | Responsibility |
|---|---|
| `include/bruce/evidence/chain.hpp` | Public bounded Exact-Evidence types and create/render/pack/replay API. |
| `src/evidence/chain.cpp` | Strict chain framing, raw CSV binding, native Exact replay and canonical result construction. |
| `apps/bruce_cli.cpp` | `evidence create` and `evidence replay` parsing, raw input reading and exclusive export. |
| `tests/unit/evidence_chain_tests.cpp` | Direct API, codec, semantic-binding and incomplete-run contracts. |
| `tests/integration/test_evidence_chain.py` | CLI workflows, file/export behavior and rehashed manipulation cases. |
| `CMakeLists.txt` | Core source registration and two CTest targets. |
| `docs/cli/commands.md` | User-facing command syntax, option scope and statement boundaries. |
| `docs/cli/formats.md` | Exact `BRUCE_EVIDENCE_CHAIN_V1` framing and replay constraints. |
| `docs/cli/results-and-control.md` | Meaning and non-proof boundary of the new evidence wrapper. |

### Task 1: Exact-Evidence Domain Codec

**Files:**
- Create: `include/bruce/evidence/chain.hpp`
- Create: `src/evidence/chain.cpp`
- Create: `tests/unit/evidence_chain_tests.cpp`
- Modify: `CMakeLists.txt:8-64,211-216`

**Interfaces:**
- Consumes: `ExactResearchTask`, `ExactResearchRun`, `parse_exact_samples`, `exact_research`, `pack_exact_research`, `replay_exact_research`, `render_exact_research`, `sha256`, `max_file_bytes`.
- Produces:
  ```cpp
  struct ExactEvidenceChainRun {
      ExactResearchRun research;
      std::string training_csv;
      std::string holdout_csv;
  };
  ExactEvidenceChainRun create_exact_evidence_chain(
      ExactResearchTask task, std::string training_csv, std::string holdout_csv,
      const std::function<bool(std::uint64_t)>& cancelled = {});
  std::string render_exact_evidence_chain(const ExactEvidenceChainRun&);
  std::string pack_exact_evidence_chain(
      const ExactEvidenceChainRun&, const ReplayCheckpoint& checkpoint = {});
  ExactEvidenceChainRun replay_exact_evidence_chain(
      std::string_view bundle, std::uint64_t operation_limit = 10000000,
      const ReplayCheckpoint& checkpoint = {});
  ```
- Produces: strict `BRUCE_EVIDENCE_CHAIN_V1` envelopes and `evidence-chain-v1` JSON whose nested `result` is the existing canonical Exact result.

- [ ] **Step 1: Write failing direct-codec tests in `tests/unit/evidence_chain_tests.cpp`**

Create a rational affine task for `y=2*x+3` with separate CSV byte strings. Assert creation/pack/replay round-trips, fixed literals, `general_proof:false`, and replay equality after source byte strings are unavailable. Add independent tests that expect errors for overlapping x-values, a rehashed `countercheck_policy` mutation, mismatched embedded training CSV, modified native result, and an operation-limited `INCOMPLETE` run.

Final-fix acceptance adds exact diagnostic assertions for rehashed malformed lengths, missing/duplicate/out-of-order fields, oversized declared blocks and total envelope, and rehashed trailing payload. Retain the stale-inner-digest rejection and add a native-result mutation with both inner and outer digests recomputed to reach native semantic replay. Embed a genuinely valid, independently replayable non-Affine native envelope and reject it at the Chain's Exact-V1 boundary. The integration workflow must also reject outer-rehashed trailing payload and strict-field framing at their intended parser/replay paths.

- [ ] **Step 2: Register only the new unit target and run it to verify RED**

Add `src/evidence/chain.cpp` to `bruce_core`; add `bruce_evidence_chain_tests` plus `evidence_chain_contracts` to `CMakeLists.txt`. Then run:

```bash
cmake -S . -B build
cmake --build build --target bruce_evidence_chain_tests
ctest --test-dir build -R '^evidence_chain_contracts$' --output-on-failure
```

Expected: compilation or test failure because `bruce/evidence/chain.hpp` and its API do not yet exist; do not continue until the failure is attributable to the absent feature.

- [ ] **Step 3: Implement the minimal `chain.hpp` and `chain.cpp` API**

Use the strict length-prefixed reader pattern from `src/application/grid_research.cpp`, not line splitting of raw blocks. `create_exact_evidence_chain` must reject non-affine/non-enumerate/int64 tasks; parse retained raw CSV bytes in the task domain; require task samples to equal the parsed samples; call `validate_exact_task`; run `exact_research`; and retain original bytes.

`pack_exact_evidence_chain` must serialize only the fixed literals, original raw bytes, `pack_exact_research(research)`, and `render_exact_evidence_chain(run)` beneath a SHA-256 digest. Reject a chain larger than `max_file_bytes` before returning it.

`replay_exact_evidence_chain` must reject malformed/extra/double fields, unknown literals, incorrect block lengths, non-Exact-V1 native bundles, invalid raw CSV, non-disjoint samples, task/sample mismatches and result mismatches. It must call `replay_exact_research` and rebuild the whole canonical body before comparing it exactly. No failure may promote a status or invoke a gate.

- [ ] **Step 4: Run the unit target to verify GREEN**

```bash
cmake --build build --target bruce_evidence_chain_tests
ctest --test-dir build -R '^evidence_chain_contracts$' --output-on-failure
```

Expected: one passing `evidence_chain_contracts` test target; all mutation and incomplete cases execute against real codec behavior.

- [ ] **Step 5: Run the directly adjacent regression targets**

```bash
ctest --test-dir build -R '^(exact_domain_contracts|research_replay_control|application_task_adapters|evidence_chain_contracts)$' --output-on-failure
```

Expected: all selected targets pass, confirming native Exact replay and application adapters remain intact.

### Task 2: Bounded CLI Create and Replay

**Files:**
- Modify: `apps/bruce_cli.cpp:212-255,383-430`
- Create: `tests/integration/test_evidence_chain.py`
- Modify: `CMakeLists.txt` near the `evidence_chain_contracts` registration from Task 1

**Interfaces:**
- Consumes: the Task 1 `create_exact_evidence_chain`, `pack_exact_evidence_chain`, `replay_exact_evidence_chain`, `render_exact_evidence_chain`; `read_bounded`; `export_exclusive`.
- Produces:
  ```text
  bruce evidence create --train FILE --holdout FILE --output FILE
      [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
      [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
      [--max-operations N] [--max-trace N] [--max-results N]
  bruce evidence replay CHAIN
  ```
- Produces: `evidence_chain_workflows` CTest entry, passing `$<TARGET_FILE:bruce>` as its sole script argument.

- [ ] **Step 1: Write failing CLI workflow tests in `tests/integration/test_evidence_chain.py`**

Follow `tests/integration/test_power_research.py`'s subprocess/temporary-file pattern. Cover: a positive rational run with fixed evidence fields, source-file deletion followed by `evidence replay`, a `REFUTED` hold-out, overlap rejected without an output file, unsupported `--strategy` rejected, existing output preserved on failure, rehashed mutations of policy/scope/independence/CSV/native/result rejected, and a `--max-operations 1` result that remains `INCOMPLETE` after replay.

- [ ] **Step 2: Run the workflow directly to verify RED**

```bash
cmake --build build --target bruce
python3 tests/integration/test_evidence_chain.py build/bruce
```

Expected: failures caused by the missing `evidence` command, not by fixture, timeout, or Python harness errors.

- [ ] **Step 3: Add the minimal `evidence` CLI branch and CTest registration**

Add help lines and a branch after `language` and before `research`. For `create`, allow exactly the documented whitelist; default to `bigint`, force `strategy="affine"` and `planner="enumerate"`, read raw CSV bytes with the existing 1 MiB limit, parse them into the task, call Task 1, then `export_exclusive` only after successful packing. For `replay`, require exactly one positional artifact and print `render_exact_evidence_chain(replay_exact_evidence_chain(read_bounded(path)))`.

Do not accept `--verification`, general-session cancellation, strategy/planner, Experience, Grid, Rule, transfer or publication options. Do not route through generic `replay` or `execute_cli`.

Register `evidence_chain_workflows` after the unit target with Python 3 and `$<TARGET_FILE:bruce>`.

- [ ] **Step 4: Run the new workflow to verify GREEN**

```bash
cmake --build build --target bruce
python3 tests/integration/test_evidence_chain.py build/bruce
ctest --test-dir build -R '^evidence_chain_(contracts|workflows)$' --output-on-failure
```

Expected: all positive, negative, source-deletion and tampering cases pass; normal negative CLI cases exit 2 and do not create/replace artifacts.

- [ ] **Step 5: Run existing Exact CLI regressions**

```bash
ctest --test-dir build -R '^(exact_domain_workflows|power_research_workflows|cli_workflows|evidence_chain_contracts|evidence_chain_workflows)$' --output-on-failure
```

Expected: all selected workflows pass and existing `research`/generic `replay` output remains stable.

### Task 3: Contract Documentation and Full Verification

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/cli/results-and-control.md`
- Modify: `docs/audit/2026-10-01-core-0102-ist-gegen-zielbild.md`

**Interfaces:**
- Consumes: the actual Task 1 format/parser literals and Task 2 CLI behavior.
- Produces: user documentation that labels the wrapper as bounded sample evidence and points to its exact replay and non-proof limits.

- [ ] **Step 1: Write documentation assertions into the integration test before changing docs**

Extend `tests/integration/test_evidence_chain.py` with no new production behavior: assert the observed JSON uses exactly the documented contract/literal strings and that `result.general_proof` is false. This makes the documentation’s critical claims executable.

- [ ] **Step 2: Run the assertion-bearing workflow to verify it fails if docs/literals were guessed incorrectly**

```bash
python3 tests/integration/test_evidence_chain.py build/bruce
```

Expected: if Task 1/2 implementation uses different literals or shape, correct implementation/tests before documenting; do not document an unverified format.

- [ ] **Step 3: Document only implemented behavior and its boundary**

Add command syntax and accepted options to `commands.md`; add exact framing, raw-byte role binding, total-size and replay rules to `formats.md`; add a status explanation to `results-and-control.md`. Update the audit’s P0 entry from planned to implemented only after the full suite passes. State explicitly that declared disjointness is not external independence and that `VERIFIED_ON_SAMPLES` plus replay is not a universal proof.

- [ ] **Step 4: Verify CLI documentation examples and focused tests**

```bash
python3 tests/integration/test_evidence_chain.py build/bruce
bash docs/cli/tutorial.sh ./build/bruce
python3 docs/cli/verify_examples.py ./build/bruce
```

Expected: the new workflow passes; existing tutorial and generated CLI example/index validation pass without changing legacy contracts.

- [ ] **Step 5: Run full project verification and record the environment limitation**

```bash
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Expected: a fresh configure/build and all tests pass. If `pkg-config` remains unavailable, record that exact blocker, then run the widest executable verification against the existing hash-matched build; do not report a fresh build as passing.

- [ ] **Step 6: Review changed files and hand off without a commit**

Because no Git repository is present, inspect the exact changed files with `git` omitted, re-read the spec against the implementation, and report files, test results, residual build limitation, and the non-proof/non-activation boundary.

## Plan Self-Review

- **Spec coverage:** Tasks 1–2 cover the bounded envelope, raw CSV roles, fixed countercheck scope, native replay, CLI and all required negative cases. Task 3 covers the user contract, audit update and suite-level verification.
- **Type consistency:** Task 1 defines the only new public type and functions; Task 2 consumes them directly without a general `ApplicationTask` extension.
- **Review focus:** Each listed failure mode has a named unit or integration test task.
- **Proportion:** The plan names interfaces, exact literals, files and checks, but leaves implementation details to the TDD cycle.
