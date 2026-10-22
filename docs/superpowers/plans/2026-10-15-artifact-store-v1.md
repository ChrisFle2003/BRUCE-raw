# Artifact Store V1 — Implementation Plan

> **For agentic workers:** execute this plan task-by-task with
> `superpowers:subagent-driven-development`. Do not modify V2/V3/V4/V5
> contracts unless a task explicitly lists a compatibility test proving no
> output change.

**Goal:** Implement a local immutable content-addressed Artifact Store V1
with bounded publication, verification, metadata-only inspection, and a
separate CLI route.

**Spec:** `docs/superpowers/specs/2026-10-15-artifact-store-v1-design.md`

**Chosen V1 object limit:** `bruce::max_file_bytes` (32 MiB), exposed as a
named Artifact Store constant and enforced before allocation/copy. This aligns
with the repository-wide bounded local-file limit and keeps the store from
creating an independent larger payload budget.

## Files and boundaries

| File | Responsibility |
|---|---|
| `include/bruce/artifact/artifact_store_v1.hpp` | IDs, metadata, error-facing API, object limit |
| `src/artifact/artifact_store_v1.cpp` | descriptor-relative validation, bounded hashing/copy, atomic publication, verification |
| `include/bruce/artifact/artifact_manifest_v1.hpp` | canonical manifest model and codec interface |
| `src/artifact/artifact_manifest_v1.cpp` | manifest encoding/decoding and metadata validation |
| `tests/unit/artifact_store_v1_tests.cpp` | store and manifest contract tests |
| `apps/artifact_store_v1_cli.hpp` | exact import/inspect/verify route and stable JSON rendering |
| `tests/unit/artifact_store_v1_cli_tests.cpp` | CLI parser/renderer contracts |
| `tests/integration/test_artifact_store_v1.py` | real-binary filesystem and output workflows |
| `apps/bruce_cli.cpp` | dispatch only for the new route |
| `CMakeLists.txt` | core sources, targets and CTest registrations |
| `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/implementation/README.md`, `docs/architecture/03-blueprint-consolidation.md` | evidence-labelled user/developer contract |
| `runs/bruce-artifact-store-v1-2026-10-15.md` | implementation and verification report |

## Task 1 — Core store and manifest

1. Add failing unit tests for empty/small/maximal objects, digest/size
   identity, idempotent duplicate publication, wrong expected digest/size,
   missing/corrupt objects, `contains`, `inspect`, and `verify`.
2. Add failing filesystem-safety tests for symlink source/components, hardlink
   source/object, non-regular files, path escape, existing destination and
   files over 32 MiB. Assert stable internal error categories and no partial
   destination.
3. Add failing manifest tests for canonical field order, bounded IDs/tokens,
   digest/size binding, atomic exclusive publication and rejection of payload
   bytes, absolute paths, unknown fields and trailing bytes.
4. Implement `ArtifactStoreV1::open`, root creation/permission checks,
   digest-derived object paths, descriptor-relative `O_NOFOLLOW` traversal,
   owner-only temporary files, streaming SHA-256/copy, `fsync`,
   `renameat2(RENAME_NOREPLACE)`, idempotent existing-object verification and
   bounded `verify`.
5. Implement the canonical `BRUCE_ARTIFACT_MANIFEST_V1` codec with a fixed
   metadata-only format. Keep the existing worker JSONL contract untouched.
6. Register targets in CMake and run the new unit tests plus existing LocalFiles,
   worker-log, worker-inspector and runtime-registry tests.
7. Save a task report and request a scoped review. A review finding enters the
   normal fix loop before Task 2 begins.

## Task 2 — Standalone CLI and integration

1. Add failing parser tests for exact syntax:
   `artifact-store-v1 import FILE --store STORE`,
   `inspect DIGEST --store STORE`, and `verify DIGEST --store STORE`.
   Reject missing/extra options, absolute/escaping store object references,
   invalid digests, and unsupported providers.
2. Implement `apps/artifact_store_v1_cli.hpp` with stable metadata-only JSON,
   compact protocol errors, no OS path leakage, no payload output, and exact
   exit behavior. `import` may read a source file; `inspect` and `verify` only
   read the digest-addressed object.
3. Dispatch the route from `apps/bruce_cli.cpp` without altering existing route
   parsing or output bytes.
4. Add real-binary workflows covering import, duplicate import, inspect,
   verify, corrupted object, symlink/hardlink attack, wrong digest/size and
   store permissions. Snapshot source/store trees to prove no accidental
   overwrite or unrelated writes.
5. Run V2/V3/V4/V5 CLI workflows and the focused Artifact/Worker tests. Save a
   task report and request a scoped review.

## Task 3 — Documentation and final verification

1. Document the exact CLI, JSON/manifest fields, object limit, error matrix,
   local security boundary and non-goals. Mark V5 integration, download, GC,
   network and external providers as concept/future work.
2. Add documentation assertions. Generated CLI artifacts may only be checked
   or regenerated through `docs/cli/verify_examples.py` in a temporary copy.
3. Run Release configure/build, focused store/worker/V5 tests, documentation
   tests, local Markdown-link checks, and the full CTest suite. Record exact
   counts, the known Cockpit socket-sandbox limitation, empty-Git limitation,
   and secret-scan result.
4. Create the final report and request a whole-change review. Only after a
   clean review mark the ledger complete.

## Verification commands

```sh
cmake -G Ninja -S . -B /tmp/bruce-artifact-store-v1-build \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bruce-artifact-store-v1-build -j2
ctest --test-dir /tmp/bruce-artifact-store-v1-build -R \
  'artifact_store_v1|worker_run_log_v1_contracts|worker_run_inspector_v1_contracts|dag_scheduler_v5' \
  --output-on-failure
python3 scripts/check_local_doc_links.py
```

The host-dependency wrapper remains subject to the repository's native-path
restriction. No package installation or network access is required.
