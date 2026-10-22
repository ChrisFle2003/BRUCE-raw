# Signed-Affine-DAG V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local C++20 Signed-Affine-DAG V1 reference library, two fixed CLI demonstrations, and executable evidence that equal values retain separate provenance and signed paths cancel exactly.

**Architecture:** Add a small `bruce_core` library unit that validates and executes an acyclic single-predecessor affine execution graph. It produces a sorted semantic-state projection keyed by `std::int64_t` result values while preserving every path record. A new always-available CLI subcommand constructs only the two fixed in-memory examples and emits stable JSON; it neither parses graph files nor writes runs.

**Tech Stack:** C++20, CMake, CTest, Python 3 `unittest`, existing BRUCE JSON and error utilities.

**Spec:** `docs/superpowers/specs/2026-10-13-signed-affine-dag-v1-design.md`

## Global Constraints

- Keep V1 under `include/bruce/signed_affine_dag/` and `src/signed_affine_dag/`, linked only through `bruce_core`; add no external dependency.
- Use only `std::int64_t` values and checked affine arithmetic/weight accumulation; never publish a partial result after overflow.
- Fixed operators are exactly `add-two-v1` (`x + 2`), `double-v1` (`2*x`), and `subtract-one-v1` (`x - 1`).
- V1 accepts only acyclic execution graphs with one optional predecessor, non-empty unique IDs, and path weights `-1` or `+1`.
- Preserve deterministic execution by lexicographic ready node ID; sort semantic states by value and their paths by node ID.
- Do not modify Phase-DAG V2/V3 contracts, add a free-form graph/formula interface, persist data, or add network, GPU, Cockpit, plugin, or cryptographic behaviour.
- Use a complete temporary copy under `/tmp` for configure/build/test runs. Do not install packages.
- `docs/cli/source-index.md`, `docs/cli/examples.json`, and `docs/cli/validation.md` may change only through `docs/cli/verify_examples.py` in that temporary copy.
- The workspace has no usable Git worktree. At each task boundary save an explicit file snapshot and checked diff under `runs/signed-affine-dag-v1-sdd/`; do not claim a commit.

## Review Focus

- **Task 1:** an empty graph or graph without a source must fail before execution and must not return an empty successful result.
- **Task 1:** two simultaneously ready nodes whose input order is reversed must still yield node-ID-sorted provenance and byte-stable results.
- **Task 1:** multiplication and signed weight accumulation at both `int64_t` bounds must return `SignedAffineDagArithmeticOverflow` without partial output.
- **Task 2:** malformed demo invocations (missing action, unknown scenario, or extra argument) must return the documented controlled usage/error JSON and not fall through to a legacy command.
- **Task 3:** CLI help, manual, generated option index, and generated examples must all show the same two fixed demos and must not imply graph-file input or persistence.

---

## File Structure

| Path | Responsibility |
|---|---|
| `include/bruce/signed_affine_dag/signed_affine_dag_v1.hpp` | Public V1 data contract and validation/execution signatures. |
| `src/signed_affine_dag/signed_affine_dag_v1.cpp` | Fixed operator lookup, graph validation, deterministic execution, semantic-state merge, checked arithmetic. |
| `tests/unit/signed_affine_dag_v1_tests.cpp` | Core reference, ordering, validation, cycle and overflow contracts. |
| `apps/signed_affine_dag_v1_cli.hpp` | Fixed `merge`/`cancellation` specs, stable JSON renderer, and argument boundary. |
| `apps/bruce_cli.cpp` | Always-available help text, include, and top-level command dispatch. |
| `tests/integration/test_signed_affine_dag_v1_cli.py` | Executable CLI JSON and error-boundary workflow. |
| `README.md` | Short capability/status entry. |
| `docs/architecture/03-blueprint-consolidation.md` | Implemented boundary and explicit non-goals. |
| `docs/cli/README.md`, `docs/cli/commands.md` | CLI availability, both demos, stable result schema and limits. |
| `tests/integration/test_cli_docs.py` | Documentation contract expectations for the new command. |

### Task 1: Signed-Affine-DAG V1 Core

**Files:**
- Create: `include/bruce/signed_affine_dag/signed_affine_dag_v1.hpp`
- Create: `src/signed_affine_dag/signed_affine_dag_v1.cpp`
- Create: `tests/unit/signed_affine_dag_v1_tests.cpp`
- Modify: `CMakeLists.txt` — add the source to `bruce_core`, add `bruce_signed_affine_dag_v1_tests`, and register `signed_affine_dag_v1_contracts`.

**Interfaces:**
- Consumes: `bruce::Error` from `bruce/core/types.hpp`, C++20 containers and `std::int64_t`.
- Produces: `AffineOperatorV1`, `SignedAffineExecutionNodeV1`, `SignedAffineDagSpecV1`, `SignedAffinePathRecordV1`, `SignedAffineSemanticStateV1`, `SignedAffineDagResultV1`, `validate_signed_affine_dag_v1(const SignedAffineDagSpecV1&)`, and `execute_signed_affine_dag_v1(const SignedAffineDagSpecV1&)` for Task 2.

