# Runtime Tree Data Mods Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first runtime-extension vertical slice: signed user-scoped data-mod packages can extend a deterministic tree, be explicitly activated, and route controlled text to `MATCHED`, `UNKNOWN`, or `AMBIGUOUS` without guessing.

**Architecture:** Add a versioned tree registry, a controlled route normalizer, an Ed25519-verified package codec, and an atomically updated per-user mod registry. The CLI mounts only active data mods; package code workers, GPU budgets, and WebUI remain separate follow-up plans because their sandbox boundary is an independent security subsystem.

**Tech Stack:** C++20, CMake/CTest, OpenSSL Crypto (SHA-256 and Ed25519 EVP), existing `LocalFiles` POSIX persistence adapter, Python CLI integration tests.

**Spec:** `docs/superpowers/specs/2026-10-09-runtime-tree-mod-host-design.md`

## Scope and prerequisite

This is plan 1 of 3. It implements declarative data mods, registry lifecycle,
tree routing, and activation. A later plan implements sandboxed Logic-Mod
workers; a third plan implements budget queues and GPU/Lab policy.

The current source tree does not build because `apps/bruce_cli.cpp` includes
the absent `management_inspection_cli.hpp`. Resolve that unrelated blocker in
its own reviewed repair before beginning Task 1. Do not silently remove the
include: recover or deliberately retract the intended management-inspection
contract first.

## Global Constraints

- Keep C++20, CMake 3.20, GMP/GMPXX, OpenSSL Crypto, CTest, and no new package manager dependency.
- A runtime package is data only in this plan: no worker executable, shared library, shell, network, GPU, or LLM hook.
- A `TreeAddressV1` is a non-empty sequence of positive `uint32_t` values; its meaning is never reused after removal.
- Routing is deterministic: exactly one match is `MATCHED`; zero is `UNKNOWN`; more than one is `AMBIGUOUS`.
- Data mods may add children only below their declared mount; they never overwrite Core nodes or active foreign nodes.
- Each active run and route report includes the tree-registry digest, mod ID, version, and package digest when applicable.
- Package verification uses Ed25519 signatures through OpenSSL EVP; test-only keys are never trusted by a production default truststore.
- Package installation is initially inactive; activate, disable, and system-wide promotion remain explicit distinct actions.
- Per-user roots are explicit CLI paths in V1; never infer a cross-user location or scan directories for executable code.
- All package/registry writes are locked, atomic, durable, and journaled; existing files are never silently overwritten.
- Treat unknown, malformed, duplicate, unsigned, untrusted, or ambiguous input as rejection, not fallback routing.

## Review Focus

- Alias collision after two otherwise valid user mods mount below one parent must yield `AMBIGUOUS`, never installation-order routing — Task 2.
- A package whose bytes change after signing must be rejected before tree parsing or installation — Task 3.
- A user mod claiming a Core address or a sibling's active address must not alter the active registry — Task 4.
- Concurrent activation from two processes must preserve one coherent generation and journal chain — Task 4.
- A route report replayed with a changed registry digest must return a version error instead of a new route — Task 5.

---

## File structure

