# Common Runtime Host V2 / Stage 3D Payload Host Design

**Status: IMPLEMENTIERT UND BELEGT** — Lokale Payload-V13/V14-Hostmigration gemäß diesem Vertrag: nativer Build Exit 0, fokussierte Regression 37/37 CTests und fünf Stage-3D-Wiederholungen mit jeweils 16/16 Testmethoden. Task-1/2/3-Reviews sind PASS; die unabhängige Abschlussprüfung wird im Stage-3D-Abschlussbericht separat protokolliert. Die Ausgangslage und Zukunftsformulierungen unten beschreiben den vor Implementierung genehmigten Entwurf; aktueller Code und Tests sind der Implementierungsbeleg.

## Auftrag und Ausgangslage

Die bestätigte Eingrenzung lautet: Die bestehende lokale V5-`payload`-Route soll denselben Batch-Host-Lifecycle wie die gewöhnliche V5-Ausführung einschließlich expliziter Retries verwenden. Die öffentliche CLI, Artifact Store V1 und die bereits vorhandene Payload-Datenflusslogik bleiben die fachliche Grundlage.

**IMPLEMENTIERT, ABER IN DIESER SPEZIFIKATIONSARBEIT NICHT AKTUELL VERIFIZIERT:** `apps/dag_scheduler_v5_cli.hpp` bereitet Payload-Eingaben aus dem lokalen Store vor, prüft Provenienz, materialisiert Dateien und veröffentlicht angenommene Ausgaben als Objekt und Manifest. Der bisherige Payload-Ausführungspfad verwaltet `WorkerAttemptHandle`, Polling, Cancel und Lifecycle-Log selbst. Stage 3C besitzt einen gemeinsamen Batch Host für die gewöhnliche V5-Ausführung, aber der Dispatch schließt `payload_route` ausdrücklich aus.

Diese Migration schafft keinen erstmaligen Payload-Datenfluss. Sie übernimmt die bereits existierende Funktion in den gemeinsamen Prozess-Lifecycle.

## Geltungsbereich

Der geplante Host-Pfad gilt für:

```text
bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE
    [--external-inputs INPUTS] [--retry N] [--workers N]
    [bestehende Budgetoptionen]
```

- Lokale Worker V1 und lokaler Artifact Store V1.
- Ein oder mehrere DAG-Knoten; `--workers 1..4`.
- Explizites `--retry 0..3`; Standard bleibt 0.
- Bestehende externe Eingangsreferenzen über `--external-inputs` zum selben lokalen Store. Der Name bezeichnet einen Eingangs-Sidecar, keinen externen Speicherdienst.
- Unveränderte Payload-V13-/External-Inputs-V14-Bundleformate, soweit die vorhandene Route sie auswählt.

`--stream-v15`, `--flow-v2` und Handoff verbleiben auf ihren bestehenden Pfaden. Das Routing entscheidet vor Ausführung anhand der vorhandenen Flags. Kein stiller Rückfall zum alten Prozess-Lifecycle innerhalb eines migrierten Laufs.

Externe Payloadspeicherdienste, Netzwerk-/GPU-Worker, freie Code-Plugins, freie Sprache, allgemeines Beweisen und übergreifende Runtime-Orchestrierung sind **DOKUMENTIERT / KONZEPT**, außerhalb dieser Stage. :codex-annotation{index="1"}

## Ziele und überprüfbare Grenzen

