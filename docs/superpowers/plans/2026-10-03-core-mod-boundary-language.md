# Core-Mod-Grenze und Language-Mod Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the first static BRUCE mod boundary and migrate the existing deterministic language path into the optional built-in Language-Mod without changing Core mathematics, status semantics, or legacy language replay.

**Architecture:** bruce_core remains unaware of concrete mods. A small bruce_mod_runtime provides manifest validation, a Core-run adapter for the eight existing ApplicationTask variants, and a versioned mod envelope. bruce_mod_language owns the migrated language source and its manifest; a separately linked catalog knows only the CMake-enabled built-in mods.

**Tech Stack:** C++20, existing CMake/Ninja build, GMP/GMPXX, OpenSSL SHA-256, existing C++ unit-test style and Python unittest integration tests.

**Spec:** `docs/superpowers/specs/2026-10-03-core-mod-architecture-design.md`

## Global Constraints

- Build only static, CMake-configured mods. Do not add .so discovery, runtime loading, sandboxing, networking, databases, GPU work, or a marketplace.
- bruce_core must not include, link, register, or branch on a concrete mod.
- The mod-run adapter accepts only the existing eight ApplicationTask variants; a new task type requires a separate Core contract.
- Only existing Core execution, replay and gates determine a result status. A mod must not activate rules, promote experience, alter budgets, or create a proof claim.
- BRUCE_BUILD_MOD_LANGUAGE=ON is the first concrete mod option. Do not add options for NanoCell, Cockpit or Phase-DAG until their own targets exist.
- Preserve the direct BRUCE_LANGUAGE_RUN_V1 replay path. New mod-initiated language exports use a versioned outer envelope instead of changing V1 bytes.
- Work from a native checkout path without : when using scripts/with-host-deps.sh; the GVFS-SFTP path deliberately rejects that wrapper.
- Preserve source history with git mv for every moved tracked Language-Mod file; update includes, CMake, tests and docs in the same commit.

## Native Build Location

All build commands run from a native checkout root, never from its GVFS-SFTP
mount. Configure the reusable enabled build before the first focused test:

~~~bash
scripts/with-host-deps.sh cmake -S . -B build/mod-language-on -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DBRUCE_BUILD_MOD_LANGUAGE=ON
scripts/with-host-deps.sh cmake --build build/mod-language-on
~~~

## Review Focus

- A forged, empty, duplicate, incompatible or unknown mod manifest must fail before Core execution — Task 1.
- An outer mod envelope with a replaced input, replaced nested language run, changed manifest or renewed outer digest must fail replay — Tasks 1 and 3.
- A language run through the mod adapter must preserve INCOMPLETE, Replay, gate fields and exact Core bundle bytes — Task 3.
- A disabled Language-Mod must not compile or register language code, while ordinary Core commands continue to build and run — Tasks 2 and 4.
- A direct historical BRUCE_LANGUAGE_RUN_V1 file must remain replayable after the Language-Mod migration — Task 3.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `include/bruce/mod/mod_api.hpp` | Stable V1 manifest, Core-run request and mod-envelope types/functions. |
| `src/mod/mod_api.cpp` | Canonical manifest/envelope validation, packing/unpacking and delegation to the checked Core session. |
| `include/bruce/mod/catalog.hpp` | Read-only fixed built-in Mod catalog. |
| `src/mod/catalog.cpp` | CMake-selected catalog assembly; never part of bruce_core. |
| `include/bruce/mods/language/mod.hpp` | Language-Mod ID, version and manifest. |
| `include/bruce/mods/language/language.hpp` | Moved public language draft contract. |
| `include/bruce/mods/language/language_run.hpp` | Moved language-to-Core bridge and replay contract. |
| `mods/language/CMakeLists.txt` | Builds bruce_mod_language only when selected. |
| `mods/language/src/*.cpp` | Moved deterministic grammar, language run and manifest implementation. |
| `tests/unit/mod_api_tests.cpp` | Manifest, envelope and Core-adapter boundary tests. |
| `tests/unit/mod_catalog_tests.cpp` | Catalog identity and CMake-enabled membership tests. |
| `tests/mods/language/*` | Moved language tests, extended with Mod provenance and legacy replay. |
| `apps/bruce_cli.cpp` | Uses the enabled Language-Mod and writes/reads the outer mod envelope. |
| `CMakeLists.txt` | Separates Core/runtime/catalog targets, adds the Language-Mod option and updates source-index inputs. |

