# Logic-Mod Artifact DAG V6 – Host-supervised Execution and Telemetry

**Status:** **IMPLEMENTIERT UND BELEGT.** V6 ist als Linux-lokaler,
hostüberwachter `run`-Pfad implementiert. Die Nachweise stehen im
[V6-Core](../../../src/mod/logic_mod_artifact_dag_v6.cpp), im
[Attempt-Handle](../../../src/mod/logic_mod_worker_attempt_v6.cpp), im
[Sidecar](../../../src/mod/logic_mod_supervision_log_v6.cpp) sowie in den
[V6-Core- (CMake-Target)](../../../tests/unit/logic_mod_artifact_dag_v5_tests.cpp),
[Attempt-](../../../tests/unit/logic_mod_worker_attempt_v6_tests.cpp),
[Sidecar-](../../../tests/unit/logic_mod_supervision_log_v6_tests.cpp) und
[CLI-Tests](../../../tests/integration/test_logic_mod_artifact_dag_v6_cli.py).
Die noch nicht implementierten Nichtziele bleiben ausdrücklich Nichtziele.

## Ausgangslage und Ziel

**IMPLEMENTIERT UND BELEGT:** Logic-Mod Artifact DAG V2 führt bis zu 16
signierte Knoten aus; V3 kann unabhängige Wellen lokal parallelisieren; V4
setzt Grenzen für Starts, Wellen-Payload, Gesamtzeit und Parallelität; V5 kann
fehlende, bereits erwartete Vorgängerartefakte hostseitig aus einem V4-Remote
Store wiederherstellen. Der signierte Logic-Mod-Worker läuft als lokaler,
versiegelter Prozess. Seine Ausführung ist an frische Autorisierung gebunden;
sein optionaler Payload ist ein hosterzeugtes, nur lesbares FD 6. Seccomp,
feste Dateideskriptoren und die bisherige Resultatannahme bleiben maßgeblich.

Belege der vorhandenen Basis:

- [DAG V4 – lokale Budgets](2026-10-17-logic-mod-artifact-dag-v4-budget-design.md),
  [DAG V5 – Remote Restore](2026-10-17-logic-mod-artifact-dag-v5-remote-restore-design.md);
- [Logic-Mod-Worker-API](../../../include/bruce/mod/logic_mod_worker_v1.hpp)
  und [Ausführungsgrenze](../../../src/mod/logic_mod_worker_v1.cpp);
- [Worker-Supervisor](../../../include/bruce/worker/worker_supervisor_v1.hpp)
  und [Linux-Umsetzung](../../../src/worker/worker_supervisor_v1.cpp).

V6 legt für jeden tatsächlich gestarteten DAG-Knoten eine **hostseitige
Attempt-Lebensdauer** und eine begrenzte, owner-only Diagnosespur festlegen.
Der Host besitzt das Attempt-Handle, überwacht den Prozess, erzwingt vorhandene
per-node Grenzen, reapt immer seine Kinder und ordnet jeden terminalen Verlauf
eindeutig ein. Telemetrie beschreibt Beobachtungen; sie wird nie zur
unverifizierten Workerbehauptung und verändert keine fachliche DAG-Wahrheit.

## Geltungsbereich

V6 umfasst ausschließlich:

1. einen hostinternen Attempt-Handle pro gestarteten Worker mit expliziten
   Zuständen und genau einmaliger Reap-Verantwortung;
2. Prozessgruppen, Parent-Death-Verhalten, Watchdog-Polling und begrenzte
   Eskalation, die an die konkrete Attempt-Prozessgruppe gebunden sind;
3. harte **pro Knoten** geltende, bereits definierte Workergrenzen für Wall
   Time, Adressraum, Input- und Outputbytes; die V4-DAG-Grenzen bleiben
   zusätzliche Laufgrenzen und werden nicht umdefiniert;
4. eine klar markierte Peak-Telemetrie mit Verfügbarkeits- und Genauigkeits-
   semantik;
5. ein vom Host allein geschriebenes, owner-only JSONL-Sidecar pro V6-Lauf;
6. eine terminale Grund-/Folgen-Taxonomie für Erfolg, fachliches Ergebnis,
   Cancel, Budgetüberschreitung, Start- und Prozessfehler;
7. gezielte Unit- und Prozessintegrationstests unter Linux.