1. Jeder gestartete Payload-Worker wird ausschließlich durch `CommonRuntimeHostV2Batch` gestartet, überwacht, beendet und gereapt.
2. Scheduler und Host erzeugen kein doppeltes Worker-Lifecycle-Log.
3. Payload-Provenienz, Digestprüfung, Dateigrenzen, Budgetierung und kanonische Receipt-Reihenfolge bleiben erhalten.
4. Ein kanonisches Worker-READY ist ein Annahmekandidat. Erst erfolgreiche Payload-Veröffentlichung und die Host-Annahme machen es zum akzeptierten DAG-Ergebnis.
5. Vorbereitung, Worker-Lauf und die vorhandenen Prüfungen vor Veröffentlichung teilen weiterhin eine monotone Attempt-Deadline. Ein frisches Worker-Zeitbudget darf diese Grenze nicht verlängern.
6. Jeder Erfolg, Vorbereitungsfehler, endgültige Workerfehler, Retry-Abbruch und Cancel erzeugt einen konsistenten Bundle-/Inspector-/Run-Log-Abschluss.

## Architektur und Verantwortlichkeiten

```text
payload CLI / Spec / optionaler External-Inputs-Sidecar
    |
    v
V5 Scheduler
    | Ready-Menge, DAG-Abhängigkeiten, Startbudget, Receipt-Reihenfolge
    | Provenienz / Eingabeauswahl / Materialisierung / Payload-Deadline
    | Retry-Policy / Store-Veröffentlichung / öffentlicher Bericht
    v
CommonRuntimeHostV2Batch
    | Start / Poll / Prozessgruppen / Cancel / Reap / JSONL-Lifecycle
    | Ergebnisprojektion / Annahme / Ablehnung / Retry-Entscheid
    v
Worker V1 -> privates output/result.bin
    |
    v
Scheduler -> ArtifactStoreV1.put_file / publish_manifest
    |
    v
Host-Annahme -> Nachfolger freigeben -> Bundle und Inspector
```

| Komponente | Verantwortung | Kennt ausdrücklich nicht |
|---|---|---|
| V5 Scheduler | DAG, Spezifikation, Payload-Referenzen, Receipt-Budget, Retryklassifikation, Bericht | rohe Supervisor-Handles im migrierten Pfad |
| Batch Host | Worker-Lifecycle, Attempt-Entscheidungen, monotone JSONL-Sequenz, Deadline-Abbruch, Cleanup | Store-Manifeststruktur und DAG-Abhängigkeiten |
| Artifact Store / Payload V13 | Provenienzprüfung, Objektprüfung, begrenzte Materialisierung, unveränderliche Veröffentlichung | Worker-Parallelität und Retry-Policy |
| Worker V1 / Supervisor | bestehender Request-/Result-Vertrag, Prozessgrenzen und private Ausgabe | öffentliches Bundle und Store-Publikation |
| Inspector | streng gültige Ereignisfolgen und Vergleich mit Berichtshistorie | Payload-Rohdaten im öffentlichen Bericht |

Die Scheduler-Zustände bleiben `V5NodeState`; insbesondere `accepted_manifest_id`, `payload_inputs`, Attempt-Historie und externe Eingangsstatistik. Der Batch Host bekommt keine Store-Zugangsdaten und keine Berechtigung, selbständig Nachfolger zu starten.

## Datenfluss

