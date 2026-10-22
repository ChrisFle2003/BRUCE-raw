# Logic-Mod Result Artifact V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow an authorized, accepted Logic-Mod run to publish its canonical
result as one immutable local Artifact Store object and bind it to replay.

**Architecture:** A host-only output preparation/publisher opens the local
Store, validates the fixed output contract and publishes a one-object READY
manifest only after the existing worker result was accepted. The existing
runner gains optional output reference/evidence fields and replays them by
fresh local resolution; the worker, its protocol and FD set stay unchanged.

**Tech Stack:** C++20, existing Logic-Mod V1/Linux Seccomp worker, Artifact
Store V1/V1.2, CMake/CTest, Python real-binary CLI workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-logic-mod-result-artifact-v1-design.md`

## Global Constraints

- Only `LogicModRunV1::result`, never the private `response_frame`, may be
  published.
- Output is exactly one local READY artifact with fixed codec
  `logic-mod-result-text-v1`, a 16,384-byte ceiling and no overwrite.
- `artifact.output.v1` is opt-in; only the four exact sorted capability sets
  in the approved spec are valid.
- `--store` occurs exactly once if either `--input-artifact` or
  `--output-manifest` occurs; each reference option is itself a singleton.
  A combined input/output run shares that one Store option. Existing
  input-free/Input-Payload behavior and record bytes remain unchanged without
  output.
- The worker sees no Store path, Store/manifest FD, output-manifest ID,
  writable FD or new syscall/network right.
- Replay does not spawn; it fresh-resolves a READY output and compares its
  complete identity and bytes with the already host-validated result.
- Do not hand-edit generated CLI artifacts. Run the CLI verifier only in a
  full temporary copy. Git metadata is unavailable in this checkout.

## Review Focus

- An output-capability denial must happen before worker spawn and must not
  publish an object or manifest; Task 2 integration pins it.
- A READY manifest with matching digest but wrong codec/origin/byte content
  must fail replay; Task 2 replay tests pin it.
- An existing different manifest ID must produce `ArtifactManifestConflict`,
  never overwrite it; Task 1 Store tests pin it.
- A non-accepted result, timeout, worker signal or output limit must leave no
  result manifest; Task 2 worker/CLI tests pin it.
- Legacy `worker.execute` and `{artifact.input.v1,worker.execute}` packages
  and their run records must retain their exact bytes; Tasks 1 and 2 pin it.

## File Structure

| File | Responsibility |
| --- | --- |
| `include/bruce/mod/logic_mod_result_artifact_v1.hpp` | Host-only output reference, prepared Store and immutable published-result identity. |
| `src/mod/logic_mod_result_artifact_v1.cpp` | Validate/prepare a local output target, publish the canonical result and fresh-resolve it for replay. |
| `include/bruce/mod/logic_mod_worker_v1.hpp` | Optional host-only output reference/evidence fields in the runner contract. |
| `src/mod/logic_mod_worker_v1.cpp` | Preflight output, fresh output binding before spawn, publish after acceptance, canonical record and replay checks. |
| `src/mod/runtime_package.cpp` | Exact four-set Logic-Mod capability allowlist. |
| `apps/logic_mod_plugin_v1_cli.hpp` | Paired output option parsing and output metadata rendering. |
| `tests/unit/logic_mod_result_artifact_v1_tests.cpp` | Publisher/preparer and Store conflict/replay-resolution contracts. |
| `tests/unit/logic_mod_payload_worker_v1_tests.cpp` | In-process runner, record compatibility and isolation tests for output combinations. |
| `tests/integration/test_logic_mod_result_artifact_v1.py` | Real signed-package CLI workflow, failure and replay coverage. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/architecture/03-blueprint-consolidation.md` | Post-implementation public boundary and status evidence. |

### Task 1: Output capability and host-only Artifact publisher

**Files:**
- Create: `include/bruce/mod/logic_mod_result_artifact_v1.hpp`
- Create: `src/mod/logic_mod_result_artifact_v1.cpp`
- Create: `tests/unit/logic_mod_result_artifact_v1_tests.cpp`
- Modify: `src/mod/runtime_package.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/logic_mod_plugin_v1_tests.cpp`

**Interfaces:**
- Produces `LogicModOutputV1 { std::filesystem::path store; std::string
  manifest_id; }` for host-only caller configuration.
- Produces a move-only `PreparedLogicModOutputV1` from
  `prepare_logic_mod_output_v1(const LogicModOutputV1&)`; it owns the opened
  `ArtifactStoreV1` and exposes no Store descriptor.
