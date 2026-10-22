# BRUCE MCP audit trace v1

**Status:** **IMPLEMENTIERT UND BELEGT** für die unten abgegrenzte lokale
Stdio-Aufzeichnung; frische Abnahme vom 2026-10-18 siehe „Aktueller Nachweis“.
Signaturen, authentifizierte Identität, Remote-Audit und mathematische
Beweisfunktion bleiben **DOKUMENTIERT / KONZEPT** außerhalb der Lieferung.

## Ziel

Der lokale Stdio-MCP-Adapter erzeugt optional einen prüflingsgebundenen,
unmittelbar geschriebenen Audit-Trace. Ein Benchmark-Organisator
kann damit nachvollziehen, welche MCP-Nachrichten an der aufgezeichneten
Adaptergrenze beobachtet und welche Antworten ausgewählt wurden. Die Funktion bleibt
ohne explizite Aktivierung vollständig aus.

Der Trace ist ein lokales, vertrauliches Artefakt. Er belegt die Konsistenz
der aufgezeichneten Adaptergrenze, nicht die Identität eines Autors, die
Zustellung an den Client, die Unveränderlichkeit gegenüber einem böswilligen
lokalen Benutzer oder die mathematische Richtigkeit einer Antwort.

## Nichtziele

- kein Netzwerkdienst, Hintergrunddienst, zentraler Mod-Dienst oder eigener
  Betriebssystembenutzer;
- keine Datenbank, globale Codex-Konfigurationsänderung oder automatische
  Installation;
- keine Änderung am C++-Kern, an der BRUCE-CLI oder am HTTP-MCP-Inspector;
- keine Aufnahme von Umgebungsvariablen, Prozessargumenten, internen
  Python-Ausnahmen oder anderen Daten außerhalb der Stdio-MCP-Grenze;
- keine digitale Signatur, Benutzeridentität oder allgemeine Beweisfunktion;
- kein Replay und keine erneute Ausführung von Toolaufrufen durch den
  Verifier.

## Aktivierung und Prozesszuordnung

Tracing wird ausschließlich aktiviert, wenn beide Variablen gesetzt sind:

```sh
BRUCE_MCP_RUN_ID=benchmark-r2-assisted \
BRUCE_MCP_TRACE_PATH=/absolute/path/benchmark-r2-assisted.jsonl \
BRUCE_MCP_BRUCE=/absolute/path/to/bruce \
BRUCE_MCP_ASSET_ROOT=/absolute/path/to/assets \
python3 tools/bruce_mcp.py
```

Sind beide Variablen nicht gesetzt, bleibt das bisherige Verhalten erhalten
und es wird keine Trace-Datei geöffnet. Ist genau eine Variable gesetzt oder
ist ihr Wert ungültig, beendet sich der Adapter vor dem Lesen von stdin mit
einem generischen Fehler auf stderr und einem von null verschiedenen
Exitstatus.

`BRUCE_MCP_RUN_ID` muss 1 bis 128 ASCII-Zeichen aus
`[A-Za-z0-9._-]` enthalten. Die Kennung wird in jedem Record wiederholt. Sie
ist eine explizite Prozesszuordnung durch den Aufrufer, keine authentifizierte
Benutzeridentität.

`BRUCE_MCP_TRACE_PATH` muss ein absoluter, noch nicht vorhandener Dateipfad
sein. `~` und Umgebungsvariablen werden nicht expandiert. Der Elternordner
muss existieren, ein echter Ordner sein und darf in seiner aufgelösten Form
nicht von der angegebenen absoluten Form abweichen. Der Adapter erstellt
keine Ordner.

## Sichere Dateierzeugung

Der Writer öffnet die Zieldatei genau einmal mit exklusiver Neuerzeugung,
Append-Modus und `0600`. Wo verfügbar wird `O_NOFOLLOW` verwendet. Danach
bestätigt `fstat`, dass das geöffnete Ziel eine reguläre Datei ist, und
`fchmod` setzt die Rechte auf `0600`. Vorhandene Dateien, symbolische Links,
nicht reguläre Ziele und unsichere Elternpfade werden abgewiesen.

