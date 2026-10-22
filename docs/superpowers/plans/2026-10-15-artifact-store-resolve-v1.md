# Artifact Store V1.2 Resolve — Implementation Plan

**Status:** COMPLETED — **IMPLEMENTIERT UND BELEGT** (2026-10-15).
Tasks 1–3 are complete; the final review (archival local path, not versioned:
`runs/bruce-artifact-store-resolve-v1/final-review.md`)
is PASS after the read-only `open_existing()` fix. The checked steps below
record completed delivery and acceptance, with verification scope and
environment limits stated here. Historical test-first/review sequencing is
only claimed where the reports record it; no all-green full-suite run is
claimed.

## Completion Evidence

- Core API/result types, shared ID validation, `resolve_manifest()`, and
  `open_existing()`: [API](../../../include/bruce/artifact/artifact_store_v1.hpp),
  [implementation](../../../src/artifact/artifact_store_v1.cpp),
  [core tests](../../../tests/unit/artifact_store_v1_tests.cpp), and
  Task 1 report (archival local path, not versioned:
  `runs/bruce-artifact-store-resolve-v1/task-1-report.md`).
- Exact metadata-only CLI/parser/error contract:
  [CLI](../../../apps/artifact_store_v1_cli.hpp),
  [CLI unit tests](../../../tests/unit/artifact_store_v1_cli_tests.cpp),
  [real-binary workflow](../../../tests/integration/test_artifact_store_resolve_v1.py),
  [CMake registration](../../../CMakeLists.txt),
  Task 2 report (archival local path, not versioned:
  `runs/bruce-artifact-store-resolve-v1/task-2-report.md`), and scoped review
  (archival local path, not versioned:
  `runs/bruce-artifact-store-resolve-v1/task-2-review.md`).
- Documentation and final acceptance:
  [commands](../../cli/commands.md), [formats](../../cli/formats.md),
  [implementation boundaries](../../implementation/README.md),
  [Blueprint](../../architecture/03-blueprint-consolidation.md),
  [documentation tests](../../../tests/integration/test_cli_docs.py),
  completion report (archival local path, not versioned:
  `runs/bruce-artifact-store-resolve-v1-2026-10-15.md`), and final review
  (archival local path, not versioned:
  `runs/bruce-artifact-store-resolve-v1/final-review.md`).

The recorded final review built successfully and passed **11/11** focused
Resolve/Artifact Store/Worker/V5 tests after the read-only fix. The completion
report also records **20** passing CLI documentation tests and generated CLI
example verification in a full temporary repository copy. The full suite
before that fix reached **183/184**: the existing
`cockpit_inspector_workflows` loopback-socket test was blocked by the sandbox.
A full-suite pass after the fix is **not verified**. Git metadata is unusable
in this checkout; no clean Git diff/status or commit is claimed. The native
path restriction on `scripts/with-host-deps.sh` remains in force.

Implementation detail relative to the original plan: resolve coverage extends
the existing `artifact_store_v1_contracts` and
`artifact_store_v1_cli_contracts` unit targets; CMake adds the dedicated
`artifact_store_resolve_v1_cli_workflows` integration test. No separate Resolve
unit executable was needed. The final review consolidates acceptance of the
core and CLI, including the F-001 read-only correction.

The completed scope is local metadata-only integrity resolution. Payload
output, materialization, network/providers, GC, repair, and automatic DAG-input
flow remain **DOKUMENTIERT / KONZEPT** or explicit non-goals.

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to implement this plan
> task-by-task. Existing Artifact Store import, inspect, verify, and all V5
> execute/inspect/publish contracts must remain byte-compatible.

**Goal:** Add a read-only `artifact-store-v1 resolve` route that resolves a
canonical manifest, verifies every referenced object completely, and emits
only ordered integrity metadata.

**Architecture:** Reuse the existing bounded manifest decoder and
descriptor-relative object verifier behind one core `resolve_manifest` API.
The CLI validates the manifest identifier before opening the store, invokes
the core API, and renders a new metadata-only contract. Resolve never writes,
materializes, downloads, or exposes payload bytes or private paths.

**Tech Stack:** C++20, existing `ArtifactStoreV1` and SHA-256 primitives,
existing JSON string escaping, CTest, and Python `unittest` integration
workflows.

**Spec:** [Resolve V1.2 specification](../specs/2026-10-15-artifact-store-resolve-v1-design.md)

## Global Constraints