V6 ergänzt ausschließlich den V6-`run`-Pfad. Die V2-, V3-, V4- und
V5-Record- und Replay-Bytes, ihre CLI-Befehle und ihre Replay-Semantik bleiben
unverändert kompatibel. Insbesondere startet Replay keinen Worker, liest keine
Telemetrie als Beweis und führt keine Remote-Restore- oder Sidecar-Reparatur
aus.

## Nichtziele und feste Grenzen

V6 behauptet ausdrücklich **nicht**:

- keine cgroup-basierte oder hostweite/mehrknotenweite Speicher-, CPU- oder
  Prozessaggregation;
- kein durchsetzbares allgemeines Operationsbudget; ein solcher Wert benötigt
  einen separaten, vertrauenswürdigen Zählervertrag;
- keine GPU-, Netzwerk-, Remote-Worker- oder Containerorchestrierung;
- keine freie ausführbare Datei, kein Script, keine dynamischen Code-Plugins
  und keinen Workerzugriff auf Storepfade, URLs oder Zugangsdaten;
- keine Änderung an Signaturprüfung, Registry-/Truststore-Autorisierung,
  Worker-Frameformat, FD-6-Fähigkeit oder Seccomp-Rechten;
- keine exakte, plattformunabhängige Peak-RAM-Messung und keine Abrechnung;
- keine Wiederaufnahme, Retry-Policy, globale Scheduler-Runtime,
  Log-Streaming-Schnittstelle, Log-Rotation oder Log-Upload.

Ein V6-Worker erhält weiterhin nur die bestehende, vor Spawn vollständig
geprüfte Ausführungsautorisierung und gegebenenfalls das versiegelte,
nur-lesbare Payload-FD 6. Der Worker kann keine Telemetrie konfigurieren,
Grenzen erhöhen, andere PID auswählen oder Signale an fremde Prozesse senden.

## Attempt-Modell und Zustände

Der Host erzeugt eine nicht kopierbare interne `AttemptHandleV6`-Instanz nur
nach vollständig erfolgreichem DAG-Preflight und unmittelbar um den vorhandenen
Worker-Spawn. Sie speichert mindestens die Host-PID, die erwartete
Prozessgruppen-ID, monotone Startzeit, effektive per-node Budgets, einen
terminalen Grund und den Reap-Zustand. PID und Gruppen-ID sind ausschließlich
intern und erscheinen nie im öffentlichen JSON, Record oder Replay.

```text
PREPARED -> SPAWNED -> RUNNING -> CANCEL_REQUESTED -> TERM_SENT -> KILL_SENT
                         |                 |                |          |
                         +-----------------+----------------+----------+
                                           REAPED -> CLASSIFIED
```

- `PREPARED` ist noch kein gestarteter Worker und erzeugt keine
  Attempt-Telemetrie außer einem optionalen Preflight-Fehler.
- `SPAWNED` wird erst nach erfolgreichem Parent/Child-Handshake angenommen;
  ein fehlgeschlagener Fork/Setup/Exec ist `PROCESS_START_FAILED` und hat
  keine laufende Gruppe.
- Nur der Host überführt einen Handle in einen terminalen Zustand. Jede
  terminale Gruppe wird genau einmal mittels `wait4` oder gleichwertig gereapt.
- Destruktion eines noch laufenden Handles ist ein Sicherheitscleanup: sie
  signalisiert ausschließlich dessen erwartete Gruppe und reapet sie, bevor
  Ressourcen freigegeben werden.

V6 schützt gegen PID-Wiederverwendung: Signale werden nur an die
beim Spawn reservierte Prozessgruppe gesendet. Kann der Host die Gruppenbindung
nicht feststellen, wird kein fremdes Ziel geraten; der Attempt scheitert
kontrolliert als `SUPERVISION_UNAVAILABLE` und der Child wird über den noch
verfügbaren direkten Handle bereinigt.

## Linux-Supervisionsvertrag

Vor dem Exec setzt der direkte Child eine eigene Prozessgruppe und
`PR_SET_PDEATHSIG` auf `SIGKILL`; nach dem Setzen wird die Parent-Beziehung
erneut geprüft, damit ein bereits gestorbener Host keinen verwaisten direkten
Worker hinterlässt. Der Host
schließt unnötige FDs, setzt bestehende Linux-Ressourcenlimits vor dem Exec und
beobachtet den konkreten Attempt mit monotone Zeit und nichtblockierendem
`waitpid`/`wait4`.

