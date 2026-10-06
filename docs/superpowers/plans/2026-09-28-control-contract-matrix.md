# Control Contract Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make BRUCE's existing session, gate, and transfer control behavior explicitly versioned, executable, and regression-tested without changing public runtime semantics.

**Architecture:** Add a small Python acceptance checker that proves the intentionally different public error envelopes, checkpoint contracts, and no-publication behavior of the three controller families. Register it in CTest, then add ADR-015, a contract matrix, and a usage record that map every documented guarantee to that checker or an existing focused workflow test. No C++ controller, CLI option, bundle format, or result schema changes in this package.

**Tech Stack:** C++20/CMake/CTest project; Python 3 standard library acceptance scripts; existing BRUCE CLI and JSON contracts.

**Spec:** [../specs/2026-09-28-control-harmonization-design.md](../specs/2026-09-28-control-harmonization-design.md)

## Global Constraints

- Preserve all existing CLI options, exit codes, JSON envelopes, error priorities, bundle formats, and mathematical/Gate/Replay behavior.
- Do not introduce a common universal budget, runtime dispatcher, Phase-DAG, UI, parallelism, or GPU execution.
- Treat `INCOMPLETE` as a replayable domain result, never as a control failure.
- Do not overwrite output targets; aborted bounded operations must leave neither a target nor a matching temporary export file.
- Use only temporary directories and inputs within acceptance scripts; never touch active rules or production artifacts.
- The administrative `experience revoke/replace` opt-in controller is a separate follow-up package and is not implemented by this plan.

## Review Focus

- **Existing CLI invocation without a new option:** it must retain its current exit code and JSON shape; Task 1 snapshots the distinct session versus simple-error envelopes.
- **Zero checkpoint budget before output:** session, gate, and transfer paths must fail before publication; Task 1 asserts the absence of both target and `*.tmp.*` residue.
- **Native bounded research:** a valid `INCOMPLETE` result remains success/replayable rather than receiving a control error; Task 1 preserves this assertion.
- **Signal and budget collision:** each documented family retains its established priority; Task 2 maps the existing signal-focused workflows rather than inventing a new global precedence.
- **Documented controller point outside instrumentation:** users must not infer CPU-, RAM-, or universal time limits from a checkpoint count; Task 3 cross-checks the wording against the contract matrix.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `scripts/verify_control_contract_matrix.py` | Dedicated process-level checker for observable session, gate, and transfer compatibility boundaries. |
| `CMakeLists.txt` | Registers the new checker as a named CTest test. |
| `docs/adr/015-control-contract-compatibility.md` | Records the decision to version and preserve—not normalize—the current controller differences. |
| `docs/contracts/06-control-and-publication.md` | Adds the compact public contract/test matrix and links the ADR and checker. |
| `docs/implementation/184-control-contract-matrix-usage.md` | Defines scope, local verification command, and evidence boundaries for the package. |
| `docs/cli/control-overview.md` | Links users from the control overview to the exact matrix; makes no CLI promise beyond it. |

### Task 1: Add executable controller-boundary acceptance coverage

**Files:**
- Create: `scripts/verify_control_contract_matrix.py`
- Modify: `CMakeLists.txt`
- Test: `scripts/verify_control_contract_matrix.py`

**Interfaces:**
- Consumes: CLI binary path as positional `Path`; `bruce --version`, `research`, `replay`, `gate-run`, `compatibility-plan`, and `compatibility-apply` contracts.
- Produces: stdout JSON `{"contract":"bruce-control-contract-matrix-v1","accepted":true,"checks":<positive integer>}` on success; nonzero exit with an assertion message on any violated compatibility boundary.

- [ ] **Step 1: Scaffold `scripts/verify_control_contract_matrix.py` as an intentionally failing acceptance checker**

Accept the CLI binary as a positional `Path`, define `verify(binary: Path) -> dict`, and make it raise `RuntimeError("control contract matrix not implemented")`. Add the normal `argparse` entry point so that invoking the script reaches this failure.

- [ ] **Step 2: Run the scaffold to verify it fails**

Run: `python3 scripts/verify_control_contract_matrix.py build/bruce`

Expected: FAIL with `control contract matrix not implemented`.

