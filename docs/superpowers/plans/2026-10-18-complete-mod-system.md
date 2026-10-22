# Complete Local Mod System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish BRUCE's built-in Language, NanoCell, and MCP mods and deliver one local-user Mod system that can build, sign, inspect, verify, install, explicitly activate, update, run, disable, and roll back signed Data and Logic Mod packages.

**Architecture:** Preserve built-ins as compile-time modules and installed mods as signed immutable packages. Overlay both in a canonical `bruce-mod-catalog-v2`; keep the existing Data registry byte-compatible, add a parallel durable Logic registry under the same owner-only root and lock, and expose lifecycle operations through one strict CLI adapter. Updates are verify-first transactions that install a new version side-by-side and switch activation only after every validation succeeds.

**Tech Stack:** C++20, CMake/Ninja/CTest, OpenSSL EVP Ed25519 and SHA-256, existing POSIX descriptor-based persistence and Logic worker sandbox, Python 3 integration tests, Markdown/CLI documentation verifier.

**Spec:** `docs/superpowers/specs/2026-10-18-complete-mod-system-design.md`

## Global Constraints

- Work as the invoking local Linux user. Do not add a BRUCE account, daemon, multi-user service, privileged installer, network download, marketplace, or implicit background updater.
- Treat source, tests, and current reproducible runs as evidence. Do not promote a concept or roadmap statement to implementation status.
- Before every task, inspect repository status and relevant diffs. Git metadata is currently unavailable (`git status` reports “not a git repository”); do not claim commits until `.git` is restored. The commit steps below are mandatory once Git is healthy and otherwise remain explicitly blocked.
- Preserve unrelated work. Do not touch historical Cockpit sources or generated CLI artifacts by hand.
- Use a full temporary copy at a native path without spaces or colons for wrapper builds and `docs/cli/verify_examples.py`.
- Keep `journal.v1`, `registry.v1`, and `packages/<digest>.brucemod` byte-compatible for Data Mods. Add Logic persistence only in the spec-defined sibling files.
- Keep `route-report-v1` byte-compatible unless `--mod-details` is explicitly selected.
- Reject before mutation: malformed packages, untrusted signatures, built-in ID collisions, Data/Logic ID collisions, same ID/version with a different digest, unsafe paths, symlinks, wrong ownership/mode, and prospective active-tree conflicts.
- Installation is inactive. Activation is explicit. Multiple immutable versions remain installed. Rollback is `mods activate ID VERSION`.
- Every public success emits canonical JSON on stdout with exit 0. Every public failure emits bounded, non-secret JSON on stderr with exit 2.
- Follow test-driven development: add one focused failing test, observe the intended failure, implement only enough to pass, then run the neighboring regression set.

## Review Focus

- Built-in help, dispatch, CMake targets, and catalog state must agree for all six valid Language/NanoCell/MCP configurations; MCP-without-NanoCell must fail configuration with the documented error.
- `mods list` without a root must not create filesystem state. A supplied existing root must be opened read-only for list/status.
- One package detector must classify Data versus Logic packages without accepting ambiguous or partially valid envelopes.
- The builder must never evaluate shell text, follow key/source/output symlinks, replace outputs, compile a worker, or leak a key/path in errors.
- Logic package installation must persist exact verified worker bytes and issue fresh bindings from digest-pinned installed bytes, not mutable source paths.
- All Data and Logic lifecycle changes must serialize through the existing `registry.lock` and validate the complete prospective active set.
- Failed `mods update` must leave generation, active version, journals, package bytes, and worker bytes unchanged; successful update must retain the old version.
- The stdio adapter `tools/bruce_mcp.py` and built-in HTTP inspector remain separate products and are never described as equivalent protocol servers.

---

## File structure

