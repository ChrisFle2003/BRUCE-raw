# Worker Boundary V1 — Design

**Status: VORSCHLAG — Variante 1 bestätigt, Spezifikation vor
Implementierungsplanung zu prüfen.**

## Ziel

Worker Boundary V1 führt eine isolierte lokale Prozessgrenze für feste Bruce-
Adapter ein. Jeder Ausführungsversuch erhält einen neuen Child-Prozess. Der
Worker liefert keine Payload-Rohbytes über das Result-Protokoll, sondern nur
geprüfte Digest-/Referenzmetadaten. Die Grenze bereitet spätere
Artefaktspeicher und Runtime-Orchestrierung vor, ohne bereits Netzwerk, GPU,
dynamische Plugins oder freie Sprache einzuführen.

## Nutzerentscheidung

Die folgenden Entscheidungen sind durch den Nutzer vorgegeben:

- Worker laufen als separate lokale Prozesse.
- Ein Worker-Ergebnis enthält nur Digest und Referenz, keine Payloadbytes.
- Wiederholungen sind ausschließlich über eine ausdrückliche CLI-Option
  erlaubt.
- Zeit, Eingabe-/Ausgabebytes, Operationen und Prozessspeicher werden als
  getrennte harte Ressourcenbudgets geführt und diagnostisch ausgewiesen.
- Die erste Bruce-eigene Übergabeschicht ist ein append-only JSONL-Run-Log;
  der Worker schreibt diese Datei nicht.
- Die Grace-Periode ist in V1 auf `1000 ms` festgelegt und keine CLI-Option.
- V5 startet standardmäßig einen Node; `--workers N` wählt zwischen Solo und
  bis zu vier gleichzeitig ausführbaren unabhängigen Nodes.

## Architektur

```text
CLI
  |
  v
lokaler Orchestrator im Bruce-Prozess
  |
  +-- private Run-/Attempt-Verzeichnisse
  |
  +-- stdin/stdout: versionierte Steuer- und Result-Rahmen
  |
  v
ein neuer Worker-Prozess pro Versuch
  |
  +-- liest freigegebene Input-Referenzen
  +-- schreibt ein Output-Artefakt in den Attempt-Bereich
  +-- liefert Status, Digest, Referenz und Nutzung
```

V1 startet ausschließlich den eingebauten, vertrauenswürdigen Bruce-Worker-
Modus. Es gibt keinen vom Nutzer gelieferten Programm-, Bibliotheks- oder
Scriptpfad. Ein Bundle, eine Spec-Datei oder ein Request kann keinen neuen
Code laden.

Ein Prozess-pro-Versuch-Modell wird gegenüber einem persistenten Pool gewählt,
weil Zustand, Deskriptoren, Speicher und Abstürze nach jedem Versuch isoliert
sind. Der Preis ist höherer Prozessstartaufwand; ein Pool ist eine spätere,
separate Optimierung.

Bruce bleibt der Supervisor. Der Worker besitzt keine eigene Autorität über
seinen Lebenszyklus. Bruce startet ihn, fragt seinen Zustand regelmäßig über
den Kontrollkanal ab und kann ihn kooperativ oder hart beenden. Der Child-
Prozess erhält keine vererbten Netzwerk- oder Fremdprozessrechte, keine
unbeschränkten Dateideskriptoren und keinen vom Request steuerbaren
Programmstart.

## Prozess- und Verzeichnisvertrag

Der Parent erzeugt für jeden Lauf einen privaten Verzeichnisbaum mit
Besitzerzugriff. Jeder Versuch erhält einen eigenen Unterordner:

```text
run/
  request/
  inputs/
  attempts/attempt-0000/
    output/
    control/
  final/
```

Input- und Output-Referenzen sind nur relative Pfade innerhalb dieses
kontrollierten Baums. Absolute Pfade, `..`, Symlinks außerhalb des Baums und
vom Worker frei erzeugte Zielpfade werden abgewiesen. Die V1-Veröffentlichung
bleibt temporär; dauerhafte content-addressed Speicherung gehört zu einer
späteren Artifact-Store-Spezifikation.

