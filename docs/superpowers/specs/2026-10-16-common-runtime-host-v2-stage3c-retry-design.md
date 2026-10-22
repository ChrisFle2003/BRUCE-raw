# Common Runtime Host V2 / Stage 3C Retry Design

**Status:** IMPLEMENTIERT UND BELEGT — gewöhnliche lokale V5-Retries im unten begrenzten Host-Pfad; siehe Abschlussnachweis.

**Vorbedingung:** Stage 3A und Stage 3B sind für die eng definierten gewöhnlichen lokalen V5-Pfade **IMPLEMENTIERT UND BELEGT**. Stage 3C erweitert nur die Retry-Fähigkeit dieses lokalen Host-Modells.

## Ziel

Stage 3C bindet explizite V5-Retries an den gemeinsamen `CommonRuntimeHostV2Batch`. Bruce bleibt für jeden Attempt der kontrollierende Host: Er startet den Worker, führt die Lifecycle-Ereignisse, prüft das Ergebnis, entscheidet über Annahme oder Retry und verhindert weitere Starts nach Cancel.

Die Retry-Policy bleibt im V5-Scheduler. Der Batch-Host kennt keine DAG-Abhängigkeiten und entscheidet nicht selbst, ob ein Fehler wiederholbar ist. Er erhält nur die explizite Information, dass ein fehlgeschlagener Attempt für den finalen Batchstatus als Retry abgeschlossen wurde.

## Geltungsbereich

Stage 3C gilt ausschließlich für:

- `bruce dag-scheduler-v5 execute SPEC --output BUNDLE`
- lokalen Worker-V1-Betrieb
- `--retry N` mit `0 <= N <= 3`
- Einzelknotenläufe mit `--workers 1` und Mehrknotenläufe mit `--workers 1..4`
- keine Payload-, Stream-, Flow- oder Handoff-Erweiterung

Die bestehende CLI bleibt unverändert. `--retry 0` behält exakt das Stage-3A-/Stage-3B-Verhalten. Payload-, Stream-, Flow- und Handoff-Routen behalten ihre bisherigen Retry-Implementierungen, bis sie eigene Host-Verträge erhalten.

Nicht Teil von Stage 3C sind automatische implizite Retries, neue Frame-Formate, externe Payloadspeicher, Worker-Pools, Netzwerk- oder GPU-Ausführung, dynamische Plugins, freie Sprache, allgemeines Beweisen und breite Runtime-Orchestrierung. :codex-annotation{index="1"}

## Bestehende Retry-Policy

Retrybar bleiben ausschließlich:

- `PROCESS_CRASHED`
- `TIMEOUT`
- `PROTOCOL_ERROR`

Nicht retrybar bleiben:

- `CANCELLED`
- `FAILED`
- `BLOCKED`
- `OUTPUT_LIMIT_EXCEEDED`
- `INPUT_LIMIT_EXCEEDED`
- `OPERATION_BUDGET_EXCEEDED`
- `MEMORY_LIMIT_EXCEEDED`
- `DIGEST_MISMATCH`
- `PROCESS_START_FAILED`

`--retry N` bezeichnet zusätzliche Versuche. Ein Node darf höchstens `N + 1` Attempts erhalten. `--workers` begrenzt gleichzeitig aktive Attempts; `max-started-nodes` zählt weiterhin nur den ersten Start eines Nodes und nicht dessen Retries.

## Architektur

```text
V5 CLI --retry N
    |
    v
V5 Scheduler: retryable(result.failure_code)
    |
    +--> record_attempt_retry_v2(batch, attempt_id)
    |       -> alter Attempt historisch abgeschlossen
    |       -> neue Attempt-ID und neuer Worker
    |
    +--> record_attempt_rejection_v2 / final failure
            -> Node terminal
            -> Nachfolger BLOCKED oder Lauf FAILED

CommonRuntimeHostV2Batch
    |
    +--> Worker V1 Attempt 1
    +--> Worker V1 Attempt 2
    +--> Worker V1 Attempt N
    |
    v
gemeinsamer JSONL-Run-Log und V5-Attempt-Historie
```

Der V5-Scheduler behält Ready-Menge, Dependency-Gating, Receipt-Reihenfolge, Retryklassifikation und öffentliche Berichtserzeugung. Der Batch-Host verwaltet mehrere Attempts desselben Nodes, deren Prozessgruppen, JSONL-Ereignisse und finale Laufbereinigung.