Die Prozessgruppe ist die Grenze für kooperative und harte Beendigung. Ein
Worker darf keine Kinder nutzen; die Gruppenbeendigung dient dennoch der
fail-closed Bereinigung, falls ein fehlerhafter Child dennoch Nachfahren
erzeugt. Fehler beim Setzen von Gruppe, PDEATHSIG, Limit, Watchdog oder Reap
führen zum kontrollierten Nichtannahme-/Cleanup-Pfad; es existiert kein
unsupervisierter Fallback.

### Harte pro-node Budgets

| Budget | Durchsetzung durch den Host | Terminaler Grund |
| --- | --- | --- |
| Wall-Time | monotone Deadline je Attempt; danach Terminierung der Gruppe | `WALL_TIME_LIMIT` |
| Adressraum | Linux `RLIMIT_AS` vor Exec; Exit/Signal wird mit Hostbeobachtung klassifiziert | `MEMORY_LIMIT` oder `PROCESS_CRASHED` bei nicht beweisbarer Ursache |
| Inputbytes | bestehende Hostprüfung vor FD-6-/Request-Freigabe | `INPUT_LIMIT` |
| Outputbytes | bestehende begrenzte Resultat-/Artefaktannahme | `OUTPUT_LIMIT` |

Die Limits sind nicht additiv über parallele Knoten und erlauben keine Aussage
über gesamte Container-/Hostressourcen. V4-`max-total-wall-ms`,
`max-total-payload-bytes`, `max-started-workers` und `max-parallel-nodes`
behalten ihre V4-Semantik; V6 führt daraus keine rückwirkende Killpflicht ab.

## Cancel und Grace-Policy

**IMPLEMENTIERT UND BELEGT:** Nach einer hostseitig beobachteten
Cancel-Anforderung wartet V6 höchstens `1000 ms`, sendet dann `SIGTERM` an die
noch gebundene Attempt-Prozessgruppe und nach höchstens weiteren `50 ms`
`SIGKILL`. Diese Grace-Perioden sind V6-Konstanten: weder Worker, Spec noch
CLI können sie ändern. Der Cancel ist hostseitig und nicht kooperativ; es gibt
keinen Worker-Cancel-Frame und keinen öffentlichen PID-basierten Cancel-Endpunkt.

Cancel gewinnt vor einer noch nicht übernommenen READY-Antwort. Bereits vom
Host übernommene Resultate bleiben gültig; nicht gestartete Nachfolger werden
nicht begonnen. Ein Cancel löst keine Wiederholung aus und erzeugt keinen
erfolgreichen DAG-Record. V6 verwendet keinen PID-basierten öffentlichen
Cancel-Endpunkt.

## Telemetrie und owner-only Sidecar

**IMPLEMENTIERT UND BELEGT:** `run` verlangt `--report REPORT`. Der Host
erzeugt genau `REPORT.run/control/supervision-v6.jsonl`; das Verzeichnis ist
`0700`, die Datei `0600`. Das Sidecar ist eine separate, diagnostische
Hostspur und keine zweite DAG-Wahrheit.

Nur der Host schreibt. Datei und Elternverzeichnisse sind descriptor-relativ,
ohne Symlink-Following, regulär, einem effektiven Benutzer gehörend und
owner-only; der Worker erhält weder Pfad noch FD. Jeder Eintrag ist begrenzt,
kanonisches JSON mit monotoner globaler Sequenz und enthält nur:

- Schema, opaque Run-/Attempt-/Node-ID, Ereignistyp und terminalen Grund;
- monotone Dauer, effektive Grenzwerte und begrenzte Usage-Zahlen;
- Telemetrie-Verfügbarkeit und Messquelle, nie Payloadbytes;
- optional Status der Hostaktion (`CANCEL_SENT`, `TERM_SENT`, `KILL_SENT`,
  `REAPED`).

