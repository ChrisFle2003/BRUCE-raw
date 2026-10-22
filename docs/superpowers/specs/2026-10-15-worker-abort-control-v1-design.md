# Worker Abort Control V1 — Design

**Status: VORSCHLAG — Architektur- und Implementierungsvertrag, vor der
Umsetzung zu prüfen.**

## Ziel

Bruce soll einen laufenden lokalen V5-Lauf über einen zweiten CLI-Aufruf
kontrolliert abbrechen können. Bruce bleibt dabei der einzige Prozess, der
Worker startet, Cancellation-Frames sendet, die feste Grace-Periode verwaltet
und die gesamte Worker-Prozessgruppe beendet. Ein Nutzer oder ein externer
Aufrufer erhält keinen Worker-PID-, Signal- oder Programmstarter-Vertrag.

Der Vertrag ergänzt die bereits vorhandene interne Funktion
`worker_v1::request_cancel`. Er führt eine owner-only Kontrollanforderung für
den konkreten V5-Run-Root ein und verbindet sie mit der vorhandenen
Scheduler-Schleife. Der Worker-Framevertrag V1, der WorkerRequest, der
WorkerResult und die Linux-Supervisor-Eskalation bleiben semantisch
unverändert.

## Repository-Evidenz

**IMPLEMENTIERT UND BELEGT:** Die lokale Worker-Grenze arbeitet mit einem
separaten Child-Prozess und einer Prozessgruppe. `request_cancel` sendet einen
`CANCEL`-Frame, deaktiviert den konkurrierenden Wall-Time-Watchdog, lässt die
feste V1-Grace ablaufen und beendet danach die Gruppe mit SIGTERM und nötigenfalls
SIGKILL. Der Child kann kooperativ ein `CANCELLED`-Resultat liefern.

Belege:

- `include/bruce/worker/worker_supervisor_v1.hpp` — öffentliche Parent-API;
- `src/worker/worker_supervisor_v1.cpp` — `request_cancel`, Grace- und
  Prozessgruppenlogik;
- `apps/dag_scheduler_v5_cli.hpp` — `worker-v1 child`, V5-Dispatch und
  bestehende Timeout-Cancellation;
- `tests/unit/worker_supervisor_v1_tests.cpp` — kooperative Cancellation,
  ignoriertes CANCEL, Grace, Reaping und Nachfahren;
- `docs/cli/formats.md` — bisher keine öffentliche Request-Cancel-Option.

**IMPLEMENTIERT UND BELEGT:** V5 schreibt den privaten Baum
`BUNDLE.run/inputs`, `BUNDLE.run/attempts` und
`BUNDLE.run/control/run-log.jsonl`. Attempt-IDs sind für den ersten Versuch
`a-1` (Single-Node) beziehungsweise `a-<node-ordinal>-1`; Retries erhöhen nur
die Versuchskomponente. Das JSONL erlaubt bereits das Event
`CANCEL_REQUESTED`, obwohl der aktuelle Scheduler dieses Event noch nicht
schreibt.

Belege:

- `apps/dag_scheduler_v5_cli.hpp` — `execute`, `attempt_id_for` und
  `WorkerRunLog`-Ereignisse;
- `include/bruce/worker/worker_run_log_v1.hpp` und
  `src/worker/worker_run_log_v1.cpp` — kanonischer append-only Vertrag;
- `include/bruce/worker/worker_run_inspector_v1.hpp` und
  `src/worker/worker_run_inspector_v1.cpp` — Prüfung des bestehenden
  `CANCEL_REQUESTED`-Übergangs.

**DOKUMENTIERT / KONZEPT:** Netzwerk-, GPU-, externe Worker-, dynamische
Plugin-, freie Sprach- und globale Runtime-Ausführung bleiben außerhalb dieses
Vertrags. Der Kontrollmarker ist kein dauerhafter Artifact Store und kein
Recovery-Mechanismus.

## Gewählte Integrationsform

Die V1-Oberfläche ist ein neuer, expliziter V5-CLI-Befehl:

```text
bruce dag-scheduler-v5 cancel BUNDLE
```

`BUNDLE` ist derselbe Zielpfad, der beim laufenden
`dag-scheduler-v5 execute` oder `dag-scheduler-v5 payload` als `--output`
angegeben wurde. Bruce leitet daraus ausschließlich den gebundenen privaten
Run-Root `BUNDLE.run/` ab. Der Befehl spricht nie direkt mit einer PID und
sendet selbst kein Signal.

