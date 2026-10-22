# Artifact Payload Stream V1.5 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` (recommended) or
> `superpowers:executing-plans` to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an explicitly selected, host-controlled chunk-production path
for V5 payload inputs while preserving the existing worker-v1 frame and all
V1.3/V1.4 behavior when the option is absent.

**Architecture:** Bruce validates and reads the already selected Artifact
Store object, writes it through a fixed 64 KiB chunk loop into a private
owner-only stream staging area, computes the final digest/size/chunk count,
and seals the result before binding the existing private `ArtifactRef` to the
worker. The worker frame remains unchanged; the first V1.5 implementation
adopts the canonical private stream directory and extends the existing
Bruce-side reader to consume its chunks sequentially.

**Tech Stack:** C++20, existing Artifact Store V1/V1.3/V1.4 APIs, SHA-256,
POSIX descriptor/file safety, CMake/CTest, and Python integration workflows.

**Spec:** `docs/superpowers/specs/2026-10-15-artifact-payload-stream-v1-design.md`

## Global Constraints

- V1.5 is activated only by the explicit payload option `--stream-v15`.
- Without `--stream-v15`, V1.3 and V1.4 parsing, reports, bytes, and behavior remain unchanged.
- The existing `worker-v1` frame and `ArtifactRef` field order remain unchanged.
- The fixed stream chunk size is exactly **65,536 bytes** and is not a user option.
- Bruce alone opens the Artifact Store and creates/adopts private stream files.
- A stream is valid only after the final digest, exact size, chunk count, and seal have been verified.
- Existing wall, input, output, operation, memory, retry, and 15-object limits remain authoritative.
- No payload bytes, private paths, OS diagnostics, PIDs, command lines, or secrets enter public output.
- No network, provider, plugin, GPU, free-language, proving, GC, repair, cache, or cross-runtime behavior is added.
- Existing import/inspect/verify/resolve, V5 execute/inspect/publish, V2/V3/V4, and replay contracts remain byte-compatible.
- Generated CLI artifacts are checked only through `docs/cli/verify_examples.py` in a temporary copy.

## Review Focus

- **Option isolation:** `--stream-v15` must be accepted only by the V5 payload route and must not alter V1.3/V1.4 output when absent; pin in the CLI parser and compatibility tests.
- **Late integrity failure:** a digest, size, or chunk failure after earlier writes must produce no accepted input and must remove the staging tree; pin in stream-core and workflow tests.
- **Seal tampering:** changed, missing, noncanonical, symlinked, or insecure `seal.json`/chunk files must fail before WorkerRequest creation; pin in the stream reader tests.
- **Budget/timeout boundary:** exactly-at-limit streams pass, one byte over limit or an expired deadline fails with cleanup; pin in materialization tests.
- **Concurrent attempt isolation:** two attempts with equal content must use separate private stream roots and deterministic metadata; pin in the multi-node workflow.

## Files and Boundaries

| File | Responsibility |
|---|---|
| `include/bruce/artifact/artifact_payload_stream_v1.hpp` | Define `ArtifactPayloadStreamV15`, fixed chunk constant, typed errors, writer/seal APIs. |
| `src/artifact/artifact_payload_stream_v1.cpp` | Implement bounded chunk writing, SHA-256/size/count accounting, owner-only staging, seal validation, atomic adoption, and cleanup. |
| `tests/unit/artifact_payload_stream_v1_tests.cpp` | Test writer, seal, limits, integrity failures, cleanup, and attempt isolation. |
| `apps/dag_scheduler_v5_cli.hpp` | Parse `--stream-v15`, select stream materialization, validate/read the sealed stream directory through the existing `ArtifactRef`, and retain old report paths. |
| `tests/unit/dag_scheduler_v5_payload_cli_tests.cpp` | Pin option acceptance/rejection, duplicate handling, and absent-option compatibility. |
| `tests/integration/test_dag_scheduler_v5_payload.py` | Exercise real V1.5 success/failure workflows and compare V1.3/V1.4 output without the option. |
| `CMakeLists.txt` | Register the stream source and focused unit/CTest target. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | Document the explicit option, private seal boundary, metadata-only behavior, and non-goals. |
| `tests/integration/test_cli_docs.py` | Assert exact V1.5 option and boundary wording. |
| `runs/bruce-artifact-payload-stream-v1-2026-10-15.md` | Record implementation evidence, verification counts, and environment limits. |