- Produces `LogicModPublishedOutputV1
  publish_logic_mod_result_artifact_v1(PreparedLogicModOutputV1&,
  const LogicModBindingV1& output_binding, std::string_view result)` with
  `manifest_id`, `object_index`, `sha256`, `size_bytes`, `codec` and
  `canonical_identity()`.
- Produces `verify_logic_mod_result_artifact_v1(const LogicModOutputV1&,
  const LogicModPublishedOutputV1&, std::string_view expected_result)` for
  fresh, READY local replay resolution.

- [ ] **Step 1: Write failing publisher/capability tests**

  Cover the four accepted sorted Logic-Mod capability sets and reject each
  other set. Test a `9/16` result publishing one object with
  `media_type=logic-mod-result-text-v1`, `origin=logic-mod-result-v1`,
  `run_id=node_id=attempt_id=MANIFEST_ID`, `status=READY`,
  `result_code=ACCEPTED`; pin its returned digest/size/codec. Add tests for
  invalid ID, non-output binding, >16,384-byte result, idempotent identical
  publication, conflicting manifest, and fresh resolve mismatch.

- [ ] **Step 2: Run the focused unit target to verify RED**

  Run: `scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-result-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release && scripts/with-host-deps.sh cmake --build /tmp/bruce-result-build --target logic_mod_result_artifact_v1_tests`

  Expected: FAIL because the output API/target does not exist.

- [ ] **Step 3: Implement the exact Logic-Mod package allowlist**

  Change the existing parser condition in `src/mod/runtime_package.cpp` to
  admit only the four ordered vectors specified above. Preserve the existing
  Data-Mod schema and all non-Logic-Mod capability checks.

- [ ] **Step 4: Implement preparation, publication and replay-resolution**

  `prepare_logic_mod_output_v1` validates `manifest_id` before opening the
  local Store. `publish_logic_mod_result_artifact_v1` requires a freshly
  issued binding with `capability()=="artifact.output.v1"`, uses
  `put_bytes(result)`, then publishes exactly the fixed one-object manifest.
  `verify_logic_mod_result_artifact_v1` uses `open_existing`,
  `resolve_manifest`, `read_manifest` and `verify` to prove READY status, one
  object, fixed media/origin values, recorded identity and exact bytes (by
  verified SHA-256 and length against `expected_result`); it returns a
  controlled Error on each mismatch.

- [ ] **Step 5: Run Task 1 GREEN and compatibility selection**

  Run the new unit target plus `logic_mod_plugin_v1_tests` through CTest.

  Expected: all selected tests pass; no generated docs changed.

- [ ] **Step 6: Record Task 1 evidence and obtain a fresh Task 1 review**

  With Git unavailable, write the RED/GREEN commands and outcomes under a new
  `.superpowers/sdd/...` ledger/report. A fresh reviewer inspects actual
  paths, Store atomicity and capability compatibility before Task 2 begins.

### Task 2: Runner, canonical record, replay and paired CLI