### Task 1: Define the versioned Mod API and Core-run boundary

**Files:**
- Create: `include/bruce/mod/mod_api.hpp`
- Create: `src/mod/mod_api.cpp`
- Create: `tests/unit/mod_api_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Define `mod_api_version_v1 = "bruce-mod-api-v1"`.
- Define `enum class ModCapabilityV1 { language_interpretation, core_task_submission, presentation, phase_execution }`.
- Define `ModManifestV1 { id, version, api_version, capabilities }`.
- Define `ModCoreRunRequestV1 { const ModManifestV1& manifest; ApplicationTask task; ExecutionControl control; VerificationBudget verification_budget; VerificationControl verification; SessionBudget session_budget; SerializationBudget serialization_budget; }`.
- Define `ExecutionSession execute_mod_core_run(const ModCoreRunRequestV1&)`; it validates the manifest and delegates unchanged to execute_checked_application.
- Define `ModRunEnvelopeV1 { manifest, input_codec, input_bytes, input_digest, payload_codec, payload_bytes, payload_digest }` and canonical `pack_mod_run_envelope` / `unpack_mod_run_envelope` under `BRUCE_MOD_RUN_V1`.

- [ ] **Step 1: Write the failing Mod-API tests**

Create the unit test in the repository's existing require(...) style. Assert that a static language manifest with nonempty ID/version, exact API version and unique capabilities is accepted. Empty/invalid IDs, empty versions, duplicate capabilities and a different API version must fail.

Pack/unpack an envelope with codecs bruce-language-draft-v1 and bruce-language-run-v1. Assert byte-canonical round trip and rejection of changed input/payload bytes, a changed manifest field with renewed outer digest, a noncanonical length and a trailing byte.

Use a small existing ApplicationTask fixture. Assert that execute_mod_core_run yields the same rendered bundle and ready session as direct execute_checked_application. An invalid manifest must fail before the fixture executes.

- [ ] **Step 2: Run the new target to verify it fails**

Run: `scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^mod_api_contracts$'`

Expected: FAIL because the Mod API target and interfaces do not exist.

- [ ] **Step 3: Implement the V1 boundary**

Implement the declared types. Validate IDs and codec identifiers as nonempty ASCII tokens matching [A-Za-z0-9_.-] with a 64-byte maximum; require an exact API version, nonempty version and no repeated capability. Use the repository's canonical Header + SHA-256 + length-prefixed-body style. Re-encode after decoding and reject noncanonical input.

execute_mod_core_run validates the manifest before passing every request field unchanged to execute_checked_application. It must not catch, translate or promote Core results/errors.

Create bruce_mod_runtime from src/mod/mod_api.cpp, link it publicly to bruce_core, and register the new unit target as mod_api_contracts.

- [ ] **Step 4: Run focused API checks**

Run: `scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^(mod_api_contracts|application_task_adapters|execution_session_states)$'`

Expected: PASS. The Core groups retain their prior results; the new boundary adds no concrete Mod dependency to bruce_core.

- [ ] **Step 5: Commit the independently reviewable boundary**

~~~bash
git add include/bruce/mod/mod_api.hpp src/mod/mod_api.cpp tests/unit/mod_api_tests.cpp CMakeLists.txt
git commit -m "feat: add static mod runtime boundary"
~~~

