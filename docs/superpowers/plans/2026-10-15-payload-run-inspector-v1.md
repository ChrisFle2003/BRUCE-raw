# Payload Run Inspector V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vorhandene V5-Payload-Runs read-only auf lokale Konsistenz prüfen und vorhandene V1.5-Streams ausdrücklich zusätzlich verifizieren.

**Architecture:** Ein eigener Payload-Inspector-Kern liest Bundle, Log und private Artefakte mit Payload-spezifischen Hostzuständen. Eine schmale neue CLI-Route rendert ausschließlich Metadaten. Der Execute-Inspector und sämtliche Writer bleiben unverändert.

**Tech Stack:** C++20, POSIX-Dateideskriptoren, bestehendes SHA-256/OpenSSL, CMake/CTest, Python-unittest; keine neue Paketabhängigkeit.

**Spec:** [Payload Run Inspector V1 Design](../specs/2026-10-15-payload-run-inspector-v1-design.md)

**Status: VORSCHLAG — Plan zur Prüfung, noch nicht ausgeführt.**

## Global Constraints

- Neue Route exakt `bruce dag-scheduler-v5 payload-inspect BUNDLE [--stream-v15]`.
- Ausschließlich V1.3-/V1.4-Payload-Reportverträge; kein Umbenennen zu Execute.
- Status `VALID`/Exit 0, `INCOMPLETE`/Exit 1, `PROTOCOL_ERROR`/Exit 2 gemäß Spec.
- `--stream-v15` ist eine Prüfanforderung, kein rückwirkender Modusnachweis.
- Keine historische Inputvollständigkeits- oder externe Provenienzbehauptung.
- Kein Workerstart, Replay, Repair, Storezugriff, Payloadwrite oder Publication.
- Alle vorhandenen Execute-, Inspect-, Publish-, Payload- und Worker-v1-Verträge bleiben unverändert.
- JSON-Tiefe maximal 64; Retry 0..3, Workers 1..4, höchstens 15 Payloadobjekte pro Attempt.
- Feste Prüffrist 30000 ms; feste Lesepuffer höchstens 65536 Bytes; Inputobjekt höchstens 1048576 Bytes und Summe höchstens Run-Inputbudget.
- Descriptor-relative no-follow Prüfung; Streamverzeichnisse 0700, Streamdateien 0600, aktueller Eigentümer, ein Dateilink.
- Öffentlich nur die exakt spezifizierten Metadaten; keine Pfade, Payloads, Stream-/Manifestdetails oder OS-Meldungen.
- Generierte CLI-Artefakte ausschließlich durch `docs/cli/verify_examples.py` in einer vollständigen temporären Kopie prüfen/aktualisieren.

## Review Focus

- Ein fehlender ganzer Streambaum darf nicht als historisch vollständig verifiziert ausgegeben werden: Task 2 pinnt null Streams plus `NOT_PROVABLE`.
- READY im Workerlog und FAILED nach Hostpublikation dürfen weder pauschal abgelehnt noch als accepted ausgegeben werden: Task 1 prüft zulässigen und gefälschten Übergang.
- Reservierte Preflight-ID nach einem Retry darf nicht als fehlender gestarteter Worker gelten: Task 1 prüft History und nächsten Ordinal.
- Mutation zwischen Report-/Log-/Streamprüfung darf kein gemischtes gültiges Ergebnis erzeugen: Tasks 1/2 injizieren Austausch nach Öffnung.
- `VALID` bei fachlich FAILED darf keinen Erfolg vortäuschen: Tasks 3/4 pinnen Exit, `run_status`, Anleitung und unveränderte Dateien.

---

## Dateigrenzen und Subagent-Aufteilung

| Task | Eigentümer/Dateien | Ergebnis und Abhängigkeit |
|---|---|---|
| 1 | neuer Kern `include/bruce/artifact/payload_run_inspector_v1.hpp`, `src/artifact/payload_run_inspector_v1.cpp`; neue Unit-Tests; CMake | Payload-Report-/Log-Abgleich und lokale Request-/Outputprüfung |
| 2 | neue private Streamprüfung in `src/artifact/payload_run_stream_inspector_v1.hpp` und `.cpp`; Kern-Aufruf; neue Unit-Tests; CMake | optionale Streamprüfung; benötigt Task-1-Typen |
| 3 | `apps/dag_scheduler_v5_cli.hpp`; neue CLI-Unit-/Integrationstests; CMake | öffentliche Route und exakter Metadatenrenderer; benötigt Tasks 1/2 |
| 4 | bestehende CLI-/Blueprint-/Implementierungsdocs und Doc-Tests | Dokumentation und Gesamtverifikation; benötigt Task 3 |