## Vorgeschlagener Batch-Vertrag

Stage 3C ergänzt den Batch-Host intern um:

```cpp
void record_attempt_retry_v2(
    CommonRuntimeHostV2Batch&, std::string_view attempt_id);
```

Die Operation ist nur für einen bereits abgeschlossenen, nicht akzeptierten Attempt zulässig. Sie:

- markiert den Attempt als historisch entschieden,
- verhindert `ARTIFACT_ACCEPTED` für diesen Attempt,
- lässt `RUN_FINISHED` durch spätere erfolgreiche Attempts auf `READY` enden,
- erzeugt keine neue Attempt-ID selbst.

Der V5-Scheduler erzeugt die nächste Attempt-ID, baut den neuen `WorkerRequest` und ruft erneut `start_batch_attempt_v2` auf.

`record_attempt_retry_v2` ist idempotent für denselben Attempt oder schlägt kontrolliert mit `PROTOCOL_ERROR` fehl, wenn der Attempt bereits angenommen, endgültig abgelehnt oder gecancelled wurde.

## Attempt- und Berichtshistorie

Attempt-IDs bleiben unverändert zur bestehenden V5-Konvention:

- Einzelknoten: `a-1`, `a-1-2`, `a-1-3`, …
- Mehrknoten: `a-<node-ordinal>-<attempt-number>`

Die vollständige Historie bleibt im bestehenden `attempts[]`-Bericht erhalten. Für jeden früheren Retry-Attempt gilt:

- `accepted=false`
- kein öffentlicher Artifact-Reference-Wert
- kein veröffentlichter Digest oder Payload-Rohbytes
- eigener Lifecycle im gemeinsamen JSONL-Log

Nur der tatsächlich angenommene READY-Attempt darf `ARTIFACT_ACCEPTED`, Referenz, Digest und Größe liefern. Ein später erfolgreicher Retry erzeugt genau eine Artefaktannahme und einen finalen `RUN_FINISHED:READY`.

## Cancel-Semantik

Cancel wird in dieser Reihenfolge verarbeitet:

1. Bruce beobachtet den owner-only Cancel-Marker.
2. Der Batch setzt seinen Cancel-Zustand.
3. Für jeden aktiven oder bereits fertig projizierten, aber noch nicht entschiedenen Attempt wird `CANCEL_REQUESTED` genau einmal geschrieben.
4. Aktive Worker werden über den bestehenden Supervisor beendet.
5. Fertige READY-Ergebnisse werden als CANCELLED entschieden.
6. Die Ready-Menge wird geleert; kein neuer Retry startet.
7. Kein Attempt erzeugt nach Cancel `ARTIFACT_ACCEPTED`.
8. Der Lauf endet genau einmal mit `RUN_FINISHED:CANCELLED`.

Cancel zwischen zwei Retries darf weder den nächsten Attempt starten noch die Historie als erfolgreichen Retry darstellen. Cancel während eines aktiven Retry-Attempts muss dessen Attempt-ID erhalten und darf keinen Folgeversuch erzeugen.

## Fehlersemantik

- Retrybare Fehler werden nur bei verbleibendem `--retry` als Retry entschieden.
- Ein retrybarer Fehler nach ausgeschöpftem Budget wird zum finalen Node-Fehler und blockiert Nachfolger.
- Nicht retrybare Fehler werden sofort final entschieden.
- Ein später erfolgreicher Retry setzt den Node auf READY; frühere Fehlversuche bleiben historisch, bestimmen aber nicht den finalen Batchstatus.
- Ein erfolgreicher Retry darf den finalen Batchstatus nicht auf FAILED belassen.
- Ein Fehler eines unabhängigen Nodes darf die Retry-Serie eines anderen Nodes nicht verändern; abhängige Nodes warten weiterhin auf einen akzeptierten Vorgänger.

## JSONL-Lifecycle

Für jeden Attempt gelten die folgenden Ereignisse (keine verpflichtende lineare Reihenfolge):

```text
ATTEMPT_STARTED
STATUS
PROCESS_EXITED
RESULT_RECEIVED
ARTIFACT_ACCEPTED       # nur beim tatsächlich akzeptierten READY-Attempt
CANCEL_REQUESTED        # nur bei beobachtetem Cancel
RESULT_REJECTED         # nur Receipt-Budget-Ablehnung eines kanonischen READY
```