### Bruce-eigene Übergabeschicht: JSONL-Run-Log

Die erste Übergabeschicht ist eine von Bruce geschriebene, append-only JSONL-
Datei unter `run/control/run-log.jsonl`. Jede Zeile ist ein eigenständiges,
validiertes JSON-Ereignis; Bruce ist der einzige Schreiber und der Worker darf
diese Datei weder öffnen noch verändern. Die Datei dient in V1 als überprüfbare
Übergabe-, Diagnose- und Recovery-Spur zwischen laufendem Prozess und späterem
Bundle. Sie ist noch kein dauerhafter Artifact Store und enthält keine rohen
Payloadbytes.

Jedes Ereignis enthält mindestens `schema`, `run_id`, `attempt_id` (bei
Run-Ereignissen optional), eine monoton steigende `seq`, `ts_unix_ms`, `event`
und den aktuellen `status`. Ereignisse mit Artefakten ergänzen ausschließlich
opake Referenzen, Bytegrößen und SHA-256-Digests. Erlaubte Ereignisse sind
`RUN_CREATED`, `ATTEMPT_STARTED`, `STATUS`, `CANCEL_REQUESTED`,
`PROCESS_EXITED`, `RESULT_RECEIVED`, `ARTIFACT_ACCEPTED` und `RUN_FINISHED`.

Bruce schreibt und validiert jede Zeile vor dem nächsten Zustandsübergang. Ein
unvollständiges JSON-Objekt, eine nicht monotone Sequenz, ein unbekannter
Ereignistyp oder ein Digest-/Größenwiderspruch macht den Lauf kontrolliert
ungültig und wird als `PROTOCOL_ERROR` gemeldet. Das Log enthält keine
Passwörter, Tokens, vollständigen lokalen Pfade, freien Kommandozeilen oder
ungefilterten Stderr-Inhalte. Die Aufbewahrung und Überführung in ein
versioniertes Run-Bundle bleibt eine spätere Entscheidung.

Der Parent startet den Child-Prozess mit einem festen internen Worker-Modus,
übernimmt stdin/stdout/stderr und beendet ihn bei Timeout oder Cancellation.
Stderr wird als interne Diagnose behandelt und nie ungefiltert in einen
öffentlichen Erfolgs- oder Fehlerbericht übernommen.

## Versioniertes Protokoll und Framekodierung

Die Framekodierung ist der Transportvertrag zwischen Bruce und dem Child. Sie
verhindert, dass ein einzelnes Newline, NUL-Byte oder ein unvollständiger
Worker-Output als vollständige Antwort interpretiert wird. Sie ist kein
Netzwerkprotokoll und wird nur über lokale Pipes verwendet.

Jeder Frame hat exakt diese Struktur:

```text
BRUCE_WORKER_FRAME_V1\n
type=<REQUEST|RESULT|STATUS_QUERY|STATUS|CANCEL|ACK|EXIT>\n
payload-bytes=<dezimal>\n
payload-sha256=<64 lowercase hex characters>\n
\n
<genau payload-bytes rohe Bytes>
```

Die vier Headerzeilen erscheinen exakt in dieser Reihenfolge. Headerwerte sind
ASCII, der Payload darf beliebige Bytes enthalten. Der Parent liest zuerst die
begrenzten Header, prüft `payload-bytes` gegen Frame- und Ressourcenlimits und
liest danach genau diese Byteanzahl. Er verifiziert anschließend SHA-256 und
verwirft Restbytes, unbekannte Typen, fehlende Felder und ungültige
Kanonizität. Request-/Result-Payloads verwenden darin eine feste Reihenfolge
von UTF-8-Feldern mit expliziter Byte-Länge; Feldnamen und Duplikate sind
streng geprüft.