## Task 1: Stream Core and Sealed Private Artifact

**Files:**
- Create: `include/bruce/artifact/artifact_payload_stream_v1.hpp`
- Create: `src/artifact/artifact_payload_stream_v1.cpp`
- Create: `tests/unit/artifact_payload_stream_v1_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: a Bruce-opened readable source descriptor, an expected
  `ArtifactIdV1`, a private attempt root, a bounded byte budget, and a
  monotonic deadline.
- Produces:
  `inline constexpr std::uint64_t artifact_payload_stream_chunk_bytes_v15 = 65536;`,
  `struct ArtifactPayloadStreamV15 { std::string stream_id; std::string reference; std::string sha256; std::uint64_t size_bytes; std::uint64_t chunk_bytes; std::uint64_t chunk_count; };`,
  `ArtifactPayloadStreamV15 write_payload_stream_v15(int source_fd, const std::filesystem::path& attempt_root, std::string_view stream_id, const ArtifactIdV1& expected, std::uint64_t max_bytes, std::chrono::steady_clock::time_point deadline);`,
  and `void verify_payload_stream_v15(const ArtifactPayloadStreamV15&, const std::filesystem::path& attempt_root);`.

- [ ] **Step 1: Write failing core tests.**
  Add tests for empty, 1-byte, exactly-65,536-byte, and multi-chunk sources;
  assert exact digest, size, `chunk_bytes=65536`, and chunk count. Add a
  snapshot test proving that a failed late read leaves no accepted stream.

- [ ] **Step 2: Run the new unit target before implementation.**
  Run `cmake --build /tmp/bruce-v14-core-build --target bruce_artifact_payload_stream_v1_tests`.
  Expected: compile or link failure because the new types and target do not exist.

- [ ] **Step 3: Implement the bounded writer.**
  Create an owner-only staging directory under the attempt root, read the
  borrowed source descriptor in fixed 65,536-byte chunks, enforce the
  monotonic deadline and `max_bytes` before each operation, update SHA-256,
  size, and chunk count, and write only through `O_NOFOLLOW|O_CLOEXEC`.
  Write canonical `header.jsonl`, chunk files, and `seal.json` after `fsync`;
  adopt the complete stream directory with an atomic rename only after all
  expected digest/size checks pass.

- [ ] **Step 4: Implement `verify_payload_stream_v15`.**
  Re-open the adopted private files descriptor-relatively, require owner-only
  regular files and the exact seal fields, recalculate digest/size/count, and
  reject any mutation, symlink, extra hardlink, unknown field, or path-like
  metadata.

- [ ] **Step 5: Run stream-core and existing Store contracts.**
  Run the new target plus `ctest --test-dir /tmp/bruce-v14-core-build -R
  '^(artifact_store_v1_contracts|artifact_payload_v1_contracts|artifact_payload_external_v1_contracts|artifact_payload_stream_v1_contracts)$' --output-on-failure`.
  Expected: all stream and existing Store/Payload contracts pass.

## Task 2: Explicit V5 Payload Selection and Worker Binding

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp`
- Modify: `tests/unit/dag_scheduler_v5_payload_cli_tests.cpp`
- Modify: `tests/integration/test_dag_scheduler_v5_payload.py`

**Interfaces:**
- Consumes: `write_payload_stream_v15`, `verify_payload_stream_v15`, and
  the existing `ArtifactPayloadSelectionV13` values produced for direct and
  V1.4 external inputs.
- Produces: `PayloadRequest::stream_v15`, parser support for one
  `--stream-v15`, and a private `worker_v1::ArtifactRef` pointing at the
  sealed stream directory with the existing digest and size.

- [ ] **Step 1: Add failing parser tests.**
  Accept one `--stream-v15` only on `dag-scheduler-v5 payload`; reject the
  option on execute/inspect/publish, reject duplicates, and assert the option
  is absent from the existing V1.3/V1.4 request defaults.