- The exact route is `bruce artifact-store-v1 resolve MANIFEST_ID --store STORE`.
- `--store` is the only option; `MANIFEST_ID` is one validated opaque token and is never treated as a filesystem path.
- The success contract is `artifact-store-resolve-v1`, with fixed field order and final LF.
- The outer success field is `status:"RESOLVED"`; the manifest's stored status is emitted separately as `manifest_status`.
- An empty manifest `origin` is emitted as the JSON string `""`; `media_type` is omitted from the public result.
- `objects` preserves canonical manifest order, including the empty-object case `[]`.
- Every object is verified against its exact manifest digest and size before any success JSON is written.
- Every syntax, store, manifest, permission, path, size, decoding, or integrity error emits exactly `{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}` plus LF on stderr and exits 2.
- Resolve is read-only and has no new diagnostic, time, CPU, or I/O budget option; existing bounded `max_file_bytes`/32 MiB checks remain authoritative.
- No payload bytes, absolute paths, private paths, OS diagnostics, PIDs, timestamps, credentials, or secrets may enter stdout or the public error envelope.
- Existing `import`, `inspect`, and `verify` output and parsing remain byte-compatible; V5 execute, inspect, and publish contracts remain unchanged.
- Resolve has no network access, process start, cache, index, GC, repair, overwrite, materialization, or DAG-input behavior.
- Use the repository's existing C++20/CMake/CTest flow. Do not install packages or use network access.

## Review Focus

- **Invalid or path-like manifest ID:** reject before opening the store; pin with parser tests for `../x`, `/x`, empty, extra arguments, and unsupported options.
- **Manifest with zero or multiple objects:** return `objects:[]` or preserve exact manifest order; pin in core and CLI rendering tests.
- **Empty origin:** preserve it as `origin:""` without substituting a path or omission; pin in the exact JSON rendering test.
- **Late object failure:** a missing, corrupted, symlinked, hardlinked, non-regular, oversized, or insecure later object must produce no success report; pin with a multi-object integration fixture.
- **Existing contract regression:** old Artifact Store routes and V5 routes must remain byte-compatible; pin with existing focused tests and a compatibility workflow.

## Files and Boundaries

| File | Responsibility |
|---|---|
| `include/bruce/artifact/artifact_store_v1.hpp` | Add the resolved-manifest/object result types and the read-only `resolve_manifest` API; expose the shared manifest-ID validation used by the CLI. |
| `src/artifact/artifact_store_v1.cpp` | Implement manifest-ID validation reuse and ordered bounded manifest/object resolution using existing safety checks. |
| `tests/unit/artifact_store_v1_tests.cpp` | Test the core resolve API, object order, empty origin/zero objects, complete verification, and read-only failure behavior. |
| `apps/artifact_store_v1_cli.hpp` | Add the exact `resolve` parser branch, route-specific error envelope, and canonical metadata-only renderer while preserving existing actions. |
| `tests/unit/artifact_store_v1_cli_tests.cpp` | Test syntax, field order, `manifest_status`, empty origin, object order, and compatibility of existing renderers. |
| `tests/integration/test_artifact_store_resolve_v1.py` | Exercise the built CLI against real stores, manifests, corruption, unsafe filesystem shapes, leaks, idempotent reads, and no-write guarantees. |
| `CMakeLists.txt` | Register the resolve unit target and integration workflow without changing existing target behavior. |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | Document the implemented route, exact JSON/error contract, integrity boundary, and non-goals with evidence labels. |
| `tests/integration/test_cli_docs.py` | Assert the documentation contains the implemented Resolve V1.2 contract and its limits. |
| `runs/bruce-artifact-store-resolve-v1-2026-10-15.md` | Record implementation status, exact verification counts, known environment limits, and security/non-goal evidence. |

## Task 1: Core Resolve API

**Files:**
- Modify: `include/bruce/artifact/artifact_store_v1.hpp`
- Modify: `src/artifact/artifact_store_v1.cpp`
- Modify: `tests/unit/artifact_store_v1_tests.cpp`

**Interfaces:**
- Consumes: existing `ArtifactManifestV1`, `read_manifest`, digest-addressed `verify`, owner-only descriptor-relative object checks, and the 32 MiB bounded-read rule.
- Produces: `ArtifactResolvedObjectV1 { ArtifactIdV1 id; std::string origin; }`, `ArtifactResolvedManifestV1 { std::string manifest_id, run_id, node_id, attempt_id, manifest_status, result_code; std::vector<ArtifactResolvedObjectV1> objects; }`, `bool is_valid_manifest_id_v1(std::string_view)`, and `ArtifactStoreV1::resolve_manifest(std::string_view) const`.

- [x] **Step 1: Write failing core tests for the resolved result model.**
  Add unit cases that publish a canonical manifest with one object, two
  objects, an empty object list, and an empty origin. Assert that
  `resolve_manifest("manifest-id")` returns the manifest ID, run/node/attempt
  IDs, `manifest_status` copied from the canonical `status` field, result code,
  exact digest/size values, empty origin as an empty string, and object order.

