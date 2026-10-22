# Phase-DAG V2 CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eine lesbare `BRUCE_PHASE_DAG_SPEC_V1` mit mehreren NanoCell-Knoten in ein exklusiv veröffentlichtes, prüf- und replaybares Phase-DAG-V2-Bundle ausführen.

**Architecture:** Der bestehende Phase-DAG-V2-Kern bleibt byte- und adapterorientiert. Eine neue, headerbasierte CLI-Schicht parst die Spec und ihre relativen Kontextdateien, erzeugt kanonische `NanoCellPhaseRequestV1`-Bytes, ruft den festen NanoCell-V2-Katalog auf und rendert nur Digest- und Statusmetadaten. Der Cockpit-Inspector erhält keine neue Ausführungslogik; ein Integrationstest übergibt ihm ausschließlich das neu erzeugte Bundle.

**Tech Stack:** C++20, CMake/CTest, Python `unittest`, bestehende `bruce_core`, `bruce_phase_dag_nanocell`, NanoCell V2 und vorhandene CLI-Dokumentationsgeneratoren.

**Spec:** `docs/superpowers/specs/2026-10-12-phase-dag-v2-cli-design.md`

## Global Constraints

- Der Phase-DAG-V2-Kern und das Laufzeitformat `BRUCE_PHASE_DAG_RUN_V2` bleiben unverändert.
- Die neue Eingabe beginnt exakt mit `BRUCE_PHASE_DAG_SPEC_V1`; alle globalen und Knotenschlüssel sind verpflichtend, eindeutig und unbekannte Schlüssel sind Fehler.
- Ausschließlich der fest eingebaute Adapter `nanocell-phase-v1` / `v1` wird verwendet; es gibt keine Plugin- oder Adapterauswahl in der Spec.
- Kontextdateien werden relativ zum Spec-Verzeichnis und mit der vorhandenen NanoCell-Grenze von 4096 Bytes gelesen.
- Die CLI darf keine bestehende Ausgabedatei überschreiben; alle Parser-, Datei-, Validierungs-, Pack- und Publikationsfehler ergeben JSON auf stderr und Exit 2.
- Fachliche `FAILED`, `BLOCKED`, `CANCELLED`, `INCOMPLETE` und `REJECTED`-Ergebnisse eines gültigen DAG-Laufs werden als Bundle gespeichert und liefern Exit 0.
- Der Befehl und seine Hilfe sind nur mit `BRUCE_BUILD_MOD_NANOCELL=ON` vorhanden.
- `docs/cli/source-index.md`, `docs/cli/examples.json` und `docs/cli/validation.md` werden ausschließlich durch `docs/cli/verify_examples.py` in einer vollständigen temporären Kopie erzeugt oder aktualisiert.
- Die abschließende Prüfung folgt dem Projekthandbuch mit `scripts/with-host-deps.sh` aus einem nativen temporären Pfad ohne `:`.

## Review Focus

- Ein relativer Kontextpfad mit `..` bleibt relativ zum Verzeichnis der Spec auflösbar und darf niemals relativ zum aktuellen Arbeitsverzeichnis interpretiert werden; Task 2 deckt einen verschachtelten Spec-Pfad ab.
- Ein Knoten mit einem fachlich fehlgeschlagenen Vorgänger erzeugt ein gültiges Bundle mit `FAILED` und `BLOCKED`, statt die gesamte CLI-Ausführung als technischen Fehler zu beenden; Task 2 deckt dies ab.
- Eine vorhandene Ausgabedatei bleibt byteidentisch, wenn `execute` dieselbe `--output`-Datei anfordert; Task 2 deckt dies ab.
- Ein korrumpiertes oder zu großes Bundle wird sowohl durch `inspect` als auch durch `replay` kontrolliert abgelehnt, ohne Rohbytes in stdout/stderr zu spiegeln; Task 3 deckt dies ab.
- Ein mit der neuen CLI erzeugtes Bundle bleibt im Loopback-Cockpit gültig und zeigt keine Kontext- oder Request-Bytes; Task 3 deckt dies ab.

---

## File Structure

