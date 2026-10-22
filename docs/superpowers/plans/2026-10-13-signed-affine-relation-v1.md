# Signed-Affine-Relation V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local, deterministic C++20 bridge that derives affine path coefficients from a Signed-Affine-DAG V1 trace and verifies their real modular relation through Toy-Relation V1.

**Architecture:** A new `signed_affine_relation_v1` core API executes the existing V1 DAG once, recursively reconstructs `m × s + c` from its path records, then converts the two selected paths into Toy-Relation provenances. The bridge delegates collision detection, relation derivation and independent verification to the existing Toy-Relation V1 API. A fixed CLI only exposes the known public merge witness.

**Tech Stack:** C++20, CMake/Ninja/CTest, existing `bruce_core`, Python `unittest`, existing CLI JSON helpers and documentation generator.

**Spec:** `docs/superpowers/specs/2026-10-13-signed-affine-relation-v1-design.md`

## Global Constraints

- Use C++20 and introduce no package or runtime dependency.
- Do not change public Signed-Affine-DAG V1 or Toy-Relation V1 structs, functions, CLI commands, or JSON reports.
- Execute the Signed-Affine-DAG V1 exactly once per bridge request; do not duplicate its executor or relation arithmetic.
- Require exactly one executed source; map `G=1`, `Q=source_value mod n`, `a=offset mod n`, `b=multiplier mod n`.
- Use checked `int64_t` composition and back-substitution; never publish a partial bridge result on error.
- Preserve requested `left`/`right` result provenance, but delegate canonical collision ordering and every relation calculation/verification to Toy-Relation V1.
- The CLI accepts only `bruce signed-affine-relation-v1 demo merge`; no free graph, formula, group, file, search, persistence, network, GPU, plugin, scheduler, V2/V3 or Cockpit interface.
- State the local toy/non-cryptographic boundary in every product-facing document that introduces this command.
- Update generated `docs/cli/source-index.md`, `docs/cli/examples.json` and `docs/cli/validation.md` only by running `docs/cli/verify_examples.py` in a complete native temporary copy.
- Use `scripts/with-host-deps.sh` for CMake, build and tests. The Git metadata is unavailable: use task snapshots plus reversible unified diffs and changed-line whitespace checks instead of commits.

## Review Focus

- A V1 result groups records by semantic output rather than topological order; Task 1 must recursively follow `predecessor_node_id` and prove a deeper, cross-state path reconstructs correctly.
- A zero source can keep V1 value arithmetic valid while repeated `double-v1` overflows the multiplier; Task 1 must prove `SignedAffineRelationCoefficientOverflow` with no result.
- Negative source values and offsets must normalize into `[0,n)` before constructing Toy coefficients; Task 1 must assert exact normalized values and a verified relation.
- Swapping selected nodes must preserve requested `left`/`right` output while Toy collision IDs and relation deltas remain canonical; Task 1 must assert both views.
- Extra, omitted, case-altered, file-like and parameter-like CLI tokens must be error-only, exit `2`, and never create a partial JSON report; Task 2 must exercise each class.

---

### Task 1: Core Signed-Affine Relation Contract

**Files:**
- Create: `include/bruce/signed_affine_relation/signed_affine_relation_v1.hpp`
- Create: `src/signed_affine_relation/signed_affine_relation_v1.cpp`
- Create: `tests/unit/signed_affine_relation_v1_tests.cpp`
- Modify: `CMakeLists.txt` (add the source to `bruce_core`, add `bruce_signed_affine_relation_v1_tests`, register `signed_affine_relation_v1_contracts`)

**Interfaces:**
- Consumes: `SignedAffineDagSpecV1`, `execute_signed_affine_dag_v1`, `ToyCyclicGroupV1`, `ToyGroupProvenanceV1`, `detect_toy_relation_collision_v1`, `derive_toy_relation_v1`, and `verify_toy_relation_v1`.
- Produces:

```cpp
struct SignedAffineRelationRequestV1 {
    SignedAffineDagSpecV1 spec;
    std::uint32_t modulus;
    std::string left_node_id;
    std::string right_node_id;
};

struct SignedAffineDerivedPathV1 {
    std::string node_id;
    std::string source_node_id;
    std::int64_t source_value;
    std::int64_t multiplier;
    std::int64_t offset;
    std::int64_t output_value;
    ToyGroupProvenanceV1 toy_provenance;
};

struct SignedAffineRelationResultV1 {
    ToyCyclicGroupV1 group;
    SignedAffineDerivedPathV1 left;
    SignedAffineDerivedPathV1 right;
    ToyCollisionRecordV1 collision;
    ToyRelationRecordV1 relation;
};

SignedAffineRelationResultV1 derive_signed_affine_relation_v1(
    const SignedAffineRelationRequestV1& request);
```

- [ ] **Step 1: Write focused failing core contracts**

Create the unit executable in the established function-table style. Add an exact merge contract for source `2`, nodes `a-add`/`z-double`, `n=101`: forms `(1,2)` and `(2,0)`, Toy states `(2,1,4)` and `(0,2,4)`, canonical collision `a-add`/`z-double`, and verified candidate `2`. Add a deeper chain that demonstrates recursive predecessor composition despite result-state ordering.

