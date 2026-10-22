# Execute Handoff Lineage V1 — Design

**Status: ENTWURF ZUR FREIGABE — Architekturvertrag vor der
Implementierungsplanung.** Diese Spezifikation beschreibt eine additive,
read-only Prüfsicht für den bestehenden V5-`execute`-Pfad. Sie behauptet noch
keine Implementierung.

## Ziel

Der normale V5-`execute`-Pfad übergibt bereits verifizierte Producer-Dateien
an direkte Consumer-Worker. Die Payload-Route besitzt dafür mit Flow V2 schon
eine persistierte Lineage-Sicht. Diese Ergänzung schließt die entsprechende
Evidenzlücke für `execute`, ohne einen zweiten Payloadtransport einzuführen.

Erfolg bedeutet:

- jede tatsächlich gestartete Consumer-Attempt erhält eine unveränderliche
  Liste ihrer direkten Producer-Inputs;
- jede Bindung enthält Producer-Node, Producer-Attempt, Digest und Bytegröße;
- die Bindungen lassen sich gegen Spec, Report, Run-Log und private Dateien
  prüfen;
- Retries, Fan-out, Fan-in und Host-Cancellation bleiben nachvollziehbar;
- bestehende Execute-Reportbytes und Worker-Verträge bleiben unverändert.

## Repository-Evidenz

**IMPLEMENTIERT UND BELEGT:** Der bestehende V5-Execute-Pfad bildet direkte
Abhängigkeiten in `make_request()` als `ArtifactRef` ab. Der Supervisor prüft
die referenzierte Datei vor dem Workerstart auf Besitzer, Regulärität,
Hardlinkzahl, Größe und SHA-256.

Belege:

- `apps/dag_scheduler_v5_cli.hpp` — `make_request()` und V5-Scheduler;
- `src/worker/worker_supervisor_v1.cpp` — bounded Inputprüfung;
- `include/bruce/worker/worker_boundary_v1.hpp` — `ArtifactRef` und
  `WorkerRequest::input_artifact_refs`;
- `tests/integration/test_dag_scheduler_v5_payload.py` — tatsächlicher
  Producer-zu-Consumer-Bytefluss.

**IMPLEMENTIERT UND BELEGT:** Die Payload-Route persistiert bereits Flow V2
mit Producer-/Consumer-Attempt, Manifest, Objektindex, Digest und Größe und
prüft diese Daten über `payload-inspect --flow-v2`.

Belege:

- `include/bruce/artifact/payload_flow_v2.hpp`;
- `src/artifact/payload_flow_v2.cpp`;
- `include/bruce/artifact/payload_flow_inspector_v2.hpp`;
- `tests/integration/test_dag_scheduler_v5_payload_flow_inspector.py`.

Die ältere V1.3-Konzeptdatei wird für diese Einordnung überholt; Quellcode und
reproduzierbare Tests sind maßgeblich.

## Abgrenzung

Diese V1 ergänzt ausschließlich die normale Route:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE [bestehende Optionen]
```

Die Payload-Route bleibt unverändert und verwendet weiterhin `--flow-v2` für
ihre Store-/Manifest-Lineage. Es gibt keinen neuen Worker-Frame, kein neues
`WorkerRequest`-Feld, keinen direkten PID-/Signalzugriff, keinen externen
Store und keine Wiederaufnahme unvollständiger Läufe.

Ohne die neue Opt-in-Option bleiben Reportbytes, Run-Tree und Ausführung
bytegleich. V2, V3, V4, V5-Payload, Artifact Store und Worker Abort Control
werden nicht semantisch geändert.

## Öffentliche Oberfläche

Die additive Option lautet:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE --handoff-v1
```

`--handoff-v1` ist einmalig, wertlos und nur zusammen mit `execute` zulässig.
Sie wird vor Spec- oder Dateizugriff syntaktisch geprüft. `payload`,
`payload-inspect`, `inspect`, `publish`, `.run`-Ziele und unbekannte Optionen
weisen sie mit dem bestehenden kompakten `PROTOCOL_ERROR`-Vertrag ab.

Im Opt-in-Modus entsteht zusätzlich:

```text
BUNDLE.handoff-v1.json
```

Die bestehende Execute-Ausgabe bleibt unverändert. Der Sidecar wird erst nach
erfolgreicher Report-Erzeugung atomar und exklusiv veröffentlicht. Bei einem
fachlich fehlgeschlagenen, aber vollständig beschriebenen Lauf entsteht der
Sidecar ebenfalls; bei Cancellation werden nur bis zur Cancellation gestartete
Attempts beschrieben.

## Lineage-Modell

Der Sidecar verwendet eine kanonische JSON-Zeile mit abschließendem LF:

```text
contract="bruce-execute-handoff-v1"
run_id
spec_digest
report_digest
nodes[]
  node_id, callback_id, request_digest, dependencies[]
attempts[]
  consumer_node_id, consumer_attempt_id, inputs[]
    input_index
    producer_node_id
    producer_attempt_id
    reference
    sha256
    size_bytes
```

`input_index` folgt exakt der bestehenden Dependency-Reihenfolge. Der
Request-Input ist kein Producer-Input und wird nicht in `inputs[]` eingetragen.
Root-Nodes besitzen eine leere Inputliste. Jeder tatsächlich gestartete
Attempt erhält genau einen Eintrag, einschließlich fehlgeschlagener Retries.
Ein reservierter, vor dem Workerstart abgebrochener Preflight-Attempt wird
nicht eingetragen.