1. Die CLI liest Spec, Store und gegebenenfalls den bereits unterstützten External-Inputs-Sidecar. Vorhandene Validierung und Fehlernamen bleiben bestehen.
2. Der Scheduler erzeugt die privaten Request-Dateien und baut Ready-Menge sowie kanonische Receipt-Reihenfolge auf.
3. Vor einem startberechtigten Node legt er Attempt-ID und `steady_clock`-Deadline fest: Beginn der Payload-Vorbereitung plus `max_wall_ms`.
4. `prepare_payload_inputs` wählt Vorgänger anhand des akzeptierten Manifestes und der Provenienz `(run_id, node_id, attempt_id)` aus. Externe Referenzen behalten ihre deklarierte Quellprovenienz.
5. `select_artifact_input_v13` und `materialize_artifact_inputs_v13` prüfen Referenz, Manifest, Objektgröße, Digest, Objektanzahl, Gesamtinput und Deadline. Die Request-Bytes zählen weiterhin zum Eingabebudget.
6. Nur erfolgreiche Vorbereitung erlaubt einen Start. Der Scheduler prüft anschließend erneut Cancel und Deadline. Kein synthetischer Workerstart bei Fehlern vor Dispatch.
7. `make_request(..., payload_route=true)` transportiert Requestdatei und private materialisierte Eingangsreferenzen. Der Host erhält Request und die absolute Attempt-Deadline; Worker-Frames ändern sich nicht.
8. Der Batch Host startet und pollt Worker und liefert Ergebnisse mit Attempt-ID. Der Scheduler puffert Ergebnisse bis zur kanonischen Receipt-Position.
9. Vor der Annahme prüft der Scheduler Cancel, Deadline, kanonisches READY und verbleibendes Gesamt-Receipt-Budget. Danach führt er die bestehende `publish_payload_output`-Logik aus.
10. Erst nach erfolgreichem Objekt- und Manifest-Publish sowie erneuter Cancel-/Deadline-Prüfung bestätigt er die Host-Annahme. Erst anschließend setzt er `accepted=true`, erhöht akzeptierte Receipt-Bytes, übernimmt Manifest-/öffentliche Referenz und gibt Nachfolger frei.
11. Bei Fehlern entscheidet er den Attempt explizit als abgelehnt oder als historischen Retry. Nicht akzeptierte Ergebnisse liefern keine öffentlichen Payload-Metadaten.
12. Nach allen terminalen Nodes beziehungsweise Cancel beendet der Host alle Prozesse und schreibt genau einmal `RUN_FINISHED`; der Scheduler veröffentlicht den bisherigen Bericht atomar.

## Erforderliche interne Host-Verträge

Die folgenden Namen sind konkrete Vorschläge für die Implementierungsplanung. Die beschriebenen Invarianten sind verpflichtend; interne Typnamen dürfen ohne Änderung der öffentlichen CLI angepasst werden.

### Absolute Deadline pro Attempt

Eine abwärtskompatible Überladung von `start_batch_attempt_v2` erhält eine optionale absolute `steady_clock::time_point`-Deadline. Bestehende Aufrufer ohne Deadline behalten Stage-3C-Verhalten. Der Host speichert die Deadline, überprüft sie beim Start und beim Polling und beendet einen abgelaufenen aktiven Attempt über den bestehenden Supervisor.

- Ein noch nicht gestarteter, bereits abgelaufener Payload-Versuch bleibt ein Scheduler-Vorbereitungsfehler, ohne `ATTEMPT_STARTED`.
- Ablauf nach Start projiziert `FAILED/TIMEOUT` unter derselben Attempt-ID. Er löst keinen globalen Batch-Cancel aus.
- Ein bereits projiziertes, noch unentschiedenes READY kann während des Wartens auf seine Receipt-Position ablaufen. Dafür benötigt der Host eine explizite Post-Result-Ablehnung mit `TIMEOUT`.
- User-Cancel hat bei bereits beobachtetem Marker Vorrang und erzeugt `CANCELLED`, keinen Retry.
- Das vorhandene Worker-Budget bleibt unverändert. Die zusätzliche Deadline begrenzt den um Vorbereitung verkürzten Restzeitraum; sie wird nicht durch einen neuen Workerstart zurückgesetzt.

### Payload-Ablehnung eines READY-Kandidaten

`record_attempt_rejection_v2` wird abwärtskompatibel um einen eng validierten Ablehnungsgrund ergänzt. Der existierende Aufruf ohne Grund bedeutet weiterhin `OUTPUT_LIMIT_EXCEEDED`.

Erlaubte zusätzliche Gründe für diese Stage:

| Grund | Effektiver Worker-Fehler | Retry |
|---|---|---:|
| `TIMEOUT` | `TIMEOUT` | nur mit explizitem Restbudget |
| `ARTIFACT_REFERENCE_INVALID` | `FAILED` | nein |
| `ARTIFACT_MANIFEST_INVALID` | `FAILED` | nein |
| `ARTIFACT_DIGEST_MISMATCH` | `FAILED` | nein |
| `ARTIFACT_STORE_VIOLATION` | `FAILED` | nein |