### Task 2: Build the fixed catalog and optional Language-Mod target

**Files:**
- Create: `include/bruce/mod/catalog.hpp`
- Create: `src/mod/catalog.cpp`
- Create: `include/bruce/mods/language/mod.hpp`
- Create: `mods/language/CMakeLists.txt`
- Create: `mods/language/src/mod.cpp`
- Create: `tests/unit/mod_catalog_tests.cpp`
- Create: `tests/integration/test_mod_build.py`
- Modify: `CMakeLists.txt`
- Modify: `apps/bruce_cli.cpp`

**Interfaces:**
- Define `const ModManifestV1& language_mod_manifest_v1();` with ID language, Mod version language-mod-v1, API version bruce-mod-api-v1, and capabilities language_interpretation plus core_task_submission.
- Define `class ModCatalogV1` with constructor `explicit ModCatalogV1(std::span<const ModManifestV1> manifests);`, `manifests()` and `find(std::string_view id)`.
- Define `const ModCatalogV1& builtin_mod_catalog_v1();`. Its members are exactly the CMake-enabled static Mods, bytewise sorted by ID.
- Add CMake option `BRUCE_BUILD_MOD_LANGUAGE` default ON. bruce_mod_catalog and bruce receive the private definition BRUCE_BUILD_MOD_LANGUAGE=0/1.

- [ ] **Step 1: Write failing catalog and disabled-build tests**

In mod_catalog_tests, assert the default catalog contains exactly language; its API/version/capabilities match the manifest; unknown lookup returns null; and duplicate or noncanonical manifest IDs are rejected.

Create test_mod_build.py. It configures a temporary native build with BRUCE_BUILD_MOD_LANGUAGE=OFF, builds bruce, verifies bruce --version and one existing non-language command succeed, then verifies bruce language ... follows the existing unknown-command error route. It must not download dependencies or change the source tree.

- [ ] **Step 2: Run the catalog tests to verify they fail**

Run:

~~~bash
scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^mod_catalog_contracts$'
scripts/with-host-deps.sh python3 tests/integration/test_mod_build.py --cmake cmake --source . --build /tmp/bruce-mod-language-off
~~~

Expected: FAIL because the catalog, target and CMake option are absent.

- [ ] **Step 3: Implement fixed compile-time registration**

Keep bruce_core free of every src/mod or mods/ source. Add bruce_mod_catalog from src/mod/catalog.cpp. With the option on, add_subdirectory(mods/language), link the catalog to bruce_mod_language and assemble exactly its static manifest. With it off, compile an empty catalog without Language symbols.

