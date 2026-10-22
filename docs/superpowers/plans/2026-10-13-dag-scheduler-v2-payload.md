# DAG-Scheduler V2 Payload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Einen eigenständigen, seriell-deterministischen DAG-Scheduler mit begrenztem und digest-geprüftem direktem Byte-Payload-Fluss bereitstellen.

**Architecture:** `dag_scheduler_v2` bleibt ein adapterneutraler Teil von `bruce_core`. Er übernimmt die V1-Auswahl- und Terminalsemantik, besitzt aber eigene V2-Typen für Request-Bytes, direkte Payload-Inputs und Outputs; V1 und Phase-DAG werden nicht eingebunden oder verändert. Ein kleiner unconditional CLI-Adapter stellt ausschließlich die feste Vier-Knoten-Referenz `flow` dar und zeigt keine Rohbytes.

**Tech Stack:** C++20, bestehende `sha256`-Funktion aus `bruce/core/types.hpp`, CMake/Ninja/CTest, Python `unittest` und der vorhandene CLI-JSON-Helfer.

**Spec:** `docs/superpowers/specs/2026-10-13-dag-scheduler-v2-payload-design.md`

## Global Constraints

- V1, Phase-DAG V2/V3, NanoCell, Cockpit, Bundle/Replay sowie bestehende CLI-Verträge bleiben unverändert.
- Keine neue Abhängigkeit, kein Thread, keine parallele Ausführung, keine Datei-, Netzwerk-, GPU- oder Plugin-Schnittstelle.
- Der V2-Kern besitzt ausschließlich die Typen und Funktionen aus Spezifikation §2; er exponiert keine V1- oder Phase-DAG-Typen.
- Specs erlauben 1..64 Knoten, höchstens 15 eindeutige Abhängigkeiten je Knoten, Startbudget 1..64 und Receipt-Budget 1..4.194.304 Bytes.
- Request- und erfolgreiche Payload-Bytes haben jeweils maximal 65.536 Bytes und benötigen den exakten `sha256(bytes)`-Digest.
- Der Scheduler startet nur den lexikographisch kleinsten bereiten Knoten; Callback-Inputs sind ausschließlich direkte `READY`-Vorgänger und nach Produzenten-ID sortiert.
- `FAILED`, `BLOCKED` und `CANCELLED` erhalten kein Output und keine Input-Payloads. Fehlerhafte Specs, Callback-Rückgaben, Exceptions und Receipt-Budgetüberläufe liefern keinen partiellen Run.
- Das Receipt-Budget zählt erfolgreiche Output-Payloads und jede gespeicherte Kopie in `input_refs`, aber keine Requests, Digests, IDs oder Codes.
- Die CLI akzeptiert nur `bruce dag-scheduler-v2 demo flow`, nutzt den Vertrag `dag-scheduler-v2-cli-v1` und zeigt Digests/Größen ohne Rohbytes.
- Die geschützten CLI-Artefakte `docs/cli/source-index.md`, `docs/cli/examples.json` und `docs/cli/validation.md` nur über `docs/cli/verify_examples.py` in einer vollständigen nativen temporären Kopie prüfen oder aktualisieren.
- Der Arbeitsbaum hat keinen verwendbaren Git-Metadatenzugang und enthält Leerzeichen im Pfad. Für jeden Build eine vollständige Kopie ohne Leerzeichen unter `/tmp` verwenden; statt Commits Snapshot, Manifest, reversiblen Unified Diff und Whitespace-Check unter `runs/dag-scheduler-v2-sdd/` ablegen.

## Review Focus

- Ein `READY`-Nachfolger bekommt nur seine direkten erfolgreichen Vorgänger, selbst wenn ein indirekter Vorgänger ebenfalls Payload besitzt; Task 1 prüft Inhalt, Digest und Reihenfolge.
- Das Receipt-Budget muss Output-Payload und jede Input-Kopie zählen; ein Überschreiten nach einem Callback darf keinen Run publizieren. Task 1 prüft beides mit einer absichtlich kleinen Grenze.
- Ein `FAILED`-Callback mit beliebigen Payloadfeldern ist ein ungültiges Callback-Ergebnis, kein normales Failed-Receipt; Task 1 prüft die genaue Fehlerklasse und Null Callback-Folgezustand.
- Die feste CLI-Demo darf niemals Klartext-Payload oder Request-Bytes schreiben, aber muss alle Größen und Digests des tatsächlichen Kerns zeigen; Task 2 prüft vollständiges JSON und Rohtextabwesenheit.
- Unterschiedliche, aber valide Callback-Rückgaben in getrennten Läufen sind nicht ohne Doppelaufruf sicher erkennbar. Task 1 beweist deshalb Repeatabilität bei gleichen Callbacks und dokumentiert diese Grenze statt einer erfundenen Nondeterminismus-Prüfung.