| File | Responsibility |
| --- | --- |
| `include/bruce/tree/registry.hpp` | Tree addresses, nodes, fragments, claim scopes, registry API and route result types |
| `src/tree/registry.cpp` | Canonical validation, mounting, digesting, deterministic matching |
| `include/bruce/tree/normalizer.hpp` | Controlled route-text normalization API |
| `src/tree/normalizer.cpp` | ASCII whitespace/case normalization and literal numeric placeholders |
| `include/bruce/mod/runtime_package.hpp` | Data-mod manifest, package and truststore types, package verification API |
| `src/mod/runtime_package.cpp` | Canonical V1 container parsing, constrained JSON schema parsing, SHA-256 and Ed25519 verification |
| `include/bruce/mod/runtime_registry.hpp` | Installed/active package registry and journal API |
| `src/mod/runtime_registry.cpp` | User-root layout, locked atomic lifecycle transitions and active-tree assembly |
| `apps/runtime_mod_cli.hpp` | `bruce mods …` and `bruce route …` command adapter |
| `apps/bruce_cli.cpp` | Delegate the two new top-level commands to the adapter |
| `CMakeLists.txt` | Compile the new sources and register C++/Python tests |
| `tests/unit/tree_registry_tests.cpp` | Tree and routing contracts |
| `tests/unit/runtime_package_tests.cpp` | Package schema, digest, signature and trust contracts |
| `tests/unit/runtime_registry_tests.cpp` | Lifecycle, ownership, journal and concurrency contracts |
| `tests/integration/test_runtime_mods.py` | End-to-end CLI installation, activation, routing and replay behaviour |
| `tests/fixtures/runtime_mods/` | Checked-in non-secret package, public key and negative fixtures |
| `docs/cli/commands.md` | Human-readable command contract |

## Task 1: Tree model and immutable mounting

**Files:**
- Create: `include/bruce/tree/registry.hpp`
- Create: `src/tree/registry.cpp`
- Create: `tests/unit/tree_registry_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `using TreeAddressV1 = std::vector<std::uint32_t>;`.
- Produces `enum class HostClaimScopeV1 { unknown, candidate_only, verified_within_supported_scope, verified_exact_result, explanation_only, research_only, requires_review, denied };`.
- Produces `TreeNodeV1`, `TreeFragmentV1`, `TreeRegistryV1`, `RouteStatusV1`, and `RouteResultV1`.
- Produces `TreeRegistryV1 TreeRegistryV1::mount(TreeFragmentV1 fragment, std::string_view owner_id) const;` and `std::string TreeRegistryV1::digest() const;`.
- Produces `TreeRegistryV1 core_tree_registry_v1();` with Core-owned roots `[0]`, `[1]`, `[1,1]`, `[1,1,1]` through `[1,1,4]`.
- Consumed by Tasks 2, 4, and 5.

- [ ] **Step 1: Write failing unit tests for canonical core nodes and successful child mounting**

Cover core paths `[1]`, `[1,1]`, `[1,1,1]` and a mod-owned `[1,1,5]`; assert canonical digest stability across equivalent insertion order.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `cmake --build <build-dir> --target bruce_tree_registry_tests && ctest --test-dir <build-dir> -R tree_registry_contracts --output-on-failure`

Expected: FAIL because the target and tree API do not yet exist.

- [ ] **Step 3: Implement the immutable tree API and canonical digest**

Reject empty/zero addresses, missing parents, duplicate IDs, duplicated keys below one parent, non-descendant mounts, and attempts to replace a node owned by another owner. Serialize nodes sorted lexicographically by address and hash that canonical representation with existing `sha256`.

- [ ] **Step 4: Add negative tests for Core overwrite and invalid mount ancestry**

Assert both errors leave the original registry and digest unchanged.

- [ ] **Step 5: Register the target and run focused verification**

Add `bruce_tree_registry_tests` and CTest name `tree_registry_contracts` following existing unit-test conventions.

Run: `ctest --test-dir <build-dir> -R tree_registry_contracts --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/bruce/tree/registry.hpp src/tree/registry.cpp tests/unit/tree_registry_tests.cpp CMakeLists.txt
git commit -m "feat: add immutable runtime tree registry"
```

## Task 2: Controlled normalizer and deterministic route matcher

**Files:**
- Create: `include/bruce/tree/normalizer.hpp`
- Create: `src/tree/normalizer.cpp`
- Modify: `include/bruce/tree/registry.hpp`
- Modify: `src/tree/registry.cpp`
- Modify: `tests/unit/tree_registry_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `TreeRegistryV1`, `TreeNodeV1`, `RouteStatusV1`, and `RouteResultV1` from Task 1.
- Produces `NormalizedRouteInputV1 normalize_route_text_v1(std::string_view text);`.
- Produces `RouteResultV1 TreeRegistryV1::route(const NormalizedRouteInputV1&) const;`.
- `TreeNodeV1` gains a canonical `std::vector<std::string> route_pattern`, whose atoms are literals or exactly `<rational>` and `<integer>`.
- Consumed by Tasks 4 and 5.

