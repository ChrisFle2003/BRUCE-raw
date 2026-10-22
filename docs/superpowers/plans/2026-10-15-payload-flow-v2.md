# Payload Flow V2 — Implementierungsplan

> **Status:** ABGESCHLOSSEN — **IMPLEMENTIERT UND BELEGT** im fokussiert
> abgenommenen lokalen Konsistenzumfang (2026-10-15). Kernmodell, Host-Lineage,
> Sidecar und read-only Inspector sind implementiert. Die drei
> Implementierungstasks und ihre unabhängigen Reviews sind unten verlinkt;
> Dokumentation und finale fokussierte Verifikation sind abgeschlossen.

**Ziel:** Den bestehenden V1.3-Payloadlauf mit einem kanonischen, lokalen
Sidecar BUNDLE.flow-v2.json ergänzen. Das Sidecar macht Producer-/Consumer-
Kanten, Attempt-/Input-Indizes sowie Spec- und Report-Digests prüfbar. Der
bestehende Legacy-Report, Worker-v1-Frame, Supervisor, V1.5-Stream und alle
V2/V3/V4-Routen bleiben unverändert.

**Architektur:** Bruce erzeugt die Lineage ausschließlich aus bereits
akzeptierten Producer-Receipts, geprüften READY-Manifesten und den tatsächlichen
Consumer-Attempts. Der Sidecar enthält nur Metadaten. Ein read-only
Flow-V2-Inspector prüft Sidecar, Legacy-Bundle, JSONL-Log und private
Input-/Output-Dateien lokal und meldet eine metadata-only Zusammenfassung.

**Nicht-Ziele:** Keine neue DAG-Scheduler-V2-API, kein Worker-v2, keine
Frame-Erweiterung, kein externer Payloadspeicher, kein Netzwerk, keine GPU,
keine freien Code-Plugins, keine freie Sprache, kein allgemeiner Beweiser,
keine Replay-/Repair-/GC-Funktion und keine übergreifende Runtime-Orchestrierung.

## Abschluss und Evidenz

| Task | Status und Beleg |
|---|---|
| 1 — Kernmodell und Codec | **IMPLEMENTIERT UND BELEGT**: Taskbericht (`runs/payload-flow-v2-task1.md`) und Review mit abschließender P1-Re-Review: PASS (`runs/payload-flow-v2-task1-review.md`); archivale lokale, nicht versionierte Pfade |
| 2 — Hostintegration und Lineage | **IMPLEMENTIERT UND BELEGT**: Taskbericht (`runs/payload-flow-v2-task2.md`) und unabhängige Review: PASS (`runs/payload-flow-v2-task2-review.md`); archivale lokale, nicht versionierte Pfade |
| 3 — Read-only Inspector | **IMPLEMENTIERT UND BELEGT**: Taskbericht der Fix-Welle (`runs/payload-flow-v2-task3.md`) und finale scoped Re-Review: PASS (`runs/payload-flow-v2-task3-review.md`); archivale lokale, nicht versionierte Pfade |
| 4 — Dokumentation und Verifikation | **IMPLEMENTIERT UND BELEGT**: abschließender Laufbericht (`runs/bruce-payload-flow-v2-2026-10-15.md`); archivaler lokaler, nicht versionierter Pfad |