Die Entscheidung ist bewusst ein neuer Hostpfad statt einer weiteren Option an
`execute`: Der laufende Prozess muss von außen ansprechbar sein, während der
öffentliche Report erst nach dem Lauf geschrieben wird. Eine Option wie
`execute --cancel` würde nur einen bereits bekannten lokalen Trigger innerhalb
des ursprünglichen Prozesses beschreiben und keinen zweiten CLI-Aufruf
ermöglichen.

## Kontrollmarker-Vertrag

Der Hostpfad verwendet die Datei:

```text
BUNDLE.run/control/cancel.request
```

Ihr kanonischer Inhalt ist exakt:

```text
BRUCE_WORKER_CANCEL_V1\n
```

Der Cancel-Befehl öffnet die Datei descriptor-relativ mit `O_CREAT | O_EXCL |
O_NOFOLLOW | O_CLOEXEC`, Modus `0600`, schreibt exakt den Marker, ruft `fsync`
auf und schließt die Datei. Eine bereits vorhandene gültige Datei ist
idempotent und erzeugt keine zweite Anforderung. Der Befehl akzeptiert keinen
freien Grund, keine PID, kein Signal, keinen Timeout und keinen Pfad zum
Workerprogramm.

Der Scheduler prüft den Marker descriptor-relativ in seiner bestehenden
Hauptschleife. Er akzeptiert nur eine reguläre, owner-only Datei mit einem
Hardlink, dem exakten V1-Inhalt und einer Größe innerhalb des festen
Markerlimits. Symlinks, Gruppen-/Fremdzugriff, zusätzliche Bytes, fehlende
Elternverzeichnisse oder andere Inhalte werden als Kontrollprotokollfehler
behandelt. Der Marker wird nicht gelöscht; er bleibt Teil der privaten
Diagnosespur.

Der Cancel-Befehl prüft die komplette private Verzeichniskette ohne
Symlink-Following und verlangt owner-only Verzeichnisse. Er folgt weder einem
Symlink im Bundlepfad noch in `BUNDLE.run/control`. Die Ausführung besitzt
weiterhin Vorrang bei konkurrierendem Abschluss: Wird der Lauf vor der
Beobachtung des Markers fertig, entsteht kein nachträglicher Worker-Abbruch.

## CLI-Ausgabe und Exitcodes

Die Syntax ist exakt `cancel BUNDLE`; Optionen und zusätzliche Argumente sind
ungültig. Der Befehl schreibt keine lokale Pfadangabe, Run-ID, PID oder
Fehlerdetails in die öffentliche Ausgabe.

| Situation | stdout | stderr | Exit |
| --- | --- | --- | ---: |
| neue Anforderung | `{"contract":"dag-scheduler-v5-cancel-v1","action":"cancel","status":"REQUESTED"}` plus LF | leer | 0 |
| bereits angefordert | gleicher Vertrag mit `status:"ALREADY_REQUESTED"` | leer | 0 |
| Lauf ist bereits fertig | gleicher Vertrag mit `status:"ALREADY_FINISHED"` | leer | 0 |
| Run-Root fehlt, ist unsicher oder Log/Marker ist ungültig | leer | `{"error":"CANCEL_TARGET_UNAVAILABLE"}` plus LF | 1 |
| falsche Syntax | leer | bestehende kompakte `PROTOCOL_ERROR`-Ausgabe | 2 |

Die drei erfolgreichen Statuswerte sind ein metadata-only Ergebnis. Der
Status `REQUESTED` bestätigt nur die durable Marker-Erstellung; er behauptet
nicht, dass jeder Worker bereits beendet ist. Die spätere Ausführung und der
`inspect`-Aufruf liefern den tatsächlichen terminalen Zustand.
Für die Statusauswahl hat ein bereits vorhandenes abschließendes
`RUN_FINISHED`-Event Vorrang vor einem vorhandenen Marker; erst danach wird
zwischen neuer und bereits vorhandener Anforderung unterschieden.

## Hostseitiger Ablauf

```text
cancel BUNDLE
    |
    v
owner-only BUNDLE.run/control prüfen
    |
    v
cancel.request atomar anlegen
    |
    v
laufender V5-Host entdeckt Marker
    |
    +--> keine neuen Nodes starten
    +--> nicht gestartete Nodes als CANCELLED markieren
    +--> für jeden aktiven Attempt CANCEL_REQUESTED loggen
    +--> request_cancel(handle) für jeden aktiven Worker
    |
    v
kooperatives CANCEL, 1000 ms Grace
    |
    +--> Workerantwort: CANCELLED
    +--> sonst: SIGTERM, nach 50 ms SIGKILL, Gruppe bereinigen
    |
    v
RUN_FINISHED=CANCELLED, Report Exit 1
```