- [ ] **Step 1: Write failing unit tests for exact controlled phrases**

Assert that `3/4 plus 5/6` matches the Core addition template, `3/4 hoch 2` can match a mounted power template, unknown verbs yield `UNKNOWN`, and a phrase matching two active patterns yields `AMBIGUOUS`.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `ctest --test-dir <build-dir> -R tree_registry_contracts --output-on-failure`

Expected: FAIL because no normalizer or matcher exists.

- [ ] **Step 3: Implement `normalize_route_text_v1` and route matching**

Normalize ASCII case and whitespace exactly as `token_normalization_version_v1`; recognize literal rational values `[-]digits[/digits]` and integers `[-]digits` as placeholders. Do not infer synonyms, reorder tokens, repair spelling, or use fuzzy matching.

- [ ] **Step 4: Add the alias-collision regression test from Review Focus**

Mount two separately valid fragments below the same parent whose active patterns both match `3/4 hoch 2`; assert `AMBIGUOUS`, all candidate addresses, and no chosen template.

- [ ] **Step 5: Run focused verification**

Run: `ctest --test-dir <build-dir> -R tree_registry_contracts --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/bruce/tree/normalizer.hpp src/tree/normalizer.cpp include/bruce/tree/registry.hpp src/tree/registry.cpp tests/unit/tree_registry_tests.cpp CMakeLists.txt
git commit -m "feat: route controlled text through runtime tree"
```

## Task 3: Signed declarative data-mod package codec

**Files:**
- Create: `include/bruce/mod/runtime_package.hpp`
- Create: `src/mod/runtime_package.cpp`
- Create: `tests/unit/runtime_package_tests.cpp`
- Create: `tests/fixtures/runtime_mods/README.md`
- Create: `tests/fixtures/runtime_mods/math-rational-power-v1.brucemod`
- Create: `tests/fixtures/runtime_mods/test-truststore-v1.pub`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `TreeFragmentV1` and `HostClaimScopeV1` from Task 1.
- Produces `RuntimeModManifestV1`, `RuntimeDataModPackageV1`, and `RuntimeModTrustStoreV1`.
- Produces `RuntimeDataModPackageV1 unpack_runtime_data_mod_package_v1(std::string_view);`.
- Produces `RuntimeModTrustStoreV1 read_runtime_mod_truststore_v1(std::string_view);`.
- Produces `void verify_runtime_data_mod_package_v1(const RuntimeDataModPackageV1&, const RuntimeModTrustStoreV1&);`.
- Consumed by Tasks 4 and 5.

- [ ] **Step 1: Write failing tests for a trusted package and the signature/digest failure cases**

Use a checked-in, explicitly non-secret Ed25519 test key pair generated only for fixtures. Assert trusted package success; one-byte payload mutation, unknown signing key, invalid signature encoding, and a Logic-Mod capability all fail before tree mounting.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `ctest --test-dir <build-dir> -R runtime_package_contracts --output-on-failure`

Expected: FAIL because the codec and test target do not exist.

- [ ] **Step 3: Implement the canonical `BRUCE_RUNTIME_DATA_MOD_V1` container and strict schema**

Use a length-prefixed canonical container whose named members are `manifest.json`, `tree.json`, `tests/golden-cases.json`, and `signature.ed25519`. The V1 parser accepts only the documented manifest/tree fields and rejects duplicate JSON object keys, unknown required-capability classes, noncanonical serialization, embedded worker files, and total package size above 1 MiB. Verify the package digest before parsing tree content; verify Ed25519 over canonical unsigned payload bytes using OpenSSL EVP.