Add mods/*.cpp and mods/CMakeLists.txt to the existing source digest inputs. Link bruce to the catalog; guard the Language header and command branch with BRUCE_BUILD_MOD_LANGUAGE. Do not add NanoCell/Cockpit/Phase-DAG options.

- [ ] **Step 4: Run both build modes**

~~~bash
scripts/with-host-deps.sh cmake -S . -B build/mod-language-on -G Ninja -DBUILD_TESTING=ON -DBRUCE_BUILD_MOD_LANGUAGE=ON
scripts/with-host-deps.sh cmake --build build/mod-language-on
scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^(mod_api_contracts|mod_catalog_contracts)$'
scripts/with-host-deps.sh python3 tests/integration/test_mod_build.py --cmake cmake --source . --build /tmp/bruce-mod-language-off
~~~

Expected: both new groups pass; the nested OFF build proves an absent Language-Mod leaves the ordinary Core CLI usable.

- [ ] **Step 5: Commit catalog and option**

~~~bash
git add include/bruce/mod/catalog.hpp src/mod/catalog.cpp include/bruce/mods/language/mod.hpp mods/language/CMakeLists.txt mods/language/src/mod.cpp tests/unit/mod_catalog_tests.cpp tests/integration/test_mod_build.py CMakeLists.txt apps/bruce_cli.cpp
git commit -m "feat: build language as optional static mod"
~~~

### Task 3: Migrate Language and bind new exports to Mod provenance

**Files:**
- Move: `include/bruce/language/language.hpp` → `include/bruce/mods/language/language.hpp`
- Move: `include/bruce/application/language_run.hpp` → `include/bruce/mods/language/language_run.hpp`
- Move: `src/language/language.cpp` → `mods/language/src/language.cpp`
- Move: `src/application/language_run.cpp` → `mods/language/src/language_run.cpp`
- Move: `tests/unit/language_tests.cpp` → `tests/mods/language/language_tests.cpp`
- Move: `tests/unit/language_run_tests.cpp` → `tests/mods/language/language_run_tests.cpp`
- Move: `tests/integration/test_language.py` → `tests/mods/language/test_language.py`
- Modify: `apps/bruce_cli.cpp` and every existing reference returned by `git grep -n 'bruce/language\\|application/language_run\\|test_language.py\\|language_tests.cpp'`
- Modify: `CMakeLists.txt`

**Interfaces:**
- execute_language_draft uses `execute_mod_core_run(ModCoreRunRequestV1{language_mod_manifest_v1(), ...})`; it may not call execute_checked_application directly.
- New language execute exports are a BRUCE_MOD_RUN_V1 envelope with manifest language-mod-v1, input codec bruce-language-draft-v1, payload codec bruce-language-run-v1, canonical draft bytes and canonical inner BRUCE_LANGUAGE_RUN_V1 bytes.
- language replay recognizes the new envelope and direct historical BRUCE_LANGUAGE_RUN_V1. The direct route stays read-only compatibility; execute never writes V1 directly.

- [ ] **Step 1: Extend moved tests before code migration**

Move the three existing language tests with git mv and preserve their prior assertions. Add assertions that a new execute export carries manifest ID language, Mod version language-mod-v1, exact API version and canonical draft/run digests. Replay must reject a replaced inner language bundle, a replaced draft and a manifest-version change under a renewed outer digest.

Build a fixture from the preserved direct BRUCE_LANGUAGE_RUN_V1 path and assert language replay still renders and verifies it. Retain existing assertions for ambiguity, source binding, INCOMPLETE, Core bundle stability and no promoted gate/proof status.

- [ ] **Step 2: Run moved tests to verify they fail**

Run: `scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^(language_contracts|language_run_contracts|language_workflows)$'`

Expected: FAIL because CMake still names old locations and no Mod envelope is emitted.

- [ ] **Step 3: Move the code and apply the adapter**

Use git mv for every listed existing Language-Mod file. Update include paths, source/test paths, CTest names and documentation references together. Remove language sources from bruce_core; add them only to bruce_mod_language, which links bruce_mod_runtime.

In the moved bridge, replace the direct checked-session call with Task 1's adapter and static Language manifest. Keep grammar, task construction, native bundle creation, explanation, phase sequence and BRUCE_LANGUAGE_RUN_V1 packing/replay bytes unchanged.

At the CLI boundary, pack the canonical draft and Language run into the Mod envelope before exclusive export. On replay, validate envelope first, require the Language manifest/codecs, require the embedded draft equals the inner run draft, then replay it. Preserve the direct V1 replay branch, JSON error envelope and exit status.

- [ ] **Step 4: Run language and Core regressions**

~~~bash
scripts/with-host-deps.sh ctest --test-dir build/mod-language-on --output-on-failure -R '^(mod_api_contracts|mod_catalog_contracts|language_contracts|language_run_contracts|language_workflows|application_task_adapters|execution_session_states)$'
scripts/with-host-deps.sh python3 tests/mods/language/test_language.py build/mod-language-on/bruce
~~~

Expected: PASS. A current Language-Mod export is envelope-bound; direct V1 replay remains accepted; Core status and gate fields remain unchanged.

- [ ] **Step 5: Commit the migration**

~~~bash
git add -A include/bruce/mods/language mods/language tests/mods/language apps/bruce_cli.cpp CMakeLists.txt
git add -u include/bruce/language include/bruce/application src/language src/application tests/unit tests/integration
git commit -m "feat: migrate language into static mod"
~~~

### Task 4: Document and validate the completed first Mod

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`, `commands.md`, `formats.md` and `control-overview.md`
- Modify: `docs/cli/verify_examples.py`
- Modify: `docs/superpowers/specs/2026-10-03-core-mod-architecture-design.md`

**Interfaces:**
- Documents BRUCE_BUILD_MOD_LANGUAGE, catalog behavior, BRUCE_MOD_RUN_V1 and direct-language V1 replay.
- Marks only the Core-Mod boundary and Language-Mod as IMPLEMENTIERT UND BELEGT after validation. NanoCell, Cockpit and Phase-DAG remain DOKUMENTIERT / KONZEPT.

- [ ] **Step 1: Write failing documentation-verifier expectations**

Extend verify_examples.py with one enabled Language-Mod command, one rejected disabled/unknown Mod path, one envelope replay and one direct V1 replay fixture. The generated command/option indexes must include language only and must not call NanoCell, Cockpit or Phase-DAG implemented Mods.

- [ ] **Step 2: Run the verifier to confirm it fails**

Run: `scripts/with-host-deps.sh python3 docs/cli/verify_examples.py build/mod-language-on/bruce`

Expected: FAIL until commands, formats and fixtures document the envelope and optional Mod behavior.

- [ ] **Step 3: Update only affected documentation**

Describe the CMake option and no-dynamic-plugin boundary in the root README. Update the consolidated architecture's Language row from a separate bounded path to an optional built-in Mod, retaining its no-free-dialog limit. Describe the outer envelope and V1 compatibility in CLI documents. Keep NanoCell isolated and Cockpit/Phase-DAG conceptual.

- [ ] **Step 4: Run final native validation**

~~~bash
scripts/with-host-deps.sh cmake -S . -B build/mod-language-final -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DBRUCE_BUILD_MOD_LANGUAGE=ON
scripts/with-host-deps.sh cmake --build build/mod-language-final --parallel 16
scripts/with-host-deps.sh ctest --test-dir build/mod-language-final --output-on-failure --parallel 8
scripts/with-host-deps.sh python3 docs/cli/verify_examples.py build/mod-language-final/bruce
git diff --check
git status --short
~~~

Expected: configure/build succeed; every registered CTest group and the documentation verifier pass; git diff --check is empty; status contains only intentional implementation changes before the final commit.

- [ ] **Step 5: Commit documentation and validation-ready result**

~~~bash
git add README.md docs/architecture/03-blueprint-consolidation.md docs/cli docs/superpowers/specs/2026-10-03-core-mod-architecture-design.md
git commit -m "docs: document language mod boundary"
~~~

## Plan Self-Review

- **Spec coverage:** Task 1 provides API, provenance envelope and checked Core adapter. Task 2 delivers CMake-selected static registration and disabled-Mod evidence. Task 3 migrates Language and preserves direct V1 replay. Task 4 documents and validates the first slice. NanoCell, Cockpit and Phase-DAG remain separate future plans.
- **Type consistency:** ModManifestV1, ModCoreRunRequestV1 and ModRunEnvelopeV1 originate in Task 1. Task 2 exports only static Language manifests through ModCatalogV1. Task 3 consumes that same manifest for Core submission and envelope provenance.
- **Review focus:** Manifest/envelope failure modes are tested in Task 1; disabled builds in Task 2; status, provenance and V1 replay in Task 3; documented command claims in Task 4.
- **Proportion and scope:** This is one complete vertical slice. It does not prescribe NanoCell, Cockpit or Phase-DAG implementation and avoids a cosmetic move outside the Language files.