| File | Responsibility |
| --- | --- |
| `include/bruce/mod/catalog.hpp`, `src/mod/catalog.cpp` | Always-present built-in feature inventory and compile-time built catalog |
| `include/bruce/mod/unified_catalog_v2.hpp`, `src/mod/unified_catalog_v2.cpp` | Canonical built-in/installed catalog projection and JSON rendering model |
| `include/bruce/mod/package_inspection_v2.hpp`, `src/mod/package_inspection_v2.cpp` | Strict Data/Logic package classification, inspection, and common verification metadata |
| `include/bruce/mod/local_builder.hpp`, `src/mod/local_builder.cpp` | Safe reusable signing input and Data/Logic package construction |
| `include/bruce/mod/logic_mod_registry_v1.hpp`, `src/mod/logic_mod_registry_v1.cpp` | Durable Logic package, worker, journal, registry, lifecycle, and binding state |
| `include/bruce/mod/installed_mod_host_v2.hpp`, `src/mod/installed_mod_host_v2.cpp` | Cross-kind collision checks, shared lock orchestration, catalog snapshot, and atomic update |
| `apps/runtime_mod_cli.hpp` | Unified `mods` lifecycle, `mods run`, and route-report adapters |
| `apps/local_mod_builder.cpp` | New builder grammar plus legacy Data-builder compatibility |
| `apps/bruce_cli.cpp` | Feature-accurate help and top-level dispatch only |
| `tests/unit/*mod*`, `tests/integration/test_*mod*.py` | Focused contracts and real-binary workflows |
| `tests/integration/test_builtin_mod_matrix.py` | Six valid feature builds plus invalid dependency configuration |
| `docs/cli/README.md`, `docs/contracts/README.md`, `docs/architecture/03-blueprint-consolidation.md` | Implemented CLI, file contracts, boundaries, and status |
| `docs/cli/verify_examples.py` | Sole generator/verifier for generated CLI artifacts |

## Wave 1 — Built-in Mods

### Task 1: Pin feature-accurate help and dispatch

**Files:**
- Modify: `apps/bruce_cli.cpp`
- Create: `tests/integration/test_builtin_mod_help.py`
- Modify: `CMakeLists.txt`

**Interfaces:** Help advertises Language only when `BRUCE_BUILD_MOD_LANGUAGE`; NanoCell only when `BRUCE_BUILD_MOD_NANOCELL`; HTTP MCP only when `BRUCE_BUILD_MOD_MCP`. Dispatch uses the same guards.

- [x] Add integration assertions for full build and each disabled command. The disabled command must return the standard JSON `UnknownCommand` failure and must not appear in `--help`.
- [x] Run the focused test against an all-OFF binary and observe the current Language-help failure.
- [x] Guard the Language help block with the existing compile definition; keep NanoCell and MCP guards symmetric and do not change enabled command text.
- [x] Run `test_builtin_mod_help.py` for the current build and the all-OFF build; then run Language, NanoCell, and MCP focused CTests in the full build.
- [ ] Commit when Git is restored: `fix: align built-in mod help with build features`. **BLOCKED:** repository Git metadata is unavailable.

### Task 2: Add an always-present built-in feature inventory

**Files:**
- Modify: `include/bruce/mod/catalog.hpp`
- Modify: `src/mod/catalog.cpp`
- Create: `tests/unit/mod_catalog_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `enum class BuiltinModBuildStateV1 { not_built, available };`.
- Add `BuiltinModFeatureV1 { id, version, build_state, capabilities }`.
- Add `std::span<const BuiltinModFeatureV1> builtin_mod_features_v1();` sorted by ID and always containing `language`, `mcp`, and `nanocell`.
- Preserve `builtin_mod_catalog_v1()` as the manifests of only built modules.

- [x] Write unit tests for the three stable feature identities, canonical order, compile-flag state, and equality between `available` entries and V1 manifests.
- [x] Run RED; the new API must be missing.
- [x] Implement static descriptors without referencing disabled module symbols; validate unique IDs, versions, capabilities, and state/catalog agreement.
- [x] Run `mod_catalog_contracts` under full and all-OFF configurations.
- [ ] Commit when Git is restored: `feat: expose built-in mod feature inventory`. **BLOCKED:** repository Git metadata is unavailable.

### Task 3: Exercise the complete built-in build matrix

**Files:**
- Create: `tests/integration/test_builtin_mod_matrix.py`
- Modify: `CMakeLists.txt`
- Modify: `tests/integration/test_cli_docs.py`

**Interfaces:** The matrix covers `000`, `100`, `010`, `110`, `011`, and `111` for Language/NanoCell/MCP. `001` and `101` fail configure with `McpRequiresNanoCell`.

- [x] Write a Python test that configures isolated Ninja Release builds, builds `bruce` and `bruce_mod_catalog_tests`, checks help/inventory, and runs feature-specific tests only when built.
- [x] Run RED and capture the current OFF-build documentation mismatch separately from actual feature failures.
- [x] Register the generated CLI snapshot test only for the canonical full-feature build; keep targeted OFF help tests active in all configurations.
- [x] Run all six valid builds and both invalid configurations. Assert no skipped built-in target is reported as passed.
- [ ] Commit when Git is restored: `test: cover built-in mod build matrix`. **BLOCKED:** repository Git metadata is unavailable.

## Wave 2 — Common catalog and package construction

### Task 4: Define and render `bruce-mod-catalog-v2`

**Files:**
- Create: `include/bruce/mod/unified_catalog_v2.hpp`
- Create: `src/mod/unified_catalog_v2.cpp`
- Create: `tests/unit/unified_mod_catalog_v2_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `ModOriginV2 { builtin, installed }`, `ModKindV2 { builtin, data, logic }`, and `UnifiedModEntryV2` with the spec fields.
- Add `UnifiedModCatalogV2 make_unified_mod_catalog_v2(features, installed_records)` and `std::string render_unified_mod_catalog_v2(...)`.
- Sort by `(id, origin, version, package_digest)` and reject duplicate identities or invalid field combinations.