- [ ] **Step 4: Add the Review Focus mutation test and package-size boundary test**

Assert neither malformed bytes nor an oversized package reaches the mount validator.

- [ ] **Step 5: Register and run the package-contract target**

Run: `ctest --test-dir <build-dir> -R runtime_package_contracts --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/bruce/mod/runtime_package.hpp src/mod/runtime_package.cpp tests/unit/runtime_package_tests.cpp tests/fixtures/runtime_mods CMakeLists.txt
git commit -m "feat: verify signed runtime data mods"
```

## Task 4: User-scoped installed/active registry and activation journal

**Files:**
- Create: `include/bruce/mod/runtime_registry.hpp`
- Create: `src/mod/runtime_registry.cpp`
- Create: `tests/unit/runtime_registry_tests.cpp`
- Modify: `src/io/local_files.cpp`
- Modify: `include/bruce/io/local_files.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes package APIs from Task 3 and registry APIs from Task 1.
- Produces `RuntimeModStateV1 { installed_inactive, active, disabled, revoked };` and `RuntimeModRegistryV1`.
- Produces `RuntimeModRegistryV1 RuntimeModRegistryV1::open(const std::string& mod_root, const RuntimeModTrustStoreV1&);`.
- Produces `void install(const RuntimeDataModPackageV1&);`, `void activate(std::string_view mod_id, std::string_view version);`, `void disable(std::string_view mod_id);`, `TreeRegistryV1 active_tree() const;`, and `std::string journal_digest() const;`.
- Produces `LocalFiles::replace_atomically(const std::string& path, std::string_view content);` for checked state transitions only.
- Consumed by Task 5.

- [ ] **Step 1: Write failing lifecycle tests using a temporary mod root**

Cover `install → installed_inactive → activate → disable`; assert package bytes are content-addressed below `<mod-root>/packages`, registry state is per root, and inactive/disabled mods do not appear in `active_tree()`.

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `ctest --test-dir <build-dir> -R runtime_registry_contracts --output-on-failure`

Expected: FAIL because the registry does not exist.

- [ ] **Step 3: Implement locked, durable registry transitions**

Create `<mod-root>` with owner-only permissions. Store package copies by SHA-256 digest, maintain a canonical registry file and append-only activation journal, use a lock file and reload/recheck state under lock, then atomically replace the registry. Reject activation if the truststore, package bytes, mount, or Golden vectors no longer verify.

- [ ] **Step 4: Add ownership, conflict, and concurrent activation tests**

Attempt Core-address override and duplicate active address; assert no generation changes. Use two processes in the unit harness to race activation; assert one coherent serialized journal chain and no partial registry file.

- [ ] **Step 5: Run focused verification**

Run: `ctest --test-dir <build-dir> -R runtime_registry_contracts --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/bruce/mod/runtime_registry.hpp src/mod/runtime_registry.cpp include/bruce/io/local_files.hpp src/io/local_files.cpp tests/unit/runtime_registry_tests.cpp CMakeLists.txt
git commit -m "feat: manage user runtime data mods"
```

## Task 5: CLI routing, inspection, and reproducible route report

**Files:**
- Create: `apps/runtime_mod_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/integration/test_runtime_mods.py`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/README.md`

**Interfaces:**
- Consumes `RuntimeModRegistryV1` from Task 4 and `TreeRegistryV1::route` from Task 2.
- Adds `bruce mods verify PACKAGE --trust-store PATH`.
- Adds `bruce mods install PACKAGE --mod-root PATH --trust-store PATH`.
- Adds `bruce mods list --mod-root PATH`.
- Adds `bruce mods activate MOD_ID VERSION --mod-root PATH`.
- Adds `bruce mods disable MOD_ID --mod-root PATH`.
- Adds `bruce route --text TEXT --mod-root PATH [--output REPORT]` and `bruce route replay REPORT --mod-root PATH`.
- Produces `route-report-v1` containing route status, candidate addresses, selected template when unique, registry digest, and active package identities/digests.

