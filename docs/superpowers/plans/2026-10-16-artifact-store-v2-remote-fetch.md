# Artifact Store V2 Remote Fetch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Materialize one digest-and-size-bound object from an allowlisted
filesystem mirror into the existing local Artifact Store V1 without exposing
payload bytes or changing prior contracts.

**Architecture:** A small Artifact Store V2 core owns canonical provider-file
parsing, safe descriptor-relative mirror reading, bounded verification, and
delegated atomic V1 publication. A closed CLI route validates all input before
opening either provider or store, then renders a metadata-only V2 report.

**Tech Stack:** C++20, POSIX descriptor-relative filesystem APIs, existing
SHA-256, Artifact Store V1, CMake/CTest, Python CLI workflows.

**Spec:** `docs/superpowers/specs/2026-10-16-artifact-store-v2-remote-fetch-design.md`

## Global Constraints

- V2 accepts only a canonical, owner-only `bruce-artifact-store-providers-v2`
  provider configuration and a named provider from that file.
- The only public route is `artifact-store-v2 fetch SHA256 SIZE --store
  LOCAL_STORE --provider PROVIDER_ID --providers PROVIDERS.json`.
- The object limit is exactly `bruce::max_file_bytes`; SHA-256 and exact size
  must match before local publication.
- V2 reports no payload bytes, mirror/local paths, URLs, OS diagnostics or
  partial success; all public failure is V2 `PROTOCOL_ERROR`, exit 2.
- Existing Artifact Store V1/V1.2, V1.3/V1.4, worker and bundle outputs stay
  byte-identical.
- Generated CLI docs are only checked/updated through `docs/cli/verify_examples.py`
  in a complete temporary repository copy.
- Git metadata is unavailable; record evidence in reports rather than claims
  about commits or clean status.

## Review Focus

- Noncanonical or unsafe provider configuration must fail before opening a
  mirror or local store; Task 1 unit tests pin it.
- Mirror path traversal, symlinks, hardlinks, permissions and special files
  must not reach the streamer; Task 1 unit tests pin each category.
- Wrong expected size/digest must leave the destination store without a new
  object; Task 1 unit tests pin the snapshot.
- Empty and maximal valid objects must stream and publish correctly; Task 1
  unit tests pin both boundary cases.
- Existing CLI bytes and absence of path/payload leaks must survive the new
  route; Task 2 Python workflows pin them.

### Task 1: Provider parser and bounded fetch core

**Files:**
- Create: `include/bruce/artifact/artifact_store_v2.hpp`
- Create: `src/artifact/artifact_store_v2.cpp`
- Create: `tests/unit/artifact_store_v2_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ArtifactStoreV1`, `ArtifactIdV1`, `bruce::max_file_bytes` and
  existing SHA-256/error conventions.
- Produces: `ProviderConfigV2 read_provider_config_v2(path)`,
  `ArtifactIdV1 fetch_to_store_v2(const ProviderConfigV2&, string_view
  provider_id, const ArtifactIdV1& expected, const filesystem::path& store)`.

- [ ] **Step 1: Write failing core contract tests**

Add tests for canonical configuration parsing, provider lookup, valid empty,
small and maximum-object fetches, repeated idempotent fetch, and a store-tree
snapshot after digest/size failure. Add rejection cases for duplicate or
unknown provider fields, unsafe config, unknown provider, root escape,
symlink/hardlink/special mirror object and insecure permissions.

- [ ] **Step 2: Run the new target RED**

Run: `scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_artifact_store_v2_tests`

Expected: FAIL because the V2 header, core source and CMake target do not
exist.

- [ ] **Step 3: Implement the V2 types and core**

Define the canonical provider parser and safe fetch API in
`artifact_store_v2.hpp/.cpp`. Derive the remote object path solely from the
validated expected digest, use descriptor-relative no-follow traversal, stream
within `max_file_bytes`, compare exact size and SHA-256, and publish through
the V1 atomic/idempotent boundary only after validation. Map all internal
failures to the established V2 error type without retaining payload bytes.