Es darf ausschließlich ein vom Host abgeschlossener, kanonischer, noch unentschiedener READY-Kandidat abgelehnt werden. Unbekannte Attempt-ID, wiederholte Entscheidung, akzeptierter oder gecancelter Attempt und unbekannter Grund schlagen kontrolliert fehl. Die effektive Host-Projektion wird FAILED, enthält keine Artefaktreferenz, keinen Digest und keine Payloadgröße; gemessene Worker-Usage bleibt soweit vorhanden als Usage erhalten.

Ein `TIMEOUT`-abgelehnter Kandidat darf anschließend ausdrücklich über `record_attempt_retry_v2` historisch abgeschlossen werden. Eine andere Ablehnung erlaubt keinen Retry. Hierfür muss die Host-Entscheidungslogik zwischen endgültiger Ablehnung und Retry-fähiger Timeout-Projektion unterscheiden; eine zuvor endgültig entschiedene Ablehnung darf nicht beliebig wieder geöffnet werden.

### Schedulerfehler vor einem Workerstart

Der Host muss einen finalen Schedulerfehler ohne Worker-Attempt aufnehmen können. Empfohlene minimale Form: eine abwärtskompatible Finalisierungsoption `scheduler_failed` an `finish_batch_v2`, standardmäßig false.

Sie lässt `RUN_FINISHED` bei Vorbereitungs- oder Startbudgetfehlern auf FAILED enden, auch wenn kein Worker lief oder alle tatsächlich gestarteten Worker erfolgreich waren. Sie erfindet keine Attempt-Historie und keine Prozesse. Global beobachtetes Cancel hat weiterhin Vorrang vor gewöhnlichem Schedulerfehler; ein ungültiger Cancel-Marker bleibt Protokollfehler gemäß vorhandenem Host-Vertrag.

### Annahme und Cancel-Linearisation

Die Host-Annahme muss den owner-only Cancel-Marker unmittelbar vor dem Annahmeentscheid prüfen. Ein dabei beobachtetes Cancel entscheidet den Kandidaten als CANCELLED und verbietet `ARTIFACT_ACCEPTED`. Ein nach dem Annahmeentscheid eintreffender Marker nimmt bereits akzeptierte Vorgängerartefakte nicht rückwirkend zurück.

Der Scheduler übernimmt Manifest-ID, akzeptierte Referenz und Nachfolgerfreigabe erst nach erfolgreicher Host-Annahme. Ein Store-Publish allein ist kein Annahmeentscheid.

## Retry-Verhalten

- `--retry N` erlaubt bis zu `N+1` tatsächlich gestartete Attempts pro Node.
- Retrybare Workerfehler bleiben ausschließlich `PROCESS_CRASHED`, `TIMEOUT`, `PROTOCOL_ERROR`.
- Ein nach Dispatch durch Payload-Deadline erzeugtes TIMEOUT unterliegt derselben expliziten Retry-Policy.
- Vorbereitungsfehler starten keinen Worker und werden wie bisher endgültig behandelt, auch wenn ihr öffentlicher `result_code` TIMEOUT oder EXTERNAL_INPUT_TIMEOUT lautet. Sie dürfen keinen ungezählten Retry-Loop erzeugen.
- Store-, Provenienz-, Digest-, Eingabe-, Receipt-, Operations- und Speicherlimitfehler sind nicht retrybar.
- Jeder Retry erhält neue Attempt-ID und neue private Materialisierungsdateien. Die Vorbereitung und Provenienzprüfung werden erneut ausgeführt; ein früheres Staging wird nicht blind übernommen.
- Historische Fehlversuche bleiben nicht akzeptiert; ein späterer erfolgreicher Attempt erzeugt genau eine Artefaktannahme je Node und kann final READY ergeben.
- `max-started-nodes` zählt Nodes, keine zusätzlichen Retry-Attempts. `--workers` zählt gleichzeitig lebende Worker, keine Vorbereitungs- oder Receipt-Wartezustände.
- Die vorhandenen Attempt-ID- und Berichtskonventionen bleiben erhalten. Keine neue öffentliche Retryoption.