Tasks nacheinander implementieren. Je Task frischen Implementer und unabhängigen
Reviewer einsetzen; gemeinsames CMake und Kernheader haben genau einen aktiven
Schreiber. Reviewer prüft Spec-Konformität und Codequalität vor dem Folgetask.
Abschließend unabhängige Gesamtreview. Keine Implementierung durch den
Koordinator neben einem laufenden Implementer. Begrenzte Korrekturrunden nach
SDD; Entscheidungen und Nachweise im `.superpowers/sdd/`-Ledger festhalten.

Vor Implementierung Spec und Plan vom Benutzer prüfen lassen; die bereits
gewählte Methode Subagent-driven bleibt erhalten. Dieser Planungsauftrag
schreibt keinen Produktcode und setzt eine neue Freigabe nicht voraus.

## Umgebungs- und Evidenzregeln

Vor Beginn aktuellen Zustand prüfen. Der letzte V1.5-Nachweis meldet fehlende
brauchbare Git-Metadaten: kein clean-tree-, Commit- oder Worktree-Nachweis.
`git status --short` und `git diff --check` erneut versuchen; bei unverändertem
Befund eng begrenzte Dateisnapshots vor Änderungen und Hash-/Whitespacevergleich
verwenden, im Ledger als Ersatz mit begrenzter Herkunftsevidenz kennzeichnen.
Kein `git init`, kein Löschen und keine fingierten Commits. Wenn Git tatsächlich
verfügbar wird, nach jedem akzeptierten Task nur dessen Dateien committen.

Der vorgeschriebene Host-Wrapper scheiterte zuletzt am importierten GMPXX-Pfad
im Checkout mit Leerzeichen. Der vorhandene native Build
`/tmp/bruce-v14-core-build` war nutzbar. Wrapper zunächst wie AGENTS.md prüfen;
bei reproduzierbarem Fehler diesen belegen und den vorhandenen nativen Build
verwenden. Keine beiläufige Build-/Dependency-Reparatur oder Installation.
Der letzte vollständige Testlauf ergab 188/189 wegen gesperrter Cockpit-Sockets;
spätere V1.5-Fixes wurden nur fokussiert verifiziert. Dies ist historische
Evidenz, kein Ergebnis dieser zukünftigen Implementierung.

### Task 1: Read-only Payload-Report-/Log-Kern

**Files:**
- Create: `include/bruce/artifact/payload_run_inspector_v1.hpp`
- Create: `src/artifact/payload_run_inspector_v1.cpp`
- Create: `tests/unit/payload_run_inspector_v1_tests.cpp`
- Modify: `CMakeLists.txt` (Core source und neues Testziel)

**Interfaces:** Namespace `bruce::artifact_v1`.
- `struct PayloadRunInspectOptionsV1 { bool verify_streams_v15{false}; };`
- `struct PayloadRunInspectionV1` mit `bool run_finished`, Strings
  `run_status`, `source_contract`, `run_id`, uint64-Feldern `last_seq`,
  `node_count`, `attempt_count`, `accepted_artifact_count`,
  `verified_stream_count`, `verified_stream_bytes`, bool `streams_requested`.
- Produces: `PayloadRunInspectionV1 inspect_payload_run_v1(const std::filesystem::path& bundle, const PayloadRunInspectOptionsV1& options = {});`
- Wirft ausschließlich `bruce::Error("PROTOCOL_ERROR")` an der öffentlichen
  Fehlergrenze. Kein renderbares Ergebnis bei unvollständiger Prüfung.
- Consumes: existierende Workerlog-Datenform und Identifier-/Budget-/Digestregeln
  als Referenz. Alter `inspect_worker_run_v1` wird nicht verändert oder benutzt,
  um Payload-Publikationsfehler durch Execute-Regeln zu schicken.