- [x] Test built/not-built entries, installed versions/states, deterministic order, forbidden built-in lifecycle/digest fields, and forbidden installed build-state fields.
- [x] Run RED for the absent target/API.
- [x] Implement the pure projection and canonical renderer; do not open files here.
- [x] Run focused unit tests twice and compare output bytes.
- [ ] Commit when Git is restored: `feat: add unified mod catalog v2`. **BLOCKED:** repository Git metadata is unavailable.

### Task 5: Add rootless `mods list` and read-only `mods status`

**Files:**
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `tests/unit/runtime_mod_cli_tests.cpp`
- Modify: `tests/integration/test_runtime_mods.py`

**Interfaces:**
- `bruce mods list [--mod-root PATH]` returns `bruce-mod-catalog-v2`.
- `bruce mods status MOD_ID [VERSION] [--mod-root PATH]` returns one exact catalog subset or `ModNotFound`.
- No root means built-ins only. A root is optional and must already exist; list/status never creates or repairs it.

- [x] Add CLI tests for rootless built-ins, not-built entries, missing root, empty existing root, exact status, unknown IDs/options, canonical JSON, stderr, and exit 2.
- [x] Run RED against the current mandatory-root V1 list behavior.
- [x] Wire the pure catalog renderer to `builtin_mod_features_v1()` and Data registry snapshots, using `open_existing` only when a root was supplied.
- [x] Run CLI/unit tests and assert the no-root case leaves a sentinel temporary directory byte-for-byte unchanged.
- [ ] Commit when Git is restored: `feat: expose unified mod list and status`.

### Task 6: Add strict package inspection and common verification

**Files:**
- Create: `include/bruce/mod/package_inspection_v2.hpp`
- Create: `src/mod/package_inspection_v2.cpp`
- Create: `tests/unit/package_inspection_v2_tests.cpp`
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `DetectedModPackageV2` as a closed variant of `RuntimeDataModPackageV1` and `LogicModPackageV1`.
- Add bounded `detect_mod_package_v2(bytes)`, `inspect_mod_package_v2(package)`, and `verify_mod_package_v2(package, trust)`.
- Add `bruce mods inspect PACKAGE` and extend `mods verify` to both kinds.

- [x] Test valid Data/Logic, truncated/oversized/ambiguous envelopes, wrong type, bad signature, missing file, FIFO/symlink, unexpected arguments, and bounded sanitized errors.
- [x] Run RED for missing commands/API.
- [x] Implement detection from exact envelope magic/schema, with no “try one parser and ignore arbitrary failure” ambiguity; parse each package once.
- [x] Render `mod-package-inspection-v2` and `mod-package-verification-v2` with kind, identity, digest, capabilities, and `verified` only after trust succeeds.
- [x] Run focused unit and real-CLI tests.
- [ ] Commit when Git is restored: `feat: inspect data and logic mod packages`.

### Task 7: Refactor secure signing input and preserve the legacy builder

**Files:**
- Modify: `include/bruce/mod/local_builder.hpp`
- Modify: `src/mod/local_builder.cpp`
- Modify: `apps/local_mod_builder.cpp`
- Modify: `tests/unit/local_builder_tests.cpp`
- Modify: `tests/integration/test_local_builder_cli.py`

**Interfaces:**
- Extend Data build paths with explicit `signing_env`; default legacy grammar still uses `<project_root>/.env`.
- Add `bruce-mod-builder data SOURCE_DIR --output PACKAGE --trust-store-output TRUST --signing-env FILE`.
- Preserve the five legacy flags and their output contract.

