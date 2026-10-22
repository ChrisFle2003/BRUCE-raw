# Toy-Relation V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a local, deterministic toy-cyclic-group relation checker with two fixed CLI demonstrations and proof that collisions yield only independently verified modular-relation candidates.

**Architecture:** `bruce_core` receives one independent toy-relation library that validates small `Z/nZ` group parameters, forms canonical points, canonicalizes a collision pair by provenance ID, derives a candidate through modular inversion, and verifies the resulting record in a separate operation. An always-available CLI constructs only the two fixed scenarios and renders stable JSON; documentation names the mathematical reference scope and its explicit non-goals.

**Tech Stack:** C++20, CMake, CTest, Python 3 `unittest`, existing `bruce::Error` and JSON helpers.

**Spec:** `docs/superpowers/specs/2026-10-13-toy-relation-v1-design.md`

## Global Constraints

- Add the core only under `include/bruce/toy_relation/` and `src/toy_relation/`, linked through `bruce_core`; add no dependency.
- Use additive `Z/nZ` only: `3 <= modulus <= 65535`, normalized `uint32_t` values, and a generator coprime to the modulus.
- Fixed demos are exactly `toy-relation-v1 demo collision` and `toy-relation-v1 demo noninvertible`; no files, user group parameters, searches, keys, curves, network, GPU, persistence, plugins, Phase-DAG V2/V3, or Signed-Affine-DAG changes.
- Canonicalize the two-provenance pair by non-empty provenance ID before collision recording and relation derivation; swapped caller input must produce identical records/reports.
- Core `NonInvertibleToyRelationCoefficient` creates no relation record. Only the fixed CLI catches that expected core outcome to render a collision-bearing, candidate-free report; all other errors remain error-only JSON on stderr with exit 2.
- Use a complete `/tmp` copy and `scripts/with-host-deps.sh` for build/test work. Do not install packages.
- Change `docs/cli/source-index.md`, `docs/cli/examples.json`, and `docs/cli/validation.md` only from `docs/cli/verify_examples.py` executed in a complete temporary copy.
- Git metadata is unavailable. Store source snapshots, diffs, reports and review artifacts under `runs/toy-relation-v1-sdd/`; do not claim commits or a successful Git status.

## Review Focus

- **Task 1:** a non-coprime generator must be rejected before point arithmetic, including a modulus where the generator is in range but has a common factor.
- **Task 1:** a caller-supplied/tampered collision record whose point or sorted IDs disagree with the two provenances must be rejected by derivation rather than trusted.
- **Task 1:** swapped provenance input must yield byte-equivalent collision/relation fields, including signs represented as normalized deltas.
- **Task 2:** malformed public demo invocations must be exit-2 error-only JSON; the scoped review must confirm the implementation catches only `NonInvertibleToyRelationCoefficient`.
- **Task 3:** manual and generated artifacts must never imply the toy candidate is secret, a discrete-log result, a search, or a Phase-DAG extension.

---

## File Structure

| Path | Responsibility |
|---|---|
| `include/bruce/toy_relation/toy_relation_v1.hpp` | Public group, provenance, collision/relation record types and four pure operations. |
| `src/toy_relation/toy_relation_v1.cpp` | Validation, canonical point calculation, pair canonicalization, extended-Euclid inversion, derivation and independent verification. |
| `tests/unit/toy_relation_v1_tests.cpp` | Algebra, validation, order invariance, tamper and verification contracts. |
| `apps/toy_relation_v1_cli.hpp` | Fixed demo factories, report renderer and strict command parser. |
| `apps/bruce_cli.cpp` | Always-available help, include and top-level routing. |
| `tests/integration/test_toy_relation_v1_cli.py` | Real CLI JSON, noninvertible report, error-boundary and deterministic-output tests. |
| `README.md`, `docs/architecture/03-blueprint-consolidation.md` | Implemented scope and explicit exclusion claims. |
| `docs/cli/README.md`, `docs/cli/commands.md` | Commands, report shape, exact expected non-success and safety boundaries. |
| `tests/integration/test_cli_docs.py` | Documentation claims bound to the specific new manual section. |

### Task 1: Toy-Relation Core and Reference Contracts

**Files:**
- Create: `include/bruce/toy_relation/toy_relation_v1.hpp`
- Create: `src/toy_relation/toy_relation_v1.cpp`
- Create: `tests/unit/toy_relation_v1_tests.cpp`
- Modify: `CMakeLists.txt` — core source plus `bruce_toy_relation_v1_tests` / `toy_relation_v1_contracts`.