- [ ] **Step 1: Snapshot Task 1 inputs and write failing core contracts**

Create `runs/signed-affine-dag-v1-sdd/base/task-1/` snapshots of `CMakeLists.txt`; add `tests/unit/signed_affine_dag_v1_tests.cpp` first. Cover:

```cpp
const auto result = bruce::execute_signed_affine_dag_v1(merge_spec());
require(state(result, 4).paths.size() == 2, "merge retains both paths");
require(state(result, 4).weight_balance == 2, "merge weight balance");
```

Add a cancellation case for value `4` with weights `+1` and `-1`, exact expected node-ID order, deterministic repeated result comparison, no-source/empty graph rejection, duplicate ID, unknown predecessor, unknown operator, invalid weight, source-field errors, cycle, affine multiplication overflow, and weight-sum overflow.

- [ ] **Step 2: Run the new target to confirm RED**

Run from a complete `/tmp` copy:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_signed_affine_dag_v1_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^signed_affine_dag_v1_contracts$' --output-on-failure
```

Expected: build/test cannot pass before the public header, source, and CMake target exist.

- [ ] **Step 3: Define the public V1 contract**

In `signed_affine_dag_v1.hpp`, declare these exact types and signatures:

```cpp
struct AffineOperatorV1 { std::string_view id; std::int64_t a, b; };
struct SignedAffineExecutionNodeV1 {
    std::string node_id;
    std::optional<std::string> predecessor_node_id;
    std::string operator_id;
    std::optional<std::int64_t> source_value;
    std::int64_t path_weight{};
};
struct SignedAffineDagSpecV1 { std::vector<SignedAffineExecutionNodeV1> nodes; };
struct SignedAffinePathRecordV1 {
    std::string node_id, predecessor_node_id, operator_id;
    std::int64_t input_value{}, output_value{}, path_weight{};
};
struct SignedAffineSemanticStateV1 {
    std::int64_t value{}, weight_balance{};
    std::vector<SignedAffinePathRecordV1> paths;
};
struct SignedAffineDagResultV1 { std::vector<SignedAffineSemanticStateV1> states; };
void validate_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec);
SignedAffineDagResultV1 execute_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec);
```

A source has `predecessor_node_id == std::nullopt`, empty `operator_id`, present `source_value`, and path weight `+1` or `-1`. A derived node has the inverse field combination.

- [ ] **Step 4: Implement strict validation and deterministic execution**

Implement only the fixed operator catalog in `signed_affine_dag_v1.cpp`. Validate the complete graph before calculating any result; use Kahn traversal with lexicographic ready IDs. Use private checked-add and checked-multiply helpers before computing `a*x+b` and before accumulating a state balance. Record each node’s full provenance, merge by output value, then sort states by `value` and paths by `node_id`. Raise the exact error codes from the spec, including `InvalidSignedAffineDagSource` for no-source specs and invalid source/derived field combinations.

- [ ] **Step 5: Run focused contracts to confirm GREEN**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_signed_affine_dag_v1_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^signed_affine_dag_v1_contracts$' --output-on-failure
```

Expected: all core contracts pass, including the five Review Focus cases owned by Task 1.

- [ ] **Step 6: Record the Task 1 review artifact**

Create `runs/signed-affine-dag-v1-sdd/reviews/task-1.diff` against the Task 1 snapshot, run `diff --check` on it, and save commands/results in `runs/signed-affine-dag-v1-sdd/reports/task-1.md`. Do not commit because Git metadata is unavailable.

### Task 2: Fixed CLI Demonstrations

**Files:**
- Create: `apps/signed_affine_dag_v1_cli.hpp`
- Modify: `apps/bruce_cli.cpp` — include, help lines, and command routing.
- Create: `tests/integration/test_signed_affine_dag_v1_cli.py`
- Modify: `CMakeLists.txt` — register `signed_affine_dag_v1_cli_workflows` with the built `bruce` path.

**Interfaces:**
- Consumes: Task 1’s `SignedAffineDagSpecV1` and `execute_signed_affine_dag_v1`.
- Produces: `int bruce::signed_affine_dag_v1_cli(int argc, char** argv)` and the always-available commands `bruce signed-affine-dag-v1 demo merge` and `bruce signed-affine-dag-v1 demo cancellation`.

- [ ] **Step 1: Snapshot Task 2 inputs and write failing CLI workflow tests**

Snapshot `apps/bruce_cli.cpp` and `CMakeLists.txt` into `runs/signed-affine-dag-v1-sdd/base/task-2/`. Create the Python workflow test first. Assert both exact help lines; parse successful JSON; assert the merge state `4` has two sorted paths and balance `2`; assert cancellation’s matching state has two paths and balance `0`. Assert missing action, unknown scenario, and extra arguments return exit `2` with only an `error` JSON field.