- [x] Add failing tests for the new grammar, legacy equivalence, multiple/unknown assignments, symlinks, hardlinks, wrong mode, unsafe ancestors/outputs, collisions, and existing outputs. Derived-public-key mismatch belongs to the embedded Data/Logic boundary in Task 8; wrong ownership remains enforced in code but cannot be created by an unprivileged test process.
- [x] Run RED for the new `data` subcommand.
- [x] Extract the descriptor-based secret reader and signing primitive into private reusable helpers; keep cleanse, `O_NOFOLLOW`, single-link, owner, `0600`, bounded input, and no shell evaluation.
- [x] Map `SOURCE_DIR` to exact `manifest.json`, `tree.json`, and `golden-cases.json`; do not scan.
- [x] Run builder unit/integration tests and compare legacy package/trust bytes to the new grammar using identical inputs.
- [ ] Commit when Git is restored: `refactor: share secure local mod signing`.

### Task 8: Build signed Logic packages without compiling workers

**Files:**
- Modify: `include/bruce/mod/local_builder.hpp`
- Modify: `src/mod/local_builder.cpp`
- Modify: `apps/local_mod_builder.cpp`
- Create: `tests/fixtures/local_logic_mod/README.md`
- Create: `tests/unit/local_logic_builder_tests.cpp`
- Create: `tests/integration/test_local_logic_builder_cli.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `LocalLogicModBuildPathsV1`, `LocalLogicModBuildResultV1`, and `build_local_logic_mod_v1`.
- Add `bruce-mod-builder logic SOURCE_DIR --output PACKAGE --trust-store-output TRUST --signing-env FILE`.
- Require exact `data.brucemod`, `logic.json`, and the single prebuilt worker filename declared by `logic.json`.

- [x] Test a valid deterministic package and reject missing/extra declared fields, nested/absolute/traversal worker names, non-regular worker, mutable source changes, digest mismatch, invalid embedded Data package, derived-public-key mismatch, and output collision.
- [x] Run RED for absent builder API/subcommand.
- [x] Construct the existing Logic V1 envelope using the shared key, verify the already-signed embedded Data layer and finished outer package against the derived truststore, then atomically publish package and truststore without replacement.
- [x] Prove the builder never invokes a compiler/shell by running with a poisoned `PATH` containing trap executables.
- [x] Run both Data and Logic builder suites.
- [ ] Commit when Git is restored: `feat: build signed local logic mods`.

## Wave 3 — Durable lifecycle, update, and execution

### Task 9: Persist Logic Mods under the shared local root

**Files:**
- Create: `include/bruce/mod/logic_mod_registry_v1.hpp`
- Create: `src/mod/logic_mod_registry_v1.cpp`
- Create: `tests/unit/logic_mod_registry_v1_tests.cpp`
- Modify: `src/mod/runtime_registry.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `LogicModRecordV1`, `LogicModSnapshotV1`, and `DurableLogicModRegistryV1::open/open_existing/install/activate/disable/snapshot/bind`.
- Persist `logic-journal.v1`, `logic-registry.v1`, `logic-packages/<digest>.brucemod`, and `logic-workers/<digest>/<worker-name>`.
- Use the existing root `registry.lock`; keep in-memory `LogicModRegistryV1` and `logic-mod-plugin-v1 session` unchanged.

- [x] Add lifecycle/recovery tests for install inactive, activation, disable, multi-version retention, restart, torn snapshot recovery from journal, corrupt journal rejection, exact worker bytes/mode, symlink/FIFO/path attacks, and concurrent processes.
- [x] Run RED for the missing durable registry.
- [x] Refactor only the minimal lock/root validation helpers from `runtime_registry.cpp` so both registries acquire the same exclusive lock and share ownership/mode policy.
- [x] Implement canonical hash-chained Logic journal and recoverable snapshot; verify package and worker again under lock before every mutation/binding.
- [x] Run Logic registry tests plus all existing Data registry and in-memory Logic tests.
- [ ] Commit when Git is restored: `feat: persist local logic mod lifecycle`.

### Task 10: Enforce one installed namespace and one prospective active tree