- [x] **Step 2: Write failing core integrity and read-only tests.**
  Cover missing manifests, invalid IDs, truncated/unknown/noncanonical or
  oversized manifests, missing objects, digest/size mismatch, corrupted
  bytes, symlinks, extra hardlinks, non-regular objects, insecure permissions,
  and a valid multi-object manifest whose later object fails. Snapshot the
  store tree before and after and assert that resolve creates, deletes,
  renames, or modifies nothing.

- [x] **Step 3: Run the focused unit test to confirm the new API is absent.**
  Run the existing Artifact Store unit target from the configured build
  directory. Expected result: the new test code fails to compile or fails at
  the missing `resolve_manifest`/result types, while existing V1 tests remain
  available for comparison.

- [x] **Step 4: Add the shared `is_valid_manifest_id_v1(std::string_view)` declaration and implementation.**
  Reuse the existing ASCII grammar and token limit used by manifest storage;
  make `publish_manifest`, `read_manifest`, and the future CLI parser call the
  same validation logic so a syntactically invalid ID is rejected before any
  store descriptor is opened.

- [x] **Step 5: Add `ArtifactStoreV1::resolve_manifest(std::string_view) const`.**
  Read the canonical manifest through the existing bounded decoder, copy its
  stored `status` into the result member named `manifest_status`, then iterate
  objects in decoded order and call the existing exact digest/size verifier.
  Return only after every object passes. Do not add a budget parameter or
  expose `media_type`, paths, file descriptors, or payload data.

- [x] **Step 6: Run the core tests and the existing Artifact Store regression tests.**
  Expected result: the new resolve unit cases pass, all prior store/manifest
  cases pass unchanged, and no new public write or repair behavior exists.

- [x] **Step 7: Complete core review and close findings.**
  Core acceptance is consolidated in the final PASS review, including the
  F-001 read-only correction. The API verifies all referenced objects,
  preserves order and empty origin, uses no new diagnostic budget, and
  returns metadata only. A separate Task 1 review before Task 2 is not
  evidenced by the available reports.

## Task 2: CLI Route and Real Workflows

