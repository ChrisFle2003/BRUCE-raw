# Worker Run Inspector / Recovery V1 — Design

**Status: VORSCHLAG — read-only Vertrag, vor der Implementierungsplanung zu
prüfen.**

## Ziel

Worker Boundary V1 erzeugt für jeden V5-Lauf einen privaten Run-Baum mit
Attempt-Artefakten und einer von Bruce geschriebenen JSONL-Spur. Dieser
Vertrag ergänzt einen lokalen, ausschließlich lesenden Inspector. Er soll
feststellen, ob der Run-Baum vollständig und vertragsgemäß ist, und daraus
einen metadata-only Bericht für Diagnose und Recovery erzeugen.

Der Inspector startet keinen Worker, führt keinen Adapter aus, schreibt keine
Datei, verändert kein JSONL und veröffentlicht kein Payload. Er rekonstruiert
nur den beobachtbaren Zustand aus den bereits vorhandenen V5-Dateien.

## Evidenzgrenze

**IMPLEMENTIERT UND BELEGT:** V5 erzeugt den privaten Baum
`BUNDLE.run/inputs`, `BUNDLE.run/attempts` und
`BUNDLE.run/control/run-log.jsonl`. Worker-Ergebnisse übertragen nur Status,
Digest, Größe, Referenz und Usage-Metadaten. Der Parent schreibt das JSONL-Log.

Belege:

- `apps/dag_scheduler_v5_cli.hpp` — V5-Run-Erzeugung, Attempt-Verzeichnisse und
  Log-Ereignisse;
- `include/bruce/worker/worker_run_log_v1.hpp` und
  `src/worker/worker_run_log_v1.cpp` — append-only JSONL-Vertrag;
- `docs/cli/formats.md` — öffentlicher V5-Bericht und private Run-Baum;
- `tests/unit/worker_run_log_v1_tests.cpp` und
  `tests/integration/test_dag_scheduler_v5_cli.py` — bestehende Nachweise.

**DOKUMENTIERT / KONZEPT:** Ein dauerhafter, content-addressed Artifact Store,
ein öffentlich auflösbarer Payload-Download, ein Netzwerktransport und eine
allgemeine Runtime-Orchestrierung existieren weiterhin nicht.

## Öffentliche CLI

Der erste Inspector-Aufruf lautet exakt:

```text
bruce dag-scheduler-v5 inspect BUNDLE
```

`BUNDLE` ist der bei `dag-scheduler-v5 execute` angegebene Metadatenbericht.
Der Inspector liest den zugehörigen privaten Seitensatz `BUNDLE.run/`.
`inspect` akzeptiert keine Optionen, keine Spec-Datei, keinen alternativen
Run-Root und keinen Worker- oder Adapterpfad.

Die bestehende Execute-Syntax, ihre Defaults, ihre Exitcodes und ihr
`dag-scheduler-v5-cli-v1`-Bericht bleiben unverändert. V2, V3 und V4 werden
nicht geändert.

### CLI-Ergebnisse

Ein vollständiger, vertragsgemäßer Lauf liefert Exit 0, leeres stderr und
genau einen JSON-Bericht mit abschließendem LF:

```json
{"contract":"dag-scheduler-v5-inspect-v1","action":"inspect","status":"VALID","run_finished":true,"run_status":"READY","run_id":"run-123","last_seq":12,"attempts":[]}
```

Ein syntaktisch und strukturell gültiger, aber vor `RUN_FINISHED` abgebrochener
Run liefert Exit 1 und einen metadata-only Bericht mit
`"status":"INCOMPLETE"` sowie `"run_finished":false`. Das ist ein
diagnostischer Recovery-Befund, keine erfolgreiche fachliche Ausführung.

Ein bereits regulär mit `FAILED`, `BLOCKED` oder `CANCELLED` beendeter V5-Run
ist als Run-Struktur gültig. Der Inspector liefert dafür Exit 0 mit
`status: "VALID"` und dem beobachteten `run_status`; er deutet den fachlichen
Fehler nicht in Erfolg um.

Fehlende oder unlesbare BUNDLE-/Run-Dateien, eine abgeschnittene JSONL-Zeile,
ungültiges JSON, nichtkanonische Felder, unzulässige Ereignisfolgen,
Sequenzfehler, Digest-/Größenfehler, fehlende Artefakte sowie Symlink- oder
Hardlink-Verstöße liefern Exit 2, leeres stdout und nur:

```json
{"error":"PROTOCOL_ERROR"}
```

Interne Pfade, Betriebssystemfehler, Stackdaten und Parserdetails werden nicht
öffentlich ausgegeben.

## Inspect-Bericht

Der Bericht bleibt auf Run- und Attempt-Metadaten beschränkt. Er darf
enthalten:

- Vertragsname, Aktion, `VALID`/`INCOMPLETE`, `run_finished`, `run_status`;
- eine validierte, opake `run_id` und die letzte bestätigte Sequenznummer;
- Attempt-ID, beobachtete Statusfolge, `accepted`-Flag und Eventanzahl;
- Payload-Digest und Bytegröße, sofern sie in einem gültigen Artefaktereignis
  enthalten sind;
- die fünf bestehenden Usage-Werte, sofern sie im Log enthalten sind;
- `artifact_verified: true`, wenn das referenzierte Artefakt vollständig
  gelesen und gegen Größe und SHA-256 geprüft wurde.

Der Bericht darf niemals enthalten:

- Request-, Input- oder Output-Payloadbytes;
- Hex- oder Base64-Darstellungen dieser Bytes;
- absolute Pfade oder interne `attempts/...`-Pfade;
- lokale Kommandozeilen, Worker-Executable, PIDs oder Stderr;
- frei übernommene JSONL-Rohzeilen oder unbekannte Felder;
- Passwörter, Tokens, Schlüssel oder andere Geheimnisse.

Der Inspector behauptet keine fachliche Richtigkeit, keinen mathematischen
Beweis, keine Benutzeridentität und keine Authentifizierung. Ein gültiger
Digest bestätigt nur die lokale Bindung zwischen Logeintrag und Datei.

## Eingabebindung und Dateigrenze

Der Inspector prüft, dass sowohl `BUNDLE` als auch `BUNDLE.run/` vorhanden,
regulär und besitzerbeschränkt sind. Er folgt keinen Symlinks für den
öffentlichen BUNDLE-Pfad, den Run-Root, `control`, `inputs`, `attempts`,
Attempt-Verzeichnisse, `output` oder Artefaktdateien.

Für jedes im JSONL referenzierte Artefakt gelten alle Regeln:

1. Referenz ist relativ, nicht leer und enthält weder absolute Bestandteile
   noch `..`, `.`-Komponenten, Backslashes oder Laufwerks-/URI-Trenner.
2. Jeder Pfadbestandteil wird unter dem geöffneten Run-Root mit
   `openat`/`O_NOFOLLOW` oder einer gleichwertig strengen Linux-Prüfung
   aufgelöst.
3. Das Ergebnis muss eine reguläre Datei mit genau einem Hardlink sein.
4. Die gelesene Größe muss dem Logeintrag entsprechen und unter dem bestehenden
   Dateilimit liegen.
5. Der gelesene Inhalt muss exakt den protokollierten SHA-256-Digest ergeben.
6. Eine Abweichung führt zu `PROTOCOL_ERROR`; das Artefakt wird nicht als
   akzeptiert oder wiederherstellbar berichtet.

Der Inspector prüft keine beliebigen Dateien außerhalb der im Log
referenzierten Artefakte. Unbekannte oder zusätzliche Dateien werden weder
ausgegeben noch als Payload geladen. Der Vertrag besitzt keine Lösch-,
Aufräum- oder GC-Funktion.

## JSONL-Validierung

Die bestehende JSONL-Kanonizität und Feldgrenze bleiben maßgeblich. Der
Inspector akzeptiert nur `schema: "worker-run-log-v1"`, die bestehende
Ereignismenge und die bestehenden Identifier-, Status-, Digest-, Größen- und
Referenzgrammatiken.

Zusätzlich prüft er den gesamten Lauf statt nur die nächste Append-Operation:

- die erste Zeile ist `RUN_CREATED`;
- die Sequenznummern sind positiv und streng steigend; ein fehlender oder
  doppelt vorkommender Verlauf ist ungültig;
- `RUN_FINISHED` kommt höchstens einmal vor und, falls vorhanden, als letzte
  Zeile;
- jedes Attempt beginnt mit `ATTEMPT_STARTED` und besitzt höchstens eine
  abgeschlossene Ergebnisfolge;
- `STATUS`, `CANCEL_REQUESTED`, `PROCESS_EXITED` und `RESULT_RECEIVED`
  gehören zu einem begonnenen Attempt;
- `PROCESS_EXITED` folgt auf einen begonnenen Attempt und precediert
  `RESULT_RECEIVED`;
- `ARTIFACT_ACCEPTED` folgt auf genau ein passendes `RESULT_RECEIVED` und
  besitzt Digest, Größe und Referenz;
- parallele V5-Attempts dürfen global ineinander verschachtelt sein; ihre
  jeweiligen per-Attempt-Folgen bleiben trotzdem gültig;
- ein Retry beginnt einen neuen Attempt mit eigener ID. Ein akzeptierter
  Attempt wird nicht erneut gestartet;
- fehlt `RUN_FINISHED`, bleibt ein ansonsten gültiger Verlauf
  `INCOMPLETE` statt `VALID`;
- eine angefangene oder nicht mit LF abgeschlossene letzte JSONL-Zeile ist
  `PROTOCOL_ERROR`, weil sie keine vollständige Übergabe darstellt.