**Files:**
- Create: `include/bruce/mod/installed_mod_host_v2.hpp`
- Create: `src/mod/installed_mod_host_v2.cpp`
- Create: `tests/unit/installed_mod_host_v2_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add `InstalledModHostV2::open/open_existing`, common `install`, `activate`, `disable`, `snapshot`, and `active_tree` operations.
- Reserve every built-in ID; reject cross-kind installed IDs and same ID/version with differing digest.
- Validate the complete Data+Logic prospective tree before commit.

- [x] Test built-in collision, Data↔Logic collision, idempotent same-digest install, different-digest conflict, two versions, cross-kind tree conflict, and no state change after rejection.
- [x] Run RED for absent host.
- [x] Implement one lock-scoped orchestration layer; do not nest independent registry locks. Stage verified bytes first, compute candidate records/tree, then append the affected kind's journal and publish its snapshot in the existing recoverable order.
- [x] Add failpoint tests around package, worker, journal, and snapshot publication boundaries and assert reopening yields either the old or complete new state, never a split active view.
- [x] Run common-host, Data registry, Logic registry, and tree tests.
- [ ] Commit when Git is restored: `feat: unify installed mod namespace and activation`.

### Task 11: Unify install, activate, disable, list, and status CLI paths

**Files:**
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `tests/unit/runtime_mod_cli_tests.cpp`
- Modify: `tests/integration/test_runtime_mods.py`
- Create: `tests/integration/test_logic_mod_lifecycle.py`

**Interfaces:** Existing spellings remain accepted. `mods install` detects kind; `activate ID VERSION` and `disable ID` target the unique installed kind; list/status include all built-ins and installed versions.

- [x] Add end-to-end tests for Data and Logic install inactive, explicit activation, disable, multiple versions, rollback, collisions, missing root/package/trust, malformed stores, conflicting options, canonical output, and generation semantics (unit host tests cover atomic collision/generation cases; CLI tests cover both package kinds).
- [x] Run RED against Data-only commands.
- [x] Route lifecycle commands through `InstalledModHostV2`; built-in lifecycle attempts return `BuiltinModImmutable` without opening/mutating the root.
- [x] Preserve invocation compatibility for existing Data CLI calls; document the intentional list response upgrade from registry V1 to catalog V2.
- [x] Run both new workflows and existing runtime/Logic session workflows.
- [ ] Commit when Git is restored: `feat: unify data and logic mod lifecycle cli`.

### Task 12: Add explicit installed Logic execution

**Files:**
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `include/bruce/mod/logic_mod_registry_v1.hpp`
- Modify: `src/mod/logic_mod_registry_v1.cpp`
- Create: `tests/integration/test_installed_logic_mod_run.py`

**Interfaces:** `bruce mods run ID --template TEMPLATE --input TEXT --mod-root ROOT` runs only the active installed Logic version using a fresh digest-pinned binding and the existing isolated worker runner.

- [x] Test valid run, inactive/disabled/Data/built-in ID, unknown template, oversized input, crash, timeout, tampered worker, concurrent disable, and repeated deterministic run identity. Capability is intentionally fixed to `worker.execute`; output limiting remains covered by the reused worker suite.
- [x] Run RED for unknown action.
- [x] Bind under registry validation, release the registry lock before child execution, run through existing worker supervision, and return structured host-validated result/evidence; never execute a pathname reopened from package source.
- [x] Run installed-run tests plus `logic_mod_plugin_v1_contracts/workflows`, worker boundary, payload, and result-artifact regressions.
- [ ] Commit when Git is restored: `feat: run active installed logic mods`.

### Task 13: Implement atomic side-by-side update and rollback

**Files:**
- Modify: `include/bruce/mod/installed_mod_host_v2.hpp`
- Modify: `src/mod/installed_mod_host_v2.cpp`
- Modify: `apps/runtime_mod_cli.hpp`
- Create: `tests/unit/mod_update_v2_tests.cpp`
- Create: `tests/integration/test_mod_update.py`
- Modify: `CMakeLists.txt`

**Interfaces:** `bruce mods update PACKAGE --mod-root ROOT --trust-store TRUST` performs verify → conflict/prospective-tree check → side-by-side install → activation. The prior version remains installed. Reapplying the already-active same digest is a no-op with no generation change.

- [x] Add Data and Logic update tests for success, inactive activation, active idempotence, same-version digest conflict, retained state, rollback, and pre/post journal failpoints. Shared package/host suites cover bad trust/signature, Built-in/cross-kind, Golden/tree, worker extraction, and every artifact publication boundary used by update.
- [x] Run RED for the absent host/CLI update action.
- [x] Implement one host transaction plan with prevalidated staged content and the existing journal commit authority sufficient to recover an interrupted activation without exposing a mixed snapshot.
- [x] Assert failures retain the previous journal/generation; assert success retains old package/version and supports rollback via `activate`.
- [x] Run update, registry, host, both lifecycle, and installed Logic run neighbors.
- [ ] Commit when Git is restored: `feat: add atomic local mod updates and rollback`.

### Task 14: Add opt-in route ownership details

**Files:**
- Modify: `apps/runtime_mod_cli.hpp`
- Modify: `tests/unit/runtime_mod_cli_tests.cpp`
- Modify: `tests/integration/test_runtime_mods.py`

**Interfaces:** Existing `route` and `route replay` remain byte-identical V1. `route --mod-details` emits `route-report-v2` with selected/candidate owner, origin, kind, version, and package digest where applicable.

- [x] Pin current V1 fixture bytes; add V2 Core-as-builtin, Data, Logic, unknown, ambiguous, and replay tests. The three optional Built-ins do not mount routing nodes, so Core is the only current Built-in route owner.
- [x] Run RED for the new flag against the baseline binary while the pre-existing V1 cases remained independently exercised.
- [x] Add ownership lookup from the unified active snapshot without changing normalization/matching; use null only for unmatched/ambiguous selection and Core's non-package version/digest.
- [x] Run V1 byte-compatibility and V2 semantic/replay suites.
- [ ] Commit when Git is restored: `feat: report route mod ownership on request`.

## Wave 4 — Documentation and release evidence

### Task 15: Document only the implemented Mod system

**Files:**
- Modify: `README.md`
- Modify: `docs/README.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/contracts/README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: relevant MCP documentation referenced by `tools/bruce_mcp.py`
- Generate only through: `docs/cli/verify_examples.py`