Der finale Laufbericht belegt den nativen Build sowie **5/5 fokussierte
CTests**, **29/29 CLI-Dokumentationstests**, lokale Linkprüfung und die
CLI-Generatorprüfung in einer vollständigen temporären Kopie. Die finale
Task-3-Re-Review belegt außerdem **13/13 direkte Inspector-Workflows**.
Quellen, Tests und CMake sind im
[abgenommenen Design](../specs/2026-10-15-payload-flow-v2-design.md#implementierungs--und-prüfbelege)
verlinkt.

Eine vollständig grüne Gesamtsuite wird nicht behauptet: Die frühere zentrale
CTest-Abnahme ergab **192/193** bestandene Ziele; das verbleibende Ziel
`cockpit_inspector_workflows` scheiterte an einer lokalen
`PermissionError`-Socketbindung. Der Host-Dependencies-Wrapper war wegen des
malformierten `PkgConfig::GMPXX`-Pfads nicht als zusätzlicher Abnahmepfad nutzbar;
verwendet wurde der native `/tmp/bruce-v14-core-build`. Fehlende Git-Metadaten
verhinderten belastbare Aussagen aus `git diff --check` und Git-Status.

## Bestehende Beleglage

- **IMPLEMENTIERT UND BELEGT:** Der direkte synchrone In-Memory-Payloadfluss
  des DAG-Schedulers V2 steht in
  include/bruce/dag_scheduler/dag_scheduler_v2.hpp,
  src/dag_scheduler/dag_scheduler_v2.cpp und den V2-Tests.
- **IMPLEMENTIERT UND BELEGT:** Der V1.3-Hostfluss nutzt
  prepare_payload_inputs() und publish_payload_output() in
  apps/dag_scheduler_v5_cli.hpp. Reproduzierbare Workflows prüfen echte
  Consumerbytes und unveränderte Legacy-Reportbytes.
- **IMPLEMENTIERT UND BELEGT:** V1.5 ist ein optionaler Stream innerhalb des
  bestehenden privaten Attempt-Layouts; der Worker konsumiert weiterhin
  input.bin.
- **IMPLEMENTIERT UND BELEGT:** V1-Payload-Inspector und Stream-Inspector
  prüfen bereits lokale Run-/Dateiintegrität. Flow V2 ergänzt nur die
  Producer-/Consumer-Lineage und die Digest-/Indexbindung.

## Öffentlicher Vertrag

Payload-Ausführung:

~~~text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE --flow-v2 [--stream-v15] [bestehende Optionen]
~~~

Flow-V2-Inspektion:

~~~text
bruce dag-scheduler-v5 payload-inspect BUNDLE --flow-v2
~~~

Regeln:

- --flow-v2 ist genau einmal erlaubt und nur in diesen beiden Routen gültig.
- --external-inputs wird mit --flow-v2 unmittelbar nach dem syntaktischen
  CLI-Parse und vor jedem Datei-Read oder jeder Datei-/Runroot-Anlage
  abgewiesen: insbesondere vor read_external_inputs_v14(), parse_spec(),
  Store-Öffnung und execute(), auch wenn die Liste leer ist. V1.4 bleibt
  separat nutzbar.
- --stream-v15 ist bei payload --flow-v2 optional; der Sidecar speichert den
  Modus. Eine Stream-Option für payload-inspect ist ungültig, weil der Inspector
  den Modus aus dem Sidecar liest.
- Ohne --flow-v2 bleiben Parsing, Dateimenge, Reportbytes, Exit-Codes und
  bestehende payload-inspect-Semantik unverändert.
- Ein vorhandenes BUNDLE, BUNDLE.run/ oder BUNDLE.flow-v2.json wird vor dem
  Lauf abgewiesen; kein Überschreiben und keine Wiederaufnahme.

## Sidecar-Vertrag

BUNDLE.flow-v2.json ist eine kompakte kanonische JSON-Zeile mit abschließendem
LF, Mode 0600. Die Schlüsselfolge ist fest:

~~~text
contract="bruce-payload-flow-v2"
run_id
spec_digest
report_digest
stream_v15
nodes[]
  node_id, callback_id, request_digest, dependencies[]
attempts[]
  consumer_node_id, consumer_attempt_id, inputs[]
    input_index, producer_node_id, producer_attempt_id,
    manifest_id, object_index, sha256, size_bytes
~~~

- report_digest ist SHA-256 der exakten Legacy-BUNDLE-Bytes.
- spec_digest ist exakt sha256(parsed.raw_bytes), also der vorhandene V5-Digest
  der eingelesenen Specbytes. Der Inspector bewertet damit nur interne
  Konsistenz; ohne Original-Spec oder Signatur wird keine historische
  Originalität/Authentizität behauptet. Eine neue kanonische Serialisierung ist
  ausdrücklich nicht Teil dieser Phase.
- nodes[] behält die ursprüngliche Spec-Reihenfolge; dependencies[] ebenso.
  attempts[] folgt der Reihenfolge von ATTEMPT_STARTED.
- Root- und tatsächlich gestartete Retry-Attempts werden gespeichert;
  reservierte, nie gestartete Preflight-IDs nicht. Vor dem Workerstart wird ein
  unveränderlicher Satz aus Dependency-Reihenfolge, Producer-Node/-Attempt,
  Manifest, Objektindex, Digest und Größe erzeugt. Erst ein erfolgreich
  geschriebenes ATTEMPT_STARTED macht ihn zum Sidecar-Attempt; spätere Retries
  überschreiben ihn nie. Jeder gestartete Fehlversuch und Retry behält den
  vollständigen eigenen Satz; pro Vorgänger entsteht ein lückenloser
  input_index.
- Es werden keine Pfade, Payloadbytes, PIDs, Kommandozeilen, errno-Texte,
  Secretwerte oder streaminternen Namen gespeichert.
- Sidecarfehler liefern FLOW_RECORD_INVALID und keinen partiellen öffentlichen
  Erfolg. Der Sidecar ist kein Dateisystem-Transaktionsjournal: ein Crash kann
  den Legacy-Report ohne Sidecar hinterlassen.

## Fehler- und Exit-Vertrag

| Situation | Verhalten |
|---|---|
| fehlendes/dupliziertes --flow-v2, falsche Route, Optionenkonflikt | bestehender Usage-Fehler, Exit 2 |
| --external-inputs mit Flow V2 | Usage-/Protocol-Fehler vor Storezugriff, Exit 2 |
| ungültige Spec, unbekannte Dependency, Zyklus | bestehender V1.3-Fehler, kein Sidecar-Erfolg |
| Preflight-/Manifest-/Digestfehler | bestehende V1.3-Fehler, Node FAILED/BLOCKED |
| Sidecar-Encode-/Write-/Adoptionsfehler | {"error":"FLOW_RECORD_INVALID"} LF, Exit 2 |
| vollständiger Sidecar mit fachlich FAILED/BLOCKED-Lauf | Legacy-Ergebnisstatus, Exit 1 |
| konsistenter fertiger Inspector-Lauf | VALID, Exit 0 |
| konsistenter unfertiger Lauf ohne Abschluss | INCOMPLETE, Exit 1 |
| JSON-/Lineage-/Datei-/Mutationfehler beim Inspector | {"error":"PROTOCOL_ERROR"} LF, Exit 2 |

Öffentliche Reports und Diagnosen enthalten keine Payloadbytes, privaten Pfade
oder unbereinigten Betriebssystemfehler.

## Sicherheits- und Ressourcenregeln

Die vorhandenen V1.3/V1.5-Budgets, Retryregeln und Workergrenzen bleiben
maßgeblich. Flow V2 führt kein zusätzliches Payloadbudget ein. Neue Sidecar- und
Inspector-Lesewege verwenden die bestehenden Größenobergrenzen, feste
Lesepuffer, O_NOFOLLOW|O_CLOEXEC, reguläre owner-only Single-Hardlink-Dateien,
descriptor-relative Auflösung sowie Vor-/Nachprüfung der Dateimetadaten. Der
Inspector schreibt, löscht oder repariert nichts.

## Umsetzung und Abnahme

Die folgenden Arbeitsschritte bewahren den ursprünglichen Auftrag und dessen
Abnahmeumfang. Ihr Abschluss ist durch die oben verlinkten Taskberichte und
finalen Reviews dokumentiert; sie sind keine offene Aufgabenliste und keine
zusätzliche Behauptung einer vollständig grünen Gesamtsuite.

### Task 1 — Kernmodell, kanonisches Sidecar und Parser

**Dateien:**

- Erstellen: include/bruce/artifact/payload_flow_v2.hpp
- Erstellen: src/artifact/payload_flow_v2.cpp
- Erstellen: tests/unit/payload_flow_v2_tests.cpp
- Ändern: CMakeLists.txt

**Ergebnis:** Bounded value types für Node-Lineage, Attempt-Bindings und
Sidecar-Metadaten; kanonisches Encode/Decode; Digest-/ID-/Index-/Reihenfolge-
und Größenvalidierung. Vor diesem Task wird die gemeinsame versionierte
Manifest-ID-Hilfe v5_manifest_id_v1(run_id,node,attempt) aus dem CLI-Inline-
Helper in eine Artifact-/V5-Vertragskomponente extrahiert. Der Kern besitzt
keine Store- oder Workeraufrufe.

- Zuerst RED-Tests für gültiges Sidecar, leere Rootinputs,
  Fan-in/Fan-out, Retries, BLOCKED-Nodes und exakte JSON-Schlüsselreihenfolge
  schreiben.
- Tests für unbekannte/duplizierte Schlüssel, falsche Zahlenform,
  fehlendes LF, trailing bytes, Übergröße, ungültige IDs/Digests,
  lückenhafte Inputindizes und doppelte Attempts ergänzen.
- Kanonisches Modell und bounded Reader/Writer implementieren; keine
  Payloadbytes oder Pfade im Modell speichern.
- Gemeinsamen Manifest-ID-Helper in
  include/bruce/artifact/v5_manifest_id_v1.hpp (mit der nötigen
  CMake-/Source-Anbindung) einführen, CLI und Inspector darauf umstellen und
  vorhandene Manifest-ID-Fixtures unverändert bestätigen.
- Fokusziel payload_flow_v2_contracts sowie bestehende Artifact- und
  Inspector-Ziele ausführen.

### Task 2 — Hostintegration payload --flow-v2 und Lineageaufzeichnung

**Dateien:**

- Ändern: apps/dag_scheduler_v5_cli.hpp
- Ändern: tests/unit/dag_scheduler_v5_payload_cli_tests.cpp
- Ändern: tests/integration/test_dag_scheduler_v5_payload.py
- Ändern: CMakeLists.txt nur für neue Quellen/Tests

**Ergebnis:** Opt-in Parserroute, Ablehnung von V1.4-Inputs, deterministische
Aufzeichnung der akzeptierten direkten Kanten und atomare Sidecaradoption nach
dem unveränderten Legacy-Report.

- Parser-RED-Tests für exakte Syntax, doppelte/unbekannte Optionen,
  --flow-v2 ohne Store/Output, --external-inputs-Konflikt und bytegleiche
  Legacy-Routen schreiben. Die Konfliktprüfung muss mit nicht vorhandener
  External-Datei und ungültigem/nicht zugreifbarem Store beweisen, dass weder
  Spec, Datei, Store noch Runroot berührt werden.
- V5-Run-Integration so erweitern, dass ein Producer erst nach
  erfolgreicher Manifestpublikation als Kantenquelle gilt und jeder
  gestartete Consumer-Attempt seine unveränderliche Bindingsammlung in
  Dependency-Reihenfolge erhält. Die Sammlung wird vor Workerstart aufgebaut,
  aber nur nach erfolgreichem ATTEMPT_STARTED in die historische Liste
  übernommen; ein gestarteter Fehler plus Retry muss beide Sätze erhalten.
- Sidecar erst nach abgeschlossenem Legacy-Bundle erzeugen; bei
  --stream-v15 den Modus markieren, aber Worker-/Streamverträge nicht ändern.
- Fan-in, Fan-out, Kette, leere Payload, Retry, Preflightfehler und
  BLOCKED-Nachfolger testen. Sicherstellen, dass Rootnodes inputs:[] erhalten.
- Exakte Inputlayouts testen: Legacy flach
  payload-inputs/n<ordinal>-<attempt-id>-<index>.bin, Stream verschachtelt
  payload-inputs/<attempt-id>/input-<index>/ mit input.bin und
  stream-payload/{header.jsonl, chunk-00000000000000000000.bin aufwärts,
  seal.json}. Die
  globale Dateimenge, leere Rootinputs, gestartete Fehlversuche und Retries
  sowie gemischte/zusätzliche Einträge müssen abgedeckt sein.
- Legacy-Reportbytes bis nach erfolgreicher Sidecar-Adoption puffern.
  Encode-, Größen-, Schreib- und Adoptionsfehler müssen stdout leer lassen,
  stderr exakt {"error":"FLOW_RECORD_INVALID"} plus LF schreiben und Exit 2
  liefern; fachlich FAILED/BLOCKED mit vollständigem Sidecar gibt Report und
  Exit 1 aus.
- V1.3, V1.4 ohne Flow, V1.5 und V2/V3/V4 bestehende Tests ausführen;
  bestehende Legacy-Reportbytes müssen unverändert bleiben.

### Task 3 — Read-only Inspector payload-inspect BUNDLE --flow-v2

**Dateien:**

- Erstellen: include/bruce/artifact/payload_flow_inspector_v2.hpp
- Erstellen: src/artifact/payload_flow_inspector_v2.cpp
- Erstellen: tests/unit/payload_flow_inspector_v2_tests.cpp
- Erstellen: tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py
- Ändern: apps/dag_scheduler_v5_cli.hpp, CMakeLists.txt

**Ergebnis:** Read-only Validierung von Sidecar, Legacy-Bundle, Workerlog und
privaten Dateien. Kein Storezugriff und keine Worker-/Adapterausführung. Der
Inspector verwendet eine integrierte PayloadFlowSnapshotV2-Reader-/Deadline-
Sicht für alle Dateien und ruft inspect_payload_run_v1() nicht als getrennten
ersten Vollscan auf; wiederverwendet werden dürfen nur gemeinsame interne
bounded Prüfhelfer.

- Gültige Kette, Fan-in/Fan-out, Root, Retry und fachlich FAILED/BLOCKED
  testen; Exit 0/1 und VALID/INCOMPLETE prüfen.
- Spec-/Report-/Run-Digest, Node-/Dependency-/Attempt-/Inputindex-
  Bindung und Producer-before-Consumer-Zeitordnung prüfen; spec_digest muss
  exakt sha256(parsed.raw_bytes) entsprechen und darf nur interne Konsistenz
  behaupten.
- Fehlende/zusätzliche/vertauschte Inputs, falsche Manifest-ID,
  falsche Größe/Digest, fremde Attempts, Symlink/Hardlink, Mutation,
  JSON-Trunkierung und gemischtes V1.3/V1.5-Layout testen.
- Beide exakten Layouts und die historische Retry-Lineage prüfen; ein
  gestarteter fehlgeschlagener Attempt darf nicht durch seinen Retry ersetzt
  werden. Manifest-IDs müssen ausschließlich über die gemeinsame
  v5_manifest_id_v1-Hilfe berechnet werden.
- Output darf nur das feste metadata-only Flow-V2-JSON oder den generischen
  PROTOCOL_ERROR enthalten; keine Pfade oder Payloadmarker.
- Bestehenden V1-Payload-Inspector und Stream-Inspector als Regression
  ausführen.

### Task 4 — Dokumentation, CLI-Artefakte und Evidenz

**Dateien:**

- Ändern: docs/cli/commands.md, docs/cli/formats.md,
  docs/architecture/03-blueprint-consolidation.md,
  docs/implementation/README.md, tests/integration/test_cli_docs.py
- Generierte CLI-Dateien nur über docs/cli/verify_examples.py in einer
  vollständigen temporären nativen Kopie prüfen/aktualisieren.
- Erstellen: runs/bruce-payload-flow-v2-<date>.md

- Flow-V2 als IMPLEMENTIERT UND BELEGT erst nach Code-/Testabnahme
  dokumentieren; vorher als VORSCHLAG/IMPLEMENTIERUNGSPLAN markieren.
- Exakte CLI-Syntax, Sidecar-Schlüsselfolge, Fehlercodes, Opt-in- und
  V1.4-Ablehnung sowie stdout-/stderr-Verhalten bei Sidecarfehlern dokumentieren.
- Fokusbuild/-tests, bestehende Payload-/Inspector-/V2/V3/V4-Ziele,
  Python-Workflows, lokale Linkprüfung und CLI-Generatorprüfung ausführen.
- Abschlussbericht mit Exit-Codes, Pass-/Skip-Zahlen und verbleibenden
  Sandbox-/Cockpit-Limitierungen schreiben. git diff --check und Status nur
  soweit das Repository-Git tatsächlich belastbare Daten liefert.

## Reproduzierbarer Prüfpfad

Der ursprüngliche Prüfpfad bleibt zur Reproduktion erhalten; die tatsächlich
ausgeführten Prüfungen und Ergebnisse stehen im abschließenden Laufbericht.
Nach einer Implementierungsänderung:

~~~sh
cmake --build /tmp/bruce-v14-core-build -j2
ctest --test-dir /tmp/bruce-v14-core-build -R 'payload_flow_v2|dag_scheduler_v5_payload|payload_run_inspector_v1|artifact_payload_stream_v1' --output-on-failure
python3 scripts/check_local_doc_links.py
~~~

Für die finale Abnahme zusätzlich den im AGENTS.md vorgeschriebenen nativen
Host-Dependencies-Pfad verwenden, sofern verfügbar, und die vollständige
CTest-/CLI-Dokumentationsprüfung ausführen. Ein vorhandener Cockpit-Socket-
Fehler ist separat als Umgebungsproblem zu dokumentieren.