Der Kontrollkanal akzeptiert `STATUS_QUERY` und `CANCEL`. Bruce fragt während
des Laufs regelmäßig den Status ab. Bei Cancellation sendet er zuerst `CANCEL`,
wartet die feste V1-Grace-Periode von `1000 ms` und beendet danach die gesamte
Prozessgruppe mit SIGTERM und nötigenfalls SIGKILL. Ein Worker, der nicht mehr
antwortet, verliert dadurch nicht die Kontrolle über den Host.

### WorkerRequest V1

```text
run_id
attempt_id
node_id
adapter_id
adapter_version
request_digest
input_artifact_refs[]
max_wall_ms
max_input_bytes
max_output_bytes
max_operations
max_memory_bytes
```

`input_artifact_refs` enthalten jeweils eine lokale Referenz, erwartete Größe
und Digest. Der Worker darf nur Referenzen verwenden, die der Parent vor dem
Start freigegeben hat.

### WorkerResult V1

```text
status              READY | FAILED | BLOCKED | CANCELLED
result_code
output_artifact_ref optional
payload_digest optional
payload_size
usage.wall_ms
usage.input_bytes
usage.output_bytes
usage.operations
usage.peak_memory_bytes
failure_code
```

`WorkerResult` enthält niemals `payload_bytes`, Requestbytes, Stackdaten,
Pfadinhalte oder beliebige stderr-Ausgaben. `output_artifact_ref` ist eine
opake logische Referenz; der interne relative Dateipfad wird nie als
öffentlicher Berichtswert verwendet. Bei `READY` prüft der Parent die
Outputreferenz, liest die Datei nur innerhalb des erlaubten Baums und verifiziert
Größe und SHA-256 erneut. Bei jedem anderen Status bleibt die Outputreferenz
leer.

## Retry-Vertrag

Ohne Option existiert genau ein Versuch. Die CLI-Option lautet:

```text
--retry N
```

`N` ist die Anzahl zusätzlicher Versuche und wird vor dem Start validiert.
Automatische Wiederholung ist nur erlaubt, wenn `--retry` explizit vorhanden
ist und der Fehler als wiederholbar klassifiziert ist:

- Child-Prozessabsturz ohne gültiges Result,
- kontrollierter Timeout,
- vorübergehender Protokoll- oder Transportabbruch.

Nicht wiederholt werden fachliche `FAILED`-, `BLOCKED`- oder `CANCELLED`-
Ergebnisse, Digestfehler und ungültige Requests. Jeder Versuch erhält eine
eigene `attempt_id`, eigene Usage-Werte und einen eigenen Outputbereich. Der
finale Receipt nennt den gewählten Versuch und die Versuchshistorie als
Metadaten.

V1 setzt eine endliche Obergrenze für `N`, die der CLI-Vertrag ausdrücklich
festlegt; ein unbeschränktes Retry ist verboten. Der konkrete V1-Vertrag
erlaubt `0 <= N <= 3`; Standard ist `N=0`.

## Öffentliche CLI und Defaults

V2, V3 und V4 bleiben unverändert. Der erste öffentliche Worker-Endpunkt ist
ein neuer, ausdrücklich experimenteller V5-Befehl:

```text
bruce dag-scheduler-v5 execute SPEC --output BUNDLE
  [--retry N]
  [--max-wall-ms N]
  [--max-input-bytes N]
  [--max-output-bytes N]
  [--max-operations N]
  [--max-memory-bytes N]
  [--workers N]
```

`bruce worker-v1 child` ist ein interner, strikt validierter Child-Modus und
wird nicht als freier Programmstarter in der normalen Hilfe beworben. Der
Parent startet diesen Modus selbst; Nutzer können keinen alternativen
Executable-Pfad angeben.

Für V1 werden folgende endliche Werte vorgeschlagen. Sie sind **VORSCHLAG**
und werden als eigene CLI-Tests eingefroren, sobald die Implementierungs-
planung beginnt:

| Option | Standard | absolutes Maximum | Zweck |
| --- | ---: | ---: | --- |
| `--retry` | `0` | `3` | zusätzliche Versuche |
| `--max-wall-ms` | `30000` | `600000` | Supervisor-Watchdog |
| `--max-input-bytes` | `65536` | `1048576` | gelesene Eingaben |
| `--max-output-bytes` | `65536` | `5242880` | erzeugtes Artefakt |
| `--max-operations` | `1000000` | `10000000` | Worker-Zähler |
| `--max-memory-bytes` | `268435456` | `2147483648` | Child-Adressraumgrenze |
| `--workers` | `1` | `4` | gleichzeitig ausführbare unabhängige Nodes |

Die Werte sind getrennte Grenzen; ein Erfolg setzt voraus, dass keine Grenze
überschritten wurde. Der CLI-Befehl zeigt die tatsächlich geltenden Werte im
Metadatenbericht. Es gibt keine Option für „unbegrenzt“. `--workers 1` ist der
Solo-Modus. Werte größer als eins erlauben bis zu `N` unabhängige, bereits
bereite Nodes gleichzeitig; Abhängigkeiten werden weiterhin erst nach
akzeptiertem Vorgänger-Receipt freigegeben. Das absolute Maximum ist vier.
Die Reihenfolge und der Inhalt des semantischen Receipts bleiben unabhängig
von der tatsächlichen Parallelität deterministisch; Laufzeitmesswerte bleiben
Diagnosewerte.

## Harte Ressourcenbudgets

Jeder WorkerRequest trägt endliche Werte für alle V1-Budgets. Ein fehlendes,
negatives oder über dem Protokolllimit liegendes Budget wird vor dem Start
abgewiesen.

| Budget | Harte Durchsetzung | Reportierte Nutzung |
| --- | --- | --- |
| Wall-Time | Parent-Monotonic-Deadline; kontrolliertes Beenden, danach Kill | verstrichene Millisekunden |
| Eingabebytes | Parent prüft freigegebene Referenzen und Lesemenge | tatsächlich gelesene Bytes |
| Ausgabebytes | Parent begrenzt Artefaktdatei und akzeptiert keinen Überschuss | erzeugte Outputbytes |
| Operationen | fester Worker-Zähler; Überschreitung erzeugt `OperationBudgetExceeded` | verwendete Operationen |
| Prozessspeicher | OS-Prozesslimit für den Child; tatsächlicher Spitzenwert zusätzlich diagnostisch | gemessene Peak-Nutzung |

Bruce setzt die Prozessgrenze vor `exec` im Child mit einer Linux-
Ressourcenbegrenzung (`RLIMIT_AS`) und überwacht die Prozessgruppe zusätzlich.
Der Parent liest die Peak-Nutzung über Linux-Prozessmetriken und bestätigt sie
mit `wait4`-/Procfs-Werten. Diese Peak-Messung ist Diagnose, nicht die
Definition der harten Adressraumgrenze. Bruce behauptet daraus keine
universelle Messgenauigkeit. Ein überzogenes Budget beendet den Versuch und
wird nicht als erfolgreicher fachlicher Output übernommen.

Beim Start setzt Bruce außerdem eine eigene Prozessgruppe, schließt nicht
benötigte Deskriptoren und aktiviert eine Parent-Death-Behandlung im Child.
Damit kann ein abgestürzter oder beendeter Supervisor keinen laufenden Worker
zurücklassen. Die Implementation muss Start, Statusabfrage, kooperative
Cancellation, SIGTERM, SIGKILL und Reaping jeweils separat testen.

## Determinismus und Replay

Semantische Worker-Ergebnisse bleiben durch Adapter-ID, Version, Request-
Digest, Input-Digests, Output-Digest, Status und Result-Code prüfbar.