- [ ] **Step 3: Add the controller-boundary invocations and result-shape assertions**

Create a temporary workspace and valid `train.csv`, `holdout.csv`, and `rules.txt`. Define `call(label, argv, exit_code)` that runs the supplied binary, parses stdout for success and stderr for failure, and records the JSON result. Assert these exact public distinctions:

```python
assert session_failure["execution"]["contract"] == "application-execution-failure-v2"
assert session_failure["execution"]["failed_at"] in {"EXECUTING", "SERIALIZING", "REPLAYING"}
assert gate_failure["error"].startswith("SessionCheckpointBudgetExceeded:")
assert transfer_failure["error"].startswith("TransferCheckpointBudgetExceeded:")
```

Use a research session with replay verification to generate a bounded source bundle. Exercise a zero-budget session export, a zero-budget `gate-run`, and a zero-budget `compatibility-plan`; after each output-capable failure, assert neither its target nor `target.tmp.*` exists. Include one limited native compatibility plan/apply case whose rendered result has `status == "INCOMPLETE"` and whose replay equals that result.

- [ ] **Step 4: Implement the minimal checker completion contract**

Finish `verify(binary: Path) -> dict` so it returns the exact `bruce-control-contract-matrix-v1` success object, a positive `checks` count, the CLI version response, and only temporary-artifact digests needed to diagnose a failure. Ensure every subprocess uses `cwd` set to the temporary directory and a 60-second timeout.

- [ ] **Step 5: Run the checker against the current binary**

Run: `python3 scripts/verify_control_contract_matrix.py build/bruce`

Expected: PASS and stdout JSON with `contract` equal to `bruce-control-contract-matrix-v1`, `accepted` equal to `true`, and `checks` greater than zero.

- [ ] **Step 6: Register the checker in CTest**

Add exactly one test adjacent to `control_flow_acceptance`:

```cmake
add_test(NAME control_contract_matrix COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_control_contract_matrix.py $<TARGET_FILE:bruce>)
```

- [ ] **Step 7: Verify the registered test**

Run: `ctest --test-dir build --output-on-failure -R '^control_contract_matrix$'`

Expected: one test passes.

- [ ] **Step 8: Commit Task 1**

```bash
git add scripts/verify_control_contract_matrix.py CMakeLists.txt
git commit -m "test: add control contract matrix acceptance"
```

If the workspace is not a Git repository, record the missing Git metadata in the handoff instead of attempting a commit.

### Task 2: Record the compatibility decision and public matrix

**Files:**
- Create: `docs/adr/015-control-contract-compatibility.md`
- Modify: `docs/contracts/06-control-and-publication.md`
- Modify: `docs/cli/control-overview.md`
- Test: `scripts/verify_control_contract_matrix.py`, existing signal-focused workflows

**Interfaces:**
- Consumes: Task 1 checker name `control_contract_matrix`; existing CTest names `result_assessment_workflows`, `experience_transfer_workflows`, `compatibility_transfer_workflows`, and `session_work_budget_workflows`.
- Produces: ADR-015 and a matrix whose controller-family rows identify contract version, budget unit, signal/budget order, error envelope, publication boundary, and CTest evidence.

- [ ] **Step 1: Write the failing documentation assertions into the new ADR outline**

Create ADR-015 with explicit sections `Status`, `Context`, `Decision`, `Unveränderte Semantik`, `Consequences`, and `Evidence`. State the falsifiable decision: no controller family is normalized in this package; an undocumented change to error shape or checkpoint order is a compatibility regression.

- [ ] **Step 2: Run the contract checker as the behavioral baseline**

Run: `ctest --test-dir build --output-on-failure -R '^(control_contract_matrix|result_assessment_workflows|experience_transfer_workflows|compatibility_transfer_workflows|session_work_budget_workflows)$'`

Expected: PASS. The command supplies evidence for the document; it does not test prose itself.

- [ ] **Step 3: Complete ADR-015 and add the matrix to `docs/contracts/06-control-and-publication.md`**

Add one table row each for session, gate, and transfer. Use the current, distinct contracts exactly:

| Family | Version/unit | Signal and budget order | Failure envelope |
| --- | --- | --- | --- |
| Session | `session-checkpoints-v1` | preserve path-specific existing order | budgetabhängig: `application-execution-failure-v2` für Sitzungsprüfpunkte, `v3` für Serialisierungsbytes |
| Gate | gate inspection checkpoints | signal before counter consumption | simple `{"error":"…"}` |
| Transfer | `transfer-checkpoints-v1` | signal, then budget check, then increment | simple `{"error":"…"}` |

For each row, name the Task 1 CTest check and the existing workflow tests that cover signal handling. State that file reads, final exports, atomically executed domain operations, process time, and memory are not universally counted.

- [ ] **Step 4: Link the matrix from the CLI control overview**

Add one concise link in the control overview's introduction or controller section. It must say that the matrix documents existing per-entry behavior and must not imply a uniform runtime or expand CLI scope.

- [ ] **Step 5: Verify document links and targeted evidence**

Run: `python3 docs/cli/verify_examples.py build/bruce`

Expected: PASS; its generated CLI-document checks accept the added internal link. Then rerun the Task 1 CTest command from Step 2 and expect PASS.

- [ ] **Step 6: Commit Task 2**

```bash
git add docs/adr/015-control-contract-compatibility.md docs/contracts/06-control-and-publication.md docs/cli/control-overview.md
git commit -m "docs: define control contract compatibility"
```

If the workspace is not a Git repository, record the missing Git metadata in the handoff instead of attempting a commit.

### Task 3: Publish the implementation scope and run the full verification set

**Files:**
- Create: `docs/implementation/184-control-contract-matrix-usage.md`
- Test: `scripts/verify_control_contract_matrix.py`, full CTest suite, CLI example verifier

**Interfaces:**
- Consumes: ADR-015, the matrix in `docs/contracts/06-control-and-publication.md`, CTest test `control_contract_matrix`.
- Produces: an implementation record that names exact changed runtime scope as **none**, exact added executable evidence, the local commands, and remaining work (`experience revoke/replace` opt-in design).

- [ ] **Step 1: Write the scope assertions for the implementation record**

State that this package changes no C++ controller, CLI argument, bundle version, result schema, or mathematical/Gate/Replay semantics. List the new checker and the matrix/ADR as its only deliverables. Identify `experience revoke/replace` opt-in control as intentionally deferred.

- [ ] **Step 2: Verify the targeted acceptance evidence**

Run: `ctest --test-dir build --output-on-failure -R '^(control_contract_matrix|control_flow_acceptance|result_assessment_workflows|experience_transfer_workflows|compatibility_transfer_workflows|session_work_budget_workflows)$'`

Expected: all selected tests pass.

- [ ] **Step 3: Complete the record with actual command outcomes**

Record the build type, test command, number of passing selected tests, and verifier outcome only after observing them. Do not copy historical counts or claim remote verification unless it is actually run.

- [ ] **Step 4: Run the full local verification**

Run: `cmake --build build && ctest --test-dir build --output-on-failure && python3 docs/cli/verify_examples.py build/bruce`

Expected: build succeeds, all configured CTest tests pass, and the CLI example verifier exits zero.

- [ ] **Step 5: Commit Task 3**

```bash
git add docs/implementation/184-control-contract-matrix-usage.md
git commit -m "docs: record control contract acceptance"
```

If the workspace is not a Git repository, record the missing Git metadata in the handoff instead of attempting a commit.

## Plan Self-Review

- **Spec coverage:** Task 1 supplies executable compatibility evidence; Task 2 supplies the versioned ADR and complete public matrix; Task 3 supplies the scope/evidence record and full verification. The deferred opt-in Experience migration is intentionally excluded because the spec leaves its budget unit and exact adapter undecided.
- **Step scan:** Each implementation step names a single file change or one command with an observable result; no task asks an implementer to select an undefined controller semantics.
- **Type consistency:** The only introduced machine interface is `verify(binary: Path) -> dict` and its exact `bruce-control-contract-matrix-v1` success contract, used consistently in Tasks 1–3.
- **Review focus:** The five high-risk inputs are mapped to Task 1's assertions, Task 2's existing-signal-test matrix, and Task 3's full regression run.
- **Proportion:** The plan specifies contracts, test values, and file ownership without prescribing implementation bodies.