`reference` ist ausschließlich die bereits validierte relative V5-Referenz.
Absolute Pfade, `..`, PIDs, Kommandozeilen, Payloadbytes, stderr und Secrets
werden nicht gespeichert. `run_id` bindet alle Kanten an denselben lokalen
Run; Cross-Run-Inputs sind ungültig.

## Erzeugungsablauf

```text
Spec validieren
↓
Producer akzeptieren
↓
Consumer-Inputs in bestehender Dependency-Reihenfolge prüfen
↓
Lineage-Satz für die neue Attempt-ID einfrieren
↓
ATTEMPT_STARTED erfolgreich loggen
↓
Lineage-Satz dauerhaft in Hostspeicher übernehmen
↓
unveränderten Worker starten
↓
Ergebnis, Retry oder Cancellation wie bisher verarbeiten
↓
Legacy-Report schreiben
↓
Sidecar atomar adoptieren
```

Die Lineage wird nur für wirklich gestartete Attempts übernommen. Ein
akzeptierter Producer-Attempt bleibt die Quelle für alle direkten Consumer,
auch wenn der Consumer später mehrfach retried. Nach einer Host-Cancellation
werden keine neuen Lineage-Einträge angelegt.

## Read-only Prüfung

Die bestehende Route erhält eine additive Inspektionsform:

```text
bruce dag-scheduler-v5 inspect BUNDLE --handoff-v1
```

Der Inspector liest `BUNDLE`, `BUNDLE.run/` und den gebundenen Sidecar ohne
Schreibzugriff. Er prüft:

1. kanonische Sidecar-Bytes, Größenlimit und JSON-Tiefe;
2. `report_digest`, `spec_digest` und `run_id` gegen die gespeicherten Daten;
3. vollständige Node-/Dependency-Übereinstimmung;
4. genau einen Lineage-Eintrag je tatsächlich gestarteter Attempt;
5. lückenlose Input-Indizes und direkte Producer-Zuordnung;
6. Attempt-ID, Status, Digest, Größe und relative Referenz gegen Run-Log,
   Report und private Dateien;
7. Fan-out, Fan-in, Retry-Historien und CANCELLED-Attempts;
8. stabile Dateien durch Identitätsprüfung vor und nach dem Lesen.

Ein vollständiger konsistenter Lauf liefert einen metadata-only Bericht:

```json
{"contract":"dag-scheduler-v5-handoff-inspect-v1","action":"inspect","status":"VALID","run_status":"READY","run_id":"example","node_count":3,"attempt_count":4,"binding_count":3,"verified_input_bytes":12,"lineage_scope":"LOCAL_CONSISTENCY"}
```

Ein konsistenter, aber nicht abgeschlossener Lauf liefert `INCOMPLETE`/Exit 1.
Fehlende, manipulierte oder widersprüchliche Daten liefern ausschließlich den
bestehenden `PROTOCOL_ERROR`/Exit-2-Vertrag. Der Inspector startet keinen
Worker, führt keinen Adapter aus und repariert keine Dateien.

## Fehler- und Crash-Verhalten

Der Sidecar ist kein Dateisystem-Transaktionsjournal. Ein Crash zwischen
Legacy-Report und Sidecar-Adoption kann einen gültigen Report ohne Sidecar
hinterlassen. `inspect --handoff-v1` meldet diesen Zustand kontrolliert als
`INCOMPLETE` oder `PROTOCOL_ERROR`, abhängig davon, ob der Lauf selbst
vollständig ist. Es gibt keine automatische Reparatur und keine implizite
Rekonstruktion aus freien Pfaden.

Bei Encode-, Größen-, Identitäts- oder Adoptionsfehlern bleibt stdout leer,
stderr enthält `{"error":"HANDOFF_RECORD_INVALID"}` und der Prozess endet
mit Exit 2. Bereits erzeugte Legacy-Dateien bleiben zur Diagnose bestehen.

## Grenzen und Sicherheitsregeln

- private Sidecar-Datei: regulär, owner-only `0600`, genau ein Hardlink;
- private Verzeichnisse: owner-only `0700`;
- Dateiöffnung: descriptor-relativ, `O_NOFOLLOW|O_CLOEXEC`;
- bestehende V5-Grenzen für Nodes, Attempts, Inputs, Bytes und Zeit gelten;
- keine ungebundenen Allokationen aus Sidecar- oder Logfeldern;
- keine Payloadbytes im Sidecar oder öffentlichen Bericht;
- keine Authentizitäts- oder Benutzeridentitätsaussage;
- keine Behauptung eines mathematischen Beweises oder einer allgemeinen
  Ausführungsgarantie.

## Abnahmekriterien

Die Implementierungsplanung muss mindestens abdecken:

1. kanonisches Sidecar für Root-Node, lineare Kette, Fan-out und Fan-in;
2. Retry mit eigenem Attempt-Lineage-Eintrag;
3. Cancellation ohne nachträglichen Lineage-Eintrag;
4. Mutation, Truncation, Symlink, Hardlink, Digest- und Größenfehler;
5. atomare Sidecar-Adoption und Fehlerausgabe;
6. read-only Inspector mit READY, FAILED, BLOCKED, CANCELLED und INCOMPLETE;
7. Bytegleichheit aller bestehenden Execute-Reports ohne `--handoff-v1`;
8. Regressionen für Worker-Frame, V2, V3, V4, Payload Flow und Artifact Store.

## Nicht Bestandteil

Freie Sprache, allgemeines Beweisen, GPU-/Netzwerkausführung, freie
Code-Plugins, externe Payloadspeicher, Recovery/Resume und große
Cross-Runtime-Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**.