## Zustände, Fehler und Cancel

```text
READY_FOR_PREPARATION
    | Vorbereitung erfolgreich + kein Cancel + Deadline offen
    v
WORKER_ACTIVE -> RESULT_PENDING -> STORE_PUBLISH -> HOST_ACCEPTED
    |                  |               |
    +-- Fehler --------+---------------+--> final FAILED -> Nachfolger BLOCKED
    |
    +-- retrybarer Fehler + Restbudget -> historischer Attempt -> neue Vorbereitung

Jeder noch nicht akzeptierte Zustand -- beobachtetes Cancel --> CANCELLED
```

Vorbereitung vor Dispatch gehört zum Scheduler und erscheint nicht als Worker-Attempt. Nach Dispatch gehört jeder Prozess zum Host. Kein Schedulerpfad darf im migrierten Modus `start_attempt`, `poll_attempt`, `request_cancel(handle)` oder `finish_attempt` direkt aufrufen.

Cancel wird vor Vorbereitung, nach Vorbereitung, vor Dispatch, vor Publikation und vor Annahme geprüft. Die hostinterne Annahmeprüfung schließt die letzte Beobachtungslücke. Bei Cancel werden keine weiteren Retries oder Nachfolger gestartet. Der Host beendet aktive Worker, entscheidet bereits projizierte unangenommene Ergebnisse und reapert alle Worker/Watchdogs. Der Lauf endet einmal mit CANCELLED.

Bereits erfolgreich veröffentlichte Storeobjekte oder Manifeste können nach einer fehlgeschlagenen Annahme beziehungsweise Cancel verbleiben. Artifact Store V1 besitzt hier keine transaktionale Rücknahme. Diese immutable Storeeinträge werden weder als akzeptiertes Ergebnis in diesem Lauf ausgegeben noch als Vorgänger freigegeben. Stage 3D führt keine automatische Löschung oder Garbage Collection ein.

Die vorhandene Veröffentlichung besitzt keinen durchgängig deadlinefähigen Store-API-Aufruf. Der Scheduler prüft die Deadline vor und nach Veröffentlichung; ein nach Ablauf zurückkehrender Publish darf nicht zur Annahme führen. Eine harte Unterbrechung jedes synchronen Store-Dateisystemaufrufs ist kein zugesicherter Bestandteil dieses Vertrags.

## Ereignisse und Inspector

Der Batch Host bleibt alleiniger Autor von Worker-Lifecycle-Ereignissen. Die bestehende Sequenz bleibt erhalten:

```text
RUN_CREATED
ATTEMPT_STARTED / STATUS
PROCESS_EXITED / RESULT_RECEIVED
[RESULT_REJECTED mit validiertem Grund | ARTIFACT_ACCEPTED]
[CANCEL_REQUESTED]
RUN_FINISHED
```

Die Darstellung ist keine starre Ereignisreihenfolge: Cancel kann vor oder nach Ergebnisprojektion auftreten. Es gelten die bestehenden strengen Zustandsprüfungen.

`RESULT_REJECTED` erweitert nur die private Ereignisvalidierung um die oben explizit aufgelisteten Gründe. Ein solches Ereignis ist metadatenarm und enthält keine Artefakt-/Usage-Felder. Der Inspector setzt den effektiven Attempt auf FAILED und entfernt die vorherigen Artefaktmetadaten, genau wie bei der bestehenden Receipt-Budget-Ablehnung. Unbekannte Gründe und doppelte Post-Result-Entscheidungen bleiben ungültig.