Verboten sind lokale Pfade, PID/PGID, Workerbefehlszeilen, rohe Frames,
Payloads, Stderr, Signaturen, Truststore-Inhalte, URLs und Geheimnisse.
Eine noch nicht terminale, aber vollständig kanonisch geschriebene
Ereignispräfix-Historie ist ein zulässiger Diagnosezwischenstand. Sie beweist
weder Erfolg noch Cleanup. Syntaktisch abgeschnittene, nichtkanonische oder
manipulierte Sidecars machen einen bereits publizierten Artifact-DAG-Record
nicht rückwirkend falsch; der read-only Validator weist sie kontrolliert als
`PROTOCOL_ERROR` ab und leitet daraus weder Erfolg, Replay noch Cleanup ab.

### Peak-Memory-Semantik

`peak_memory_bytes` ist Diagnose und kein Nachweis der `RLIMIT_AS`-Einhaltung.
Unter Linux darf der Host periodisch `/proc/<pid>/status` (`VmHWM`) lesen und
nach Reap `wait4`/`rusage.ru_maxrss` einbeziehen. Der Wert ist das Maximum der
erfolgreich beobachteten Quellen; er kann fehlend oder zu niedrig sein, wenn
der Prozess vor einer Messung endet oder Procfs nicht verfügbar ist.

Die Telemetrie trägt deshalb exakt eine Verfügbarkeit:

| `peak_memory_availability` | Bedeutung |
| --- | --- |
| `OBSERVED` | mindestens eine zulässige Linux-Quelle lieferte einen Wert; keine universelle Genauigkeitsbehauptung |
| `UNAVAILABLE` | keine zulässige Quelle verfügbar; der Wert fehlt statt `0` |
| `NOT_APPLICABLE` | kein Prozess wurde gestartet |

Ein Workerwert kann niemals Hosttelemetrie ersetzen. `0` ist nur erlaubt,
wenn die verwendete OS-Quelle tatsächlich Null meldet; fehlend wird nicht zu
Null normalisiert. Procfs-Leseprobleme dürfen keinen laufenden Worker stoppen,
aber sie müssen sichtbar bleiben.

## Terminale Taxonomie

Das V6-Sidecar trennt einen groben `status` von genau einem
`terminal_reason`. `status` ist einer von `READY`, `FAILED`, `BLOCKED`,
`CANCELLED` oder `SUPERVISION_FAILED`. Gründe sind geschlossen und
versioniert:

```text
READY_ACCEPTED
WORKER_FAILED
WORKER_BLOCKED
HOST_CANCELLED
WALL_TIME_LIMIT
MEMORY_LIMIT
INPUT_LIMIT
OUTPUT_LIMIT
PROCESS_START_FAILED
PROCESS_CRASHED
PROTOCOL_ERROR
DIGEST_MISMATCH
SUPERVISION_UNAVAILABLE
```

`READY_ACCEPTED` ist nur nach vorhandener Signatur-/Bindungs-/FD6-/Seccomp-
Prüfung und bestehender Resultatannahme möglich. `MEMORY_LIMIT` darf nur
ausgegeben werden, wenn der Host dafür eine definierte Linux-Evidenz hat;
ansonsten lautet die ehrliche Klassifikation `PROCESS_CRASHED`. Diese Taxonomie
ändert weder die eingebetteten Run-V1-Records noch deren Antworten.

## Bedrohungen und Fehlerpfade

| Szenario | V6-Maßnahme | Sicherheitsgrenze |
| --- | --- | --- |
| Worker hängt oder ignoriert Cancel | monotone Deadline, feste Grace, Gruppe terminieren und reapen | keine neue Workerautorisierung |
| Host stirbt nach Fork | PDEATHSIG für den direkten Child plus eigene Gruppe | Die bestehende Seccomp-Grenze erlaubt keine Nachfahren; PDEATHSIG allein bindet keine Nachfahren |
| PID wird wiederverwendet | keine Signalentscheidung aus nackter PID; Gruppenbindung/Handle erforderlich | keine Signale an fremde Prozesse |
| Worker fälscht Usage | Host misst getrennt; Workerangaben sind keine Autorität | nur markierte Hosttelemetrie publizieren |
| Procfs ist nicht lesbar | `UNAVAILABLE`, kein falscher Nullwert | Budgetdurchsetzung bleibt unabhängig |
| RLIMIT_AS beendet Prozess unklar | nur bei evidenzgestützter Zuordnung `MEMORY_LIMIT`, sonst Crash | keine falsche Ursache behaupten |
| Sidecar ist Symlink/FIFO/fremdbeschreibbar | descriptor-relativ fail-closed; Run wird nicht in unsichere Datei geschrieben | keine Log-Exfiltration/-Injection |
| Log wird syntaktisch abgeschnitten | Validator meldet ungültige Telemetrie; keine automatische Reparatur | fachliche Records bleiben separat |
| Result trifft während Cancel ein | vor Übernahme beobachteter Cancel gewinnt | kein unautorisierter READY-Receipt |
| Parallelwelle enthält mehrere Workers | Handles einzeln, aber in einer Scheduleriteration canceln; alle reapen | keine ungebundenen Worker |

