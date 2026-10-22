# Runtime Orchestrator V1 – lokale Multi-Run-Steuerung

**Status:** **IMPLEMENTIERT UND BELEGT.** Runtime Orchestrator V1 ist ein
bewusst kleiner, Linux-lokaler foreground-Host über bereits akzeptierten
Logic-Mod-Artifact-DAG-V6-Läufen. Die belegte Oberfläche ist
`bruce runtime-orchestrator-v1 serve`; sie hat weder ein persistentes
Queue-Format noch Wiederaufnahmeverhalten.

## Ausgangslage und Evidenz

Die folgenden Abhängigkeiten sind **IMPLEMENTIERT UND BELEGT**, aber nur in
ihrem jeweils dokumentierten lokalen Umfang:

| Baustein | Bestehende Grenze | Beleg |
| --- | --- | --- |
| Logic-Mod Artifact DAG V2–V4 | Ein lokaler DAG mit maximal 16 Knoten, stabiler Kahn-Reihenfolge, Wave-Admission und per-Lauf V4-Grenzen | [V4-Abschnitt im Blueprint](../../architecture/03-blueprint-consolidation.md#logic-mod-artifact-dag-v4), [V4-Core](../../../src/mod/logic_mod_artifact_dag_v4.cpp) |
| Artifact DAG V5 | Optionales hostseitiges HTTPS-Restore nur für erwartete direkte Vorgänger vor FD 6 | [V5-Abschnitt im Blueprint](../../architecture/03-blueprint-consolidation.md#logic-mod-artifact-dag-v5), [V5-Core](../../../src/mod/logic_mod_artifact_dag_v5.cpp) |
| Artifact DAG V6 | Ein lokaler V6-Run mit direkter Linux-Worker-Supervision und verpflichtendem Sidecar | [V6-Spezifikation](2026-10-17-logic-mod-artifact-dag-v6-supervision-telemetry-design.md), [V6-Core](../../../src/mod/logic_mod_artifact_dag_v6.cpp) |
| V6 Attempt-Handle | Direkter Child, Prozessgruppe, Reap, feste Cancel-Eskalation und pro-Knoten-Telemetrie | [Attempt-Implementierung](../../../src/mod/logic_mod_worker_attempt_v6.cpp) |
| V6-Sidecar | Owner-only Diagnose-JSONL unter `REPORT.run/control/supervision-v6.jsonl`; keine zweite fachliche Wahrheit | [Sidecar-Implementierung](../../../src/mod/logic_mod_supervision_log_v6.cpp), [öffentlicher V6-CLI-Adapter](../../../apps/logic_mod_artifact_dag_v6_cli.hpp) |

Der [Blueprint](../../architecture/03-blueprint-consolidation.md) ordnet V1
als die einzige belegte allgemeine Multi-Run-Steuerung ein. Prioritäten,
globale harte Ressourcenbudgets, Persistenz und weitergehende Runtime-
Orchestrierung bleiben außerhalb dieses Vertrags.

## Ziel und Leitprinzipien

**IMPLEMENTIERT UND BELEGT:** Runtime Orchestrator V1 nimmt mehrere vollständig beschriebene
V6-Run-Aufträge lokal entgegen, reiht sie deterministisch ein und startet nur
so viele *gesamte V6-Runs*, wie seine globalen hostseitigen Zulassungsgrenzen
erlauben. Er delegiert einen zugelassenen Lauf unverändert an V6; er ersetzt
weder V6-Preflight noch dessen Wave-Scheduler, Artifact Store, Remote Restore,
Worker-Authorisierung, FD-6-Übergabe, Attempt-Handle oder Sidecar.

Der Orchestrator ist eine **Host-Steuerungsschicht**, keine neue fachliche
DAG- oder Worker-Runtime:

```text
Orchestrator-Client
        |
        v
lokaler Queue-/Admission-Host
        |  (globale Slots und deklarierte Reservierungen)
        +------------------------------+
        |                              |
        v                              v
  V6-Run A (bestehend)           V6-Run B (bestehend)
        |                              |
        v                              v
 V6 Sidecar A                    V6 Sidecar B
        |                              |
        +------------> lokale Stores / V2–V6-Records
```

Folgende Regeln sind bindend für V1:

1. Ein Auftrag enthält einen vollständigen, bereits lokalen V6-Run-Plan:
   Spec-Datei, lokaler Store, Recordziel, Reportwurzel und nur die heute
   zulässigen V6-Run-Kontrollen. Er übergibt keinen Workerpfad, PID, FD,
   Payload, rohe Request-/Responseframes, Truststore-Inhalt, Credential oder
   freien Befehl.
2. Der Orchestrator startet einen V6-Lauf ausschließlich über dessen
   wohldefinierte Library-/Host-Schnittstelle. Er startet nie direkt einen
   Logic-Mod-Worker und interpretiert keinen Workerframe.
3. V2–V6-Spec-Digest, DAG-ID, erfolgreiche Recordbytes, Replay und
   Artifact-Identitäten bleiben unverändert. Orchestratorsteuerung wird nie
   in sie serialisiert.
4. Ein V6-Sidecar bleibt Diagnose eines Runs. Der Orchestrator gibt nur eine
   flüchtige opaque `sidecar_ref` aus; sie ist kein Pfad, kann vor einer
   Sidecar-Datei existieren und veranlasst weder Lesen noch Validierung oder
   Ableitung einer Zusammenfassung. Sidecareinträge autorisieren weder Erfolg
   noch Cancel noch Recordpublikation.
5. Jeder Akzeptanz-, Start-, Cancel-, Drain- und Abschlussübergang erhält
   eine eindeutige Orchestratorursache. Es gibt keinen stillen Retry und
   keine automatische Wiederaufnahme.

## Geltungsbereich von V1

V1 umfasst ausschließlich:

- einen einzelnen lokalen Orchestratorprozess für einen effektiven Benutzer;
- eine begrenzte In-Memory-Queue mit explizit konfigurierten Maxima;
- deterministische Admission und FIFO-Fairness über Aufnahmeordnung;
- globale Begrenzung gleichzeitig **aktiver V6-Runs** und deklarierter
  Run-Reservierungen;
- Host-Cancel eines einzelnen Queued- oder aktiven Runs samt vollständigem
  Drain vor dessen terminalem Orchestratorzustand;
- einen metadatenarmen Statusabruf sowie eine flüchtige opaque
  Sidecar-Referenz ohne Sidecar-Leseautorität;
- einen klaren Prozessende-/Restart-Grenzvertrag ohne behauptete Recovery.

### Nichtziele

V1 implementiert oder behauptet nicht:

- keine GPU-, Netzwerk-, Container-, Remote-Host- oder Multi-Maschinen-
  Ausführung;
- keine freie Sprache, allgemeines Beweisen, neue DAG-Knotentypen, dynamische
  Knoten, freie Code-Plugins oder neue Workerfähigkeiten;
- keine globale CPU-, RAM-, Prozess-, I/O- oder cgroup-Abrechnung;
- keine Addition von V6-`RLIMIT_AS`-Werten zu einer beweisbaren Host-
  Speichergrenze;
- keine Prioritäten, Quoten, Nutzergruppen, Preemption, Aging, Retry- oder
  Backoff-Policy; FIFO ist die einzige V1-Fairnessregel;
- keine automatische Wiederaufnahme, Reparatur, Record-Neuschreibung,
  Artifact-Rollback, Sidecar-Reparatur, Garbage Collection oder Store-
  Replikation;
- keine HTTP-, RPC-, WebSocket-, Cockpit- oder Mehrbenutzer-API;
- keine öffentliche Offenlegung von Pfaden, PID/PGID, Commandline,
  Frame-/Payloadbytes, Signaturen, URLs, Credentials oder Telemetrie-
  Rohwerten.

## Auftragsidentität und Zustandsmodell

**IMPLEMENTIERT UND BELEGT:** Bei erfolgreicher lokaler Annahme vergibt der Host eine opaque
`run_id`. Sie ist keine DAG-ID, kein Spec-Digest, kein Recordname und keine
Authentisierung. Der Host hält zusätzlich einen Aufnahmezähler `enqueue_seq`
ab 1. Die Paarung `(enqueue_seq, run_id)` bestimmt eine totale und
reproduzierbar darstellbare Reihenfolge innerhalb einer Hostlaufzeit.

```text
SUBMITTED
   | admission + sichere lokale Planprüfung
   v
QUEUED ---- cancel ----> CANCELLED
   |
   | alle globalen Slots/Reservierungen verfügbar
   v
ADMITTED -> STARTING -> RUNNING ---- cancel ----> DRAINING
                                  |                 |
                                  v                 v
                              FINISHED          CANCELLED
                                  |
                                  +--> SUCCEEDED | FAILED | BLOCKED
```

`SUBMITTED` ist nur eine flüchtige Einreichungsphase und wird nicht als
beobachtbarer Dauerzustand benötigt. Die dauerhaften V1-Zustände sind:

| Zustand | Bedeutung | Ressourcenreservierung |
| --- | --- | --- |
| `QUEUED` | vollständig angenommener Auftrag wartet | keine |
| `ADMITTED` | globale Slots und deklarierte Reservierungen sind atomar reserviert | ja |
| `STARTING` | V6-Aufruf beginnt; noch kein erfolgreicher V6-Run belegt | ja |
| `RUNNING` | V6-Lauf aktiv oder im V6-internen Drain | ja |
| `DRAINING` | Cancel wurde an V6 übergeben; Orchestrator wartet auf dessen Rückkehr | ja |
| `SUCCEEDED` | V6 hat einen akzeptierten Lauf zurückgegeben | freigegeben |
| `FAILED` | V6-Preflight, V6-Lauf, lokale Startgrenze oder Orchestratorfehler | freigegeben |
| `BLOCKED` | deklarierte V1-Zulassung kann niemals erfüllt werden | freigegeben |
| `CANCELLED` | Queue-Cancel oder nach vollständigem V6-Drain beobachteter aktiver Cancel | freigegeben |
| `LOST_ON_HOST_EXIT` | nur nach künftigem explizitem Journal/Restart-Import; V1 erzeugt ihn nicht selbst | keine |

Ein V6-internes Ergebnis `DAG_HOST_CANCELLED` wird in der Orchestratoransicht
nur zu `CANCELLED`, wenn der Orchestrator selbst den Cancel ausgelöst oder
weitergeleitet hat. Andernfalls ist es ein V6-Laufergebnis unter `FAILED` mit
dem begrenzten, nicht sensiblen Grund `V6_CANCELLED_EXTERNALLY`. Das verhindert
eine falsche Zuschreibung von Autorität.

## Admission, Reservierungen und Fairness

### Verbindliche V1-Globalgrenzen

Der Host besitzt feste, beim Start geprüfte Grenzen:

| Grenze | Bedeutung | Durchsetzung |
| --- | --- | --- |
| `max_queued_runs` | maximale Zahl wartender akzeptierter Aufträge | Einreichung wird vor jeder V6-Datei-/Store-/Workeraktion abgewiesen |
| `max_active_runs` | maximale Zahl aus `ADMITTED`, `STARTING`, `RUNNING`, `DRAINING` | atomare Slotreservierung vor V6-Aufruf |
| `max_reserved_parallel_nodes` | Summe jeder deklarieren V6-`max_parallel_nodes` aktiver Runs | atomare Reservierung; V6 selbst bleibt je Run maßgeblich |
| `max_reserved_started_workers` | Summe jeder deklarieren V6-`max_started-workers` aktiver Runs | Admission-Reservierung, keine Aussage über tatsächlich gestartete Prozesse |
| `max_reserved_payload_bytes` | Summe jeder deklarieren V6-`max-total-payload-bytes` aktiver Runs | Admission-Reservierung, keine Aussage über Storegröße oder RAM |

Die Werte sind **keine** cgroup-, RAM- oder CPU-Grenzen. Insbesondere sind
V6-Werte bei nicht gesetzten optionalen V4-Grenzen für eine globale
Reservierung nicht sicher ableitbar. V1 verlangt deshalb eine vollständige,
endliche Orchestrator-Reservierung für jeden Auftrag. Ein erster
Implementierungsplan muss entscheiden, ob dies durch verpflichtende
Orchestratorfelder geschieht oder ob er engere V6-Standardwerte definiert;
beides darf die V6-CLI-Semantik nicht rückwirkend verändern.

### Deterministische FIFO-Regel

1. Der Host prüft eingehende Aufträge synchron und weist syntaktisch oder
   sicherheitsseitig ungültige Aufträge ab, ohne eine Queueposition zu
   verbrauchen.
2. Erfolgreich angenommene Aufträge erhalten den nächsten `enqueue_seq` und
   werden ausschließlich aufsteigend danach gereiht.
3. Der **älteste** `QUEUED`-Auftrag wird geprüft. Passen alle Reservierungen,
   wird er zugelassen. Passen sie nicht, wird kein jüngerer Auftrag
   überholt. Der Scheduler wartet, bis Ressourcen frei werden oder der
   Auftrag als unzulässig/abgebrochen endet.
4. Ein Auftrag, dessen einzelne deklarierte Reservierung eines der
   Hostmaxima übersteigt, wird als `BLOCKED` mit `RESERVATION_EXCEEDS_HOST`
   terminalisiert, statt die Queue dauerhaft zu blockieren.
5. Ein erfolgreich zugelassener Auftrag behält seine Reservierung bis V6
   vollständig zurückkehrt und alle V6-Worker bereits gedraint sind.

Damit ist die Politik deterministisch und starvation-frei für eine endliche
Folge zulässiger Aufträge, sofern aktive Läufe zurückkehren. Sie optimiert
keinen Durchsatz: Head-of-line-Blocking ist in V1 absichtlich möglich und
sichtbar. Prioritäten, Backfilling und Aging sind spätere, getrennte
Entscheidungen.

## Ausführung, Cancel und Drain

Ein zugelassener Auftrag erhält eine private, einmalige V6-Controlkopie. Der
Orchestrator darf nur seine eigene zusätzliche Cancel-Quelle daran anbinden;
er verändert keine V6-Budgets nach Admission und schreibt weder Spec noch
Record noch Sidecar. V6 behält die Verantwortung für:

- Preflight vor Worker-/Artifact-Aktionen,
- V4-Wave-Admission, V5-Remote-Restore und FD-6-Prüfung,
- direkte Attempts, feste V6-Grace und die Beendigung/Reap seiner Worker,
- Recordpublikation und per-Run Sidecar.

### Queue-Cancel

Ein Cancel gegen `QUEUED` entfernt den Auftrag atomar vor Start. Es gibt dann
keinen V6-Call, keinen Worker, keine Storemutation, keinen Record und kein
Sidecar durch diesen Auftrag. Der terminale Grund lautet `CANCELLED_QUEUED`.

### Aktiver Cancel

Ein Cancel gegen `ADMITTED`, `STARTING` oder `RUNNING` latscht genau einmal
`cancel_requested` im Orchestrator und übergibt die bestehende hostseitige
Cancelquelle an V6. Der Orchestrator startet danach keine neue Arbeit dieses
Runs. Er bleibt in `DRAINING`, bis V6 zurückkehrt; erst dann gibt er alle
Reservierungen frei und veröffentlicht `CANCELLED`. Er sendet keine Signale
an PID/PGID, liest keine Worker-FDs und verkürzt nicht die feste V6-Grace.

Falls der V6-Aufruf vor einem tatsächlich gestarteten Worker scheitert, bleibt
der Auftrag `FAILED`, außer der Orchestratorcancel wurde vorher eindeutig
gelatcht und V6 bestätigt kontrolliertes Ende. Dieser Vorrang muss in Tests
explizit festgelegt werden; er darf nicht aus Timing geschätzt werden.

## Status und Berichtssicht

**IMPLEMENTIERT UND BELEGT:** V1 besitzt eine lokale, metadatenarme Statusansicht mit nur:

- Orchestratorvertrag ohne Host-Epoch-ID;
- `run_id`, `enqueue_seq`, Zustand und geschlossenem terminalen Grund;
- Aufnahme-, Start- und Endreihenfolge als monotone Zähler, keine Uhrzeiten
  mit forensischem Anspruch;
- deklarierte Reservierungswerte und deren Admission-Entscheidung;
- bei bekanntem V6-Abschluss: `accepted`, V6-Status und DAG-ID;
- eine flüchtige opaque `sidecar_ref`; sie ist kein Pfad, kann auch vor
  Sidecar-Erzeugung vorhanden sein und verknüpft oder validiert kein Sidecar.

Die Ansicht enthält weder lokale Pfade noch PIDs, Prozessgruppen,
Commandlines, URLs, Credentials, Payloads, Input-/Outputinhalte, Signaturen,
rohe V6-Frames noch Rohtelemetrie. Der Orchestrator liest oder validiert ein
Sidecar nicht. Seine Ereignisse ändern nicht
den Orchestratorzustand, reparieren keinen Lauf und erzeugen keinen Replay.

V1 schreibt **kein** neues kanonisches Runrecordformat. Ein kompletter,
erfolgreicher V6-Record bleibt der alleinige fachliche Laufnachweis; der
Orchestratorstatus ist Betriebsmetadatum.

## Persistenz, Hostende und Recovery-Grenze

V1 ist absichtlich in-memory. Queue, Zähler, Reservierungen, aktive
Cancel-Latches und Statusansicht enden mit dem Orchestratorprozess. Beim
Hostende gilt:

- Die bestehende V6-PDEATHSIG-/Attempt-Grenze ist für jeden direkten Worker
  maßgeblich; der Orchestrator behauptet darüber keine zusätzliche
  Nachkommen- oder Fremdprozessbereinigung.
- V6 kann bereits einen Record oder Artefakte veröffentlicht haben. Diese
  bleiben nach ihren bestehenden Verträgen bestehen; der Orchestrator rollt
  sie nicht zurück.
- Es gibt keine automatische Wiederaufnahme oder Rekonstruktion der Queue.
  Ein späterer Prozess darf V1-Aufträge nicht aus zufälligen Report-, Store-
  oder Recorddateien ableiten.
- Ein künftiges persistentes V2 benötigt ein eigenes, owner-only,
  descriptor-relatives Journal mit einer atomaren Definition für queued,
  admitted, active und nach Restart *unresolved* Aufträge. Es darf nicht
  behaupten, einen vor dem Absturz aktiven V6-Run sicher fortsetzen zu können.

## Sicherheitsmodell und Fehlermodi

| Bedrohung oder Fehler | V1-Reaktion | Grenze |
| --- | --- | --- |
| Bösartiger Auftrag mit Pfad/URL/Code | enger V6-Planparser; nur erlaubte V6-Eingaben und lokale sichere Dateiprüfung | kein generischer Process Launcher |
| Queueüberflutung | `max_queued_runs` vor Aufnahme; keine implizite Platte-/RAM-Queue | keine Mehrbenutzerquoten |
| Unendliches Warten am Queuekopf | nur nachgewiesenes Überschreiten als `BLOCKED`; sonst bewusst FIFO-Warten | keine Backfill-Policy |
| Globales Budget erschöpft | keine neue Admission; aktive V6-Runs werden nicht retroaktiv aufgrund einer Reservierung gekillt | keine echte Hostressourcenmessung |
| Cancel-Rennen | atomarer Latch, keine neuen Starts, vollständiger V6-Drain vor Freigabe | V6 definiert Worker-Timing |
| Sidecar-Manipulation | keine Statusautorität; optional read-only V6-Validator weist es fail-closed ab | vorhandener Record bleibt davon getrennt |
| Hostcrash | keine Recoverybehauptung; keine automatische Queue-Rekonstruktion | bestehende V6 Child-Grenzen bleiben maßgeblich |
| PID/PGID-Verwechslung | Orchestrator signalisiert nie direkt | nur V6-Attempt kennt seine Gruppe |
| Reservation-Fälschung | Werte sind begrenzte Hostoptionen, nicht Workerangaben; Admission prüft sie vor V6 | kein Beweis tatsächlicher Nutzung |

## Linux- und Plattformgrenzen

V1 ist zunächst Linux-lokal, weil V6s sichere Attempt-Grenze `fork`, eigene
Prozessgruppen, `prctl(PR_SET_PDEATHSIG)`, `wait4`, `RLIMIT_AS`, Procfs-
Beobachtung und Seccomp voraussetzt. Der Orchestrator darf daraus keine
hostweite Ressourcenhoheit ableiten. Insbesondere ersetzt eine Summe von
deklarierten V6-Limits weder cgroups noch `systemd`-Scopes und kann physische
RAM-, CPU-, FD-, Netzwerk- oder Disk-I/O-Nutzung nicht beweisen oder hart
erzwingen.

Der erste Implementierungsschritt muss einen einzigen effektiven Benutzer
voraussetzen. Mehrere gleichzeitige Orchestratorprozesse sind getrennte
Controller und teilen keine In-Memory-Reservierung. Cross-user-Fairness,
Service-Manager-Integration und persistente Hostlocks liegen außerhalb V1.

## Öffentliche Schnittstelle

**IMPLEMENTIERT UND BELEGT:** Die einzige öffentliche V1-Eingabeform ist die
strikte foreground-CLI `bruce runtime-orchestrator-v1 serve` mit kanonischem,
auf 65536 Bytes begrenztem JSONL auf stdin/stdout. Ihre Befehle sind
`submit`, `status` und `cancel`. Unix-Socket, HTTP, Remote-RPC, WebSocket und
Cockpit sind nicht implementiert. Es gelten:

- `submit` darf einen Auftrag nur nach vollständiger syntaktischer,
  Ownership- und Reservierungsprüfung als `QUEUED` ausgeben;
- `status` ist read-only und startet, repariert oder replayt nichts;
- `cancel` akzeptiert nur eine opaque `run_id`, nie PID/PGID oder Pfad;
- keine V2–V6-`replay`-Route erhält Orchestratoroptionen oder eine
  Orchestrator-Abhängigkeit;
- alle Fehler werden als geschlossene, dokumentierte Orchestratorgründe
  ausgegeben, ohne interne Pfade oder Secrets.

Die fünf Globalgrenzen haben bewusst keine Defaults oder behaupteten
Kapazitätswerte: Jeder `serve`-Aufruf muss sie positiv und explizit liefern.
Die CLI begrenzt zusätzlich die V6-Planwerte auf die bestehenden V6-Bereiche
(`max_parallel_nodes` 1–4, `max_total_wall_ms` 1–60000,
`max_total_payload_bytes` 0–8388608, `max_started_workers` 1–16). Diese
Parsergrenzen sind keine Deployment-Kalibrierung.

## Testbare Abnahmebedingungen

Die Implementierung ist **IMPLEMENTIERT UND BELEGT**, nachdem folgende
Nachweise in der Task-6-Akzeptanzmatrix bestanden:

1. **FIFO:** Drei zulässige Aufträge starten in `enqueue_seq`-Reihenfolge;
   ein jüngerer, kleinerer Auftrag überholt einen nicht passenden Kopf nicht.
2. **Atomare Admission:** Parallele Einreichungen überschreiten weder
   `max_active_runs` noch irgendeine deklarierte Reservierung. Kein Auftrag
   wird zweimal gestartet.
3. **Unmögliche Reservierung:** Ein Auftrag über einem Einzelhostmaximum wird
   vor V6-Ausführung, Workerstart oder Storemutation terminal `BLOCKED`; ein
   read-only V4-Preflight darf bestehende Storezustände prüfen. Die Queue macht
   danach Fortschritt.
4. **Queue-Cancel:** Cancel vor Admission erzeugt keinen V6-Aufruf,
   Sidecar, Record, Storeobjekt oder Worker.
5. **Aktiver Cancel:** Der Orchestrator latched genau einmal, V6 drainiert
   alle gestarteten Attempts, danach werden Reservierungen genau einmal
   freigegeben und kein späterer Knoten/Run beginnt wegen dieses Auftrags.
6. **Keine V6-Semantikänderung:** Für identische deterministische V6-Läufe
   bleiben V2–V6-Records bytegleich; V6-Replay bleibt worker-, remote- und
   sidecarfrei.
7. **Sidecar-Grenze:** Manipulierte, abgeschnittene, symlinkartige oder
   unlesbare Sidecars verändern keinen Orchestratorabschluss und führen
   weder zu Reparatur noch zu Workerstart.
8. **Datenminimierung:** Öffentliche Statusausgabe enthält keine Pfade,
   PID/PGID, URL, Credential, Payload, Frame, Signatur oder Rohtelemetrie.
9. **Hostende:** Ein Test beendet den Orchestrator während eines V6-Runs und
   belegt ausschließlich die dokumentierten V6-Cleanup-Garantien; keine
   unbewiesene Recovery wird behauptet.
10. **Linux-Fehler:** Fehler bei Queue-/Planprüfung, V6-Start, Cancel,
    Drain und Statusabruf hinterlassen keine Orchestrator-FD-/Lock-Leaks und
    keine fortbestehenden Orchestratorreservierungen.

## Nächste Entscheidungen außerhalb V1

**OFFENE ENTSCHEIDUNG:** Persistenz oder Recovery benötigen einen eigenen
Vertrag für owner-only Journal, atomare Zustände und den Umgang mit nach
Hostende unaufgelösten Aufträgen. V1 importiert dafür absichtlich keinerlei
Dateien. Ebenso benötigen Multi-User-Steuerung, Socket-/HTTP-APIs, harte
globale Ressourcenlimits, Prioritäten und eine über V6 hinausgehende Runtime
eigene Spezifikationen und Belege.

Die vorhandenen V6-Läufe bleiben der fachliche Ausführungsbaustein; V1 ergänzt
nur deren belegte lokale Multi-Run-Zulassung und Lebenszyklussteuerung.