### Task 1: Adapterneutraler V2-Payload-Scheduler

**Files:**
- Create: `include/bruce/dag_scheduler/dag_scheduler_v2.hpp`
- Create: `src/dag_scheduler/dag_scheduler_v2.cpp`
- Create: `tests/unit/dag_scheduler_v2_tests.cpp`
- Modify: `CMakeLists.txt` (V2-Quelle, `bruce_dag_scheduler_v2_tests`, `dag_scheduler_v2_contracts`)

**Interfaces:**
- Produces: alle `DagScheduler*V2`-Typen sowie `validate_dag_scheduler_spec_v2(const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2)` und `execute_dag_scheduler_v2(const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2, const DagSchedulerControlV2& = {})`.
- Consumed by: Task 2 über ein fest sortiertes `DagSchedulerCallbackRegistryV2` und `DagSchedulerRunV2`.

- [ ] **Step 1: Die roten Core-Vertragsprüfungen schreiben**

  Lege `tests/unit/dag_scheduler_v2_tests.cpp` im bestehenden Function-Table-Stil an. Verwende echte Funktionszeiger-Callbacks mit Aufrufzählern und sichere die Kernfälle als vollständige Projektionen ab:

  ```cpp
  require(observed_inputs == "a-source/SOURCE_READY/alpha/...;", "direct payload input changed");
  require(run.receipts.at(3).output->payload_bytes == "alpha:beta|gamma", "join payload changed");
  ```

  Decke `flow`, dynamische lexikographische Ready-Auswahl, direkte sortierte Inputs, Request-/Payload-Digest-Manipulation, 65.537-Byte-Payload, `failed` mit Payload, Callback-Exception, transitive Blockierung, Cancellation, Startbudget, alle Katalog-/Graphvalidierungen, kein partieller Run beim Receipt-Budgetüberlauf und mindestens zwei identische Wiederholungsläufe ab. Prüfe beim Budgetüberlauf explizit, dass Output-Bytes plus Input-Kopien gezählt werden.

- [ ] **Step 2: RED in einer nativen temporären Kopie belegen**

  Kopiere den Arbeitsbaum vollständig nach `/tmp/bruce-dag-scheduler-v2-task1-src`, konfiguriere mit `scripts/with-host-deps.sh` in einem separaten `/tmp`-Build und baue nur `bruce_dag_scheduler_v2_tests`.

  Expected: FAIL, weil Header, Core-Quelle, CMake-Ziel oder öffentliche V2-Symbole noch fehlen. Speichere den tatsächlichen RED-Auszug unter `runs/dag-scheduler-v2-sdd/task-1/`.

- [ ] **Step 3: Den V2-Vertrag minimal implementieren**

  Implementiere in `dag_scheduler_v2.hpp/.cpp` exakt die Signaturen aus der Spezifikation. Übernimm nur die V1-artige Validierung und serielle Auswahl, aber mit eigenen V2-Strukturen. Der Publish-Pfad muss erfolgreiche Outputs und kopierte direkte Input-Payloads vor dem Anhängen budgetieren; bei Überlauf `DagSchedulerV2ReceiptBudgetExceeded` werfen. `failed` verwirft und verbietet Payloadfelder. Callbacks erhalten `request_bytes` unverändert und einen nach `producer_node_id` sortierten `std::span`.

- [ ] **Step 4: GREEN und benachbarte Regressionen beweisen**

  Baue in der nativen Kopie und führe aus:

  ```sh
  scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-dag-scheduler-v2-task1-build --output-on-failure -R '^(dag_scheduler_v2_contracts|dag_scheduler_v1_contracts|phase_dag_v3_contracts)$'
  ```

  Expected: alle drei Tests PASS; insbesondere keine Änderung am V1- oder Phase-DAG-Vertrag.

- [ ] **Step 5: Nicht-Git-Wiederherstellungspunkt ablegen**

  Lege Quellsnapshot, Dateimanifest, Unified Diff gegen den Task-Start, Reverse-Dry-Run und einen Check auf nachträgliche Leerzeichen unter `runs/dag-scheduler-v2-sdd/task-1/` ab.

### Task 2: Feste V2-CLI-Referenz und echter Binary-Workflow