**Interfaces:**
- Consumes: C++20 standard library and `bruce::Error`.
- Produces for Task 2: types and exact signatures below.

- [ ] **Step 1: Snapshot Task 1 inputs and write failing algebra contracts**

Save the original `CMakeLists.txt` under `runs/toy-relation-v1-sdd/base/task-1/`; create the unit test before any production file. Cover the positive `n=101, G=1, Q=17` collision (`(2,3)` and `(36,1)`, point `53`, deltas `2`/`34`, candidate `17`, verified); the `n=15` noninvertible pair (`(0,5)` and `(5,0)`); swapped input equality; different points; invalid modulus/generator/public point/coefficient/IDs; tampered collision record; and wrong candidate verification.

- [ ] **Step 2: Run RED before production files exist**

In a complete `/tmp` copy:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_toy_relation_v1_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^toy_relation_v1_contracts$' --output-on-failure
```

Expected: the target/header is absent; record this as RED, not as a passing empty test selection.

- [ ] **Step 3: Define the public contract**

Declare these exact public types and operations:

```cpp
struct ToyCyclicGroupV1 { std::uint32_t modulus, generator, public_q; };
struct ToyCoefficientStateV1 { std::uint32_t a, b, canonical_point; };
struct ToyGroupProvenanceV1 { std::string provenance_id; ToyCoefficientStateV1 coefficient_state; };
struct ToyCollisionRecordV1 {
    std::uint32_t canonical_point;
    std::string existing_provenance_id, new_provenance_id;
};
enum class ToyRelationVerificationStatusV1 { pending, verified };
struct ToyRelationRecordV1 {
    std::uint32_t modulus, coefficient_delta, value_delta, candidate_k;
    ToyRelationVerificationStatusV1 verification_status{ToyRelationVerificationStatusV1::pending};
};
ToyCoefficientStateV1 make_toy_coefficient_state_v1(const ToyCyclicGroupV1&, std::uint32_t a, std::uint32_t b);
ToyCollisionRecordV1 detect_toy_relation_collision_v1(const ToyGroupProvenanceV1&, const ToyGroupProvenanceV1&);
ToyRelationRecordV1 derive_toy_relation_v1(const ToyCyclicGroupV1&, const ToyGroupProvenanceV1&, const ToyGroupProvenanceV1&, const ToyCollisionRecordV1&);
ToyRelationRecordV1 verify_toy_relation_v1(const ToyCyclicGroupV1&, const ToyGroupProvenanceV1&, const ToyGroupProvenanceV1&, const ToyRelationRecordV1&);
```

`detect` and later operations must sort the pair by provenance ID for a stable record. `make` validates the group and requires `a,b < modulus` instead of silently normalizing caller input.

- [ ] **Step 4: Implement validation, derivation and independent verification**

Implement modular point arithmetic with `uint64_t` intermediates. Validate every group parameter before arithmetic: range, `generator`, `public_q`, and `gcd(generator, modulus) == 1`. Validate non-empty distinct IDs, current canonical-point consistency, collision equality, and record equality before derivation. Use extended Euclid to invert only the canonical `coefficient_delta`; throw exact `NonInvertibleToyRelationCoefficient` when no inverse exists. Derivation produces status `pending` without calling the verifier. Verification independently checks candidate consistency with `Q == kG mod n`, re-evaluates both points, then returns the record with `verified`; it throws `RejectedToyRelationVerification` for every mismatch.

- [ ] **Step 5: Run GREEN and regression contracts**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_toy_relation_v1_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^(toy_relation_v1_contracts|signed_affine_dag_v1_contracts|phase_dag_v2_contracts|phase_dag_v3_contracts)$' --output-on-failure
```

Expected: all focused contracts pass, including the three Task 1 Review Focus cases.

- [ ] **Step 6: Save Task 1 evidence**

Write `runs/toy-relation-v1-sdd/reviews/task-1.diff`, check added/removed patch bodies for trailing whitespace, reverse-check it with `patch --dry-run -R -p0`, and record full commands/results in `runs/toy-relation-v1-sdd/reports/task-1.md`.

### Task 2: Fixed Toy-Relation CLI Demos

**Files:**
- Create: `apps/toy_relation_v1_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_toy_relation_v1_cli.py`
- Modify: `CMakeLists.txt` — register `toy_relation_v1_cli_workflows`.

**Interfaces:**
- Consumes: all Task 1 public operations.
- Produces: `int bruce::toy_relation_v1_cli(int argc, char** argv)` and exact commands `bruce toy-relation-v1 demo collision` / `bruce toy-relation-v1 demo noninvertible`.

