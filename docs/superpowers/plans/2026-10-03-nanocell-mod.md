# NanoCell-Mod V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Den bestehenden NanoCell-V1-Kern als optionalen, statisch gebauten BRUCE-Mod ausliefern, der neue Exporte mit der Mod-Herkunft bindet und direkte V1-Replays erhält.

**Architecture:** `bruce_core` verliert die konkrete NanoCell-Implementierung. `bruce_mod_nanocell` besitzt den unveränderten inneren V1-Kern, ein Manifest sowie einen äußeren `BRUCE_MOD_RUN_V1`-Umschlag. Der Mod-Katalog registriert ihn nur bei aktiviertem CMake-Schalter; NanoCell bleibt ein technischer Lauf ohne `ApplicationTask`, Core-Gate oder Evidenzaufwertung.

**Tech Stack:** C++20, CMake/Ninja, OpenSSL SHA-256, GMP/GMPXX, CTest, Python `unittest`.

**Spec:** `docs/superpowers/specs/2026-10-03-nanocell-mod-design.md`

## Global Constraints

- Der innere `BRUCE_NANOCELL_RUN_V1`-Vertrag und seine drei Referenztechniken bleiben bytegenau/replaybar.
- `MATCHED`, `NO_MATCH`, `CONTEXT_REJECTED` und `INCOMPLETE` bleiben technische NanoCell-Statuswerte, nie mathematische Evidenz oder Regelautorität.
- Es gibt keinen neuen `ApplicationTask`, keine `ExecutionSession`-Phase, kein Core-Gate und keine dynamische Plugin-Suche.
- `BRUCE_BUILD_MOD_NANOCELL=OFF` kompiliert, verlinkt und registriert keinen NanoCell-Mod und registriert den CLI-Befehl nicht.
- Die neue äußere Serialisierung nutzt ausschließlich `ModRunEnvelopeV1`; die bestehende direkte V1-Datei bleibt ein reiner Lese-Kompatibilitätsweg.
- Bestehende Quellen werden mit `git mv` verschoben; der alte Include-Pfad bleibt in diesem Schritt ein Weiterleitungsheader.
- Jeder Implementierungsschritt beginnt mit einem absichtlich roten Test, verwendet keine neuen Laufzeitabhängigkeiten und endet mit einem fokussierten Commit.

## Review Focus

- Ein syntaktisch korrekter, neu gehashter äußerer Umschlag mit abweichendem Request/Payload-Paar muss im NanoCell-Mod-Replay abgelehnt werden (Task 2).
- Ein direkter historischer `BRUCE_NANOCELL_RUN_V1` muss über `nanocell replay` weiterhin ohne Umschreiben funktionieren (Task 3).
- Bei ausgeschaltetem NanoCell-Mod darf weder die CLI-Dispatch-Route noch der Hilfetext den Befehl anbieten (Task 3).
- Der Katalog muss bei Language- und NanoCell-Mod beide Manifeste byteweise nach ID sortiert enthalten und bei ausgeschaltetem NanoCell kein verwaistes Manifest erzeugen (Task 1).
- Ein NanoCell-`MATCHED` darf im JSON und der Dokumentation nicht als `VERIFIED_ON_SAMPLES`, Gate-Erfolg oder Regelentscheidung erscheinen (Task 3 und 4).

---

### Task 1: Statischen NanoCell-Mod bauen und katalogisieren

**Files:**
- Create: `include/bruce/mods/nanocell/mod.hpp`
- Create: `mods/nanocell/CMakeLists.txt`
- Create: `mods/nanocell/src/mod.cpp`
- Move: `include/bruce/nanocell/nanocell.hpp` → `include/bruce/mods/nanocell/nanocell.hpp`
- Create: `include/bruce/nanocell/nanocell.hpp` (Weiterleitungsheader)
- Move: `src/nanocell/nanocell.cpp` → `mods/nanocell/src/nanocell.cpp`
- Move: `tests/unit/nanocell_tests.cpp` → `tests/mods/nanocell/nanocell_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/bruce/mod/mod_api.hpp`
- Modify: `src/mod/mod_api.cpp`
- Modify: `src/mod/catalog.cpp`
- Modify: `tests/unit/mod_catalog_tests.cpp`
- Modify: `tests/integration/test_mod_build.py`