Bundle-Inspector, Worker-Run-Inspector und Retry-Historienabgleich müssen denselben effektiven Status sehen. Für einen reinen Vorbereitungsfehler sind `RUN_CREATED` und `RUN_FINISHED:FAILED` ohne erfundene Attempt-Ereignisse gültig. Payload-Rohbytes, private Stagingpfade und Storepfade gelangen nicht in öffentliche Berichte.

## Kompatibilität

- Bestehende öffentliche Befehle, Optionsnamen, Standardwerte und Maximalwerte bleiben erhalten.
- Standardbudgets bleiben `30000 ms`, `65536` Eingabebytes, `65536` Ausgabebytes, `1000000` Operationen und `268435456` Speicherbytes; die bestehenden CLI-Maxima bleiben maßgeblich.
- Worker-V1-Frames, Artifact-Store-V1-Objekte/-Manifeste, Payload-V13-Auswahl und External-Inputs-V14-Verträge bleiben unverändert.
- Bundleverträge, Metadatenhygiene und kanonische Receipt-Reihenfolge bleiben erhalten. Lauf-ID, Zeitstempel und private JSONL-Ereignisse sind keine Zusage byteidentischer Laufartefakte.
- Host-API-Erweiterungen besitzen unveränderte Standardpfade für Stage 3A/3B/3C.
- Stream-/Flow-/Handoff-Flags wählen weiterhin die bisherigen Implementierungen. Ihre Regressionstests sichern die Abgrenzung.

## Abnahmetests

| Bereich | Verpflichtender Nachweis |
|---|---|
| Basis | Einzelknoten-Payload und mehrstufige DAG liefern dieselben Bytes, Manifestprovenienz und Reportverträge über den Batch Host |
| Parallelität | Unabhängige Nodes respektieren workers=1 und workers>1; Receipt-Reihenfolge bleibt deterministisch |
| Lokale externe Inputs | Bestehender V14-Sidecar funktioniert; falsche Quellprovenienz, fehlende/korrupte Objekte und überschrittene Inputs werden abgelehnt |
| Vorbereitungsfehler | Kein Workerstart, keine erfundene Historie, Nachfolger BLOCKED, RUN_FINISHED FAILED, Inspect erfolgreich |
| Annahme | Genau eine Host-Annahme erst nach erfolgreichem Publish; public/private Referenzen konsistent |
| Publishfehler | Digest-/Storefehler nach Worker-READY entfernen Payloadmetadaten und verhindern Nachfolger/Retry; Inspect akzeptiert begründete Ablehnung |
| Deadline | Verzögerte Materialisierung, aktiver Worker und auf Receipt wartendes READY überschreiten das gemeinsame Budget nicht durch Annahme; post-Publish-Ablauf wird abgelehnt |
| Retry | Erfolg nach TIMEOUT/Crash/Protokollfehler, Erschöpfung, neue Stagingdateien, eindeutige IDs, vollständige Historie und korrektes Nodes-Startbudget |
| Nicht retrybar | Store-/Provenienz-/Digest-/Limitfehler werden auch mit retry=3 nicht erneut gestartet |
| Cancel | Vor Dispatch, während Worker, zwischen Retries, bei wartendem READY und während Publication: kein nach beobachtetem Cancel akzeptiertes Ergebnis, kein weiterer Start |
| Cleanup | Erfolg, Fehler, Retry-Erschöpfung und Cancel hinterlassen keine lebenden Worker oder Watchdogs |
| Inspector | Gültige Ablehnungsgründe akzeptieren; unbekannte Gründe, falsche Reihenfolge, doppelte Entscheidung und Annahme nach Cancel ablehnen |
| Regression | Stage 3A/B/C, allgemeine V5-CLI/Abort/Inspector, vorhandene Payload-/External-/Stream-/Flow-/Handoff-Suites bleiben erfolgreich |