**Files:**
- Modify: `apps/artifact_store_v1_cli.hpp`
- Modify: `tests/unit/artifact_store_v1_cli_tests.cpp`
- Create: `tests/integration/test_artifact_store_resolve_v1.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `is_valid_manifest_id_v1`, `ArtifactStoreV1::resolve_manifest`, and the result types from Task 1.
- Produces: exact parser support for `resolve`, `render_resolve(const ArtifactResolvedManifestV1&)`, and the route-specific compact error envelope `artifact-store-resolve-v1`.

- [x] **Step 1: Write failing parser and renderer tests.**
  Accept exactly `resolve MANIFEST_ID --store STORE`; reject missing or extra
  arguments, path-like/invalid IDs, missing `--store`, unsupported options,
  and `.manifest`/absolute-path substitutions. Assert exact JSON field order:
  `contract`, `action`, `status`, `manifest_id`, `run_id`, `node_id`,
  `attempt_id`, `manifest_status`, `result_code`, `objects`, followed by no
  additional fields. Assert each object has `sha256`, `size_bytes`, `origin`
  in that order, including an empty origin and multiple objects.

- [x] **Step 2: Verify focused CLI unit coverage.**
  The recorded focused runs pass with resolve parser/renderer coverage and
  unchanged import/inspect/verify assertions. The original planned failing
  pre-implementation CLI run is not separately recorded in the Task 2 report.

- [x] **Step 3: Extend the CLI parser and renderer.**
  Add the `resolve` action and usage string, validate the manifest ID before
  `ArtifactStoreV1::open_existing`, call `resolve_manifest`, and render one canonical
  success line with `status:"RESOLVED"` and `manifest_status` from the
  manifest. Use existing JSON escaping for all controlled output values.

- [x] **Step 4: Add the route-specific error path.**
  Resolve syntax and runtime failures must write no stdout and exactly
  `{"contract":"artifact-store-resolve-v1","error":"PROTOCOL_ERROR"}` plus
  LF to stderr with exit 2. Keep the existing `artifact-store-v1` envelope
  and behavior byte-compatible for import, inspect, and verify.

- [x] **Step 5: Register Resolve coverage in CMake.**
  Core and CLI cases extend the existing unit targets linked to `bruce_core`
  with repository warning flags. The dedicated Python workflow is registered
  as `artifact_store_resolve_v1_cli_workflows` with the built `bruce` binary.
  Existing test commands and generated CLI files remain unchanged.

- [x] **Step 6: Write real-binary workflows and make them pass.**
  Create valid stores/manifests through existing publication paths and bounded
  fixtures. Cover one object, multiple objects in manifest order, zero
  objects, empty origin, repeated resolve with byte-identical output, missing
  and oversized manifests, corrupted/missing/wrong-size objects, symlink,
  hardlink, non-regular and insecure object entries, invalid IDs, late-object
  failure, no payload/path/OS-error leakage, and unchanged store trees after
  success or failure. Assert exact stdout/stderr and exit codes.

- [x] **Step 7: Run compatibility workflows.**
  Run the existing Artifact Store import/inspect/verify workflows and the V5
  execute/inspect/publish tests. Expected result: all existing output bytes
  remain unchanged and Resolve adds no V5 option or output field.

- [x] **Step 8: Request a scoped review for Task 2 and complete its fix loop.**
  Review checks parser-before-store validation, route-specific errors, no
  output on partial verification, exact field ordering, and absence of
  private paths or payload bytes.

## Task 3: Documentation and Final Verification

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/implementation/README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `tests/integration/test_cli_docs.py`
- Create: `runs/bruce-artifact-store-resolve-v1-2026-10-15.md`

**Interfaces:**
- Consumes: the implemented route and JSON contract from Tasks 1–2, plus the existing Artifact Store and V5 documentation.
- Produces: evidence-labelled documentation that distinguishes Resolve V1.2 from payload readback, materialization, GC, network providers, and future DAG payload flow.

- [x] **Step 1: Update the CLI and format documentation.**
  Document the exact command, fixed success field order, `manifest_status`,
  empty-origin behavior, manifest-order guarantee, zero-object behavior,
  complete verification semantics, route-specific error envelope, exit code,
  read-only guarantee, and the unchanged existing routes.

- [x] **Step 2: Update architecture and implementation-boundary documentation.**
  Mark Resolve as **IMPLEMENTIERT UND BELEGT** only after verification, keep
  payload output/materialization, retention/GC, external providers, network,
  GPU execution, plugins, free language, general proving, and runtime
  orchestration as **DOKUMENTIERT / KONZEPT** or non-goals, and state that no
  new diagnostic budget exists in V1.2.

- [x] **Step 3: Add documentation assertions.**
  Extend `tests/integration/test_cli_docs.py` to require the exact route,
  contract, `manifest_status`, empty-origin and non-goal wording. Check any
  generated CLI artifacts only through `docs/cli/verify_examples.py` in a
  temporary copy; do not hand-edit generated files.

- [x] **Step 4: Run build and focused verification.**
  Configure a Release CMake build with testing enabled, build it, run the
  Resolve unit/integration tests, existing Artifact Store tests, Worker/V5
  compatibility tests, and CLI documentation tests. Expected result: all
  relevant tests pass.

- [x] **Step 5: Record repository-wide checks and limitations.**
  The completion report records local-link checks and the pre-fix full CTest
  result of 183/184, with the existing Cockpit loopback-socket sandbox failure.
  Post-fix acceptance is the 11/11 focused run. Git checks cannot establish
  a clean diff/status because metadata is unavailable. A separate defensive
  secret-scan result is not recorded in the cited completion report; no such
  pass or commit is claimed.

- [x] **Step 6: Write the final report and request whole-change review.**
  Record evidence labels, changed files, exact commands and counts, public
  contract behavior, security boundary, and remaining future work in
  `runs/bruce-artifact-store-resolve-v1-2026-10-15.md`. Mark the plan complete
  only after the whole-change review passes.

## Verification Commands

```sh
cmake -G Ninja -S . -B /tmp/bruce-artifact-store-resolve-v1-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bruce-artifact-store-resolve-v1-build -j2
ctest --test-dir /tmp/bruce-artifact-store-resolve-v1-build -R \
  'artifact_store_resolve_v1|artifact_store_v1|worker_run_log_v1|worker_run_inspector_v1|dag_scheduler_v5' \
  --output-on-failure
python3 tests/integration/test_cli_docs.py \
  /tmp/bruce-artifact-store-resolve-v1-build/bruce
python3 scripts/check_local_doc_links.py
```

The host-dependency wrapper remains subject to the repository's native-path
restriction. No package installation or network access is required.

## Self-Review

- **Spec coverage:** the core API, five settled implementation decisions, ordered
  verification, compact error contract, compatibility boundary, CLI tests,
  documentation, and final evidence are assigned to Tasks 1–3.
- **Type consistency:** Task 1 defines `ArtifactResolvedObjectV1`,
  `ArtifactResolvedManifestV1`, `is_valid_manifest_id_v1`, and
  `resolve_manifest`; Task 2 consumes those exact names and returns only the
  renderer/error contract built from them.
- **Review focus coverage:** each listed input class has an explicit test step
  in Task 1 or Task 2, including the later-object failure and empty origin.
- **Proportion:** the plan fixes interfaces, field order, failure semantics,
  and verification commands without prescribing implementation bodies beyond
  what the specification requires.