**Files:**
- Modify: `include/bruce/mod/logic_mod_worker_v1.hpp`
- Modify: `src/mod/logic_mod_worker_v1.cpp`
- Modify: `apps/logic_mod_plugin_v1_cli.hpp`
- Modify: `tests/unit/logic_mod_payload_worker_v1_tests.cpp`
- Create: `tests/integration/test_logic_mod_result_artifact_v1.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 1 `LogicModOutputV1`, `PreparedLogicModOutputV1` and
  `LogicModPublishedOutputV1`.
- Extends `LogicModRunV1` only with optional output evidence fields:
  `output_present`, `output_manifest_id`, `output_object_index`,
  `output_sha256`, `output_size_bytes`, `output_codec`.
- Extends `run_logic_mod_worker_v1(..., const std::optional<LogicModInputV1>&,
  const std::optional<LogicModOutputV1>&)` and the matching replay function
  with the final optional output argument; call sites with no output preserve
  their old behavior and bytes.

- [ ] **Step 1: Write failing runner and real-CLI tests**

  Extend fixture package construction to generate output-only and combined
  capability packages. Assert successful output publication, canonical output
  fields, no local path in record/JSON and replay without spawning. Add
  probe workers verifying that the existing FD/security boundary remains
  unchanged. In the real CLI test cover valid output-only and input+output
  sessions, every unpaired/duplicate `--output-manifest`/`--store` form,
  disabled/revoked/missing output capability, store conflict, non-accepted
  result, changed/missing/non-READY/wrong-codec output artifact and legacy
  record bytes.

- [ ] **Step 2: Run RED against the current runner and CLI**

  Run the new CTest target and Python workflow from `/tmp/bruce-result-build`.

  Expected: failure because the output options, evidence and publication path
  do not exist.

- [ ] **Step 3: Add output preflight and fresh binding to the runner**

  When output is supplied, preflight its local target before fork. Immediately
  before sealing/spawn, issue the fresh `artifact.output.v1` binding alongside
  any existing fresh input/`worker.execute` bindings, and require same
  package/registry/worker/template identity. Do not pass this binding or its
  output configuration to the child.

- [ ] **Step 4: Publish only after host acceptance and encode evidence**

  Leave the worker protocol and Seccomp/FD mapping untouched. After
  `accept_response` succeeds, call Task 1 publisher and copy returned identity
  into `LogicModRunV1`. Do not write output fields or a manifest for every
  non-accepted status. Extend canonical request/record parsing so the output
  suffix is present only for output runs; retain exact old bytes for both
  legacy and input-only records.

- [ ] **Step 5: Add replay verification and CLI route**

  Replay must require output-option/record presence to match, use Task 1's
  fresh resolver and compare the stored artifact to the host-validated result;
  it must never call the worker. Parse `--output-manifest` and `--store` as
  one shared `--store` singleton for either/both input and output references;
  emit only output identity metadata in the success JSON.

- [ ] **Step 6: Run Task 2 GREEN and full relevant selection**

  Run focused runner, publisher, Plugin and real-binary CLI tests, then the
  Artifact Store/Logic-Mod compatibility selection. Report exact passed,
  failed and environment-limited counts separately.

- [ ] **Step 7: Record Task 2 evidence and obtain a fresh Task 2 review**

  Require an adversarial review of record byte compatibility, fresh-binding
  timing, child FDs/Seccomp, publication ordering and replay no-spawn proof.
  Resolve any findings before Task 3.

### Task 3: Documentation, end-to-end verification and whole-change review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/superpowers/specs/2026-10-16-logic-mod-result-artifact-v1-design.md`
- Create: `runs/logic-mod-result-artifact-v1/reports/final-review.md`

**Interfaces:**
- Consumes the actual Task 1/2 option names, record field order and error
  behavior; documentation must not introduce an unimplemented mode.

- [ ] **Step 1: Update public documentation from final implementation**

  Document the opt-in pair, exact capability sets, 16-KiB limit, one-object
  READY result, host-only publishing, replay requirements and explicit
  exclusions. Update the design status only after tests and review prove the
  implementation. Never hand-edit generated CLI files.

- [ ] **Step 2: Verify generated CLI material only in a complete temporary copy**

  Make a complete temporary repository copy, run
  `docs/cli/verify_examples.py` there, and record whether it changed anything.
  Copy back only verifier-generated artifacts when the verifier itself requires
  an update; otherwise leave them untouched.

- [ ] **Step 3: Run final repository checks**

  Run the standard `/tmp/bruce-result-build` configure/build/CTest path, the
  relevant focused test matrix, local Markdown link checker and whitespace
  check. State socket sandbox restrictions separately if reproduced. Attempt
  `git diff --check` and status only to record the expected unavailable Git
  metadata condition; do not substitute a claim of clean Git state.

- [ ] **Step 4: Write evidence and obtain fresh whole-change review**

  The report must list changed surface, exact tests, known limits and the
  final reviewer verdict. The reviewer checks source, docs, generated files,
  security boundary and replay behavior as one feature.

## Plan Self-Review

- **Spec coverage:** Tasks 1 and 2 cover authorization, one-object publication,
  fixed codec/size, isolation, transcript/replay and failure ordering; Task 3
  covers documentation and full verification. No spec requirement lacks a task.
- **Step scan:** Every implementation task begins with RED, names its public
  interface and ends in focused GREEN/review; no implementation body is copied
  into the plan.
- **Type consistency:** Task 1 defines the output types consumed by Task 2;
  Task 2 owns the optional runner fields and CLI; Task 3 only reads final
  behavior.
- **Review focus:** Each listed risk is assigned to Task 1 or Task 2 tests.
- **Proportion:** Three independently reviewable tasks mirror the publisher,
  runner/CLI, and evidence boundaries; no unrelated runtime work is included.