Jeder Record wird als eine kanonische UTF-8-JSON-Zeile vollständig
geschrieben und anschließend mit `fsync` synchronisiert. Kann ein Record
nicht vollständig geschrieben oder synchronisiert werden, sendet der Adapter
keine weitere MCP-Antwort, meldet einen generischen Trace-Fehler auf stderr
und beendet sich ungleich null. Der Trace kann ohne Footer oder mit einem
unvollständigen Record verbleiben. Eine fehlgeschlagene Footer-Synchronisierung
führt zur bestmöglichen Rücknahme auf den letzten synchronisierten Präfix;
bei defektem Speichermedium gibt es keine zusätzliche Haltbarkeitsgarantie.

## Format `bruce-mcp-trace-v1`

Die Datei ist JSONL. Jeder Record ist ein JSON-Objekt mit
`trace_version: "bruce-mcp-trace-v1"`, `run_id`, `record_type`, `sequence`,
`previous_hash` und `record_hash`.

Die kanonische Darstellung verwendet `json.dumps` mit sortierten Schlüsseln,
kompakten Trennzeichen, UTF-8, `ensure_ascii=False` und `allow_nan=False`.
`record_hash` ist SHA-256 über die kanonische UTF-8-Darstellung desselben
Records ohne das Feld `record_hash`. `previous_hash` enthält den Hash des
vorherigen Records. Der Header verwendet 64 ASCII-Nullen.

### Header

Der erste Record hat `record_type: "header"` und `sequence: 0`. Er enthält:

- `trace_version` und `run_id`;
- `adapter_name: "bruce-mcp"`;
- `adapter_version: "0.3"`;
- `created_unix_ns` als UTC-Unixzeit in Nanosekunden;
- `previous_hash` und `record_hash`.

Die MCP-Serverversion wird ebenfalls von `0.2` auf `0.3` erhöht, damit ein
Client den tracefähigen Adapterstand erkennen kann. Daraus folgt keine neue
MCP-Capability und kein neues Tool.

### Exchange

Für jede vollständig aus stdin eingelesene Zeile folgt genau ein Record mit
`record_type: "exchange"`. Seine Sequenz beginnt bei 1 und steigt ohne
Lücken. Er enthält:

- `received_unix_ns` und `completed_unix_ns`;
- `duration_monotonic_ns`, gemessen mit einer monotonen Uhr;
- `request_bytes` und `request_sha256` über den vom Python-Textstream
  beobachteten, als UTF-8 codierten Nachrichtentext;
- `request_text` ohne abschließenden Zeilentrenner und `request`, falls die
  Nachricht höchstens 16384 Bytes groß ist;
- `response_text` und `response` für eine erzeugte JSON-RPC-Antwort;
- `response_text: null` und `response: null` für Notifications;
- `outcome` mit genau einem Wert aus `response`, `notification` oder
  `message_too_large`;
- `previous_hash` und `record_hash`.

Der Adapter konstruiert den Response-Text genau einmal. Derselbe String wird
im Trace gespeichert und anschließend, ergänzt um genau einen Zeilentrenner,
nach stdout geschrieben. Der Trace belegt damit das vom Adapter ausgewählte
Ausgabepayload, nicht dessen Empfang durch den Client.