Tests dürfen vorhandene Fake-Worker beziehungsweise testseitige Dateisystem-/Zeitverzögerung verwenden. Keine Fault-Schalter im Produktionspfad einführen. Prozess-/Cancel-Races werden durch explizite Synchronisation reproduzierbar geprüft; unbegründete Dauerschleifen oder ausschließliche Sleep-Annahmen sind kein Ersatz.

## Entscheidungen für die Planung

**VORSCHLAG:** Ein gemeinsamer migrierter Batch-Schedulerpfad für normale Payload-V13 und lokale External-Inputs-V14, einschließlich Einzelknoten, verhindert zwei neue Host-Lifecycles. Gemeinsame Hilfsfunktionen dürfen aus vorhandener V5-Logik extrahiert werden, soweit sie diese Migration tragen.

**VORSCHLAG:** Absolute Deadline als optionales Startargument, begründete Post-READY-Ablehnung und Scheduler-Failure-Flag bilden die minimalen Host-Erweiterungen. Sie ersetzen keine DAG- oder Storelogik.

**OFFENE ENTSCHEIDUNG:** Die Implementierungsplanung legt die exakten C++-Optionstypen/Überladungen fest. Dabei müssen Deadline-, Timeout-Retry-, Cancel- und Post-Result-Entscheidungen eindeutig sein und bestehende Aufrufer unverändert kompilieren.

**OFFENE ENTSCHEIDUNG:** Die Planung benennt die konkreten synchronisierten Test-Harnesses für die Publish-/Cancel- und Receipt-/Deadline-Races. Die oben beschriebenen Ergebnisse sind unabhängig vom Testmechanismus verbindlich.

Es bestehen keine offenen Produktentscheidungen zu CLI-Defaults, Retry-Maximum, Workerzahl oder Storetechnologie in dieser Stage; die vorhandenen Verträge sind bindend.

## Quellbelege und Ausgangsdateien

- [V5 CLI und Scheduler](../../../apps/dag_scheduler_v5_cli.hpp): `execute`, `execute_multi_node_v2`, `prepare_payload_inputs`, `publish_payload_output`, `make_request`, `payload_failure_name`, `retryable`, `V5NodeState`.
- [Batch-Host-Schnittstelle](../../../include/bruce/application/common_runtime_host_v2_batch.hpp) und [Implementierung](../../../src/application/common_runtime_host_v2_batch.cpp): Start/Poll/Annahme/Ablehnung/Retry/Cancel/Finalisierung.
- [Payload-Auswahl und Materialisierung](../../../include/bruce/artifact/artifact_payload_v1.hpp): vorhandene Payload-Verträge.
- [Artifact Store V1](../../../include/bruce/artifact/artifact_store_v1.hpp): Objekt-/Manifest-Publikation.
- [Worker-Run-Inspector](../../../src/worker/worker_run_inspector_v1.cpp): streng validiertes `RESULT_REJECTED`, derzeit ausschließlich OUTPUT_LIMIT_EXCEEDED.
- [Batch-Host-Tests](../../../tests/unit/common_runtime_host_v2_batch_tests.cpp), [Worker-Inspector-Tests](../../../tests/unit/worker_run_inspector_v1_tests.cpp).
- [Vorhandene Payload-Integration](../../../tests/integration/test_dag_scheduler_v5_payload.py), [Stage-3C-Integration](../../../tests/integration/test_common_runtime_host_v2_stage3c.py).
- [Stage-3C-Vertrag](2026-10-16-common-runtime-host-v2-stage3c-retry-design.md).

Nach Freigabe dieser schriftlichen Spezifikation folgt ein eigener Implementierungsplan mit Aufgaben, Reviewgrenzen und Prüfkommandos. Der Status wird erst nach Implementierung und aktuellen Nachweisen geändert.