- [ ] **Step 2: Run the CLI workflow to confirm RED**

Build `bruce`, then run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^signed_affine_dag_v1_cli_workflows$' --output-on-failure
```

Expected: FAIL because the command, help entries, target registration, and renderer do not yet exist.

- [ ] **Step 3: Implement fixed demo construction and stable JSON rendering**

Create `apps/signed_affine_dag_v1_cli.hpp`. Implement only the two in-memory specs defined in the design. Render:

```text
{"contract":"signed-affine-dag-v1-cli-v1","scenario":...,"states":[...]}
```

Each path object renders `node_id`, `predecessor_node_id` (`null` for a source), `operator_id`, `input_value`, `output_value`, and `path_weight`; each state renders `value`, `weight_balance`, and `paths`. Use existing `json_string` helpers for all strings. Reject every command form except exactly `demo merge` or `demo cancellation` with controlled `bruce::Error` messages. Register this CLI unconditionally in `apps/bruce_cli.cpp`, because the core is not a NanoCell mod.

- [ ] **Step 4: Run focused CLI tests to confirm GREEN**

Run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^(signed_affine_dag_v1_contracts|signed_affine_dag_v1_cli_workflows)$' --output-on-failure
```

Expected: both targets pass; malformed invocation coverage satisfies the Task 2 Review Focus item.

- [ ] **Step 5: Record the Task 2 review artifact**

Create `runs/signed-affine-dag-v1-sdd/reviews/task-2.diff` against the Task 2 snapshot, run `diff --check`, and save commands/results in `runs/signed-affine-dag-v1-sdd/reports/task-2.md`. Do not commit.

### Task 3: Documentation and Generated CLI Evidence

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify only through generator if changed: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`
- Modify: `tests/integration/test_cli_docs.py`

**Interfaces:**
- Consumes: Task 2’s exact help lines, commands, and `signed-affine-dag-v1-cli-v1` result contract.
- Produces: documentation that identifies the reference prototype as implemented only after tests pass, explains its two fixed demos, and states its non-goals.

- [ ] **Step 1: Write failing documentation-contract assertions**

Extend `tests/integration/test_cli_docs.py` first. Assert the manual names both exact commands, describes only fixed in-memory demonstrations, includes the result contract name, and says it does not accept graph files, persist runs, or extend Phase-DAG V2/V3. Add this new test to the existing documentation test invocation.

- [ ] **Step 2: Run documentation contracts to confirm RED**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^cli_documentation_determinism$' --output-on-failure
```

Expected: FAIL because the new command and boundary statements are absent from the manual.

- [ ] **Step 3: Update authored documentation and regenerate derived artifacts**

Update the README and architecture blueprint only with implemented, tested claims. Update the CLI manual overview and `commands.md` with the exact syntax, fixed scenarios, stable report shape, deterministic ordering, overflow/error behaviour, and non-goals. Then, in a complete temporary copy, run:

```sh
python3 docs/cli/verify_examples.py /tmp/bruce-build/bruce
```

Apply only generator-produced changes for the three protected files; never edit those files manually. Preserve the distinction between the small implemented V1 prototype and the larger Affine Phase-DAG concept.

- [ ] **Step 4: Run documentation, link, and relevant regression verification**

Run:

```sh
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^(signed_affine_dag_v1_contracts|signed_affine_dag_v1_cli_workflows|cli_documentation_determinism)$' --output-on-failure
python3 scripts/check_local_doc_links.py
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
```

Expected: focused contracts all pass. Report the complete CTest count or any environment-specific failure separately; do not imply the whole project is correct from a focused run.

- [ ] **Step 5: Record documentation and final review artifacts**

Create `runs/signed-affine-dag-v1-sdd/reviews/task-3.diff`, combine all task diffs as `final.diff`, run `diff --check`, and save verification output plus generated-artifact evidence in `runs/signed-affine-dag-v1-sdd/reports/task-3.md`. Request an independent read-only final review of the combined diff before reporting completion.

## Self-Review

- **Spec coverage:** Task 1 covers isolation, fixed affine operators, graph separation, provenance, cancellation, deterministic ordering, errors, and overflow. Task 2 covers both fixed demos and stable CLI JSON. Task 3 covers all documentation and generated CLI artifacts.
- **Step scan:** Each code step pins concrete names, types, values, result fields, or command forms; all verification commands name their CTest targets.
- **Type consistency:** Task 2 consumes only `SignedAffineDagSpecV1` and `execute_signed_affine_dag_v1` from Task 1. The CLI function and JSON contract have one spelling throughout.
- **Review Focus:** Each of the five identified high-risk input classes is assigned to a named task and test step.
- **Proportion:** Three independently reviewable tasks map to the core, CLI, and docs boundaries without prescribing implementation bodies beyond the contract.