Für valide oder fehlerhafte Nachrichten innerhalb des Nachrichtenlimits ist
`request_text` vollständig vorhanden. Der bisherige Größenentscheid zählt die
als UTF-8 codierte eingelesene Zeile einschließlich des gelieferten
Zeilentrenners. Subprozess-stdin erhält CRLF empirisch unverändert; es findet
hier keine universelle Zeilenumbruchnormalisierung statt. Für
`request_text`, `request_bytes` und `request_sha256` wird genau ein
abschließendes LF oder CRLF entfernt. Der aufgezeichnete Inhalt ist keine
Behauptung über ursprüngliche Betriebssystembytes vor der Textdecodierung. `request` enthält
das geparste Objekt oder `null`, wenn JSON-Parsing fehlschlägt oder der
geparste Wert in der kanonischen UTF-8-JSON-Darstellung dieses Traceformats
nicht darstellbar ist. Dazu gehören etwa Unendlichkeit aus `1e400` und
escapte einzelne UTF-16-Surrogate ohne gültiges Paar. In diesem Fall bleiben
`request_text`, `request_bytes` und `request_sha256` vollständig erhalten;
nur das strukturierte `request` ist `null`. Der Wert wird weder konvertiert
noch durch eine andere strukturierte Darstellung ersetzt. Das bisherige
Parsing und die ausgewählte MCP-Antwort bleiben unverändert; nachfolgende
Nachrichten und der reguläre EOF-Footer werden weiter aufgezeichnet. Der
Verifier akzeptiert diese vorhandene `null`-Darstellung, ohne den Rohtext
erneut zu parsen oder auszuführen. `response`
enthält exakt das Objekt, aus dem `response_text` erzeugt wurde.

Für eine Nachricht oberhalb von 16384 Bytes sind `request_text` und
`request` aus Gründen der Speicher- und Datenträgerbegrenzung `null`.
`request_bytes`, `request_sha256` und `outcome: "message_too_large"` bleiben
vorhanden. Die erzeugte `MessageTooLarge`-Antwort wird vollständig erfasst.

### Footer

Bei regulärem EOF schreibt der Adapter einen letzten Record mit
`record_type: "footer"`. Seine Sequenz folgt unmittelbar auf den letzten
Exchange. Er enthält `message_count`, `completed_unix_ns`,
`previous_hash` und `record_hash`. Der Hash dieses Footers ist der finale
Kettenhash.

Ein Prozessabbruch, Trace-Schreibfehler oder Signal kann den Footer
verhindern. Ein ansonsten gültiger Trace ohne Footer ist ausdrücklich
`INCOMPLETE` und wird nicht still als erfolgreicher Lauf behandelt.

## Writer- und Verifier-Grenzen

`tools/bruce_mcp_trace.py` enthält die gemeinsame, abhängigkeitsfreie
Implementierung für:

- Validierung von Run-ID und Zielpfad;
- exklusive sichere Dateierzeugung;
- kanonische Serialisierung und Hashverkettung;
- Header-, Exchange- und Footer-Erzeugung;
- reine Verifikation vorhandener Trace-Dateien.

`tools/bruce_mcp.py` importiert das Modul. Bei deaktiviertem Tracing wird kein
Writer erzeugt. Die Toolliste und alle bestehenden Toolschemas bleiben
unverändert.

`tools/verify_bruce_mcp_trace.py TRACE.jsonl` ist ein dünner lokaler
Einstiegspunkt. Er gibt genau ein JSON-Objekt auf stdout aus. Exitcode 0 gilt
nur für `VALID`; alle anderen Ergebnisse liefern Exitcode 1.

Der Verifier liest keine Assets, startet BRUCE nicht und führt keine
aufgezeichnete Nachricht erneut aus. Er prüft Format, Version, erlaubte
Recordtypen, Feldtypen, Run-ID-Konstanz, lückenlose Sequenz, kanonische
Recordhashes, `previous_hash`, Headerposition, genau einen abschließenden
Footer und dessen `message_count`.

Die Statuswerte sind:

- `VALID`: vollständiger Header, gültige lückenlose Exchanges, gültiger
  Footer und intakte Hashkette;
- `INCOMPLETE`: gültiger Präfix mit Header und Exchanges, aber kein Footer;
- `TAMPERED`: syntaktisch lesbare Records mit Hash-, Reihenfolge-,
  Run-ID-, Zähler- oder Verkettungswiderspruch;