- [ ] **Step 1: Failing tests schreiben.** Fixturebuilder erzeugt reale
  kanonische Solo-/Multi-Reports und Logs nach den aktuellen Renderern.
  `valid_v13_and_v14` erwartet richtige Counts, Status und Digestsicherheit;
  `host_preflight_without_started_attempt` erwartet VALID für reservierte ID
  ohne Log/History; `retry_then_preflight_failure` erhält frühere History;
  `publication_failure_retains_unaccepted_ready_result` erwartet FAILED-Run,
  null accepted Count und erfolgreich geprüfte unaccepted Outputdatei.
  Negativtests verändern jeweils genau eine Bedingung der Spec-Zustandstabelle
  und erwarten `PROTOCOL_ERROR`; insbesondere erfundene Hostfehlercodes,
  Acceptance trotz FAILED, falsche Retry-ID, fremder Logattempt, manipulierte
  Usage, Duplikate, Reihenfolge, Budgetüberlauf und fehlender Request/Output.
- [ ] **Step 2: RED nachweisen.** Neues Ziel
  `bruce_payload_run_inspector_v1_tests`/CTest
  `payload_run_inspector_v1_contracts` registrieren; Build/CTest muss wegen
  fehlender neuer Implementierung oder konkreter Assertion fehlschlagen.
- [ ] **Step 3: Kern implementieren.** Kanonischen Reportparser und eigenen
  Payload-Logzustandsabgleich im neuen `.cpp` kapseln. Bestehende Inspector-
  Lese-/Parsermuster als Referenz verwenden, ohne private Funktionen aus dem
  großen CLI-Header in den Core zu importieren. Deadline vor Opens/Reads/Hashes;
  Metadatensnapshots, Endabgleich, overflow-sichere Summen und begrenzte Allokation.
  Spec-Tabelle für host-only Fälle explizit modellieren, keine globale Lockerung
  von Status-/Digest-/Historyregeln. Bis Task 2 muss Option true kontrolliert
  abgewiesen werden, statt ungeprüfte Streams erfolgreich zu melden.
- [ ] **Step 4: Fehler- und Mutationsregressionen ergänzen.** `incomplete_is_not_truncated`
  entfernt nur finale RUN_FINISHED-Zeile (INCOMPLETE), schneidet anschließend
  eine JSON-Zeile ab (PROTOCOL_ERROR). `no_symlink_fifo_hardlink` testet jeden
  gelesenen Artefakttyp. `snapshot_mutation` tauscht Datei/Log nach Öffnung aus;
  `deadline_before_next_read` injiziert Zeitablauf und prüft keinen weiteren
  Payloadread. Testhooks ausschließlich Testlinker/Interposition, keine neuen
  Produktoptionen. Dateien bleiben byte-/mode-/mtime-identisch.
- [ ] **Step 5: GREEN und Review.** Build und
  `ctest --test-dir /tmp/bruce-v14-core-build -R '^(payload_run_inspector_v1_contracts|worker_run_inspector_v1_contracts)$' --output-on-failure`
  müssen Exit 0 liefern. Reviewer erhält Spec, Taskdiff und RED/GREEN-Ausgaben;
  akzeptierten Stand im Ledger sichern, Commit nur bei nutzbarem Git.

### Task 2: Explizite Prüfung erhaltener Streams

**Files:**
- Create: `src/artifact/payload_run_stream_inspector_v1.hpp`
- Create: `src/artifact/payload_run_stream_inspector_v1.cpp`
- Create: `tests/unit/payload_run_stream_inspector_v1_tests.cpp`
- Modify: `src/artifact/payload_run_inspector_v1.cpp`, `CMakeLists.txt`

**Interfaces:** Privater Namespace `bruce::artifact_v1::detail`.
- `struct PayloadStreamInspectionV1 { std::uint64_t count{}, bytes{}; };`
- Produces: `PayloadStreamInspectionV1 inspect_payload_run_streams_v1(const std::filesystem::path& run_root, const std::vector<std::string>& started_attempt_ids, std::uint64_t max_input_bytes, std::chrono::steady_clock::time_point deadline);`
- Consumes: Task 1 liefert ausschließlich verifizierte gestartete IDs und das
  Report-Inputbudget. Bestehender `ArtifactPayloadStreamV15` und dreiparametriger
  `verify_payload_stream_v15` bleiben unverändert.

