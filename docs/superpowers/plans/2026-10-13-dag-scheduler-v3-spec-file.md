# DAG-Scheduler V3 Spec File Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide `bruce dag-scheduler-v3 execute SPEC`, which reads one bounded, canonical DAG Spec file and executes it through the existing DAG-Scheduler V2 core.

**Architecture:** V3 is a header-only CLI/parser adapter in `apps/`. It decodes and validates its external file format, maps each node onto `DagSchedulerSpecV2` plus one of three fixed callbacks, and calls `execute_dag_scheduler_v2` unchanged. A V3-specific renderer exposes stable metadata and V2 receipts/trace while never serializing input or output bytes.

**Tech Stack:** C++20, existing BRUCE core (`LocalFiles`, SHA-256, JSON utility and DAG-Scheduler V2), CMake/CTest, Python `unittest` process integration tests, Markdown documentation.

**Spec:** `docs/superpowers/specs/2026-10-13-dag-scheduler-v3-spec-file-design.md`

## Global Constraints

- Do not modify `include/bruce/dag_scheduler/dag_scheduler_v2.hpp` or `src/dag_scheduler/dag_scheduler_v2.cpp`; V3 is an adapter over its existing validation, execution, budgets and receipts.
- The sole public V3 route is exactly `bruce dag-scheduler-v3 execute SPEC`; wrong arguments emit only `{"error":"Usage: bruce dag-scheduler-v3 execute SPEC"}\n` on stderr and exit 2.
- Read a maximum of 65,536 bytes. Specs are ASCII without NUL or CR, use one optional final LF, and use exactly one blank line between a fixed globals block and fixed node blocks.
- Require the exact `BRUCE_DAG_SCHEDULER_SPEC_V3` header; no comments, whitespace trimming, unknown or duplicate fields, implicit defaults, noncanonical node/dependency order, uppercase hex or malformed `request-hex`.
- Support only `literal-v1` (zero dependencies), `append-v1` (one), and `join-v1` (two through fifteen); use outcome codes `LITERAL_READY`, `APPENDED_READY`, `JOINED_READY`.
- The V3 report contract is `dag-scheduler-v3-cli-v1`, with `action`, `spec_version`, raw-byte `spec_digest`, truthful `scheduler_version`, DAG budgets, V2 receipts and V2 trace. It must contain no raw request/payload bytes or their hex text.
- Do not hand-edit generated `docs/cli/source-index.md`, `docs/cli/examples.json` or `docs/cli/validation.md`; use the existing verifier only in a full, native `/tmp` copy without spaces.
- Preserve V1, V2 and Phase-DAG behavior. Keep V3 unconditional: it must work with NanoCell/Cockpit OFF.
- The user selected **Subagent-driven** execution. Use one implementation worker and a fresh reviewer per task after this plan has been approved.

## Review Focus

- A valid file with `request-hex=00` must yield correct NUL-byte digests/sizes while neither raw binary nor source hex appears in stdout; Task 1’s success and leak assertions own this.
- Concatenating valid V2-sized inputs can exceed 65,536 bytes; Task 2’s focused adapter test owns the overflow-safe rejection before a large output is produced.
- Alternate line endings, leading/trailing/double separators, and missing final newline can accidentally bypass a line parser; Task 1’s parser matrix owns these exact byte cases.
- Dependencies can be valid IDs but not canonical or cyclic; Task 1 separately pins parser-order rejection and V2 structural-cycle rejection before any report exists.
- A report can be valid JSON while leaking hex through an unintended field; Task 1 compares whole expected metadata and checks forbidden raw/hex sequences across repeated byte-identical runs.

---

## File Structure

| File | Responsibility |
|---|---|
| `apps/dag_scheduler_v3_cli.hpp` | V3-only bounded file parser, fixed callback registry, V2 invocation and metadata-only JSON rendering. |
| `apps/bruce_cli.cpp` | Unconditional include, help line and dispatcher route for the V3 CLI. |
| `tests/integration/test_dag_scheduler_v3_cli.py` | Real-binary V3 contract: valid DAG report, raw-byte stability, strict parser failures and exact usage. |
| `tests/unit/dag_scheduler_v3_cli_tests.cpp` | Focused C++ adapter-level checks that cannot be reached safely with a 65 KiB input file, notably concatenation size guard. |
| `CMakeLists.txt` | Registers V3 integration and unit contracts with existing CTest conventions. |
| `tests/integration/test_cli_docs.py` | Pins the documentation’s V3 claims to the implemented public boundary. |
| `docs/cli/README.md` | Adds V3 to the installed CLI scope and index. |
| `docs/cli/commands.md` | Documents command, result contract, limits, error boundary and non-goals. |
| `docs/cli/formats.md` | Gives the normative V3 Spec grammar and canonical example. |
| `docs/architecture/03-blueprint-consolidation.md` | Classifies V3 truthfully as an implemented V2 adapter when implementation and tests are complete. |