- `INVALID_FORMAT`: leere Datei, ungültiges JSONL, unbekannte Version oder
  Recordtypen sowie fehlende beziehungsweise falsch typisierte Pflichtfelder.

Die strukturierte Ausgabe enthält mindestens `status`, `trace_version`,
`run_id`, `message_count`, `final_hash` und eine stabile, nicht sensible
`reason`. Nicht anwendbare Werte sind `null`.

## Fehler- und Datenschutzgrenze

Aktiviertes Tracing ist fail-closed. Eine ungültige Konfiguration oder ein
nicht sicher erzeugbarer Trace verhindert den Serverstart mit Exitcode 2 und
`BruceMcpTraceConfigurationError`. Ein späterer
Schreibfehler beendet den Prozess, statt eine unprotokollierte Antwort zu
senden; Schreib-/Sync- und stdout-Ausgabefehler liefern Exitcode 3 und
`BruceMcpTraceWriteError`. Ein Ausgabefehler verhindert den Footer; ein
teilweise geschriebener Record kann `INVALID_FORMAT` statt `INCOMPLETE`
ergeben. Die Rücknahme eines fehlgeschlagenen Footers ist bei defektem
Speicher nur bestmöglich. Fehlermeldungen nennen eine stabile Fehlerklasse, aber weder
Dateiinhalte noch interne Python-Ausnahmen.

Requests und Responses können vertrauliche lokale Dateinamen, mathematische
Eingaben und Ergebnisdaten enthalten. Der Anwender muss Trace-Dateien wie die
zugehörigen Run-Artefakte schützen. Der Adapter nimmt keine automatische
Redaktion vor, weil diese die geforderte vollständige Grenzaufzeichnung
zerstören würde. Umgebungsvariablen, der konfigurierte BRUCE-Pfad, der
Asset-Root und Unterprozessargumente werden nicht als Konfigurationsmetadaten
aufgezeichnet. Solche Zeichenfolgen bleiben enthalten, falls sie bereits im
Request oder Response vorkommen. Der Trace ist kein Remote-Audit-Dienst.

Die SHA-256-Kette macht nachträgliche Änderungen, Entfernungen und
Umordnungen erkennbar, solange ein erwarteter finaler Hash oder die
ursprüngliche Datei zum Vergleich vorhanden ist. Da derselbe lokale Benutzer
die ganze Datei einschließlich aller Hashes neu erzeugen kann, ist sie keine
Signatur und kein Schutz gegen einen böswilligen lokalen Benutzer.

## Tests und Abnahme

`tests/integration/test_bruce_mcp.py` enthält isolierte temporäre
Trace-Verzeichnisse und folgende Fälle:

1. Ohne Trace-Variablen entsteht keine Datei; bestehende stdout-Antworten
   bleiben unverändert.
2. Initialize, `notifications/initialized`, `tools/list`, erfolgreicher
   Toolaufruf und EOF erzeugen einen `VALID`-Trace mit byteidentischen
   Response-Strings.
3. Die Datei ist regulär und hat Modus `0600`.
4. Genau eine gesetzte Variable, ungültige Run-ID, relativer Pfad,
   vorhandenes Ziel, Zielsymlink, unsicherer Elternpfad und nicht
   beschreibbares Ziel scheitern vor dem MCP-Loop.
5. Parsefehler und ungültige JSON-RPC-Envelopes erscheinen mit ihren
   tatsächlich gesendeten Antworten im Trace.
6. Eine übergroße Nachricht enthält nur Länge und Digest des Requests, aber
   die vollständige `MessageTooLarge`-Antwort.
7. Ein gültiger Präfix ohne Footer ergibt `INCOMPLETE`.
8. Änderung, Löschung oder Umordnung eines Records ergibt `TAMPERED`.
9. Kaputtes JSONL, unbekannte Version und falsche Feldtypen ergeben
   `INVALID_FORMAT`.