- [ ] **Step 1: Failing tests schreiben.** Fixtures benutzen bestehenden
  Streamwriter nur zum Testaufbau. `canonical_present_streams` prüft leer,
  Einchunk, 65536-Grenze, Mehrchunk und mehrere Attempts; `no_history_claim`
  prüft fehlenden/entfernten gesamten Baum mit count 0. `name_is_not_proof`
  legt passend benannte falsche Header/Seal/Chunks an und erwartet Fehler.
  `compatibility_matches_stream` verändert allein input.bin und erwartet Fehler.
  `reject_unknown_or_unstarted_attempt` und `strict_input_membership` prüfen
  fremde/reservierte IDs, Indexlücken, Index 15, source.tmp, Stagingreste und
  Legacy-Dateien. Alle bleiben nach Fehlerrückgabe unangetastet.
- [ ] **Step 2: RED nachweisen.** Testziel
  `bruce_payload_run_stream_inspector_v1_tests`, CTest
  `payload_run_stream_inspector_v1_contracts` hinzufügen und reproduzierbar
  fehlschlagenden Lauf dokumentieren.
- [ ] **Step 3: Streamprüfung implementieren.** Strenges begrenztes Layout,
  Header/Seal inhaltlich lesen, Metadaten daraus ableiten und an bestehenden
  Verifier mit derselben Deadline geben. Kompatibilitätsdatei unabhängig hashen.
  Snapshotprüfung umfasst Elternverknüpfung und exakte Mitgliedschaft;
  Stream-/Inputdateien nie reparieren. Task-1-Kern ruft den neuen privaten
  Helfer nur bei expliziter Option auf und übernimmt ausschließlich Counts.
- [ ] **Step 4: Mutations-/Budgettests ergänzen.** Chunk, Seal, input.bin und
  Elternverzeichnis während des Lesens austauschen; Ablauf nach erstem Stream
  injizieren. Expect PROTOCOL_ERROR, keine späteren Reads und keine Löschung.
  Per-Objektlimit, Summe, Überlauf und Pfadsicherheit aller Ebenen abdecken.
- [ ] **Step 5: GREEN und Review.**
  `ctest --test-dir /tmp/bruce-v14-core-build -R '^(payload_run_inspector_v1_contracts|payload_run_stream_inspector_v1_contracts|artifact_payload_stream_v1_contracts)$' --output-on-failure`
  liefert Exit 0. Review prüft insbesondere keine Dateinamenheuristik und keinen
  historischen Inputnachweis. Stand wie Task 1 sichern.

### Task 3: CLI, Metadatenreport und reale Läufe

**Files:**
- Modify: `apps/dag_scheduler_v5_cli.hpp`, `CMakeLists.txt`
- Create: `tests/unit/dag_scheduler_v5_payload_inspector_cli_tests.cpp`
- Create: `tests/integration/test_dag_scheduler_v5_payload_inspector.py`

**Interfaces:** Im bestehenden CLI-detail-Namespace:
- `struct PayloadInspectRequestV1 { std::filesystem::path bundle; bool stream_v15{false}; };`
- `PayloadInspectRequestV1 parse_payload_inspect_v1(std::vector<std::string_view> args);`
- `std::string render_payload_inspection_v1(const artifact_v1::PayloadRunInspectionV1& inspection);`
- `int payload_inspect_v1(const PayloadInspectRequestV1& request);`
- Consumes: Task-1-Kern plus Task-2-Option; kein eigener Dateiscanner in der CLI.

- [ ] **Step 1: Failing CLI-tests schreiben.** Exakte Argumentgrammatik und
  Spec-JSON-Schlüsselreihenfolge prüfen. `stream_check` aus angeforderter Option,
  immer `input_completeness=NOT_PROVABLE`, `external_provenance=NOT_CHECKED`.
  Protokollfehler exakt eine sichere stderr-Zeile/Exit 2/kein stdout.
- [ ] **Step 2: RED mit echter Binary nachweisen.** Neue CTest-Einträge
  `dag_scheduler_v5_payload_inspector_cli_contracts` und
  `dag_scheduler_v5_payload_inspector_workflows` registrieren. Fehlende Route
  muss gezielt fehlschlagen; alte Route vorher als Baseline erfassen.
- [ ] **Step 3: Parser/Dispatcher/Renderer implementieren.** Nur neue Route
  ergänzen; bestehende Execute-/Inspect-/Publish-Branches bleiben funktional
  und in Fehlermeldungen unverändert. Core-Ausnahme an sicherer Fehlergrenze
  abfangen; keinerlei `error.what()` oder private Daten rendern.