**Interfaces:**
- Produces `ModCapabilityV1::nanocell_execution`, encoded as the next canonical capability value after `phase_execution`.
- Produces `const ModManifestV1& nanocell_mod_manifest_v1()` with `id == "nanocell"`, `version == "nanocell-mod-v1"`, API `bruce-mod-api-v1` and exactly `{nanocell_execution}`.
- Produces CMake target `bruce_mod_nanocell` and option `BRUCE_BUILD_MOD_NANOCELL` (default `ON`).
- Consumes the existing public NanoCell V1 types without changing their names, versions, techniques, statuses or direct packing functions.

- [ ] **Step 1: Write the failing catalog/build-isolation tests**

Extend `tests/unit/mod_catalog_tests.cpp` to include `bruce/mods/nanocell/mod.hpp`, require a two-entry sorted default catalog (`language`, `nanocell`), exact NanoCell manifest equality and `find("nanocell")`. Extend `tests/integration/test_mod_build.py` to configure both `BRUCE_BUILD_MOD_LANGUAGE=OFF` and `BRUCE_BUILD_MOD_NANOCELL=OFF`, then require `grid` success plus exit-2 `UnknownCommand` errors for both `language` and `nanocell`.

- [ ] **Step 2: Run the focused tests and verify they fail**

Run: `scripts/with-host-deps.sh cmake -S . -B build/nanocell-red -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release` followed by `scripts/with-host-deps.sh ctest --test-dir build/nanocell-red -R '^(mod_catalog_contracts|nanocell_contracts)$' --output-on-failure`.

Expected: compilation/test failure because the NanoCell Mod header, manifest, CMake target and disabled-command behavior do not yet exist.

- [ ] **Step 3: Move NanoCell ownership and add its manifest/build registration**

Use `git mv` for the V1 implementation and contract test. Create the forwarding old header that only includes `bruce/mods/nanocell/nanocell.hpp`. Remove NanoCell source/test linkage from `bruce_core`; add the guarded `mods/nanocell` subdirectory and target linked to `bruce_mod_runtime`. Add the `nanocell_execution` enum encode/decode cases. Give `bruce_mod_catalog` and `bruce` both CMake definitions (`0` or `1`) for Language and NanoCell, and conditionally register manifests in ascending ID order. Build NanoCell tests and the catalog test whenever their corresponding target is enabled.

- [ ] **Step 4: Run focused enabled and disabled build checks**

Run: `scripts/with-host-deps.sh cmake --build build/nanocell-red --target bruce_mod_nanocell bruce_mod_catalog bruce_nanocell_tests bruce_mod_catalog_tests` and `scripts/with-host-deps.sh ctest --test-dir build/nanocell-red -R '^(nanocell_contracts|mod_catalog_contracts)$' --output-on-failure`.

Run: `scripts/with-host-deps.sh python3 tests/integration/test_mod_build.py --cmake cmake --source . --build build/nanocell-off`.

Expected: all focused checks pass; the no-mod CLI retains `grid` and rejects both disabled commands.

- [ ] **Step 5: Commit the self-contained build boundary**

```bash
git add CMakeLists.txt include/bruce/mod include/bruce/mods/nanocell include/bruce/nanocell \
  mods/nanocell src/mod tests/mods/nanocell tests/unit/mod_catalog_tests.cpp \
  tests/integration/test_mod_build.py
git commit -m "feat: build nanocell as optional static mod"
```

### Task 2: Herkunftsgebundenen NanoCell-Mod-Run implementieren

