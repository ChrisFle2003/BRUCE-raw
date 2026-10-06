# Lokale Regelprüfung – Core 0.102.0

[Handbuch](README.md) · [Befehle](commands.md) · [Regelsyntax](formats.md#regeldatei-bruce_rules_v1)

## Zweck und Wirkung

Ein Vorschlag bewahrt die bisherige und die vorgeschlagene Regelfassung. Nach ausdrücklicher Bestätigung exportiert Bruce die neue Fassung in eine noch nicht vorhandene Datei. Diese beiden Exportbefehle verwalten keine aktive Regelversion. Seit Core 0.91 gibt es dafür einen separaten [lokalen Regelspeicher](active-rules.md). `compatibility --rules DATEI` verwendet weiterhin direkt die angegebene Datei und verlangt keinen Freigabenachweis. Erfahrungsdaten und ein allgemeines „weiter“ bestätigen keinen Vorschlag.

## Vorschlag erstellen

```text
bruce rules-propose OLD NEW PROPOSAL
```

| Position | Bedeutung |
| --- | --- |
| OLD | Vorhandene bisherige Regeldatei im Format `BRUCE_RULES_V1` |
| NEW | Vorhandene vorgeschlagene Regeldatei desselben Formats |
| PROPOSAL | Neue Vorschlagsdatei; Elternverzeichnis muss existieren |

Genau drei Positionsargumente; keine Optionen, Standardwerte, `--output`, `--yes` oder Sitzungsbudgets. Dateiendungen sind frei. Relative Pfade beziehen sich auf das aktuelle Arbeitsverzeichnis; Pfade mit Leerzeichen in Shell-Anführungszeichen setzen. `-` bezeichnet einen gewöhnlichen Dateinamen, nicht stdin/stdout.

OLD und NEW werden jeweils mit höchstens 1048576 Bytes gelesen und mit dem bestehenden Regelparser geprüft. Danach werden beide kanonisiert: vereinheitlichte Abstände, dezimale Ganzzahlen und Zeilenumbrüche; Snapshotkennung und Regelreihenfolge bleiben erhalten. Kanonisch identische Fassungen werden abgelehnt. Eine geänderte Snapshotkennung zählt bereits als Änderung. Die Prüfung beurteilt keine fachliche Zweckmäßigkeit.

Erfolg: Exit 0 und genau ein JSON-Objekt auf stdout:

```json
{"proposal":"<64-stelliger SHA-256-Digest>","approved":false}
```

`proposal` ist der Digest des Paketkörpers, keine Dateipfadkennung. Der Export überschreibt keine vorhandene Datei. OLD und NEW bleiben unverändert.

## Vorschlag prüfen und exportieren

```text
bruce rules-approve PROPOSAL CURRENT OUTPUT
```

| Position | Bedeutung |
| --- | --- |
| PROPOSAL | Vorhandener Vorschlag, maximal 3145728 Bytes |
| CURRENT | Aktuelle bisherige Regeldatei; kanonischer Inhalt muss OLD im Vorschlag entsprechen |
| OUTPUT | Noch nicht vorhandene Ausgabedatei für die bestätigte neue Regelfassung |

Auch hier genau drei Positionsargumente und keine Optionen. CURRENT darf ein anderer Dateipfad als das ursprüngliche OLD sein: Verglichen wird der kanonische Inhalt. NEW wird nicht erneut gelesen; maßgeblich ist die im Vorschlag gespeicherte Fassung.

Prüfungsreihenfolge:

1. Vorschlag begrenzt lesen und Prüfsumme überprüfen.
2. Paketversion und beide eingebetteten kanonischen Regelfassungen prüfen; identische Fassungen ablehnen.
3. CURRENT lesen, kanonisieren und mit der eingebetteten Basis vergleichen.
4. Prüfen, dass sowohl stdin als auch stdout ein Terminal sind.
5. Beide vollständigen Fassungen anzeigen: jede alte Zeile mit `-`, jede neue mit `+`. Das ist eine vollständige Gegenüberstellung, kein minimaler Zeilendifferenzbericht. Anschließend erscheint der JSON-quotierte Ausgabepfad.
6. Eine Zeile einlesen. Nur `CONFIRM `, gefolgt vom vollständig angezeigten Digest, stimmt zu. Zusätzliche Leerzeichen, andere Groß-/Kleinschreibung oder Dateiende lehnen ab.
7. CURRENT erneut lesen und vergleichen; erst danach OUTPUT exklusiv erzeugen.

Es gibt keinen Wiederholungsdialog bei falscher Eingabe. Erneuter Versuch: Befehl erneut starten. Bei normaler Ablehnung wird OUTPUT nicht erzeugt. Eine vorhandene Ausgabe wird erst beim Export zurückgewiesen; der Dialog kann zuvor trotzdem erscheinen.

Erfolg: Exit 0. Nach dem Klartextdialog folgt dieses JSON auf stdout:

```json
{"approved_export":true,"confirmation":"<Bestätigungsdigest>","proposal":"<Paketdigest>"}
```

OUTPUT enthält ausschließlich die neue kanonische `BRUCE_RULES_V1`-Datei, weder ein Forschungsbundle noch eine Signatur oder ein Entscheidungsjournal. `approved_export` bezeichnet den erfolgreichen Dialog-/Exportpfad; es ist kein Identitätsnachweis.

**Nicht in eine JSON-Pipeline umleiten:** `rules-approve ... > antwort.json` oder `rules-approve ... | python3 -m json.tool` scheitert an der Terminalprüfung. Fehler erscheinen als JSON auf stderr mit Exit 2; wenn der Fehler erst nach dem Dialog auftritt, steht auf stdout bereits Klartext. SIGINT/SIGTERM besitzen hier keinen besonderen Teilbefund- oder Sitzungsabbruchvertrag; Betriebssystembeendigung kann andere Exitcodes liefern.

## Vollständiges Beispiel mit eigenen Testdateien

Vom Projektstamm in einem interaktiven Terminal ausführen. Das neue temporäre Verzeichnis verhindert Überschreiben vorhandener Dateien:

```bash
review_dir=$(mktemp -d)
printf 'BRUCE_RULES_V1\nsnapshot bisher\n' > "$review_dir/bisher.rules"
printf 'BRUCE_RULES_V1\nsnapshot vorschlag\nrule positiv positive value ge 0 0 1 manual -\n' > "$review_dir/neu.rules"
./build/bruce rules-propose "$review_dir/bisher.rules" "$review_dir/neu.rules" "$review_dir/aenderung.proposal"
./build/bruce rules-approve "$review_dir/aenderung.proposal" "$review_dir/bisher.rules" "$review_dir/bestaetigt.rules"
```

Nun beide Fassungen prüfen und nur bei Zustimmung die angezeigte `CONFIRM …`-Zeile eingeben. Der Digest hängt auch vom Ausgabepfad ab und kann deshalb nicht vorab aus dieser Anleitung kopiert werden.

Nach erfolgreicher Bestätigung kann die Datei ausdrücklich für eine Auswahl verwendet werden:

```bash
printf 'id,domain,layer,x,y,value\n1,demo,0,2,6,64\n' > "$review_dir/kandidaten.csv"
./build/bruce compatibility --rules "$review_dir/bestaetigt.rules" --candidates "$review_dir/kandidaten.csv"
```

Der leere Ausgangssnapshot ist gültig. Dies initialisiert keinen globalen Regelspeicher. Vorschlag und alte Datei zum Nachvollziehen aufbewahren.

## Paketformat und Digestbindung

Die erzeugte Vorschlagsdatei besteht aus:

```text
SHA256(PAYLOAD) + LF + PAYLOAD
```

PAYLOAD besteht bytegenau aus `BRUCE_RULE_PROPOSAL_V1`, LF, der mit C++ `std::quoted` geschriebenen alten kanonischen Fassung, LF, der ebenso geschriebenen neuen Fassung und LF. `std::quoted` umschließt den Text mit doppelten Anführungszeichen und maskiert Anführungszeichen sowie Backslashes; enthaltene Zeilenumbrüche bleiben echte Zeilenumbrüche. **Es ist kein JSON-Stringformat.**

Der Bestätigungsdigest ist SHA-256 über die vollständigen eingelesenen Vorschlagsbytes, gefolgt von `LF + OUTPUT + LF` (hier ist `OUTPUT` das wörtliche Schlüsselwort) und dem unveränderten dritten Positionsargument. Er bindet beide Fassungen und die angezeigte Schreibweise des Ausgabepfads. Pfade werden dafür nicht aufgelöst: `neu.rules` und `./neu.rules` ergeben unterschiedliche Digests. Das Arbeitsverzeichnis und die Identität des Zielverzeichnisses werden nicht mitgebunden.

## Fehler und Abhilfe

| Fehler / Präfix | Ursache und nächster Schritt |
| --- | --- |
| `Usage: rules-propose …` | Falsche Argumentanzahl bei einem der beiden Befehle; genau drei Pfade übergeben |
| `CannotReadFile: …`, `FileReadError` | Eingabe fehlt oder kann nicht vollständig gelesen werden; Pfad und Leserechte prüfen |
| `InputByteLimit` | Eine Eingabe überschreitet ihre oben genannte Bytegrenze |
| Regelparserfehler | Ungültige Version, Kennung, Regelzeile, Zahl oder Grenze; [Regelsyntax](formats.md#regeldatei-bruce_rules_v1) und [Diagnoseindex](source-index.md) verwenden |
| `RuleProposalUnchanged` | OLD und NEW sind kanonisch gleich |
| `RuleProposalDigestMismatch` | Erste Zeile fehlt oder Digest passt nicht zum Paketkörper; Originalvorschlag wiederherstellen oder neu erstellen |
| `InvalidRuleProposal` | Paketstruktur, Version oder kanonischer Inhalt ungültig, zusätzliche Inhalte oder identische Fassungen |
| `RuleBaseChanged` | CURRENT entspricht nicht mehr der eingebetteten Basis, vor oder nach dem Dialog; neuen Vorschlag gegen die aktuelle Basis erstellen |
| `RuleApprovalRequiresTerminal` | stdin oder stdout ist kein Terminal; interaktiv ohne Ein-/Ausgabeumleitung aufrufen |
| `RuleApprovalDeclined` | Bestätigungszeile stimmt nicht exakt oder Eingabe endet; keine neue Regeldatei |
| `CannotCreateExport: …` | Temporäre Datei im Zielverzeichnis nicht anlegbar; Elternverzeichnis und Schreibrechte prüfen |
| `ExportExistsOrUnavailable: …` | Ziel existiert oder exklusives Anlegen scheitert; anderen neuen Zielpfad wählen beziehungsweise Dateisystem prüfen |
| `ExportWriteFailure`, `ExportSyncFailure`, `ExportCloseFailure` | Schreiben oder Abschluss des Exports fehlgeschlagen; Speicherplatz und Dateisystem prüfen |
| `DigestFailure`, `BundleByteLimit` | Gemeinsame Digest-/Exporthilfe schlägt fehl oder deren 32-MiB-Grenze wird überschritten |

Die zuerst erreichte Prüfung bestimmt den Fehler; die Tabelle ist keine alternative Prioritätsordnung. Dateisystemfehler enthalten teilweise betriebssystemspezifische Zusätze.

## Bewusste Grenzen

Die Prüfsumme ist keine Signatur. Ein Terminal beweist keine menschliche Identität; ein lokales Programm kann einen Terminaldialog nachbilden. Das Quellenfeld `manual` ist ebenfalls kein Freigabenachweis. Dies ist die vereinbarte lokale Bedienkontrolle.

CURRENT-Prüfung und OUTPUT-Export sind keine gemeinsame Transaktion. Ein anderer Schreiber kann die Basis nach der letzten Prüfung ändern. Dieser Einzeldatei-Export besitzt keine Aktivierung, Sperre, Ablaufzeit, einmalige Verbrauchskennung oder dauerhafte Protokollierung der Entscheidung. Derselbe Vorschlag kann bei unveränderter Basis erneut für einen neuen Ausgabepfad bestätigt werden. Der exklusive Export synchronisiert die Dateiinhalte, gibt aber keine umfassende Zusage für Stromausfall und Verzeichnis-Dauerhaftigkeit.

Die beiden Regelbefehle unterstützen weder Forschungs-/Replaybudgets noch `inspect-run`, `assess-run`, `gate-run` oder `replay` für Vorschlagsdateien. Diese Befehle erwarten mathematische Ergebnisbundles. Weitere Architektur: [lokaler Freigabevertrag](../contracts/05-human-rule-approval.md).

## Anwendungsschnittstelle ab Core 0.90

Die CLI nutzt für Paketprüfung und Digestbindung jetzt `bruce::RuleProposal`. Der Terminalablauf und die oben beschriebenen Dateiformate bleiben unverändert. [Architektur und C++-Einstiege](../implementation/170-rule-proposal-application-usage.md).