- [ ] **Step 1: Snapshot Task 2 inputs and write failing real-binary workflows**

Snapshot `apps/bruce_cli.cpp` and `CMakeLists.txt`. Write the Python workflow before the CLI header. Assert both exact help lines, complete successful collision report including verified candidate `17`, complete candidate-free noninvertible report with controlled non-success status, byte-stable repeated reports, and malformed invocation boundaries. Assert every reachable malformed command form remains an exit-2 error-only JSON response; the Task 2 implementation review must confirm that its sole caught core error is `NonInvertibleToyRelationCoefficient`.

- [ ] **Step 2: Run RED**

Build `bruce` and invoke the direct Python workflow. Expected: failures solely because help/routing/CLI rendering are absent; record the number of real failing test methods/assertions.

- [ ] **Step 3: Implement fixed scenarios, strict parser and renderer**

Implement exactly the two specs from the design. The collision path performs detect → derive → verify and renders group fields, ID-sorted provenances, collision fields, relation fields, and `VERIFIED`. The noninvertible path performs detect → derive, catches only `NonInvertibleToyRelationCoefficient`, and renders a status such as `NO_UNIQUE_RELATION` plus collision data without `candidate_k` or relation record. Use `json_string` for all strings. Require exactly four arguments and `demo` plus one permitted scenario. Route and help the command outside optional NanoCell/Cockpit guards.

- [ ] **Step 4: Run GREEN including optional-mod exclusion build**

Run the focused CLI/core CTests and a separate complete-copy build with both NanoCell and Cockpit disabled. Expected: default workflows pass; the command/help and all its eight-or-more real workflow assertions also pass in the disabled build.

- [ ] **Step 5: Save Task 2 evidence**

Create a reversible Task 2 diff and report commands, outputs, byte-stability evidence and broad-suite/environment limits in `runs/toy-relation-v1-sdd/reports/task-2.md`.

### Task 3: Documentation and Generated CLI Evidence

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `tests/integration/test_cli_docs.py`
- Generator-only if output changes: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: the exact Task 2 command/result contract and its verified noninvertible report.
- Produces: documentation that labels Toy-Relation V1 as an implemented local reference only after evidence passes and preserves its non-crypto/non-search boundary.

- [ ] **Step 1: Write failing documentation assertions**

Extend `tests/integration/test_cli_docs.py` with assertions scoped to the `toy-relation-v1` manual section: exact command forms, `toy-relation-v1-cli-v1`, `n=101`/`k=17`, collision/noninvertible behavior, no arbitrary parameters/files/searches/secrets, and no Phase-DAG V2/V3 extension.

- [ ] **Step 2: Run RED**

Run `cli_documentation_determinism` in the temporary build. Expected: it fails because the dedicated manual section and implemented-boundary claims do not yet exist.

- [ ] **Step 3: Update authored documents and run the unchanged generator**

Document the two fixed scenarios, canonical pair ordering, independent verification, candidate-free noninvertible result, exact JSON contract, error boundary and limitations. Update README/blueprint only with tested claims. Run `python3 docs/cli/verify_examples.py /tmp/bruce-build/bruce` in a complete temporary copy; do not extend the generator unless an explicit new scope approves it. Copy back only generator-produced protected files. If the generator does not execute the demos, state that its coverage is limited and point to the dedicated real-binary tests.

- [ ] **Step 4: Run focused and broad verification**

Run the new core/CLI/docs CTests with Signed-Affine and V2/V3 regression contracts, the local-link checker, and the full CTest suite. Report exact totals and any environment-specific Socket/Node restriction without claiming a fully green broad suite if it is not one.

- [ ] **Step 5: Save final evidence and obtain independent whole-feature review**

Save Task 3 and combined final diffs, run reverse patch/whitespace checks, write the report, and dispatch an independent read-only final reviewer over the combined package before reporting completion.

## Self-Review

- **Spec coverage:** Task 1 implements exact group algebra, canonical states, collision/relation records, modular inverse and independent verification; Task 2 adds only fixed demos; Task 3 documents exact implemented boundaries and generator limits.
- **Step clarity:** public names, argument values, error rules and report fields are fixed in Tasks 1–2; verification commands name their targets.
- **Type consistency:** Task 2 consumes precisely the types/functions declared in Task 1. `ToyRelationRecordV1` has `pending` before and `verified` after the separate verifier throughout.
- **Review Focus:** all five likely failure modes map to a named task/test step.
- **Scope:** tasks match core/CLI/docs boundaries and do not integrate the existing Signed-Affine or Phase-DAG systems.