Die Markerprüfung erfolgt vor jedem Start weiterer Ready-Nodes und während der
Live-Attempt-Prüfung. Eine bereits laufende begrenzte Input-/Store-Operation
wird bis zu ihrem bestehenden lokalen Deadline-/Chunk-Punkt beendet; V1 führt
keinen unbounded synchronen Kopiervorgang ein.

Die Cancellation wird im Parent linearisiert:

- Wird der Marker vor der Receipt-Übernahme beobachtet, gewinnt die
  Host-Cancellation. Der Attempt wird als `CANCELLED` verbucht, ein READY-
  Ergebnis wird nicht akzeptiert und es gibt keinen Retry.
- Wurde ein Ergebnis bereits als Receipt übernommen, bleibt dieser Receipt
  erhalten. Noch nicht gestartete oder weiterhin laufende Nachfolger werden
  abgebrochen.

Bei mehreren `--workers` werden alle zu diesem Zeitpunkt aktiven Handles
innerhalb derselben Scheduler-Iteration angefordert. Bruce wartet nicht auf
einen einzelnen Worker, bevor er die übrigen aktiven Handles adressiert.

## Run-Log- und Report-Vertrag

Für jeden aktiven Attempt schreibt Bruce genau eine zusätzliche Zeile:

```json
{"schema":"worker-run-log-v1",...,"attempt_id":"a-1","event":"CANCEL_REQUESTED","status":"CANCELLED"}
```

Danach folgen wie bei anderen terminalen Attempts `PROCESS_EXITED` und
`RESULT_RECEIVED` mit `status:"CANCELLED"`; sie enthalten keine Digest-,
Größen- oder Referenzfelder. `ARTIFACT_ACCEPTED` wird für einen abgebrochenen
Attempt nie geschrieben. Nicht gestartete Nodes besitzen keinen Attempt und
keine Attempt-Historie; ihr öffentlicher Receipt ist `CANCELLED` mit
`attempt_id:null`, `reference:null`, `payload_digest:null` und
`payload_size:0`.

Das Run-Level-Event `RUN_FINISHED` erhält bei einer Host-Cancellation den
Status `CANCELLED`. Damit bleibt der bestehende Inspector-Vergleich zwischen
Run-Log und V5-Report gültig. Ein Single-Node-Report trägt bereits den
terminalen Workerstatus `CANCELLED`; ein Multi-Node-Report erhält denselben
Top-Level-Status und die einzelnen CANCELLED-Receipts. Erfolgs- und normale
Fehlerläufe behalten ihre bisherige Bytefolge.

Der Execute-/Payload-Aufruf schreibt den normalen V5-Report, beendet sich mit
Exit 1 und startet keine neuen Attempts mehr. `--retry` wird nach einer
Host-Cancellation vollständig ignoriert; ein CANCELLED-Resultat ist nicht
retryable. `inspect BUNDLE` bleibt read-only und darf den Marker nur als
privaten Kontrollbestandteil tolerieren, nicht verändern.

## Ressourcen- und Sicherheitsgrenzen

- Der feste `1000 ms`-Grace-Wert bleibt unveränderbar und wird nicht als neue
  CLI-Option exponiert.
- Die vorhandenen fünf Budgets bleiben unverändert: Wall-Time,
  Eingabebytes, Ausgabebytes, Operationen und Prozessspeicher.
- Cancellation beendet nur Worker, die zum konkret gebundenen Run-Root
  gehören; es gibt keine globale oder PID-basierte Abschaltung.
- Die Prozessgruppenlogik, Parent-Death-Signale, Reaping und Nachfahren-
  Bereinigung bleiben beim vorhandenen Supervisor.
- Kontrollmarker enthalten keine Payloadbytes, Befehlszeilen, Tokens,
  Benutzergründe oder Stderr-Inhalte.
- Ein fremder Benutzer kann bei korrekt gesetzten owner-only Rechten weder
  den Marker erstellen noch einen fremden Run-Root als Ziel verwenden.
- Eine manipulierte oder symlinkte Kontrollstruktur wird fail-closed als
  `CANCEL_TARGET_UNAVAILABLE` beziehungsweise `PROTOCOL_ERROR` behandelt.