**Files:**
- Create: `apps/dag_scheduler_v2_cli.hpp`
- Create: `tests/integration/test_dag_scheduler_v2_cli.py`
- Modify: `apps/bruce_cli.cpp` (unconditional V2-Route und Hilfezeile)
- Modify: `CMakeLists.txt` (CTest `dag_scheduler_v2_cli_workflows`)

**Interfaces:**
- Consumes: Task 1s `DagSchedulerSpecV2`, `DagSchedulerCallbackEntryV2` und `execute_dag_scheduler_v2`.
- Produces: `int dag_scheduler_v2_cli(int argc, char** argv)` und den stdout-JSON-Vertrag `dag-scheduler-v2-cli-v1` für ausschließlich `demo flow`.

- [ ] **Step 1: Den roten echten CLI-Workflow schreiben**

  Lege die Python-Integration im Muster von `test_dag_scheduler_v1_cli.py` an. Formuliere eine vollständige JSON-Fixture für vier nach `node_id` sortierte Receipts und den chronologischen Trace. Jeder erfolgreiche Receipt enthält nur Metadaten, etwa:

  ```python
  {"result_code": "JOIN_READY", "payload_size": 16,
   "payload_digest": hashlib.sha256(b"alpha:beta|gamma").hexdigest()}
  ```

  Prüfe die korrekten Inputmetadaten für `b-transform` und `d-join`, finalen Zeilenumbruch, leeres stderr, mehrere byteidentische Läufe, Hilfezeile sowie jede ungültige Argumentkombination mit Exit 2 und genauem Usage-JSON. Prüfe zusätzlich, dass `alpha`, `beta` und `gamma` außerhalb der JSON-Metadaten nicht im Bericht vorkommen.

- [ ] **Step 2: RED gegen das aktuelle Binary beweisen**

  Baue `bruce` ohne V2-Route in der nativen Kopie und führe aus:

  ```sh
  python3 tests/integration/test_dag_scheduler_v2_cli.py /tmp/bruce-dag-scheduler-v2-task2-build/bruce
  ```

  Expected: FAIL durch fehlenden Befehl oder falsches Schema. Speichere den echten Fehlerauszug in `runs/dag-scheduler-v2-sdd/task-2/`.

- [ ] **Step 3: Feste Callbacks, Flow-Spec, Renderer und Route implementieren**

  Implementiere nur `a-source`, `b-transform`, `c-independent` und `d-join` mit den fixierten Requests und Payloads aus Spezifikation §5. Die Callbacks validieren ihre jeweils erwarteten direkten Inputs. Der Renderer schreibt Request-Digest und -größe sowie Input-/Output-Digest und -größe, aber keine Bytes. Registriere die Route vor optionalen Modulen und füge nur `bruce dag-scheduler-v2 demo flow` zur Hilfe hinzu.

- [ ] **Step 4: Standard- und abgeschalteten-Modul-GREEN beweisen**

  Führe den Workflow in einem Standardbuild und einem frischen Build mit deaktiviertem NanoCell/Cockpit aus. Vergleiche die zwei Rohantworten byteweise.

  Expected: beide Workflows PASS und beide `flow`-Antworten identisch.

- [ ] **Step 5: Scheduler-Regressionen und Wiederherstellungspunkt ablegen**

  Führe V2-Core/CLI sowie V1, Phase-DAG V2/V3, Signed-Affine-Relation und Toy-Relation fokussiert aus. Speichere Snapshot, Manifest, reversiblen Diff, Rohantwortvergleich und Whitespace-Check unter `runs/dag-scheduler-v2-sdd/task-2/`.

