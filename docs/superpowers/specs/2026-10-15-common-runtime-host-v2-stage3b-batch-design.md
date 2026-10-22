# Common Runtime Host V2 / Stage 3B Batch Design

**Status:** IMPLEMENTIERT UND BELEGT — der Vertrag ist für den engen
gewöhnlichen V5-Mehrknotenpfad umgesetzt und durch fokussierte Unit-, CTest-
und Integrationstests belegt. Die historische Designbeschreibung und die
ausdrücklich ausgeschlossenen Bereiche bleiben erhalten.

**Vorbedingung:** Stage 3A ist für den V5-Einzelknotenpfad **IMPLEMENTIERT UND BELEGT**. Dieser Vertrag erweitert den Host nur für gewöhnliche lokale V5-`execute`-Runs mit mehreren Nodes.

## Ziel

Stage 3B ergänzt eine gemeinsame Host-Schicht für mehrere lokale Worker-V1-Versuche innerhalb eines V5-Laufs. Bruce bleibt der einzige Host: Er erzeugt den Run-Kontext, startet und überwacht Worker, führt das gemeinsame JSONL-Protokoll, steuert Abbruch und übernimmt Ergebnisse erst nach den bestehenden Digest-, Größen- und Referenzprüfungen.

Der vorhandene V5-DAG-Scheduler bleibt die Quelle für Spec-Parsing, topologische Reihenfolge, Dependency-Gating, Receipt-Reihenfolge und `BLOCKED`-Fortpflanzung. Stage 3B ersetzt diese Geschäftslogik nicht, sondern bindet ihre Worker-Grenze an einen gemeinsamen Batch-Host.

## Geltungsbereich

Stage 3B gilt ausschließlich, wenn alle folgenden Bedingungen erfüllt sind:

- öffentliche Route: `bruce dag-scheduler-v5 execute SPEC --output BUNDLE`
- mindestens zwei Nodes oder ausdrücklich `--workers N` im Mehrknotenpfad
- `--workers` zwischen 1 und 4
- `--retry 0` beziehungsweise der bestehende Retry-Default
- keine Payload-Route, kein `--flow-v2`, kein `--stream-v15`, kein `--handoff-v1`
- lokaler, hostkontrollierter Worker-V1-Prozess

Der bestehende V5-Pfad bleibt für Retry-, Payload-, Stream-, Flow- und Handoff-Läufe unverändert, bis dafür eigene Verträge vorliegen.

Nicht Teil von Stage 3B sind automatische Retries, Worker-Pools, neue Frame-Formate, externe Payloadspeicher, Netzwerk- oder GPU-Ausführung, dynamische Plugins, freie Sprache, allgemeines Beweisen und übergreifende Runtime-Orchestrierung. :codex-annotation{index="1"}

## Architektur

```text
V5 Spec-Datei
    |
    v
V5 DAG-Parser und Validierung
    |
    v
CommonRuntimeHostV2Batch
    |  gemeinsamer Run-Kontext
    |  gemeinsamer JSONL-Sequenzzähler
    |  zentrale Cancel-Steuerung
    |  Worker-Handle je aktivem Attempt
    v
Worker V1 Supervisor-Prozesse
    |
    v
kanonische Receipt-Reihenfolge und V5-Bericht
```

Ein Batch besitzt genau einen Run-Logger und genau einen terminalen `RUN_FINISHED`-Eintrag. Jeder Attempt erhält weiterhin die vorhandenen Worker-V1-Frames, eine eigene Attempt-ID und die Ereignisse `ATTEMPT_STARTED`, `STATUS`, `PROCESS_EXITED`, `RESULT_RECEIVED` sowie bei erfolgreicher Validierung `ARTIFACT_ACCEPTED`.

Mehrere einzelne `CommonRuntimeHostV2Run`-Objekte dürfen nicht denselben Run-Root verwalten. Der Batch übernimmt deshalb Sequenzvergabe, Lifecycle-Abschluss und die Zuordnung der Worker-Ergebnisse zentral.

## Vorgeschlagener Vertrag

Die konkrete Signatur wird im Implementierungsplan festgelegt, muss aber semantisch folgende Fähigkeiten bereitstellen:

- `start_batch(request, context)` validiert den gesamten Host-Kontext vor dem ersten Spawn.
- `start_attempt(batch, worker_request)` startet einen einzelnen Worker-Handle unter dem gemeinsamen Batch.
- `poll_batch(batch)` liefert fertig gewordene Attempts, ohne Receipt-Reihenfolge oder Artifact-Annahme vorwegzunehmen.
- `request_cancel_batch(batch)` ist idempotent und schreibt vor jeder weiteren Ergebnisübernahme genau einmal `CANCEL_REQUESTED` je laufendem Attempt.
- `finish_batch(batch)` beendet alle noch aktiven Worker, reapt sie und schreibt genau einmal `RUN_FINISHED`.

Der öffentliche V5-Bericht bleibt unverändert. Der Batch gibt nur validierte `WorkerResult`-Metadaten an den bestehenden V5-Renderer zurück; Child-PIDs, absolute Pfade, stderr und Payload-Rohbytes bleiben intern.

## Host-Verantwortung und Budgets