## Fehler- und Zustandsmatrix

| Zustand bei Markerbeobachtung | Bruce-Aktion | Ergebnis |
| --- | --- | --- |
| mehrere aktive Worker | alle `request_cancel` aufrufen | aktive Attempts `CANCELLED` |
| nur Ready-Warteschlange | keine Starts mehr, alle offenen Nodes markieren | offene Nodes `CANCELLED` |
| Retry wäre möglich | Retry-State schließen | kein weiterer Versuch |
| Worker antwortet kooperativ | Resultat validieren, aber Cancellation linearisiert verbuchen | kein Artifact-Accept |
| Worker ignoriert CANCEL | feste Grace, SIGTERM, 50-ms Eskalationsfenster, SIGKILL | `CANCELLED`, Prozessgruppe beendet |
| Run beendet vor Markerbeobachtung | Marker nicht nachträglich anwenden | bestehendes Ergebnis bleibt gültig |
| ungültiger Marker | keinen Worker beeinflussen | kontrollierter Protokollfehler |
| Parent stirbt | bestehendes PDEATHSIG/Watchdog-Verhalten | kein verwaister Worker |

## Testbare Abnahme

1. `cancel BUNDLE` erstellt genau einmal einen owner-only Marker und ist bei
   Wiederholung idempotent.
2. Fehlende, symlinkte, fremde oder manipulierte Run-Roots werden ohne
   Marker-Erstellung kontrolliert abgewiesen.
3. Ein laufender Single-Node-V5-Run erkennt den Marker, schreibt
   `CANCEL_REQUESTED`, beendet den Worker und veröffentlicht einen
   `CANCELLED`-Report mit Exit 1.
4. Ein kooperativer Worker und ein Worker, der CANCEL ignoriert, liefern beide
   `CANCELLED`; der zweite wird nach der festen Grace vollständig gereaped.
5. Bei `--workers 2..4` werden alle aktiven Worker beendet, keine neuen Nodes
   gestartet und abhängige beziehungsweise unabhängige Warteschlangen-Nodes
   als `CANCELLED` berichtet.
6. Eine Host-Cancellation startet niemals einen Retry, auch wenn der
   ursprüngliche Fehler retryable gewesen wäre.
7. Ein bereits akzeptierter Receipt bleibt erhalten, wenn der Marker erst
   danach beobachtet wird; die verbleibenden offenen Nodes werden beendet.
8. Run-Log-Sequenzen bleiben monoton, Artefaktfelder bleiben bei CANCELLED
   leer, und der bestehende Worker-Inspector akzeptiert den vollständigen
   Verlauf.
9. `cancel` und Execute geben keine Payloadbytes, lokalen Pfade, PIDs,
   Kommandozeilen oder Stderr-Inhalte aus.
10. Erfolgreiche und normale V5-Fehlerläufe sowie V2/V3/V4-Ausgaben bleiben
    byteidentisch.
11. Der Volltest wird um die neue Unit-/CLI-/Workflow-Gruppe ergänzt; die
    bekannte Cockpit-Socket-Sandbox bleibt separat ausgewiesen.

## Nicht Bestandteil

Nicht Bestandteil dieses V1-Vertrags sind:

- Pause, Resume, Kill einzelner frei gewählter Attempts oder Signalwahl durch
  den Nutzer;
- Live-Statusstreaming, Remote-Control, Netzwerk- oder Multi-Host-Abbruch;
- Cancel-Gründe, Benutzeridentitäten, Authentifizierung zwischen Hosts oder
  ein unveränderlicher signierter Control-Log;
- Worker-Pool, GPU-/Netzwerkausführung, freie Code-Plugins, freie Sprache,
  allgemeines Beweisen oder eine globale Runtime-Orchestrierung;
- Artifact-Store-GC, Payload-Download, Run-Recovery oder Wiederaufnahme;
- Änderungen an V2, V3, V4 oder dem bestehenden Worker-Frameformat.

## Offene Entscheidungen für spätere Versionen

- **OFFENE ENTSCHEIDUNG:** ob ein späterer Control-Socket atomare Status-
  abfragen und mehrere getrennte Requests unterstützen soll;
- **OFFENE ENTSCHEIDUNG:** ob ein signierter/benutzergebundener Control-
  Auftrag für Mehrbenutzer-Hosts nötig wird;
- **OFFENE ENTSCHEIDUNG:** ob ein abgebrochener Run als eigener persistent
  wiederaufnehmbarer Zustand neben `CANCELLED` geführt wird.