- [ ] **Step 2: Add a failing real-binary workflow.**
  Run a two-node payload DAG with `--stream-v15`; assert a successful result,
  stable output digest, private stream cleanup/adoption, and no stream paths
  or payload bytes in stdout, stderr, bundle, or run log.

- [ ] **Step 3: Integrate stream materialization in `prepare_payload_inputs`.**
  Keep selection/provenance checks unchanged. When `stream_v15` is true,
  create one isolated stream root per selected input, call the core writer,
  verify its seal, and add the existing relative `ArtifactRef` for the adopted
  stream directory. Extend Bruce's `read_approved` path to validate the
  sealed directory and consume chunk files sequentially into the existing
  adapter input value. When false, execute the current V1.3/V1.4 path
  byte-for-byte.

- [ ] **Step 4: Preserve host authority and cleanup.**
  Ensure any stream failure occurs before `ATTEMPT_STARTED`, maps to the
  existing controlled payload failure path, removes staging directories, and
  never changes Store contents or worker frame fields.

- [ ] **Step 5: Run the focused CLI and workflow tests.**
  Run `ctest --test-dir /tmp/bruce-v14-core-build -R
  '^(dag_scheduler_v5_payload_cli_contracts|dag_scheduler_v5_payload_workflows)$' --output-on-failure`.
  Expected: V1.5 workflow passes and all existing V1.3/V1.4 cases remain green.

## Task 3: Documentation and Evidence

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/implementation/README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `tests/integration/test_cli_docs.py`
- Create: `runs/bruce-artifact-payload-stream-v1-2026-10-15.md`

**Interfaces:**
- Consumes: the implemented option, private seal semantics, unchanged
  WorkerRequest contract, and focused workflow evidence.
- Produces: evidence-labelled documentation distinguishing V1.5 stream
  staging from public payload output, downloads, network providers, and
  future orchestration.

- [ ] **Step 1: Document the exact option and boundary.**
  Add `--stream-v15`, fixed 65,536-byte chunks, seal verification,
  metadata-only behavior, cleanup guarantees, and the unchanged V1.3/V1.4
  path. Mark unimplemented network/GPU/plugin/language/proving/orchestration
  features as **DOKUMENTIERT / KONZEPT**.

- [ ] **Step 2: Add documentation assertions.**
  Extend `tests/integration/test_cli_docs.py` for the option, stream contract,
  no-worker-frame-change claim, and non-goals.

- [ ] **Step 3: Run documentation verification.**
  Run `python3 tests/integration/test_cli_docs.py /tmp/bruce-v14-core-build/bruce`
  and `python3 scripts/check_local_doc_links.py`. Check generated CLI artifacts
  only in a complete temporary copy through `docs/cli/verify_examples.py`.

- [ ] **Step 4: Write the evidence report.**
  Record exact commands, test counts, the known Cockpit socket sandbox limit,
  wrapper-build limitation if encountered, and unavailable Git checks without
  claiming repository cleanliness.

## Task 4: Full Verification and Review

- [ ] **Step 1: Build the complete Release tree.**
  Run `cmake --build /tmp/bruce-v14-core-build` and require exit 0.

- [ ] **Step 2: Run focused V1.5 plus compatibility tests.**
  Run stream core, Artifact Store, Worker, V5 payload, V5 publish/inspect,
  V3, V4, and documentation contracts. Record exact pass counts.

- [ ] **Step 3: Run the full CTest suite once.**
  Record all passed tests and isolate any pre-existing sandbox failure; do not
  claim a clean full suite if Cockpit socket creation remains blocked.

- [ ] **Step 4: Run final link and workspace checks.**
  Run local Markdown link checks, `git diff --check`, and `git status --short`
  where the repository metadata is usable. Report unavailable checks exactly.

- [ ] **Step 5: Perform a final compatibility review.**
  Confirm that a run without `--stream-v15` produces byte-identical V1.3/V1.4
  report shapes and that no worker-v1 frame field or existing route changed.