Ein Retry-Entscheid beendet den fehlgeschlagenen Attempt für den Batch, ohne `ARTIFACT_ACCEPTED` zu schreiben. Die Sequenz bleibt global monoton. `RUN_FINISHED` erscheint genau einmal nach dem letzten finalen Node- oder Cancel-Zustand und nachdem alle Worker/Watchdogs gereapt sind.

## Abnahmekriterien

1. `retry=0` startet pro Node höchstens einen Attempt und bleibt zu Stage 3B byte-/reportkompatibel.
2. `retry=2` erlaubt höchstens drei Attempts pro Node und erzeugt eindeutige IDs.
3. `TIMEOUT`, `PROCESS_CRASHED` und `PROTOCOL_ERROR` wiederholen sich nur bei verbleibendem Retry-Budget.
4. `FAILED`, `BLOCKED`, `CANCELLED`, Digest- und Limitfehler erzeugen keinen Retry.
5. Ein später erfolgreicher Attempt erzeugt genau eine Artefaktannahme und einen finalen READY-Lauf.
6. Ein erschöpfter Retry endet mit dem letzten Fehler, vollständiger Historie und ohne Artefakt.
7. Ein Mehrknotenlauf bewahrt getrennte Node-Historien, Worker-Limits, Receipt-Reihenfolge und BLOCKED-Propagation.
8. Cancel zwischen Retries startet keinen weiteren Attempt.
9. Cancel während eines aktiven Retry-Attempts akzeptiert kein spätes READY.
10. Inspector und bestehende V5-CLI-Verträge akzeptieren die vollständige Retry-Historie.
11. Kein Worker oder Watchdog bleibt nach Erfolg, Fehler, Retry-Erschöpfung oder Cancel zurück.

## Belege und Ausgangslage

Die bestehende Retryklassifikation und Attempt-ID-Erzeugung stehen in `apps/dag_scheduler_v5_cli.hpp`. Stage 3B besitzt bereits `record_attempt_rejection_v2` für kanonische READY-Ergebnisse, aber noch keine explizite Entscheidung für einen historischen Retry-Attempt. Genau diese Lücke schließt Stage 3C, ohne die öffentliche V5-Syntax oder Worker-V1-Frames zu ändern.

## Implementierungsentscheidungen und Abnahme

**IMPLEMENTIERT UND BELEGT:** `record_attempt_retry_v2` ist eine eigene interne
Batch-API. Die Finalisierung wertet die letzte Entscheidung je Node aus;
historische Retry-Fehler erzwingen kein FAILED nach späterem Erfolg. Die
Annahmeexklusivität je Node wird durch das serielle Retry-Routing des
Schedulers gewährleistet, nicht als unabhängige Mehrfach-READY-Sperre im Host.

Der Inspector prüft alle Attemptfolgen und gleicht sie mit der vollständigen
Berichtshistorie ab. Eine einmalige `CANCEL_REQUESTED/CANCELLED`-Entscheidung
nach einem projizierten, noch unentschiedenen Ergebnis setzt dessen effektiven
Status auf CANCELLED und entfernt die Artefaktmetadaten. Bereits angenommene
oder abgelehnte Ergebnisse dürfen nicht erneut entschieden werden.

`RESULT_REJECTED/OUTPUT_LIMIT_EXCEEDED` ergänzt die private Ereignismenge für
die ausdrückliche Receipt-Budget-Ablehnung. Es ist ausschließlich nach einem
kanonischen READY mit Artefakt, vor Annahme/Cancel/Ablehnung und ohne
Artefakt-/Usage-Felder zulässig; der effektive Inspectorstatus wird FAILED.
Der ursprüngliche Entwurf ließ diese beiden Post-Result-Entscheidungen offen;
die ergänzten Inspector-Tests haben ihre Notwendigkeit belegt.

Belege: [Batch-Verträge](../../../tests/unit/common_runtime_host_v2_batch_tests.cpp),
[Inspector-Verträge](../../../tests/unit/worker_run_inspector_v1_tests.cpp),
[Stage-3C-Integration](../../../tests/integration/test_common_runtime_host_v2_stage3c.py)
und der lokale Abschlussnachweis
`runs/common-runtime-host-v2-stage3c/reports/final-review.md`.