## Schnittstelle und Kompatibilität

**IMPLEMENTIERT UND BELEGT:** Die öffentliche Form ist
`logic-mod-artifact-dag-v6 run SPEC_JSON --store LOCAL_STORE --record
DAG_RECORD --report REPORT [--remote-restore HTTPS_BASE] [V4-RUN-OPTIONEN]`.
`replay` hat ausschließlich seine sechs V2-Grundargumente und weist jede
Run-Option vor I/O ab. Die fünf V4-Grenzen und V5-Restore bleiben
Hostkontrollen; sie verändern weder Spec-Digest, DAG-ID noch kanonische
V2–V5-Recordbytes. V6 erzeugt keinen neuen Recordumschlag: erfolgreiche
deterministische V2–V6-Läufe bleiben bytekompatibel, und Replay bleibt
offline, workerfrei und sidecarfrei.

## Abnahmebelege

1. Ein gültiger V6-Knoten erhält nur den bestehenden versiegelten Worker und,
   falls nötig, das unveränderte FD 6; ein Worker sieht keinen Storepfad,
   Sidecarpfad oder Netzwerk-FD.
2. Jeder gestartete Attempt wird bei Erfolg, Startfehler, Crash, Timeout und
   Cancel genau einmal gereapt; Tests belegen keine verbleibende Gruppe.
3. Wall-Time wird in der Poll-Schleife als `WALL_TIME_LIMIT` klassifiziert,
   wenn der Child noch läuft. Die Poll-Reihenfolge drainiert/reapet einen
   bereits terminalen READY-Child vor der Deadline-Prüfung; V6 behauptet daher
   keine streng vor der Deadline erfolgende Ready-Annahme.
4. `RLIMIT_AS` wird vor Exec gesetzt. Eine beweisbare öffentliche
   `MEMORY_LIMIT`-Zuordnung ist nicht abgenommen; unklare Signalenden bleiben
   `PROCESS_CRASHED`.
5. Parallele unabhängige Knoten bleiben durch V4 begrenzt; ein Cancel adressiert
   alle aktiven Handles und startet keinen Nachfolger.
6. Übernommene READY-Ergebnisse bleiben bei späterem Cancel erhalten;
   nicht übernommene READY-Rennen werden als `CANCELLED` abgelehnt.
7. Jeder Peak-Wert enthält die definierte Verfügbarkeit; fehlendes Procfs
   erzeugt keinen erfundenen Wert und stoppt nicht den Worker.
8. Symlink, FIFO, Hardlink, fremde Rechte, syntaktisch abgeschnittenes JSON
   und unbekannte Felder im Sidecar werden fail-closed behandelt, ohne
   Payload- oder Pfadinformationen öffentlich auszugeben. Ein vollständig
   kanonisches, noch nicht terminales Lifecycle-Präfix bleibt lesbar.
9. V2–V5-Unit-/Integrationstests und V2–V5-Replay bleiben bytekompatibel;
   V6-Replay startet keinen Worker und liest keinen Remote-Endpunkt.
10. Linux-Setup und Featureprüfungen laufen teilweise im frisch geforkten
    Child. Fehlschläge werden über den Startfehlerkanal als
    `PROCESS_START_FAILED` bereinigt; es gibt keinen stillen schwächeren Modus.

## Verbleibende Grenzen

**DOKUMENTIERT / KONZEPT:** Eine evidenzgestützte öffentliche
`MEMORY_LIMIT`-Zuordnung ist nicht Teil der V6-Abnahme; unklare
adressraumbezogene Prozessenden bleiben `PROCESS_CRASHED`. Ebenso nicht
implementiert sind cgroup- oder hostweite Aggregate, GPU-/Netzwerk-Worker,
Retries, globale Orchestrierung und eine öffentliche Live-Steuerung.