| Datei | Änderung | Verantwortung |
| --- | --- | --- |
| `apps/phase_dag_v2_cli.hpp` | neu | Privater CLI-Parser, Spec-zu-V2-Abbildung, Ausführung, Replay/Inspect und JSON-Übersicht. |
| `apps/bruce_cli.cpp` | ändern | Bedingtes Include, Routing und exakte Hilfe für `phase-dag-v2`. |
| `CMakeLists.txt` | ändern | CLI an `bruce_phase_dag_nanocell` binden und den Workflow-Test registrieren. |
| `tests/mods/nanocell/test_phase_dag_v2_cli.py` | neu | End-to-End-Verträge für Spec, Ausführung, Fehler und Bundles. |
| `tests/mods/cockpit/test_cockpit_inspector.py` | ändern | Reales CLI-Bundle durch den bestehenden Cockpit-Inspect-Endpunkt prüfen. |
| `docs/cli/README.md` | ändern | Produktgrenze von Bibliothek zu verfügbarer V2-CLI aktualisieren. |
| `docs/cli/commands.md` | ändern | Befehle, Status-/Fehler- und Bundleverhalten dokumentieren. |
| `docs/cli/formats.md` | ändern | `BRUCE_PHASE_DAG_SPEC_V1`, Felder und relative Kontextpfade dokumentieren. |
| `docs/cli/results-and-control.md` | ändern | JSON-Übersicht und fachliche Outcome-Semantik erklären. |
| `docs/architecture/03-blueprint-consolidation.md` | ändern | Die produktive, bewusst begrenzte V2-CLI vom weiterhin fehlenden allgemeinen Scheduler abgrenzen. |
| `docs/cli/verify_examples.py` | ändern | Reale Mehrknoten-Spec und alle drei Befehle messen; den neuen Parser als Optionsquelle berücksichtigen. |
| `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md` | generiert | Nur vom Verifikationsskript in der temporären Vollkopie aktualisiert. |

## Contract Decisions

`apps/phase_dag_v2_cli.hpp` stellt innerhalb des CLI-Übersetzungskontexts bereit:

```cpp
int phase_dag_v2_cli(int argc, char** argv);
```

Es akzeptiert ausschließlich:

```text
bruce phase-dag-v2 execute SPEC --output BUNDLE
bruce phase-dag-v2 inspect BUNDLE
bruce phase-dag-v2 replay BUNDLE
```

Alle erfolgreichen Unterbefehle geben einen JSON-Vertrag `phase-dag-v2-cli-v1` aus. Die Übersicht enthält `action` (`execute`, `inspect`, `replay`), `dag_version`, `dag_id`, `node_count`, `bundle_digest` und eine nach Knoten geordnete `receipts`-Liste mit `node_id`, `outcome` und `failure_code`. Sie enthält keine Kontext-, Request-, Output- oder Evidence-Bytes.

`execute` setzt für jeden Knoten fest `adapter_id="nanocell-phase-v1"`, `adapter_version="v1"`, `core_layout_version="nanoneuron-core-v1"`, packt den `NanoCellPhaseRequestV1`, berechnet `request_digest` mit `sha256` und ruft `execute_phase_dag_v2(spec, phase_dag_nanocell_catalog_v2())`. Es schreibt danach `pack_phase_dag_run_v2(run)` mit `LocalFiles::publish_new_atomically`. Vor der atomaren No-Replace-Veröffentlichung erzeugen Validierungs-, Ausführungs- und Packfehler sowie Zielkollisionen kein neues Bundle. Nach erfolgreicher Veröffentlichung kann `ExportCommitDurabilityUnknown` bei fehlgeschlagener Verzeichnissynchronisation auftreten; das bereits sichtbare Bundle darf dann nicht als Rollback gelöscht werden.

`inspect` und `replay` lesen maximal 5 MiB, rufen beide `replay_phase_dag_run_v2(bundle, phase_dag_nanocell_catalog_v2())` auf und rendern die bestätigte Laufansicht. Der unterschiedliche `action`-Wert ist ihre einzige absichtliche Ausgabedifferenz.

## Tasks

### Task 1: CLI-Boundary, Build-Anbindung und Verfügbarkeit

**Files:**
- Create: `apps/phase_dag_v2_cli.hpp`
- Modify: `apps/bruce_cli.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/mods/nanocell/test_phase_dag_v2_cli.py`

**Interfaces:**
- Consumes: `bruce::phase_dag_nanocell_catalog_v2()`, `bruce::execute_phase_dag_v2`, `bruce::replay_phase_dag_run_v2`, `bruce::LocalFiles`.
- Produces: Bedingte Top-Level-Route `phase-dag-v2` und `bruce::phase_dag_v2_cli(int, char**)` für die folgenden Aufgaben.