### Task 1: Real-binary V3 contract and build registration

**Files:**
- Create: `tests/integration/test_dag_scheduler_v3_cli.py`
- Modify: `CMakeLists.txt:251-255`

**Interfaces:**
- Consumes: the future executable route `bruce dag-scheduler-v3 execute SPEC`.
- Produces: CTest `dag_scheduler_v3_cli_workflows`, executable specification for Tasks 2–3.

- [ ] **Step 1: Write the failing real-binary integration test**

Create `DagSchedulerV3CliTests` following `test_dag_scheduler_v2_cli.py`. Its `call()` must use temporary Spec files and its success helper must require exit 0, empty stderr and exactly one final newline. Use these exact Spec bytes (including one final LF):

```text
BRUCE_DAG_SCHEDULER_SPEC_V3
dag-id=example
max-started-nodes=4
max-total-receipt-bytes=4096

node=a-source
depends=
callback=literal-v1
request-hex=616c706861

node=b-transform
depends=a-source
callback=append-v1
request-hex=3a62657461

node=c-independent
depends=
callback=literal-v1
request-hex=0000

node=d-join
depends=b-transform,c-independent
callback=join-v1
request-hex=73656e74696e656c2d6a6f696e2d7633
```

Compute expected SHA-256 values with `hashlib.sha256`, including the Spec digest from the written bytes. Assert all four `READY` receipts, the three V3 result codes, sorted direct `input_refs`, output sizes/digests and V2 trace chronology. Assert the raw report excludes `alpha`, `beta`, `0000`, `616c706861`, `3a62657461`, `73656e74696e656c2d6a6f696e2d7633`, `request_bytes` and `payload_bytes`. Add five repeat invocations and byte-identical-output assertions.

Add parser matrix subtests for: wrong header; CRLF; NUL; leading, double and trailing block separators; unknown key; duplicate key; wrong field sequence; unsorted nodes; unsorted dependencies; odd, uppercase and non-hex request text; invalid callback; invalid callback arity; one-byte-over-limit file; and cycle. Each must be nonzero, produce no stdout and not contain the supplied raw request/hex. Test the exact compact usage error for missing, wrong, extra and option-like arguments. Test `--help` contains exactly `bruce dag-scheduler-v3 execute SPEC`.

- [ ] **Step 2: Register and demonstrate the red test**

Add after the existing V2 workflow registration:

```cmake
add_test(NAME dag_scheduler_v3_cli_workflows COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dag_scheduler_v3_cli.py $<TARGET_FILE:bruce>)
```

Run: `ctest --test-dir /tmp/bruce-dag-scheduler-v3 -R '^dag_scheduler_v3_cli_workflows$' --output-on-failure`

Expected: FAIL because the public V3 route has not been added.

- [ ] **Step 3: Preserve the failing test as the contract for Task 2**

Do not weaken expected output or convert parser failures into accepted behavior. The red test is complete when it demonstrates that V3 is absent, not when unrelated build configuration fails.

- [ ] **Step 4: Commit task boundary if Git metadata is usable**

Commit only the V3 integration test and CMake registration with message `test: specify dag scheduler v3 CLI contract`. If this checkout continues to lack usable Git metadata, record the inability instead of fabricating a commit.

### Task 2: V3 parser/adapter, CLI dispatch and focused overflow proof

**Files:**
- Create: `apps/dag_scheduler_v3_cli.hpp`
- Create: `tests/unit/dag_scheduler_v3_cli_tests.cpp`
- Modify: `apps/bruce_cli.cpp:11,441,637`
- Modify: `CMakeLists.txt:620-623`

**Interfaces:**
- Consumes: `LocalFiles::read(path, 65536)`, `DagSchedulerSpecV2`, `DagSchedulerCallbackRegistryV2`, `execute_dag_scheduler_v2`, `sha256`, `json_string`, and the real-binary contract from Task 1.
- Produces: `inline int bruce::dag_scheduler_v3_cli(int argc, char** argv)`, plus `bruce::dag_scheduler_v3_cli_detail::parse_spec(const std::filesystem::path&) -> ParsedDagSchedulerSpecV3` and `render(const DagSchedulerRunV2&, std::string_view spec_digest) -> std::string` for direct unit coverage.

- [ ] **Step 1: Write the failing adapter-level test**