Add tests for reversed selection, signed normalization, source-count and selection failures, unequal outputs, known `n=15` noninvertibility, coefficient overflow at source zero with repeated doubles, stable repeated output, and no-mutation sentinel behavior after every rejection.

- [ ] **Step 2: Configure and run the focused core target to prove RED**

Run:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-signed-affine-relation-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-signed-affine-relation-build --target bruce_signed_affine_relation_v1_tests
```

Expected: configuration or compilation fails because the new contract/source/target does not yet exist; record the missing symbol or target evidence.

- [ ] **Step 3: Implement the exact core API and CMake target**

Implement `derive_signed_affine_relation_v1(const SignedAffineRelationRequestV1&)` in the new source. Execute V1 once, collect each unique `SignedAffinePathRecordV1` by `node_id`, require exactly one source, and recursively memoize forms from predecessor to child. Verify each trace link and recompute each recorded output using checked arithmetic. Normalize signed `source_value`, multiplier, and offset modulo `request.modulus` before creating the Toy group and two Toy provenances. Require different existing selected node IDs and equal selected V1 outputs; then call Toy collision, derive, and verify functions in that order. Return only the verified record.

Define exactly the errors from the spec for bridge-only conditions; propagate existing V1 and Toy errors unchanged. Register the source and test target in CMake using the current Signed-Affine/Toy test pattern and `-Wall -Wextra -Werror`.

- [ ] **Step 4: Run the focused core contracts to prove GREEN**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-signed-affine-relation-build --target bruce_signed_affine_relation_v1_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-signed-affine-relation-build -R '^signed_affine_relation_v1_contracts$' --output-on-failure
```

Expected: exit `0`; every new contract passes.

- [ ] **Step 5: Run adjacent core regressions and record the recovery checkpoint**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-signed-affine-relation-build -R '^(signed_affine_relation_v1_contracts|signed_affine_dag_v1_contracts|toy_relation_v1_contracts|phase_dag_v2_contracts|phase_dag_v3_contracts)$' --output-on-failure
```

Expected: all selected tests pass. Save a source snapshot, a reversible unified diff, and the changed-line trailing-whitespace result under `runs/signed-affine-relation-v1-sdd/`; this is the required replacement for the unavailable commit checkpoint.

### Task 2: Fixed CLI Witness and Real-Binary Contract

**Files:**
- Create: `apps/signed_affine_relation_v1_cli.hpp`
- Create: `tests/integration/test_signed_affine_relation_v1_cli.py`
- Modify: `apps/bruce_cli.cpp` (include and unconditional route)
- Modify: `CMakeLists.txt` (register `signed_affine_relation_v1_cli_workflows` beside the existing fixed-demo workflows)

**Interfaces:**
- Consumes: `SignedAffineRelationRequestV1` and `derive_signed_affine_relation_v1(const SignedAffineRelationRequestV1&)` from Task 1.
- Produces: `int signed_affine_relation_v1_cli(int argc, char** argv)` accepting only `demo merge`, with stdout contract `signed-affine-relation-v1-cli-v1`.

- [ ] **Step 1: Write failing real-binary workflow tests**

Write a Python `unittest` modeled on the current fixed-demo workflow files. Its positive test asserts the complete decoded JSON document: source `2`, group `{modulus:101,generator:1,public_q:2}`, requested `a-add` then `z-double` forms and Toy states, canonical collision, and verified relation including `candidate_k:2`. Require empty stderr, final newline and exact byte stability across repeated calls. Add help-line, omitted, unknown, case-altered, extra, `--modulus`, `--input`, `--search`, and file-like scenario tests that require empty stdout and error JSON on stderr with exit `2`.

- [ ] **Step 2: Run the workflow test to prove RED**

Build `bruce`, then run the new script against the binary:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-signed-affine-relation-build --target bruce
python3 tests/integration/test_signed_affine_relation_v1_cli.py /tmp/bruce-signed-affine-relation-build/bruce
```

Expected: the new command is absent or the required report schema does not exist; retain the failure output.

- [ ] **Step 3: Implement the fixed CLI and route**

Create `signed_affine_relation_v1_cli.hpp` following the existing inline CLI adapter pattern. Build exactly the public V1 merge spec, issue request `{spec, 101, "a-add", "z-double"}`, render all fields asserted by the workflow in deterministic key and array order, then emit exactly one trailing newline. Reject every other argument tuple with `Error("Usage: bruce signed-affine-relation-v1 demo merge")`. Include the header and route `signed-affine-relation-v1` before optional module routes so it is available with NanoCell/Cockpit disabled. Register the Python workflow CTest.

- [ ] **Step 4: Run default and disabled-module CLI contracts to prove GREEN**

Run the real workflow directly and via CTest in the default build. Then configure `/tmp/bruce-signed-affine-relation-off-build` with `-DBRUCE_BUILD_MOD_NANOCELL=OFF -DBRUCE_BUILD_MOD_COCKPIT=OFF`, build `bruce`, and run the identical workflow against that binary.