- [ ] **Step 1: Write the failing availability tests**

  Add `PhaseDagV2CliAvailabilityTests` to `tests/mods/nanocell/test_phase_dag_v2_cli.py`. Assert that an enabled binary lists exactly the three `phase-dag-v2` help lines and rejects `bruce phase-dag-v2` plus an unknown subcommand with exit 2 and JSON error. Add a temporary NanoCell-disabled CMake build case that proves both the command and its help are absent.

- [ ] **Step 2: Run the new availability test to verify it fails**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce`

  Expected: FAIL because `phase-dag-v2` is not routed or listed.

- [ ] **Step 3: Add the guarded CLI route and target dependency**

  Create `apps/phase_dag_v2_cli.hpp` with the declared `phase_dag_v2_cli` entry point. In `apps/bruce_cli.cpp`, include and route it only inside `#if BRUCE_BUILD_MOD_NANOCELL`; add the three exact help lines in the same guard. In `CMakeLists.txt`, link target `bruce` privately to `bruce_phase_dag_nanocell` only in the existing NanoCell-enabled branch, and register `phase_dag_v2_cli_workflows` to run the new Python test with `$<TARGET_FILE:bruce>`.

- [ ] **Step 4: Run the availability test to verify it passes**

  Run: `ctest --test-dir /tmp/bruce-build --output-on-failure -R phase_dag_v2_cli_workflows`

  Expected: PASS once the route, help and NanoCell-off behavior are present.

- [ ] **Step 5: Commit the task when Git metadata is available**

  Use message: `feat: expose phase dag v2 cli boundary`.

### Task 2: `BRUCE_PHASE_DAG_SPEC_V1` parsing and executable bundle creation

**Files:**
- Modify: `apps/phase_dag_v2_cli.hpp`
- Modify: `tests/mods/nanocell/test_phase_dag_v2_cli.py`

**Interfaces:**
- Consumes: Task 1 route; `NanoCellPhaseRequestV1`, `pack_nanocell_phase_request_v1`, `sha256`, `LocalFiles::read` and `LocalFiles::publish_new_atomically`.
- Produces: `execute SPEC --output BUNDLE`, a canonical `PhaseDagSpecV2`, a `BRUCE_PHASE_DAG_RUN_V2` bundle and the `phase-dag-v2-cli-v1` execute report.

- [ ] **Step 1: Write failing success and outcome tests**

  In `PhaseDagV2CliExecutionTests`, create a nested `spec/` directory with two binary contexts and a spec containing sorted nodes `a`, `b`, `c`, where `b` depends on `a` and `c` is independent. Assert `execute` returns exit 0, `contract == "phase-dag-v2-cli-v1"`, `action == "execute"`, `node_count == 3`, only digest/status metadata, and writes a `BRUCE_PHASE_DAG_RUN_V2\n` bundle. Add a separate spec whose first node uses `technique=context-conflict-v1` and whose child depends on it; assert `FAILED`/`BLOCKED` receipts, exit 0 and an output bundle.

- [ ] **Step 2: Run the success/outcome tests to verify they fail**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliExecutionTests`

  Expected: FAIL because no parser or executor exists.

- [ ] **Step 3: Implement the strict parser and Spec-to-V2 mapping**

  In `apps/phase_dag_v2_cli.hpp`, read the Spec at a bounded 64 KiB limit. Require the header, require exactly one global value for `dag-id`, `max-started-nodes`, `max-receipt-bytes`, `max-trace-entries`, and require exactly one of every node key from the design. Treat blank lines as node boundaries; reject duplicate/unknown keys, incomplete blocks and assignments without exactly one `=`. Parse comma-separated `depends` into a non-empty-safe, lexicographically sorted, duplicate-free vector. Resolve `context-file` relative to `SPEC.parent_path()`, read no more than 4096 bytes, parse exactly 16 hexadecimal core characters and validate decimal unsigned integers before creating each canonical NanoCell request. Sort nodes by `node_id`, call `validate_phase_dag_spec_v2`, execute the fixed catalog, pack it and publish it exclusively. Render the exact JSON contract from the resolved `PhaseDagRunV2` and `sha256(bundle)`.

- [ ] **Step 4: Run the success/outcome tests to verify they pass**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliExecutionTests`

  Expected: PASS with both a normal mixed-dependency graph and a saved partial outcome.