**Files:**
- Modify: `include/bruce/mods/nanocell/nanocell.hpp`
- Modify: `mods/nanocell/src/nanocell.cpp`
- Modify: `mods/nanocell/src/mod.cpp`
- Create: `tests/mods/nanocell/nanocell_mod_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `struct NanoCellModRunV1 { NanoCellRunV1 nanocell; };`.
- Produces `execute_nanocell_mod_v1(const NanoCellRequestV1&, const NanoCellControlV1& = {})`, `pack_nanocell_mod_run_v1(const NanoCellModRunV1&)` and `replay_nanocell_mod_run_v1(std::string_view)`.
- Consumes `nanocell_mod_manifest_v1()`, `pack_mod_run_envelope`, `unpack_mod_run_envelope`, `execute_nanocell_v1`, `pack_nanocell_run_v1` and `replay_nanocell_run_v1`.
- Produces an outer envelope with codecs `bruce-nanocell-request-v1` and `bruce-nanocell-run-v1`.

- [ ] **Step 1: Write failing Mod-run contract tests**

Create `tests/mods/nanocell/nanocell_mod_tests.cpp`. Use the existing masked-match request values to require: the expected manifest/capability; deterministic outer packing; exact outer input/payload digests; successful outer replay preserving `MATCHED`; direct V1 packing/replay unchanged; and rejection of trailing data, bad codec, changed manifest version, changed payload with renewed digest, and changed request with renewed digest.

- [ ] **Step 2: Run the new test and verify it fails**

Run: `scripts/with-host-deps.sh cmake --build build/nanocell-red --target bruce_nanocell_mod_tests`.

Expected: target/function names are absent before implementation.

- [ ] **Step 3: Implement canonical request binding and outer replay checks**

Keep the existing private canonical request encoder/decoder as the sole request codec. Add the three public Mod-run functions in `mods/nanocell/src/mod.cpp`: canonicalize the request, run the unchanged V1 executor, pack its unchanged inner bundle as payload, and create the outer envelope. On replay, require the exact NanoCell manifest/codecs; decode the outer request; replay the inner V1 bundle; require that its request re-encodes identically to the outer input; then require outer canonical round-trip equality. Do not call `execute_mod_core_run` or any Core gate.

- [ ] **Step 4: Run NanoCell contract coverage**

Run: `scripts/with-host-deps.sh cmake --build build/nanocell-red --target bruce_nanocell_tests bruce_nanocell_mod_tests` followed by `scripts/with-host-deps.sh ctest --test-dir build/nanocell-red -R '^(nanocell_contracts|nanocell_mod_contracts|mod_api_contracts)$' --output-on-failure`.

Expected: all direct V1 and new outer Mod-run tamper/replay checks pass.

- [ ] **Step 5: Commit the NanoCell Mod-run contract**

```bash
git add include/bruce/mods/nanocell mods/nanocell tests/mods/nanocell CMakeLists.txt
git commit -m "feat: bind nanocell runs to mod provenance"
```

### Task 3: CLI-Export, historische Replay-Kompatibilität und Mod-Aus-Verhalten

**Files:**
- Modify: `apps/bruce_cli.cpp`
- Modify: `apps/nanocell_cli.hpp`
- Move: `tests/integration/test_nanocell_cli.py` → `tests/mods/nanocell/test_nanocell.py`
- Create: `tests/fixtures/nanocell-v1-masked-match.nanocell`
- Modify: `tests/integration/test_mod_build.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes `execute_nanocell_mod_v1` for `nanocell run` and `replay_nanocell_mod_run_v1` for new exports.
- Preserves `replay_nanocell_run_v1` as a read-only fallback when input begins with `BRUCE_NANOCELL_RUN_V1\n`.
- Produces the unchanged JSON contract `nanocell-cli-v1`; it gains no proof/gate fields.

- [ ] **Step 1: Write failing CLI compatibility tests**

Move and extend the Python NanoCell workflow test. Require that a newly exported file begins `BRUCE_MOD_RUN_V1\n`, has NanoCell manifest and codecs, replays to the same `nanocell-cli-v1` JSON, and rejects a semantically altered re-hashed outer request/payload. Add the fixed direct V1 fixture and require its replay to still yield the established masked-match JSON. Require the no-mod build's `nanocell` command and its help listing to be absent.

- [ ] **Step 2: Run the workflow test and verify it fails**

Run: `scripts/with-host-deps.sh ctest --test-dir build/nanocell-red -R '^(nanocell_cli_workflows|mod_build_language_off)$' --output-on-failure`.

Expected: current CLI writes only an inner V1 bundle and still registers NanoCell without its CMake option.

- [ ] **Step 3: Route the CLI through the NanoCell Mod**

Guard NanoCell includes, help text and dispatch in `apps/bruce_cli.cpp` with `BRUCE_BUILD_MOD_NANOCELL`. In `apps/nanocell_cli.hpp`, preserve argument parsing, bounded file input and JSON rendering; replace the execute/export path with `NanoCellModRunV1` and outer packing. For replay, select the historical direct V1 parser only on its exact header; otherwise require the outer NanoCell Mod parser. Keep exclusive publication and the 5703-byte direct V1 read ceiling; derive/introduce a separately bounded outer limit from the NanoCell Mod contract rather than accepting unbounded input.

- [ ] **Step 4: Run CLI and build-matrix verification**

Run: `scripts/with-host-deps.sh ctest --test-dir build/nanocell-red -R '^(nanocell_contracts|nanocell_mod_contracts|nanocell_cli_workflows|mod_catalog_contracts|mod_build_language_off)$' --output-on-failure`.

Run the existing no-mod script once more in a fresh `build/nanocell-off` directory.

Expected: the new outer export, historical direct V1 fixture, disabled CLI path and unchanged NanoCell statuses all pass.