### Task 3: Dokumentation und geschützte CLI-Evidenz

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/03-blueprint-consolidation.md`
- Modify: `docs/cli/README.md`
- Modify: `docs/cli/commands.md`
- Modify: `tests/integration/test_cli_docs.py`
- Generated only through verifier: `docs/cli/source-index.md`, `docs/cli/examples.json`, `docs/cli/validation.md`

**Interfaces:**
- Consumes: Task 2s exakten Befehl, Vertragsversion, feste Spec und JSON-Metadaten.
- Produces: Dokumentation, die den lokalen V2-Payload-Fluss erst nach grüner Ausführung als **IMPLEMENTIERT UND BELEGT** kennzeichnet und alle größeren Runtime-Ansprüche als **DOKUMENTIERT / KONZEPT** lässt.

- [ ] **Step 1: Roten Dokumentationsvertrag ergänzen**

  Ergänze in `test_cli_docs.py` eine eigenständige Methode, die ausschließlich den V2-Abschnitt aus `docs/cli/commands.md` liest. Sie verlangt Befehl, Vertragsversion, vier Knotennamen, serielle lexikographische Auswahl, direkte sortierte Inputs, 65.536-Byte-Grenze, SHA-256, Receipt-Budget inklusive Input-Kopien, verborgen gehaltene Rohbytes, Exit 2, keine freien Eingaben sowie die Grenzen Bundle/Replay/Persistenz/Phase-DAG-Adapter/Parallelität/GPU/Netzwerk.

- [ ] **Step 2: RED über die vorhandene Dokumentation belegen**

  Führe den selektierten unittest mit der bestehenden Binary-first-Aufrufreihenfolge aus.

  Expected: FAIL, weil der V2-Handbuchabschnitt noch fehlt. Speichere den Auszug unter `runs/dag-scheduler-v2-sdd/task-3/`.

- [ ] **Step 3: Dokumentation schreiben und geschützte Artefakte korrekt behandeln**

  Ergänze ausschließlich die V2-relevanten Stellen. Beschreibe die Nicht-Erkennbarkeit gültiger, aber unterschiedlicher Callback-Rückgaben über getrennte Läufe ehrlich. Kopiere dann den vollständigen Baum in einen nativen `/tmp`-Pfad, führe den unveränderten Verifier dort mit dem gebauten Standardbinary aus und übernimm nur die drei von ihm generierten CLI-Dateien, falls sie sich ändern. Halte fest, ob der Verifier V2 tatsächlich aufruft.

- [ ] **Step 4: Dokumentations-GREEN und lokale Links prüfen**

  Führe den gesamten `test_cli_docs.py`-Satz, den Verifier, den neuen realen Workflow und einen lokalen Markdown-Link-Check aus.

  Expected: alle V2-spezifischen Dokumentations- und Workflowprüfungen PASS; eine schon bekannte OFF-Konfigurationsgrenze des globalen Generatortests wird getrennt dokumentiert, nicht als V2-Fehler kaschiert.

- [ ] **Step 5: Vollständige Abnahme und finalen Nicht-Git-Nachweis sichern**

  Erzeuge eine frische native `/tmp`-Kopie und führe die AGENTS-Abnahme aus:

  ```sh
  scripts/with-host-deps.sh cmake -G Ninja -S . -B /tmp/bruce-dag-scheduler-v2-final -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
  scripts/with-host-deps.sh cmake --build /tmp/bruce-dag-scheduler-v2-final
  scripts/with-host-deps.sh ctest --test-dir /tmp/bruce-dag-scheduler-v2-final --output-on-failure
  ```

  Halte den bekannten Cockpit-Socket-Sandbox-Effekt getrennt fest und wiederhole nur diesen Test kontrolliert, falls nötig. Speichere finalen Snapshot, Manifest, vorwärts/rückwärts anwendbaren Unified Diff, Whitespace-Check, Testprotokoll und Generatorbefund unter `runs/dag-scheduler-v2-sdd/final/`.

## Plan Self-Review

- **Spec coverage:** Task 1 implementiert jeden öffentlichen V2-Typ, die Validierung, direkte Payloadweitergabe, Terminalzustände und Receipt-Budget. Task 2 implementiert die einzige CLI-Oberfläche und deren reale Ausgaberegeln. Task 3 sichert alle erklärenden und generierten Dokumentationsbelege ab.
- **Step scan:** Jeder Task enthält einen konkreten roten Test, einen echten RED-Nachweis, eine abgegrenzte Implementierung, einen grünen Nachweis und einen Wiederherstellungspunkt ohne Git.
- **Type consistency:** Der Plan verwendet durchgehend `DagSchedulerNodeSpecV2`, `DagSchedulerInputV2`, `DagSchedulerOutputRefV2`, `DagSchedulerCallbackEntryV2`, `DagSchedulerRunV2`, `validate_dag_scheduler_spec_v2` und `execute_dag_scheduler_v2` aus Spezifikation §2.
- **Review focus:** Direkte Inputs, mehrfache Budgetzählung, fehlgeschlagene Payload-Ergebnisse, Rohdatenleck im CLI und nicht prüfbare Callback-Reinheit sind jeweils einem expliziten Testschritt zugeordnet.
- **Proportion:** Die drei Aufgaben liefern jeweils einen eigenständig testbaren Kern-, Schnittstellen- oder Dokumentationsschritt und erweitern keine unbeteiligten Subsysteme.