- [ ] **Step 4: Register and run core tests GREEN**

Add the source/target/test in CMake and run:

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce_artifact_store_v2_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^artifact_store_v2_contracts$' --output-on-failure
```

Expected: target builds with `-Werror`; 1/1 test passes.

### Task 2: Closed CLI route and compatibility workflows

**Files:**
- Create: `apps/artifact_store_v2_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/unit/artifact_store_v2_cli_tests.cpp`
- Create: `tests/integration/test_artifact_store_v2.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `read_provider_config_v2`, `fetch_to_store_v2`, `ArtifactIdV1`.
- Produces: closed `bruce artifact-store-v2 fetch` route and canonical
  `artifact-store-fetch-v2` metadata-only result.

- [ ] **Step 1: Write failing CLI/unit workflow tests**

Add parser tests requiring exactly `fetch SHA256 SIZE --store ... --provider
... --providers ...`, including duplicate/missing/extra argument rejection.
Add Python workflows for a valid fetch, idempotence, no path/payload leakage,
all unsafe source families, no partial destination object, and byte-identical
V1/V1.2 import/inspect/verify/resolve outputs before and after V2 use.

- [ ] **Step 2: Run the CLI tests RED**

Run the named V2 Python test module against the current binary.

Expected: failure because `artifact-store-v2` is absent.

- [ ] **Step 3: Implement `artifact_store_v2_cli`**

Parse all arguments before opening files. Invoke the Task 1 API only after
complete validation. Render exactly `contract`, `action`, `status`,
`provider`, `sha256`, `size_bytes` in that order; map every exception to the
fixed V2 protocol-error envelope. Register the top-level route without
touching V1 dispatch behavior.

- [ ] **Step 4: Run focused CLI GREEN and compatibility checks**

```sh
scripts/with-host-deps.sh cmake --build /tmp/bruce-build --target bruce bruce_artifact_store_v2_cli_tests
scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build -R '^(artifact_store_v2_cli_contracts|artifact_store_v2_cli_workflows|artifact_store_v1_contracts|artifact_store_v1_cli_contracts|artifact_store_v1_cli_workflows|artifact_store_resolve_v1_cli_workflows)$' --output-on-failure
```

Expected: every selected test passes.

### Task 3: Documentation, complete evidence and review

**Files:**
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/superpowers/specs/2026-10-16-artifact-store-v2-remote-fetch-design.md`
- Create: `runs/artifact-store-v2-remote-fetch/reports/final-review.md`

**Interfaces:**
- Consumes: finished V2 core and CLI contracts.
- Produces: accurate user documentation, evidence report and a review-ready
  implementation.

- [ ] **Step 1: Document the implemented boundary**

Describe the exact command, canonical provider configuration, metadata-only
success/failure behavior, 32 MiB limit, local materialization and excluded
network/write/credential features. Mark claims only after the final checks.

- [ ] **Step 2: Run final verification**

Run native configure/build and the relevant V1/V2 CTest selection, local
Markdown links, whitespace checks, and the generated CLI verifier in a full
temporary repository copy. Record commands, exit codes, test counts, hashes
and known environment limits in the final report.

- [ ] **Step 3: Obtain a fresh whole-change review**

Require a reviewer independent from all implementers to inspect core, CLI,
tests and docs for spec compliance, security-boundary violations and legacy
compatibility before marking the feature complete.

## Plan Self-Review

- Every specification requirement maps to Task 1 (safe/provider fetch), Task
  2 (public closure and compatibility), or Task 3 (documentation/evidence).
- The provider/configuration and fetch interfaces are produced before the CLI
  consumes them.
- Each Review Focus item has an owning Task 1 or Task 2 test.
- The plan does not invent an HTTPS, credential, write, cleanup or automatic
  DAG feature outside the approved scope.