- [ ] **Step 4: Reale Integrationsmatrix ausführen.** V1.3/V1.4, Solo/Multi,
  Stream an/aus, direkte/externe gemischte Eingaben, READY und FAILED, Retry,
  Preflight mit BLOCKED, Publikationsfehler, fehlende Endzeile und Korruption.
  Vorher/Nachher Snapshot einschließlich Store; tatsächliche Bytewerte und
  deren Hex sowie private Namen in stdout/stderr ausschließen. Inspektion
  nach Entfernen des Store muss gleich bleiben. Zweimal prüfen ergibt dieselbe
  Ausgabe. Alter Inspector weist Payload weiterhin ab, neuer Execute ab.
- [ ] **Step 5: GREEN und Review.** Neue zwei CTests sowie alle vorhandenen
  `dag_scheduler_v5`-, Worker-Inspector- und Stream-Tests ausführen.
  Vorher/Nachher Execute-/Inspect-Ausgaben bytegleich; Ausführungsausgaben nur
  vorhandene wall_ms/peak_memory_bytes maskieren. Keine neu maskierten Felder.
  Reviewer prüft Kontaminationsfreiheit alter Verträge; Stand sichern.

### Task 4: Bedienung, Evidenz und Gesamtabnahme

**Files:**
- Modify: `docs/cli/commands.md`, `docs/cli/formats.md`,
  `docs/architecture/03-blueprint-consolidation.md`, `docs/implementation/README.md`
- Modify: `tests/integration/test_cli_docs.py`
- Create: `runs/bruce-payload-run-inspector-v1-2026-10-15.md`

**Interfaces:** Ausschließlich bestätigte neue CLI- und Statusverträge aus Task 3.

- [ ] **Step 1: Doc-Vertrag zuerst testen.** Tests verlangen neue Route,
  alle drei Status, `VALID` bei FAILED, ausdrückliche Streamanforderung,
  NOT_PROVABLE/NOT_CHECKED und keine historischen Vollständigkeitsversprechen.
- [ ] **Step 2: Dokumentation aktualisieren.** Beispiele mit echten lokalen
  Payload-Läufen; Limit und Fehlerfall verständlich erklären. Status
  IMPLEMENTIERT UND BELEGT erst nach erfolgreicher Prüfung; bekannte
  Build-/Sandbox-/Git-Grenzen genau datieren.
- [ ] **Step 3: Dokumentationsprüfung.**
  `python3 tests/integration/test_cli_docs.py` und
  `python3 scripts/check_local_doc_links.py`; beide Exit 0.
  CLI-Verifier mit den existierenden dokumentierten Parametern nur in einer
  vollständigen temporären Kopie ausführen; Generatdateien im Workspace auf
  unveränderte Hashes prüfen.
- [ ] **Step 4: Abnahme durchführen.** Vollständiger Build, fokussierte neue
  und bestehende CTests, danach einmal vollständiges CTest. Exakte Befehle,
  Exitcodes, Pass/Fail/Skip-Zahlen und Ursache jedes nicht grünen Ergebnisses
  im Runbericht festhalten. Git-/Whitespace-/Linkprüfungen gemäß AGENTS.md.
- [ ] **Step 5: Unabhängige Gesamtreview.** Spec, Plan, Rulings, Taskreports,
  Dateisnapshots und Abnahmebericht übergeben. Keine Gesamtfreigabe ohne
  Reviewergebnis; verbleibende Limitierungen ausdrücklich benennen.

## Selbstprüfung des Plans

Spec-Abdeckung: Report/Log und Hostzustände Task 1; Stream-/Mutationsgrenzen
Tasks 1/2; CLI, Status, Leckfreiheit und Kompatibilität Task 3;
Dokumentation und Belege Task 4. Jeder Review-Focus hat einen konkreten Test.
Typen zwischen Tasks stimmen überein; der Streamhelper bleibt privat.
Kein Task verlangt Produktcodeänderungen am alten Worker-/Execute-Inspector,
Storewriter, Payloadwriter oder Workerframe. Fehlende historische Nachweise
werden öffentlich ausdrücklich benannt und nicht durch neue versteckte
Schreibvorgänge nachgerüstet.