Create `bruce_dag_scheduler_v3_cli_tests` with a minimal internal Spec construction or parser fixture that reaches `append-v1`/`join-v1` with a total result larger than 65,536 bytes. Assert `Error` code `DagSchedulerV3PayloadLimitExceeded` and prove no `DagSchedulerRunV2` is returned. Also directly parse a small valid LF-only fixture and assert its V2 spec has scheduler version `dag-scheduler-v2`, sorted node IDs, decoded request bytes and their SHA-256 digests.

Register it with the existing C++ unit-test pattern:

```cmake
add_executable(bruce_dag_scheduler_v3_cli_tests tests/unit/dag_scheduler_v3_cli_tests.cpp)
target_link_libraries(bruce_dag_scheduler_v3_cli_tests PRIVATE bruce_core)
target_compile_options(bruce_dag_scheduler_v3_cli_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME dag_scheduler_v3_cli_contracts COMMAND bruce_dag_scheduler_v3_cli_tests)
```

Run: `ctest --test-dir /tmp/bruce-dag-scheduler-v3 -R '^dag_scheduler_v3_cli_contracts$' --output-on-failure`

Expected: FAIL because the adapter header/functions do not yet exist.

- [ ] **Step 2: Implement `apps/dag_scheduler_v3_cli.hpp`**

Define in `bruce::dag_scheduler_v3_cli_detail`:

```cpp
struct ParsedDagSchedulerSpecV3 {
    DagSchedulerSpecV2 spec;
    std::string raw_bytes;
};
ParsedDagSchedulerSpecV3 parse_spec(const std::filesystem::path& path);
DagSchedulerRunV2 execute(const ParsedDagSchedulerSpecV3& parsed);
std::string render(const DagSchedulerRunV2& run, std::string_view spec_digest);
```

Read exact bytes with the existing bounded `LocalFiles::read`; reject non-ASCII, NUL and CR before line parsing. Require the fixed header, block count, exact field names/order, exactly one blank separator, strict decimal budgets, strictly increasing nodes/dependencies and lower-even hex. Decode hex without accepting alternate forms. Build each `DagSchedulerNodeSpecV2` with the decoded request and its SHA-256 digest, then call V2 validation using a lexicographically sorted fixed registry whose callback IDs are `append-v1`, `join-v1`, `literal-v1`.

Implement three noncapturing callback functions matching `DagSchedulerCallbackFnV2`. They check their input count, assign their mandated codes, calculate SHA-256 after producing the payload, and reject any overflow before `append` or `join` allocates/constructs an oversized result. The concatenation guard sums `inputs[].payload_bytes.size()` and separator copies with checked arithmetic; its error is exactly `DagSchedulerV3PayloadLimitExceeded`.

Call `execute_dag_scheduler_v2(parsed.spec, registry)` exactly once only after parsing/structural validation. Renderer must use V2 receipt data, duplicate V2’s explicit enum-to-string conversion locally, preserve receipt/trace order, render `contract`, `action`, `spec_version`, `spec_digest`, `scheduler_version`, `dag_id`, both budgets, `receipts` and `trace`, and never append payload/request byte strings.

Implement the public CLI function with the exact `argc == 4`, `execute` and non-option path check. It emits the exact compact usage error directly like V2; on success it writes the renderer plus one newline.

- [ ] **Step 3: Add the unconditional CLI wiring**

Include `dag_scheduler_v3_cli.hpp` beside V2 in `apps/bruce_cli.cpp`, add the exact help line beside V2, and dispatch `command == "dag-scheduler-v3"` before optional NanoCell/Cockpit branches. Do not change any optional compilation guards.

- [ ] **Step 4: Run focused verification and make it green**

Run:

```sh
ctest --test-dir /tmp/bruce-dag-scheduler-v3 -R '^(dag_scheduler_v3_cli_workflows|dag_scheduler_v3_cli_contracts|dag_scheduler_v2_cli_workflows|dag_scheduler_v2_contracts)$' --output-on-failure
```

Expected: all four tests pass. Then run the relevant OFF build’s V2/V3 workflow tests, demonstrating that V3 is independent of NanoCell/Cockpit.

- [ ] **Step 5: Commit task boundary if Git metadata is usable**

Commit V3 header, dispatcher wiring and unit test with message `feat: execute DAG scheduler v3 specs`. Otherwise record the Git limitation.

### Task 3: Documentation contracts, generated-artifact verification and full acceptance