- [ ] **Step 1: Write an end-to-end Python CLI test for the complete user flow**

In a temporary directory, verify fixture package, install it, assert it is initially inactive, activate it, route `3/4 hoch 2` with `--output route.report`, replay that report, disable the package, and assert that the same route becomes `UNKNOWN`. Parse stdout JSON and assert stderr/exit 2 for an untrusted package.

- [ ] **Step 2: Run the integration test to verify it fails**

Run: `ctest --test-dir <build-dir> -R runtime_mod_workflows --output-on-failure`

Expected: FAIL because the CLI commands do not exist.

- [ ] **Step 3: Implement `runtime_mod_cli` without enlarging `main()` further**

Dispatch `mods` and `route` in `apps/bruce_cli.cpp`; put all option validation and JSON rendering in `apps/runtime_mod_cli.hpp`. Require every path explicitly. Emit only JSON to stdout, use existing simple error JSON and exit 2, and never print truststore or signature material.

- [ ] **Step 4: Add the route-report replay regression test from Review Focus**

Persist a `route-report-v1`, alter the registry by disabling the active package, invoke `bruce route replay route.report --mod-root PATH`, and assert `RouteRegistryDigestMismatch` rather than a newly selected route.

- [ ] **Step 5: Update CLI documentation and regenerate derived CLI artifacts only through the existing verifier**

Document all commands, their explicit paths, inactive-by-default behavior, `route-report-v1`, and `MATCHED`/`UNKNOWN`/`AMBIGUOUS` semantics. Run `docs/cli/verify_examples.py` only in a complete temporary copy as required by `AGENTS.md`; copy back only verifier-produced artifacts when the comparison succeeds.

- [ ] **Step 6: Run focused and full verification**

Run:

```bash
cmake --build <build-dir>
ctest --test-dir <build-dir> -R 'tree_registry_contracts|runtime_package_contracts|runtime_registry_contracts|runtime_mod_workflows' --output-on-failure
ctest --test-dir <build-dir> --output-on-failure
git diff --check
```

Expected: all registered tests pass, generated CLI artifacts are deterministic, and no whitespace errors appear.

- [ ] **Step 7: Commit**

```bash
git add apps/runtime_mod_cli.hpp apps/bruce_cli.cpp CMakeLists.txt tests/integration/test_runtime_mods.py docs/cli
git commit -m "feat: expose runtime data mods and routing"
```

## Deferred plans

### Sandboxed Logic-Mod workers

Before implementation, select and document a Linux-enforced sandbox that can actually deny network and constrain filesystem access. A plain child process is insufficient for that claim. The worker plan will define the `ExecutionRequestV1` / `CandidateResultV1` framing, mandatory host validation, and deny-by-default behavior when the sandbox backend is unavailable.

### Hardware budget and Lab queue

After data mods and worker sandboxing have replay contracts, add `ZERO_GPU`, `CPU_ONLY`, `GPU_MICRO`, `LAB_QUEUE`, and `DENY` admission policy. GPU scheduling, WebUI, MCP, and clusters stay out of that first budget plan.

## Plan self-review

- **Spec coverage:** The tree, signed packages, user ownership, inactive installation, activation journal, deterministic routing, digest binding, and first CPU-only scope map to Tasks 1–5. Worker isolation and hardware scheduling are intentionally separate follow-up plans because their security and platform contracts are independent.
- **Type consistency:** `TreeRegistryV1`, `RuntimeDataModPackageV1`, `RuntimeModTrustStoreV1`, and `RuntimeModRegistryV1` are introduced before later tasks consume them.
- **Review Focus coverage:** Alias collision is Task 2; package mutation is Task 3; ownership collision and concurrent activation are Task 4; registry-digest replay mismatch is Task 5.
- **Proportion:** The plan specifies interfaces, test proofs, and boundaries without prescribing function bodies beyond canonical security-relevant formats.