Zeit, Peak-Speicher und andere Laufzeitmesswerte sind Diagnosewerte und werden
nicht in einen byteidentischen semantischen Replay-Digest aufgenommen. Dadurch
bleiben Bundles trotz unterschiedlicher Prozesslaufzeiten vergleichbar. Eine
spätere V5-Bundle-Spezifikation muss Usage-Metadaten deshalb ausdrücklich von
kanonischen Ergebnisbytes trennen.

Worker Boundary V1 behauptet noch keinen portablen Offline-Replay eines
externen Artefakts. Dafür braucht es den nachgelagerten content-addressed
Artifact Store.

## Fehler und Sicherheitsgrenze

Der Parent klassifiziert mindestens:

```text
INVALID_REQUEST
PROTOCOL_ERROR
PROCESS_START_FAILED
PROCESS_CRASHED
TIMEOUT
CANCELLED
INPUT_LIMIT_EXCEEDED
OUTPUT_LIMIT_EXCEEDED
OPERATION_BUDGET_EXCEEDED
MEMORY_LIMIT_EXCEEDED
DIGEST_MISMATCH
FAILED
BLOCKED
READY
```

Öffentliche CLI-Berichte zeigen nur versionierte Fehlercodes, Versuchszahlen,
Budgets, Nutzungswerte und Digests. Sie zeigen keine lokalen Pfade,
Befehlszeilen, Stderr-Inhalte oder Payloadbytes.

V1 bleibt lokal auf dem Host. Es gibt keine Netzwerkverbindung, keine
Authentifizierung zwischen Hosts, keine dynamischen Plugins, keine GPU-
Ausführung und keine Benutzerkontrolle über das Workerprogramm.

## Testbare Abnahme

1. Ein neuer Versuch startet einen separaten Child-Prozess und übernimmt ein
   gültiges Digest-/Referenzergebnis.
2. Der Worker liefert keine Payloadbytes über das Result-Protokoll.
3. Ein Prozessabsturz, Timeout oder Cancellation beendet genau diesen Versuch.
4. Ohne `--retry` wird niemals ein zweiter Versuch gestartet.
5. Mit `--retry N` werden höchstens `N` zusätzliche, zulässige Versuche
   gestartet.
6. Fachliche `FAILED`-, `BLOCKED`- und `CANCELLED`-Ergebnisse werden nicht
   automatisch wiederholt.
7. Alle fünf Ressourcenbudgets werden einzeln begrenzt, gemessen und im
   Usage-Modell ausgewiesen.
8. Falsche Outputgröße oder Digest wird vor Receipt-Übernahme abgewiesen.
9. Jede Versuchseröffnung, Statusänderung, Cancellation und Artefaktannahme
   erscheint als validierte JSONL-Zeile im Bruce-Run-Log.
10. `--workers 1` startet den Solo-Modus; Werte bis zum Maximum erlauben
    mehrere unabhängige Nodes ohne Ausführung nicht freigegebener Abhängigkeiten.
11. V2-, V3- und V4-Verträge bleiben byteidentisch und alle bisherigen Tests
   bleiben grün.
12. Es werden keine beliebigen Programme, Pfade, Plugins oder Netzwerkziele
    aus WorkerRequest oder Bundledaten ausgeführt.

## Nicht Bestandteil

Nicht Bestandteil von Worker Boundary V1 sind ein dauerhafter Artefaktspeicher,
ein Worker-Pool, Netzwerk-/GPU-Adapter, dynamische Code-Plugins, freie
Spracheingabe, ein allgemeiner Beweiser oder eine globale Multi-Run-
Orchestrierung. Diese Schritte benötigen eigene Spezifikationen.

## Noch zu präzisieren

- **OFFENE ENTSCHEIDUNG:** ob der spätere Artifact Store die temporären
  Attempt-Verzeichnisse direkt übernimmt oder über eine eigene Übergabeschicht
  liest;
- **OFFENE ENTSCHEIDUNG:** ob das JSONL-Run-Log später als unveränderliche
  Begleitdatei erhalten bleibt oder in ein versioniertes Run-Bundle überführt
  wird.