**Files:**
- Modify: `docs/cli/README.md:1-12, after V2 scheduler entry`
- Modify: `docs/cli/commands.md:before dag-scheduler-v2 section`
- Modify: `docs/cli/formats.md:before Phase-DAG V2 section`
- Modify: `docs/architecture/03-blueprint-consolidation.md:after V2 scheduler section`
- Modify: `tests/integration/test_cli_docs.py`

**Interfaces:**
- Consumes: final public V3 behavior from Task 2 and its passing reports.
- Produces: documentation marked **IMPLEMENTIERT UND BELEGT**, paired with tests that prevent it from drifting from the actual CLI.

- [ ] **Step 1: Write the failing documentation contract test**

Add `test_dag_scheduler_v3_manual_matches_implemented_scope` to `CliDocumentationTests`. Isolate the `## \`dag-scheduler-v3\`` section and require: implementation status; exact command and usage JSON; header; `dag-scheduler-v3-cli-v1`; `dag-scheduler-spec-v3`; `dag-scheduler-v2`; `spec_digest`; the three callbacks and result codes; ASCII/LF-only/CRLF/65.536 bounds; direct sorted V2 inputs; no raw request/payload bytes; no free callbacks/parallelism/GPU/network/persistence/bundle/replay; `test_dag_scheduler_v3_cli.py`; and the generated-artifact verifier limitation.

Run: `python3 tests/integration/test_cli_docs.py /tmp/bruce-dag-scheduler-v3/bruce`

Expected: FAIL because the V3 manual section does not exist.

- [ ] **Step 2: Document only the implemented V3 contract**

Update the four Markdown documents. Present V3 as a limited parser/CLI adapter over V2, include the canonical four-node example and the metadata-only report boundary. Describe all supported input fields, callback arities and limits, exact error behavior, deterministic ordering, and the rejected scope. Do not claim persistence, replay, arbitrary callbacks, parallelism, Cockpit support or confidentiality. Replace only V2 text that incorrectly says a V3 adapter is merely a concept; preserve Phase-DAG V3 language because it is a separate contract.

- [ ] **Step 3: Run documentation and runtime checks**

Run:

```sh
python3 tests/integration/test_cli_docs.py /tmp/bruce-dag-scheduler-v3/bruce
ctest --test-dir /tmp/bruce-dag-scheduler-v3 -R 'dag_scheduler_(v1|v2|v3)_(cli_workflows|contracts)' --output-on-failure
```

Expected: all selected tests pass.

Copy the complete repository to a native, space-free `/tmp` directory and run its existing `docs/cli/verify_examples.py` through `scripts/with-host-deps.sh`. Do not edit generated artifacts by hand. If the generator does not invoke V3, retain the established generated output and document the V3 process test as the runtime evidence; if generated output changes, inspect and include only verifier-produced changes.

- [ ] **Step 4: Perform complete build and test verification from a native copy**

From a complete native `/tmp` copy without spaces, run the project-prescribed configure, build and CTest sequence:

```sh
scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-dag-scheduler-v3 -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
scripts/with-host-deps.sh cmake --build /tmp/bruce-dag-scheduler-v3
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-dag-scheduler-v3 --output-on-failure
```

Record total pass/fail/skip counts. If the known sandbox `cockpit_inspector_workflows` socket restriction recurs, record it separately and re-run only that unchanged test with permitted host access; do not treat an otherwise green permitted test as a sandbox result.

- [ ] **Step 5: Perform final repository checks and commit if possible**

Inspect all changed paths, run `git diff --check` and `git status --short` when Git metadata is usable, and verify local Markdown links. If the existing checkout still has unusable Git metadata, state that limitation and use the native copy’s file/status evidence without inventing Git output. Commit documentation and generated verifier output, if any, with message `docs: document DAG scheduler v3 specs`.

## Plan Self-Review

- **Spec coverage:** Task 1 covers file shape, exact CLI boundary, report determinism, binary inputs and negative parsing; Task 2 covers V2 mapping, fixed callback semantics and safe payload joining; Task 3 covers documentation, generated-artifact limits and full acceptance. No Spec requirement is without a task.
- **Step scan:** Every task begins red, supplies exact file/symbol boundaries, has a focused green command, and ends at a reviewable state. No implementation body is prescribed beyond fixed external contract decisions.
- **Type consistency:** Task 2 defines the only new C++ interfaces (`ParsedDagSchedulerSpecV3`, `parse_spec`, `execute`, `render`, `dag_scheduler_v3_cli`); Tasks 1 and 3 interact solely via the process route.
- **Review focus:** Each listed high-risk boundary maps to Task 1 or Task 2 tests explicitly.
- **Proportion:** Three deliverable tasks match the three independent boundaries: external contract, adapter/runtime, and documentation/full verification. V2 core remains out of scope.