10. Ein erzwungener Trace-Schreibfehler beendet den Adapter ungleich null,
    ohne eine weitere stdout-Antwort zu senden.
11. Der Verifier startet weder BRUCE noch liest er konfigurierte Assets.

Die bestehende CTest-Registrierung `bruce_mcp_workflows` bleibt der
maßgebliche Integrationspfad. Zusätzlich werden ein frischer Release-Build,
der vollständige CTest-Lauf, die lokale Markdown-Linkprüfung,
`docs/cli/verify_examples.py` in einer temporären vollständigen Kopie und ein
Vergleich gegen die vor Beginn erzeugte Baseline ausgeführt. Die ursprüngliche
Arbeitskopie besitzt keine verwendbaren Git-Metadaten. Git-Status, Diff und
Checkpoints der Abnahme gehören ausschließlich zur vollständigen temporären
Prüfkopie, nicht zum ursprünglichen Benutzerprojekt.

### Aktueller Nachweis

**IMPLEMENTIERT UND BELEGT (2026-10-18):** Eine neue vollständige Kopie unter
`/tmp/bruce-mcp-trace-acceptance.4lXm37/source` wurde über den vorhandenen
Abhängigkeitswrapper als Release mit Tests in `/tmp/bruce-mcp-trace-build`
konfiguriert und gebaut. `bruce_mcp_workflows` besteht mit allen 59 MCP-Tests.
Der vollständige CTest-Lauf umfasst 286 Tests: 281 bestehen unmittelbar,
fünf scheitern ausschließlich an verweigerten lokalen Sockets (`EPERM`).
Gezielte Wiederholungen dieser fünf Tests außerhalb der Sandbox bestehen
vollständig; es verbleiben keine ungeklärten Testfehler und keine auf
CTest-Ebene übersprungenen Tests. Die HTTP-Inspector-Suite enthält vier
übersprungene Python-Untertests: zwei Konfigurationsvarianten und zwei
DOM-Prüfungen wegen fehlendem Node.js im Suchpfad. Diese sind kein Nachweis
dieser DOM-Pfade und keine Einschränkung der lokalen Trace-Prüfung.

`docs/cli/verify_examples.py` prüft in der frischen Kopie 172 Beispiele;
alle drei generierten Artefakte stimmen bytegenau mit den unveränderten
Artefakten der Implementierungsquelle überein. Die lokale Linkprüfung meldet
511 Dokumente, 3119 Links und null defekte Links. Ein echter Prozesslauf mit
Initialize, Initialized-Notification, Tools/List, `bruce_calculate` für `2+3`
und EOF verifiziert `VALID`, `message_count: 4`, strukturierten Wert `5`,
Dateimodus `0600` und bytegleiche Response-Texte. Konfigurierte Executable-,
Asset-Root- und Trace-Pfade sowie ein unabhängiger Umgebungsmarker fehlen im
Trace. Diese Aussage betrifft hinzugefügte Metadaten, nicht beliebige
vertrauliche Zeichenfolgen innerhalb von MCP-Nachrichten.

## Dokumentation

Aktualisiert werden die aktuelle Stdio-MCP-Dokumentation und die lokale
Sicherheitsgrenze. Sie beschreiben:

- die beiden Opt-in-Variablen und ein minimales Start-/Verifikationsbeispiel;
- Dateirechte, exklusive Neuerzeugung und fail-closed Verhalten;
- vollständige Grenzaufzeichnung ohne automatische Redaktion;
- die vier Verifierstatus;
- die Grenzen von Hashkette, Laufkennung und mathematischer Aussagekraft;
- die unveränderte Trennung zwischen Stdio-MCP-Adapter und HTTP-Inspector.

Die generierten CLI-Artefakte werden nicht von Hand geändert. Da weder ein
BRUCE-CLI-Befehl noch dessen Hilfe verändert wird, wird keine Änderung dieser
Artefakte erwartet.