Expected: every invocation passes; both successful JSON byte streams are identical.

- [ ] **Step 5: Run fixed-demo regressions and record the recovery checkpoint**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-signed-affine-relation-build -R '^(signed_affine_relation_v1_contracts|signed_affine_relation_v1_cli_workflows|signed_affine_dag_v1_contracts|signed_affine_dag_v1_cli_workflows|toy_relation_v1_contracts|toy_relation_v1_cli_workflows)$' --output-on-failure
```

Expected: all selected tests pass. Save the Task 2 snapshot, reversible diff, byte-comparison evidence and whitespace check in `runs/signed-affine-relation-v1-sdd/`.

### Task 3: Documentation, Generated CLI Evidence and Acceptance

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `tests/integration/test_cli_docs.py`
- Generated only through verifier: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: Task 2’s fixed command, exact JSON schema, exit behavior and source/test evidence.
- Produces: product documentation that labels only the new proven reference as **IMPLEMENTIERT UND BELEGT**, preserves the broader roadmap as **DOKUMENTIERT / KONZEPT**, and names the generator coverage limit if it does not run the fixed demo.

- [ ] **Step 1: Add failing documentation assertions**

Extend `test_cli_docs.py` with a dedicated method that isolates the new `signed-affine-relation-v1` manual section and requires the exact command, contract name, `m × s + c`, source `2`, `G=1`, `Q=2`, `n=101`, both path IDs, modulo mapping, canonical collision, verified `candidate_k=2`, fixed in-memory boundary, exit `0`/`2`, reference to the real workflow, generator command, and the explicit no-search/no-secret/no-DLP/no-real-curve/no-V2/V3 boundary.

- [ ] **Step 2: Run the documentation method to prove RED**

Run the single documentation method against the Task 2 binary:

```sh
python3 tests/integration/test_cli_docs.py CliDocumentationTests.test_signed_affine_relation_demo_manual_matches_implemented_scope /tmp/bruce-signed-affine-relation-build/bruce
```

Expected: failure because the dedicated manual section and asserted contract text are absent.

- [ ] **Step 3: Author scoped documentation and regenerate protected artifacts correctly**

Document the new verified local reference in README, the architecture blueprint, CLI overview and command manual. State that V1 modules remain unchanged; explain that only a public fixed source derives the public candidate. Do not claim generic relation search, cryptographic value, or a general scheduler.

Copy the complete repository to a native `/tmp` path excluding ignored local work directories, run `docs/cli/verify_examples.py` there via `scripts/with-host-deps.sh`, inspect its changed protected files, and copy back only generated output that the verifier produced. Do not hand-edit those three files. If no new example executes the demo, document that exact limit and cite the real CLI workflow test.

- [ ] **Step 4: Run documentation, generator, link and focused acceptance checks**

Run the full documentation test executable, then the relevant CTests:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-signed-affine-relation-build -R '^(cli_documentation_determinism|signed_affine_relation_v1_contracts|signed_affine_relation_v1_cli_workflows|signed_affine_dag_v1_contracts|signed_affine_dag_v1_cli_workflows|toy_relation_v1_contracts|toy_relation_v1_cli_workflows|phase_dag_v2_contracts|phase_dag_v3_contracts)$' --output-on-failure
```

Run the repository’s local Markdown-link checker if present. Expected: all selected checks pass, generated artifacts agree with the temporary-copy verifier, and no documentation link is broken.

- [ ] **Step 5: Run standard full verification and record final recovery evidence**

Run the AGENTS.md configure/build/full-CTest sequence in a new `/tmp` build directory. Report the command, exit code and any socket-limited Cockpit result separately; do not call a socket-sandbox failure a product regression without a controlled rerun. Create a final reversible diff, reverse-apply dry run, changed-line whitespace check and snapshot manifest under `runs/signed-affine-relation-v1-sdd/`.

## Plan Self-Review

- **Spec coverage:** Task 1 implements every mathematical, contract, source-count, checked-arithmetic, canonicalization, error and no-partial-result requirement. Task 2 owns route, report and disabled-module availability. Task 3 owns only truthfully scoped documentation and generated-artifact handling.
- **Step scan:** Every task starts RED, implements an exact named interface, proves GREEN, runs adjacent regressions and retains a recovery checkpoint. The plan has no placeholder decision or unspecified public field.
- **Type consistency:** Tasks 2 and 3 consume the `SignedAffineRelationRequestV1`, `SignedAffineRelationResultV1` and `derive_signed_affine_relation_v1` names defined in Task 1; the CLI contract name is consistently `signed-affine-relation-v1-cli-v1`.
- **Review focus coverage:** Recursive non-topological trace reconstruction, coefficient-only overflow, signed normalization and selection/canonicalization belong to Task 1; hostile public argument shapes belong to Task 2.
- **Proportion:** Three independently reviewable deliverables map directly to core semantics, public interface and documentation/acceptance. No plan task introduces work outside the approved local-reference scope.