- [ ] **Step 5: Commit the CLI boundary**

```bash
git add apps/bruce_cli.cpp apps/nanocell_cli.hpp tests/mods/nanocell \
  tests/fixtures/nanocell-v1-masked-match.nanocell tests/integration/test_mod_build.py CMakeLists.txt
git commit -m "feat: route nanocell cli through static mod"
```

### Task 4: Dokumentation, generated CLI reference und Abschlussvalidierung

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/cli/control-overview.md`
- Modify: `docs/cli/verify_examples.py`
- Regenerate: `docs/cli/examples.json`, `docs/cli/source-index.md`, `docs/cli/validation.md`
- Modify: `docs/superpowers/specs/2026-10-03-core-mod-architecture-design.md`
- Modify: `docs/superpowers/specs/2026-10-03-nanocell-mod-design.md`

**Interfaces:**
- Documents `BRUCE_BUILD_MOD_NANOCELL`, the optional static target and the exact scope boundary.
- Documents new outer NanoCell Mod exports plus direct V1 replay compatibility without describing NanoCell as a Core-run proof or general session.
- Extends the existing CLI verifier to require the outer header and successful NanoCell replay.

- [ ] **Step 1: Update the documentation verifier expectation first**

In `docs/cli/verify_examples.py`, assert that `matched.nanocell` starts with `BRUCE_MOD_RUN_V1\n` while retaining the existing expected `nanocell-cli-v1` technical JSON. Run the verifier before changing the CLI documentation.

Run: `scripts/with-host-deps.sh python3 docs/cli/verify_examples.py build/nanocell-red/bruce`.

Expected: failure before the Task-3 outer export is present, or success after Task 3 with generated documentation files changed only as prescribed.

- [ ] **Step 2: Document the implemented boundary and explicit non-goals**

Update the root status, architecture table and CLI manuals to distinguish: implemented NanoCell Mod + outer provenance export; legacy direct V1 replay; and intentionally absent `ApplicationTask`, Core gate, free language, Cockpit and Phase-DAG. Replace stale “isolated core in `bruce_core`” wording with the actual optional-Mod ownership. Mark this NanoCell design **TEILWEISE IMPLEMENTIERT UND BELEGT** only for items verified by the final tests; leave later architecture unchanged as concept.

- [ ] **Step 3: Run full validation from a fresh enabled build**

Run: `scripts/with-host-deps.sh cmake -S . -B build/nanocell-final -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DBRUCE_BUILD_MOD_LANGUAGE=ON -DBRUCE_BUILD_MOD_NANOCELL=ON`.

Run: `scripts/with-host-deps.sh cmake --build build/nanocell-final`.

Run: `scripts/with-host-deps.sh ctest --test-dir build/nanocell-final --output-on-failure --parallel 8` (use non-overlapping test-ID chunks if the runner's time window requires it; every CTest must have a recorded PASS result).

Run: `scripts/with-host-deps.sh python3 docs/cli/verify_examples.py build/nanocell-final/bruce`.

Expected: complete CTest suite, generated CLI reference and local Markdown links pass.

- [ ] **Step 4: Inspect the final diff and commit documentation**

Run: `git diff --check master...HEAD` and `git status --short`.

Expected: no whitespace errors and only intentional documentation/generated-reference changes.

```bash
git add README.md docs/architecture/03-blueprint-consolidation.md docs/cli \
  docs/superpowers/specs/2026-10-03-core-mod-architecture-design.md \
  docs/superpowers/specs/2026-10-03-nanocell-mod-design.md
git commit -m "docs: document nanocell mod boundary"
```

## Plan Self-Review

- **Spec coverage:** Tasks 1–3 cover optional build, manifest/catalog, outer provenance binding, legacy replay, CLI behavior and no Core-task integration. Task 4 covers status wording, documentation and full acceptance.
- **Step scan:** Every task starts red, defines the affected interface, ends in a focused verification and commit, and introduces no unrelated refactoring.
- **Type consistency:** `NanoCellModRunV1`, `execute_nanocell_mod_v1`, `pack_nanocell_mod_run_v1`, `replay_nanocell_mod_run_v1`, `nanocell_mod_manifest_v1` and `nanocell_execution` are used consistently across tasks.
- **Review focus:** Each listed replay, legacy, disabled-build, catalog and status risk is assigned to a concrete test task.
- **Proportion:** The plan implements only the approved static NanoCell Mod boundary; generic observations, Core task types, gates, Cockpit and Phase-DAG remain excluded.
