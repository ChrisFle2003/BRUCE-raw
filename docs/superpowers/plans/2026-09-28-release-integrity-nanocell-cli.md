# Release-Integrität und NanoCell-CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen frischen, CI-geprüften Releasepfad und eine replaybare NanoCell-V1-CLI bereitstellen, ohne bestehende ApplicationTask- oder Regelautorisierungsgrenzen zu verändern.

**Architecture:** NanoCell erhält einen fokussierten CLI-Adapter neben dem bestehenden Top-Level-Dispatcher und bleibt von `ApplicationTask` getrennt. Der Adapter konstruiert ausschließlich feste NanoCell-Referenztechniken, rendert einen versionierten JSON-Vertrag und nutzt den vorhandenen exklusiven Bundleexport. CI baut aus einem neuen Buildpfad, testet den vollständigen CTest-Bestand und prüft die generierten CLI-Dokumentationsartefakte in einer Kopie.

**Tech Stack:** C++20, CMake/CTest/CPack, GMP/GMPXX, OpenSSL Crypto, Python 3 `unittest`, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-28-release-integrity-nanocell-cli-design.md`

## Global Constraints

- CMake mindestens 3.20, C++20; keine neue produktive Laufzeitabhängigkeit.
- NanoCell verwendet ausschließlich `execute_nanocell_v1`, `pack_nanocell_run_v1`, `replay_nanocell_run_v1` und die drei festen Referenztechniken.
- NanoCell bleibt außerhalb von `ApplicationTask`, Research-Gates, Grid und Experience.
- Erfolgreiche CLI-Antworten sind JSON; Fehler sind `{\"error\":\"…\"}` auf stderr mit Exitcode 2.
- Exporte überschreiben niemals ein bestehendes Ziel.
- Doku-Regeneration darf CI-Arbeitsbaumdateien nicht verändern.
- Terminalbestätigung ist kein Produktions-Authentifizierungsmodell; keine Autorisierungsschnittstelle in diesem Vorhaben einführen.
- Der vorliegende Snapshot hat keine nutzbare Git-Historie; Commits nur ausführen, wenn ein echter Git-Checkout verfügbar ist.
- Vor jedem lokalen Build einen neuen temporären Pfad setzen: `BRUCE_FRESH_BUILD=$(mktemp -d /tmp/bruce-build-XXXXXX)`.

## Review Focus

1. Ein Core-String mit falscher Länge oder nicht-hexadezimalem Byte muss vor der NanoCell-Ausführung als JSON-Fehler enden.
2. Ein technisch gültiges, aber verändertes NanoCell-Bundle muss von `nanocell replay` abgelehnt werden.
3. Ein `INCOMPLETE`-Lauf bleibt ein erfolgreicher fachlicher NanoCell-Befund, nicht ein Prozessfehler.
4. Ein vorhandener Exportpfad bleibt byteidentisch und darf durch NanoCell nicht überschrieben werden.
5. Ein Fehler nach erfolgreicher Dateiveröffentlichung darf nicht behaupten, das Ziel sei zurückgerollt oder dauerhaft synchronisiert.

## Dateiübersicht

| Pfad | Verantwortung |
|---|---|
| `apps/nanocell_cli.hpp` | NanoCell-Argumentparser, Request-Konstruktion, JSON-Rendern und CLI-Dispatch |
| `apps/bruce_cli.cpp` | `nanocell` zum Top-Level-Dispatch und zur Hilfe hinzufügen |
| `src/io/local_files.cpp` | exklusiven Publishpfad nach `link()` mit Directory-fsync abschließen |
| `tests/integration/test_nanocell_cli.py` | Black-box-Vertrag für NanoCell run/replay/export/Fehler |
| `tests/unit/local_files_tests.cpp` | Bestandsschutz und Fehlersemantik des Exports |
| `CMakeLists.txt` | NanoCell-Integrationstest registrieren |
| `docs/cli/verify_examples.py` | NanoCell-Beispiele und Generatorunterstützung ergänzen |
| `README.md`, `docs/architecture/03-blueprint-consolidation.md` | Iststand gegenüber Zielarchitektur korrigieren |
| `docs/cli/*.md`, `examples.json`, `source-index.md`, `validation.md` | CLI-Vertrag, Beispiele und generierte Referenz aktualisieren |
| `.github/workflows/ci.yml` | frischer Build, Tests, Doku-Diff und Secret-Scan |

### Task 1: Directory-fsync für exklusiven Export

**Files:**
- Modify: `src/io/local_files.cpp:55-87`
- Test: `tests/unit/local_files_tests.cpp:24-56`

**Interfaces:**
- Consumes: `LocalFiles::publish_new(const std::string&, std::string_view, const ReplayCheckpoint&)`.
- Produces: unveränderte öffentliche Signatur; erfolgreiche Rückkehr bedeutet Datei- und Verzeichnissynchronisation nach Publish.

- [ ] **Step 1: Ergänze in `local_files_tests.cpp` einen Test für Bestandsschutz und fehlende temporäre Dateien nach einem erfolgreichen Publish.**

Der Test veröffentlicht `output`, versucht denselben Zielpfad erneut und stellt sicher, dass der Originalinhalt erhalten bleibt und kein `*.tmp.*` bleibt.

- [ ] **Step 2: Führe den fokussierten Test aus und bestätige den Ausgangszustand.**

Run: `cmake --build "$BRUCE_FRESH_BUILD" --target bruce_local_files_tests && "$BRUCE_FRESH_BUILD"/bruce_local_files_tests`  
Expected: PASS vor der Codeänderung; der Test dokumentiert das bestehende Invariante.

- [ ] **Step 3: Ergänze den Publishpfad in `LocalFiles::publish_new`.**

Öffne das Elternverzeichnis vor der Veröffentlichung mit einem Directory-FD. Nach erfolgreichem `link(temp, path)` und `unlink(temp)` rufe `fsync(directory_fd)` auf. Bei Fehler vor `link` entferne weiterhin die Temp-Datei und veröffentliche kein Ziel. Bei Fehler nach `link` wirf `Error("ExportCommitDurabilityUnknown")`; das Ziel bleibt bestehen, ein Rollback wird nicht versucht.

- [ ] **Step 4: Führe die lokalen Dateitests aus.**

Run: `cmake --build "$BRUCE_FRESH_BUILD" --target bruce_local_files_tests && "$BRUCE_FRESH_BUILD"/bruce_local_files_tests`  
Expected: PASS.

- [ ] **Step 5: Committe die isolierte Änderung, falls Git verfügbar ist.**

```bash
git add src/io/local_files.cpp tests/unit/local_files_tests.cpp
git commit -m "fix: sync exclusive export directory"
```

### Task 2: NanoCell-CLI-Adapter und Vertragstests

**Files:**
- Create: `apps/nanocell_cli.hpp`
- Modify: `apps/bruce_cli.cpp:1-30,162-288,292-647`
- Create: `tests/integration/test_nanocell_cli.py`
- Modify: `CMakeLists.txt` in `BUILD_TESTING`

**Interfaces:**
- Consumes: `NanoCellRunV1 execute_nanocell_v1(const NanoCellRequestV1&, const NanoCellControlV1& = {})`, `std::string pack_nanocell_run_v1(const NanoCellRunV1&)`, `NanoCellRunV1 replay_nanocell_run_v1(std::string_view)`.
- Produces: `int bruce::nanocell_cli(int argc, char** argv)` in `apps/nanocell_cli.hpp`; Commands `bruce nanocell run …` and `bruce nanocell replay FILE`; JSON contract `nanocell-cli-v1`.

- [ ] **Step 1: Schreibe `test_nanocell_cli.py` mit dem erfolgreichen Run→Export→Replay-Fall.**

Verwende Core `1020a54050607080`, `--active-index 2`, `--context-id reference.context_1`, eine Datei mit `nanocell-reference-context-v1`, Technik `masked-match-v1`, und prüfe `contract == "nanocell-cli-v1"`, `status == "MATCHED"`, `final_state == 160`; Replay muss dieselbe JSON-Nutzlast liefern und das exportierte Bundle muss existieren.

- [ ] **Step 2: Führe den neuen Integrationstest aus und bestätige sein Fehlschlagen.**

Run: `python3 tests/integration/test_nanocell_cli.py "$BRUCE_FRESH_BUILD"/bruce`  
Expected: FAIL, weil der Befehl noch unbekannt ist.

- [ ] **Step 3: Implementiere `bruce::nanocell_cli(int argc, char** argv)` in `apps/nanocell_cli.hpp`.**

`run` verlangt `--core`, `--active-index`, `--context-id`, `--context-file` und `--technique`; akzeptiert optional `--max-operations`, `--max-trace-entries`, `--output`. Der Coreparser akzeptiert exakt 16 Zeichen `[0-9A-Fa-f]` und erzeugt acht Bytes. Die Technikzuordnung ist exakt `masked-match-v1`, `rotate-match-v1`, `context-conflict-v1`. Kontextbytes werden per `read_bounded` gelesen; der Digest ist `sha256(context_bytes)`. Standardbudgets sind 16 beziehungsweise 18. `replay` akzeptiert exakt einen Dateipfad, liest maximal die NanoCell-Run-Grenze über den vorhandenen begrenzten Leser und ruft nur `replay_nanocell_run_v1` auf.

Der Renderer gibt Request-identifizierende Felder nicht als Kontextbytes aus, aber Status, `final_state` oder `null`, Technik-/Core-/Kontextdigests, Verbrauch und geordneten Trace aus. Der CLI-Adapter verwendet `export_exclusive` ausschließlich nach erfolgreichem Packen.

- [ ] **Step 4: Verdrahte NanoCell im Top-Level-Dispatcher und der Hilfe.**

In `apps/bruce_cli.cpp` Header einbinden, bei `command == "nanocell"` vor den bisherigen Fachzweigen an `nanocell_cli(argc, argv)` delegieren und die beiden Befehle in `help()` dokumentieren. Den bestehenden globalen Exception-Handler unverändert nutzen.

- [ ] **Step 5: Registriere den Integrationstest in CMake.**

Füge innerhalb von `BUILD_TESTING` genau einen `add_test(NAME nanocell_cli_workflows COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_nanocell_cli.py $<TARGET_FILE:bruce>)` hinzu.

- [ ] **Step 6: Erweitere `test_nanocell_cli.py` um die Review-Focus-Fälle.**

Prüfe `NO_MATCH` mit aktivem Byte `B5`, `CONTEXT_REJECTED` über `context-conflict-v1` mit Referenzkontext, `INCOMPLETE` über `rotate-match-v1` mit `--max-operations 1`, ungültiges Core-Hex, unbekannte Technik, fehlenden Kontext, korruptes Bundle und einen vorhandenen `--output`-Pfad. Bei Fehlern darf kein neues Ziel entstehen; beim vorhandenen Ziel muss dessen Byteinhalt unverändert bleiben.

- [ ] **Step 7: Führe NanoCell-Unit- und Integrationstests aus.**

Run: `cmake --build "$BRUCE_FRESH_BUILD" --target bruce_nanocell_tests bruce && "$BRUCE_FRESH_BUILD"/bruce_nanocell_tests && python3 tests/integration/test_nanocell_cli.py "$BRUCE_FRESH_BUILD"/bruce`  
Expected: alle drei Befehle Exit 0.

- [ ] **Step 8: Committe die NanoCell-CLI-Aufgabe, falls Git verfügbar ist.**

```bash
git add apps/nanocell_cli.hpp apps/bruce_cli.cpp tests/integration/test_nanocell_cli.py CMakeLists.txt
git commit -m "feat: add nanocell cli workflows"
```

### Task 3: CLI-Referenz und Architekturtexte aktualisieren

**Files:**
- Modify: `docs/cli/verify_examples.py`
- Modify: `README.md`, `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`, `docs/cli/commands.md`, `docs/cli/formats.md`, `docs/cli/results-and-control.md`, `docs/cli/local-files.md`
- Modify: `docs/cli/examples.json`, `docs/cli/source-index.md`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: die in Task 2 eingeführte NanoCell-CLI.
- Produces: reproduzierbare Dokuartefakte, die den aktuellen CLI-Vertrag beschreiben.

- [ ] **Step 1: Ergänze zuerst die fehlenden NanoCell-Beispiele im Generator.**

Der temporäre Beispielordner erhält die Referenzkontextdatei. Ergänze einen erfolgreichen `nanocell run --output`, `nanocell replay`, `INCOMPLETE` und mindestens einen negativen Parser-/Technikfall. Die erwarteten Werte sind die in Task 2 definierten Vertragswerte.

- [ ] **Step 2: Führe den Generator aus und bestätige, dass die bestehenden Artefakte zunächst abweichen.**

Run: `python3 docs/cli/verify_examples.py "$BRUCE_FRESH_BUILD"/bruce && git diff -- docs/cli/source-index.md docs/cli/examples.json docs/cli/validation.md`  
Expected: NanoCell-bezogene Artefaktänderungen; bei nicht verfügbarem Git `diff -u` gegen vorab kopierte Dateien verwenden.

- [ ] **Step 3: Dokumentiere den NanoCell-Vertrag und korrigiere Architekturdrift.**

Beschreibe Befehle, Pflichtoptionen, Defaults, Hexformat, Kontextdatei, feste Techniken, Statuswerte, Replay und Nicht-Ziele in den CLI-Seiten. Korrigiere README und Blueprint: NanoCell-Core und begrenzter Sprachpfad sind implementiert; freie Sprache, NanoCell-Integration in die allgemeine Task-Runtime, Cockpit, DAG und GPU bleiben nicht implementiert. Dokumentiere `ExportCommitDurabilityUnknown` und seine Post-Link-Semantik in `local-files.md` und `results-and-control.md`.

- [ ] **Step 4: Regeneriere die drei abgeleiteten Artefakte und prüfe Doku-/CLI-Konsistenz.**

Run: `python3 docs/cli/verify_examples.py "$BRUCE_FRESH_BUILD"/bruce`  
Expected: Exit 0; `source-index.md`, `examples.json` und `validation.md` enthalten NanoCell-Fälle.

- [ ] **Step 5: Führe die vorhandene Doku-Referenzprüfung erneut aus.**

Run: `python3 docs/cli/verify_examples.py "$BRUCE_FRESH_BUILD"/bruce`  
Expected: Exit 0 ohne weitere Änderungen an den drei generierten Dateien.

- [ ] **Step 6: Committe Dokumentation und Generator, falls Git verfügbar ist.**

```bash
git add README.md docs/architecture docs/cli
git commit -m "docs: document nanocell cli and release boundaries"
```

### Task 4: Frischer GitHub-Actions-Releasecheck

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: CMake build, CTest-Registrierung, Doku-Generator und checked-in Artefakte.
- Produces: Push-/Pull-Request-Workflow mit vier Gates: frischer Build, vollständige Tests, Doku-Diff, Secret-Scan.

- [ ] **Step 1: Erstelle einen lokalen, isolierten Doku-Diff-Probeablauf und bestätige ihn mit dem frischen Binary.**

Kopiere den Quellbaum ohne `build*`, `.git`, `.agents` und `.codex` in ein temporäres Verzeichnis, führe dort `verify_examples.py` aus und vergleiche die drei erzeugten Artefakte mit den Originalen. Der Probe darf den Arbeitsbaum nicht ändern.

- [ ] **Step 2: Führe den Probeablauf aus und bestätige den Ausgangszustand.**

Run: `BRUCE_DOC_COPY=$(mktemp -d /tmp/bruce-doccheck-XXXXXX); rsync -a --exclude='.git' --exclude='.agents' --exclude='.codex' --exclude='build*' ./ "$BRUCE_DOC_COPY"/; python3 "$BRUCE_DOC_COPY"/docs/cli/verify_examples.py "$BRUCE_FRESH_BUILD"/bruce; diff -u docs/cli/source-index.md "$BRUCE_DOC_COPY"/docs/cli/source-index.md; diff -u docs/cli/examples.json "$BRUCE_DOC_COPY"/docs/cli/examples.json; diff -u docs/cli/validation.md "$BRUCE_DOC_COPY"/docs/cli/validation.md`  
Expected: Exit 0 und kein Arbeitsbaum-Diff.

- [ ] **Step 3: Implementiere `.github/workflows/ci.yml`.**

Trigger: `push` und `pull_request`. Schritte: Checkout; Ubuntu-LTS; Installation von CMake, Ninja, Compiler, pkg-config, GMP/GMPXX-, OpenSSL- und Python-Entwicklungspaketen; `cmake -S . -B build-ci -G Ninja -DBUILD_TESTING=ON`; Build; `ctest --test-dir build-ci --output-on-failure`; Doku-Diff in temporärer Kopie gegen `build-ci/bruce`; Secret-Scan mit auf konkrete Version/Commit fixiertem Tool. Jeder Gatefehler beendet den Workflow.

- [ ] **Step 4: Validiere die Workflow-Syntax und prüfe alle lokalen CI-Befehle, die die Umgebung zulässt.**

Run: `cmake -S . -B "$BRUCE_FRESH_BUILD" -G Ninja -DBUILD_TESTING=ON && cmake --build "$BRUCE_FRESH_BUILD" && ctest --test-dir "$BRUCE_FRESH_BUILD" --output-on-failure`  
Expected: Exit 0; bei fehlenden Hostbibliotheken klar als Umgebungsblocker berichten. GitHub-Remoteausführung ist erst nach Push verifizierbar.

- [ ] **Step 5: Committe CI und gegebenenfalls den Doku-Probehelfer, falls Git verfügbar ist.**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: verify fresh build tests docs and secrets"
```

### Task 5: Abschließende Abnahme und künftige Regelautorisierung dokumentieren

**Files:**
- Modify: `docs/adr/013-human-rule-authority.md`
- Modify: `docs/contracts/05-human-rule-approval.md`
- Modify: `docs/implementation/174-local-cli-acceptance-profile.md`

**Interfaces:**
- Consumes: unveränderte bestehende Regelstore-/Terminalbestätigungs-CLI.
- Produces: klare Entwicklungs-/Produktionsgrenze ohne neue Runtime-Schnittstelle.

- [ ] **Step 1: Ergänze einen Doku-Testfall bzw. eine explizite Abnahmeaussage für die Nicht-Autorisierungsgrenze.**

Die Aussage muss festhalten, dass Digest und lokale `CONFIRM`-Eingabe konkrete Inhalte binden, aber keine unabhängige Akteursidentität nachweisen, und dass NanoCell-CLI keine Regelaktivierung auslösen kann.

- [ ] **Step 2: Aktualisiere ADR, Vertrag und Abnahmeprofil.**

Ergänze eine „zukünftige Produktionsautorisierung“-Sektion mit Anforderungen: Bindung von Änderung, Ziel, Basisgeneration, Akteur und unabhängiger Entscheidung; getrennte Berechtigungen; Audit, Widerruf und Migration. Markiere sie klar als nicht implementiert. Keine Optionen, APIs oder automatische Aktivierung ergänzen.

- [ ] **Step 3: Führe die vollständige Test- und Dokuabnahme aus.**

Run: `ctest --test-dir "$BRUCE_FRESH_BUILD" --output-on-failure && python3 docs/cli/verify_examples.py "$BRUCE_FRESH_BUILD"/bruce`  
Expected: Exit 0 für beide Befehle; anschließend prüfen, dass die drei generierten Dokuartefakte unverändert bleiben.

- [ ] **Step 4: Führe einen finalen Review der Änderungen gegen die Spezifikation durch.**

Prüfe insbesondere: keine `ApplicationTask`-Integration, kein Überschreiben von Exporten, keine neue Regelautorisierungsruntime, CI verwendet einen frischen Buildpfad, Doku unterscheidet implementiert/geplant.

- [ ] **Step 5: Committe die Abnahmedokumentation, falls Git verfügbar ist.**

```bash
git add docs/adr/013-human-rule-authority.md docs/contracts/05-human-rule-approval.md docs/implementation/174-local-cli-acceptance-profile.md
git commit -m "docs: define production rule authority boundary"
```

## Plan-Selbstprüfung

- **Spec coverage:** Tasks 1–5 decken alle zehn Spezifikationsabschnitte ab; die spätere Regelautorisierung bleibt dokumentiert und explizit nicht implementiert.
- **Typkonsistenz:** NanoCell verwendet ausschließlich die existierenden V1-Signaturen; der neue CLI-Einstieg heißt durchgängig `bruce::nanocell_cli(int, char**)`.
- **Review Focus:** Die fünf genannten Fälle sind in Task 1 oder Task 2 konkret getestet; die Post-Link-Semantik ist zusätzlich in Task 3 dokumentiert.
- **Scope:** Keine Datenbank, API, Plugin-, Cockpit-, GPU- oder DAG-Implementierung vorgesehen.