- [ ] **Step 5: Write failing validation and publication tests**

  Add table-driven tests for invalid header, unknown/duplicate/missing key, invalid core hex, missing context, context larger than 4096 bytes, unsorted or duplicate dependencies, unknown dependency, cycle and unacceptable budgets. Each case must exit 2, print no stdout and leave `--output` absent. Add a pre-existing-output test that checks its bytes do not change after `execute`. Include the Review Focus case whose `context-file=../contexts/a.bin` is resolved from the spec directory, not the process directory.

- [ ] **Step 6: Run the validation tests to verify they fail**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliValidationTests`

  Expected: FAIL until every invalid form is rejected and exclusive publication is wired.

- [ ] **Step 7: Implement precise validation and atomic exclusive publication**

  Complete validation, execution, packing and report preparation before `LocalFiles::publish_new_atomically`; these failures and destination collisions leave no new bundle. A directory sync failure after successful atomic publication may yield `ExportCommitDurabilityUnknown` with a visible bundle and no success report; do not delete that bundle as rollback or reinterpret the established `Error` path. Preserve the existing global `bruce_cli.cpp` JSON stderr/exit-2 wrapper. Ensure the parser does not normalize invalid dependency order or duplicate values into valid data. Exercise the real CLI path with post-publication unlink failure and directory-sync failure injection.

- [ ] **Step 8: Run the full Phase-DAG-V2 CLI test module**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce`

  Expected: PASS, including success, partial outcomes, relative paths, invalid Specs and output collision.

- [ ] **Step 9: Commit the task when Git metadata is available**

  Use message: `feat: execute phase dag v2 specs`.

### Task 3: Bundle inspection, deterministic replay and Cockpit interoperability

**Files:**
- Modify: `apps/phase_dag_v2_cli.hpp`
- Modify: `tests/mods/nanocell/test_phase_dag_v2_cli.py`
- Modify: `tests/mods/cockpit/test_cockpit_inspector.py`

**Interfaces:**
- Consumes: Task 2 bundle creation and fixed NanoCell catalog; existing `replay_phase_dag_run_v2`; existing Cockpit `POST /api/v1/inspect` endpoint.
- Produces: `inspect BUNDLE` and `replay BUNDLE`, each with a validated `phase-dag-v2-cli-v1` report; evidence that a CLI bundle is Cockpit-inspectable.

- [ ] **Step 1: Write failing inspect/replay tests**

  Extend the CLI test module to execute a three-node Spec, then invoke `inspect` and `replay`. Assert both return exit 0, have the same DAG/receipt data and bundle digest as the execute result, differ only by `action`, and leave the bundle byte-identical. Add corrupt-header, digest-mutation and 5 MiB-plus-one-byte inputs; assert exit 2, JSON stderr and no raw bundle marker/context bytes in output.

- [ ] **Step 2: Run inspect/replay tests to verify they fail**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliInspectionTests`

  Expected: FAIL because `inspect` and `replay` are not implemented.

- [ ] **Step 3: Implement confirmed bundle views**

  In `apps/phase_dag_v2_cli.hpp`, implement `inspect` and `replay` with exactly one positional bundle argument and no options. Read at most 5 MiB, call `replay_phase_dag_run_v2` with `phase_dag_nanocell_catalog_v2`, calculate the full bundle SHA-256 only after bounded loading and render the common public receipt summary. Never expose request, context, output or evidence bytes.

- [ ] **Step 4: Run inspect/replay tests to verify they pass**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliInspectionTests`

  Expected: PASS for deterministic valid bundles and controlled malformed/oversized inputs.

- [ ] **Step 5: Write the failing Cockpit interoperability test**

  In `tests/mods/cockpit/test_cockpit_inspector.py`, build a temporary V1 spec and contexts, use the actual `bruce phase-dag-v2 execute` command, then POST the saved bundle to the managed loopback Cockpit. Assert HTTP 200, `VALID`, the expected DAG ID/node dependencies and absence of context/request bytes.

- [ ] **Step 6: Run the Cockpit interoperability test to verify it passes**

  Run: `ctest --test-dir /tmp/bruce-build --output-on-failure -R cockpit_inspector_workflows`

  Expected: PASS; the existing Cockpit implementation needs no new route or execution feature.