- Die fünf bestehenden Worker-Budgets bleiben unverändert: Wall-Zeit, Input-Bytes, Output-Bytes, Operationen und Prozessspeicher.
- `--workers` begrenzt die gleichzeitig aktiven Worker, nicht die Gesamtzahl gestarteter Nodes.
- `max-started-nodes` bleibt ein separates Gesamtbudget.
- `RLIMIT_AS` und Peak-Telemetrie bleiben pro beobachtetem Worker-PID; es wird kein aggregiertes Prozessgruppen-Speicherlimit behauptet.
- Die Grace-Periode bleibt hostseitig fest `1000 ms`.
- Ein Batch darf keinen Worker starten, nachdem der Cancel-Marker als `REQUESTED` beobachtet wurde.

## DAG- und Ergebnissemantik

1. Bruce validiert die vollständige DAG vor dem ersten Worker-Spawn.
2. Nur Nodes ohne offene Abhängigkeiten kommen in die Ready-Menge.
3. Der Batch startet höchstens `--workers` aktive Attempts.
4. Ein Nachfolger wird erst nach akzeptiertem Vorgänger-Artifact freigeschaltet.
5. Receipt-Übernahme erfolgt in der bestehenden kanonischen Spec-Reihenfolge, unabhängig von der Fertigstellungsreihenfolge.
6. Ein nicht akzeptiertes Ergebnis markiert den Node als fehlgeschlagen und propagiert `BLOCKED` transitiv an abhängige Nodes.
7. Bei Cancel werden alle aktiven Worker angefordert, wartende Nodes nicht gestartet und abhängige Nodes nicht freigeschaltet.
8. Ein Worker-`READY`, das nach dem Batch-Cancel noch eintrifft, wird als `CANCELLED` behandelt und darf kein `ARTIFACT_ACCEPTED` erzeugen.
9. Ohne Retry bleibt jeder Node bei höchstens einem Attempt.

## Lifecycle-Log

Der gemeinsame Run-Log muss eine monotone Sequenz ohne Lücken innerhalb dieses Hosts bewahren. Die minimale Reihenfolge ist:

```text
RUN_CREATED
ATTEMPT_STARTED (je Attempt)
STATUS (je Attempt)
...
CANCEL_REQUESTED (je laufendem Attempt, optional)
PROCESS_EXITED (je Attempt)
RESULT_RECEIVED (je Attempt)
ARTIFACT_ACCEPTED (nur validiertes READY)
RUN_FINISHED
```

`RUN_FINISHED` darf erst geschrieben werden, wenn alle Worker-Prozesse und ihre Watchdogs beendet und reaped sind. Nach einem Cancel darf kein neues `ATTEMPT_STARTED` folgen.

## Fehler- und Abbruchsemantik

- Ungültiger Host-Kontext oder Budget-Mismatch schlägt vor dem ersten Spawn mit `INVALID_REQUEST` fehl.
- Timeout, Prozessabsturz, Protokollfehler, Output-Limit und Cancel behalten die bestehenden `FailureCode`- und V5-Berichtswerte.
- Ein Fehler eines Nodes blockiert seine Nachfolger, beendet aber unabhängige Ready-Nodes nur dann, wenn der bestehende V5-Lauf dies bereits vorsieht.
- Ein ungültiger Abort-Marker beendet den Batch kontrolliert mit `PROTOCOL_ERROR` und ohne Artefaktannahme.
- Cancel ist idempotent; wiederholte Marker oder CLI-Anfragen dürfen keine doppelten Cancel-Ereignisse erzeugen.

## Abnahmekriterien

Die Implementierung ist erst als Stage 3B belegt, wenn mindestens folgende Fälle reproduzierbar bestehen:

1. Zwei unabhängige Root-Nodes laufen mit `--workers 2` über den gemeinsamen Host.
2. Ein Join-Node startet erst nach der erfolgreichen Annahme beider Vorgänger.
3. `--workers 1` bleibt deterministisch und erzeugt dieselbe kanonische Receipt-Reihenfolge wie zuvor.
4. Nie mehr als `--workers` Worker sind gleichzeitig aktiv.
5. `max-started-nodes` wird nicht mit dem Worker-Limit verwechselt.
6. Externer Cancel beendet alle aktiven Worker und startet keine wartenden Nodes.
7. Ein kontrolliert verspätetes `READY` nach `CANCEL_REQUESTED` erzeugt weder Artefaktannahme noch erfolgreiche V5-Ausgabe.
8. Fehler und transitive `BLOCKED`-Zustände bleiben im bestehenden Reportformat erhalten.
9. Kein Worker oder Watchdog bleibt nach erfolgreichem Abschluss, Fehler oder Cancel zurück.
10. Stage-3A-Einzelknoten sowie bestehende V2-, V3-, V4-, Payload-, Flow- und Handoff-Tests bleiben grün, weil diese Pfade unverändert bleiben.

## Belege und Ausgangslage

Die bestehende Mehrknoten-DAG ist in `apps/dag_scheduler_v5_cli.hpp` bereits implementiert: Ready-Menge, Successor-Gating, `--workers`-Begrenzung, Receipt-Reihenfolge, Blockierung und Cancel. Die Stage-3A-Fassade ist in `include/bruce/application/common_runtime_host_v2.hpp` und `src/application/common_runtime_host_v2.cpp` auf einen einzelnen Worker ausgelegt. Der neue Batch-Vertrag schließt genau diese Lücke.

## Offene Entscheidungen für die Implementierungsplanung

- exakte Batch-API und Ownership der internen Worker-Handles
- ob der bestehende V5-Run-Logger direkt erweitert oder durch einen kleinen Host-Logger-Adapter gekapselt wird
- genaue Poll-Rückgabeform für mehrere fertig gewordene Attempts
- testbare Erkennung der Worker-/Watchdog-Bereinigung ohne öffentliche PID-Ausgabe