Die Prüfung ist in-memory begrenzt auf das bestehende Dateilimit. Sie liest
keine unbegrenzten Dateien und allokiert nicht anhand unvalidierter JSON-
Felder.

## Ereignis- und Recovery-Modell

```text
RUN_CREATED
    |
    +--> ATTEMPT_STARTED
             |
             +--> STATUS / CANCEL_REQUESTED (optional, wiederholbar)
             |
             +--> PROCESS_EXITED
                       |
                       +--> RESULT_RECEIVED
                               |
                               +--> ARTIFACT_ACCEPTED
                               |
                               +--> neuer ATTEMPT_STARTED bei zulässigem Retry
    |
    +--> RUN_FINISHED
```

Die Darstellung beschreibt die per-Attempt- und Run-Regeln. Bei mehreren
`--workers` dürfen unabhängige Attempt-Zweige zeitlich verschachtelt werden;
die JSONL-Sequenz bleibt die einzige beobachtete Chronologie.

Ein `INCOMPLETE`-Bericht ist nur eine lokale Zustandsdiagnose. Der Inspector
setzt keinen Worker fort, wiederholt keinen Attempt und übernimmt keinen
unbestätigten Output. Fortsetzung, Bereinigung und ein dauerhafter Store
benötigen einen eigenen, späteren Vertrag.

## Komponenten und Verantwortlichkeiten

| Komponente | Verantwortung | Status |
| --- | --- | --- |
| `WorkerRunLog` | unveränderter Bruce-eigener Append-Vertrag | IMPLEMENTIERT UND BELEGT |
| `WorkerRunInspectorV1` | vollständige read-only Log-, Ereignis- und Artefaktprüfung | VORSCHLAG |
| V5-CLI `inspect` | BUNDLE → `BUNDLE.run` binden und Bericht rendern | VORSCHLAG |
| Linux-Dateiprüfung | no-follow, Besitzer-/Hardlink-/Digest-Prüfung | VORSCHLAG |
| V5 Execute | bestehender Worker-/DAG-Lauf | IMPLEMENTIERT UND BELEGT |
| Artifact Store | dauerhafte content-addressed Speicherung | DOKUMENTIERT / KONZEPT |

Der Inspector darf nicht in `bruce_core` als allgemeiner Scheduler oder
Plugin-Katalog enden. Er ist eine read-only Auswertung des konkreten V5-
Runvertrags.

## Nicht Bestandteil

Nicht Bestandteil dieses V1-Vertrags sind:

- Fortsetzung oder Wiederaufnahme eines abgebrochenen Runs;
- Cancel- oder Statussteuerung über die CLI;
- Änderung der V5-Execute-Semantik oder des V5-Reports;
- ein dauerhafter Artifact Store, Deduplication, GC oder Retention;
- Payload-Download, Payload-Export oder externe Referenzauflösung;
- Bundling, Replay oder erneute Adapterausführung;
- Netzwerk, GPU, Remote-Worker oder Worker-Pool;
- dynamische Code-Plugins, signierte Logic-Mods oder freie Programme;
- freie Sprache, ein allgemeiner Beweiser oder eine globale Runtime-
  Orchestrierung;
- Änderungen an V2, V3, V4, Phase-DAG-V2/V3 oder statischen Mod-Katalogen.

## Testbare Abnahme

1. Ein vollständiger V5-Lauf wird wiederholt inspectiert und liefert
   byteidentischen metadata-only JSON-Output.
2. Ein regulär fehlgeschlagener, geblockter oder stornierter V5-Lauf wird als
   strukturell gültig mit seinem Runstatus berichtet.
3. Ein Run ohne `RUN_FINISHED` wird als `INCOMPLETE` berichtet und nicht
   fortgesetzt.
4. Eine abgeschnittene JSONL-Zeile, fehlende LF, unbekanntes Event,
   nichtmonotone Sequenz oder unzulässige Eventfolge ergibt kontrolliert
   `PROTOCOL_ERROR`.
5. Veränderte, fehlende, zu große, symlinkte oder hardlinkte Artefakte werden
   vor einem gültigen Bericht abgewiesen.
6. Referenzen mit absolutem Pfad, `..`, `.`, Backslash oder außerhalb des
   Run-Roots werden abgewiesen.
7. Der Inspect-Aufruf startet keinen Worker, ruft keinen Adapter auf und
   schreibt keine Datei.
8. Öffentliche Inspect-Ausgabe enthält weder Payloadbytes noch lokale Pfade,
   Kommandozeilen oder Stderr.
9. Bestehende V5-Execute-, Worker-, V2-, V3- und V4-Tests bleiben unverändert
   erfolgreich.

## Spätere Ausbaustufen

Nach einem bestandenen Inspector kann ein eigener Artifact-Store-Vertrag
geplant werden. Erst danach sollte geprüft werden, ob V5-Referenzen dauerhaft
auflösbar werden. Worker-backed Logic-Mods und allgemeine Orchestrierung
bleiben davon getrennte Entscheidungen.