- [ ] **Step 7: Commit the task when Git metadata is available**

  Use message: `feat: inspect and replay phase dag v2 bundles`.

### Task 4: User documentation and generated CLI evidence

**Files:**
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `docs/cli/formats.md`
- Modify: `docs/cli/results-and-control.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/verify_examples.py`
- Regenerate in a full temporary copy: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`
- Modify: `tests/mods/nanocell/test_phase_dag_v2_cli.py`

**Interfaces:**
- Consumes: the final executable CLI and the exact output contract from Tasks 1–3.
- Produces: source-derived help/options/diagnostic evidence and user-facing explanation that distinguishes the bounded CLI from a general scheduler.

- [ ] **Step 1: Write failing documentation-evidence tests**

  Add `PhaseDagV2CliDocumentationTests` to assert that the manual documents all three exact commands, `BRUCE_PHASE_DAG_SPEC_V1`, relative `context-file` behavior, the fixed NanoCell adapter, the 4096-byte context cap, exclusive output, and exit-0 partial outcomes. Assert the generated index contains `### \`phase-dag-v2\`` and the measured examples contain successful `execute`, `inspect`, `replay` and one rejected Spec.

- [ ] **Step 2: Run documentation tests to verify they fail**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliDocumentationTests`

  Expected: FAIL because neither the manual nor generated artifacts describe this command.

- [ ] **Step 3: Update prose and the generator source**

  Add a dedicated V2 CLI section to `docs/cli/commands.md`, the strict text grammar to `docs/cli/formats.md`, outcome/JSON semantics to `docs/cli/results-and-control.md`, and a concise capability update to `docs/cli/README.md` plus `docs/architecture/03-blueprint-consolidation.md`. State that the implemented CLI is serial, bounded and NanoCell-only; it does not claim a generic scheduler, arbitrary adapters, GPU execution, a general prover or free-language input. In `docs/cli/verify_examples.py`, create deterministic contexts/Spec, measure the three commands plus an invalid Spec, include `apps/phase_dag_v2_cli.hpp` in option extraction and add its own source-index paragraph.

- [ ] **Step 4: Regenerate generated CLI artifacts in a temporary full repository copy**

  Copy the whole repository to a native temporary path, build the enabled CLI there, then run `python3 docs/cli/verify_examples.py /absolute/path/to/bruce`. Bring back only the three generator-owned files after reviewing their diff; do not hand-edit them.

- [ ] **Step 5: Run documentation tests to verify they pass**

  Run: `python3 tests/mods/nanocell/test_phase_dag_v2_cli.py /tmp/bruce-build/bruce PhaseDagV2CliDocumentationTests`

  Expected: PASS with source-index and examples derived from the final command.

- [ ] **Step 6: Run final relevant verification**

  Run from a native temporary full copy:

  ```sh
  scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
  scripts/with-host-deps.sh cmake --build /tmp/bruce-build
  scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-build --output-on-failure
  python3 docs/cli/verify_examples.py /tmp/bruce-build/bruce
  python3 scripts/check_local_doc_links.py
  git diff --check
  ```

  Expected: configuration/build succeed; all CTest cases, generator and link check pass; `git diff --check` is run only when the execution checkout has Git metadata.

- [ ] **Step 7: Commit the task when Git metadata is available**

  Use message: `docs: document phase dag v2 cli`.

## Self-Review

- **Spec coverage:** Task 1 covers conditional availability; Task 2 covers the complete V1 text format, fixed adapter, bounded contexts, execution, partial outcomes and exclusive output; Task 3 covers both read paths and Cockpit compatibility; Task 4 covers all mandated documentation and generated evidence. No spec requirement is unassigned.
- **Step scan:** Each implementation step names its file, boundary and exact outcome. Each test step gives concrete test classes and assertions; no task requires an unbounded or generic scheduler decision.
- **Type consistency:** Every task uses `phase_dag_v2_cli(int, char**)`, `PhaseDagSpecV2`, `NanoCellPhaseRequestV1`, `phase_dag_nanocell_catalog_v2()`, `execute_phase_dag_v2`, `pack_phase_dag_run_v2` and `replay_phase_dag_run_v2` consistently.
- **Review focus:** All five listed risks have a named owning test in Tasks 2 or 3.
- **Proportion:** The plan fixes public names, formats, limits and evidence paths without reproducing parser or adapter source code.
