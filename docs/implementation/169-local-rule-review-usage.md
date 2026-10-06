# Core 0.89: lokale Regelprüfung und bestätigter Export

Der neue Ablauf setzt die lokale Variante der [menschlichen Regelprüfung](../contracts/05-human-rule-approval.md) um.

```sh
./build/bruce rules-propose bisher.rules vorgeschlagen.rules aenderung.proposal
./build/bruce rules-approve aenderung.proposal bisher.rules bestaetigt.rules
```

Der zweite Befehl zeigt beide vollständigen Fassungen und das Ausgabeziel. Nur die exakte angezeigte Bestätigungszeile erzeugt die neue Datei. Die Eingabe erfolgt im Terminal; es gibt kein `--yes`. Anschließend kann `bestaetigt.rules` ausdrücklich als `--rules` bei `compatibility` verwendet werden. Bestehende Dateien werden nicht ersetzt. Für eine erste Einrichtung dient ein gültiger leerer Snapshot als Ausgangsfassung.

Der Vorschlag bewahrt alte und neue Inhalte. Der Bestätigungsdigest bindet zusätzlich den angegebenen Ausgabepfad. Geänderte Ausgangsregeln, beschädigte Vorschläge und nichtinteraktive Eingaben werden abgelehnt. Nach Ablehnung wird keine Ausgabe erzeugt.

Die Integrationstests verwenden ausschließlich temporäre Testregeln und ein Pseudoterminal. Sie prüfen Erstellung, nichtinteraktive Ablehnung, ausdrückliche Ablehnung, erfolgreichen Export, Überschreibschutz, veraltete Basis und Beschädigung.

## Offen

Kein globaler Aktivierungsdienst, keine atomare Verwaltung aktiver Versionen und kein dauerhaftes Entscheidungsjournal. Bestehende direkte Regeldateiaufrufe bleiben möglich. Der lokale Dialog ist eine Bedienkontrolle und kein technischer Nachweis menschlicher Identität. Dies ist kein vollständiger Architekturabschluss.