- [x] Update help grammar, examples, catalog/package/journal/report contracts, local-user trust boundary, multi-version update/rollback behavior, OFF builds, and explicit exclusions.
- [x] Clearly separate built-in HTTP MCP inspector from the stdio MCP protocol adapter and state which one each command/tool invokes.
- [x] Run the CLI verifier in a fresh full temporary copy. Compare `source-index.md`, `examples.json`, and `validation.md` byte-for-byte with the repository; apply generator-driven changes only.
- [x] Run all local Markdown-link checks and scan documented commands/options against parser/tests.
- [ ] Commit when Git is restored: `docs: describe complete local mod lifecycle`.

### Task 16: Full clean verification and independent review

**Files:**
- Create only if the repository's existing evidence convention requires it: `runs/complete-mod-system/reports/final-review.md`

- [x] Make a full temporary source copy at a native path without spaces/colons and record its digest/baseline. Never use an old build as sole evidence.
- [x] Configure, build, and run Release tests with `BUILD_TESTING=ON`; record exact CTest totals, skips, disabled tests, and any sandbox-only retries separately.
- [x] Run Debug and configured sanitizer/static-analysis/format targets only when present; report absent tooling as not configured, not failed.
- [x] Run all six valid built-in configurations, both invalid MCP dependency configurations, builder/lifecycle/update workflows, deterministic repetitions, stdio MCP tests, built-in HTTP MCP tests, and the CLI doc verifier.
- [x] Run `git diff --check`, local link validation, secret-pattern scan over intended report output, and final status. If Git remains unavailable, compare against the initial byte baseline and state that Git verification/commits were impossible.
- [x] Review the whole change specifically for lock ordering, crash recovery, TOCTOU, path traversal/symlink/hardlink handling, bounded parsing/output, error redaction, old contract compatibility, and docs overclaiming.
- [x] Classify every result using the repository labels and do not call the system fully functional unless all relevant checks pass.
- [ ] Commit when Git is restored: `test: verify complete local mod system`.

## Execution checkpoints

1. After Tasks 1–3, Built-in Mods are independently releasable and all OFF builds are proven.
2. After Tasks 4–8, catalog, inspection, and both package builders are independently usable without mutating installations.
3. After Tasks 9–14, local Data/Logic lifecycle, execution, update, and rollback form one coherent product.
4. Tasks 15–16 may claim implementation only for checks that actually passed in the final clean copies.

## Plan Self-Review

- The dependency order is acyclic: feature inventory → unified projection → package detection/building → durable Logic state → cross-kind host → CLI/update/run → docs/evidence.
- Built-ins are never repackaged and installed mods never masquerade as built-ins.
- Legacy Data files and the low-level in-memory Logic session remain compatibility surfaces; new functionality is additive except the explicitly versioned `mods list` response upgrade.
- The update design retains old versions and requires explicit rollback activation, matching the approved product decision. :codex-annotation{index="1"}
- No task introduces a BRUCE system user, daemon, server-side multi-tenancy, downloads, or automatic activation outside the explicit `update` command.
- Every mutation has negative, concurrency, crash-boundary, and no-state-change assertions before documentation is updated.
