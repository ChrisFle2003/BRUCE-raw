# Core 0.94: gemeinsame Ausführungssteuerung der Übergaben

`TransferControl` zählt optionale gemeinsame Prüfpunkte und prüft Abbruch vor Budgetüberschreitung. Beide Transfer-Anwendungseinstiege nehmen einen optionalen `ReplayCheckpoint` entgegen. Digestverarbeitung, native Quellenprüfungen, Experience-Aufbau/-Nachprüfung sowie Z-Kandidatenprüfung sind angebunden. Callbackfehler dürfen nicht als fachliche Zahlenfehler oder regulärer Teilbefund verschluckt werden.

Der vorhandene Experience-Anwendungseinstieg und `append_experience` leiten den optionalen Callback an vorhandene Quellenprüfungen weiter. Die anderen Experience-Aktionen behalten ihre bisherigen atomaren Abschnitte; es wird keine durchgängige neue Steuerung dieser Aktionen behauptet. Ohne Callback bleiben fachliche Ausgaben und Formate unverändert.

Die CLI ergänzt `--max-transfer-checkpoints` und `--transfer-cancellation` bei `experience apply-add` und `compatibility-apply`. Ein gemeinsames Konto reicht bis zum letzten Kontrollpunkt vor dem Export; die Kompatibilitätsserialisierung verwendet ebenfalls diesen Callback. Der kontrollierte Bericht umschließt den bisherigen Transferbericht mit Verbrauchsdaten. Pläne, Snapshots und mathematische Bundles erhalten keine neue Version.

[CLI-Vertrag mit Feldern, Priorität, Tests und Grenzen](../cli/transfer-control.md).

Offen bleiben Planungsvorläufe, inneres Dateilesen und Export, weitere atomare Arbeit, allgemeine Verwaltungssteuerung und universelle Operations-/RAM-/Zeitbudgets. Das historische [Abnahmeprofil Core 0.93](174-local-cli-acceptance-profile.md) wird nicht nachträglich zu einem Nachweis dieser neuen Funktion umgeschrieben.

## Verifikation am 26.09.2026

- Lokal (Debug): 117/117 CTest-Testgruppen erfolgreich; CLI-Verifikation mit 106 Beispielen, 69 dokumentierten Optionen, 370 Diagnoseeinträgen und gültigen Links.
- Übertragung: 32 geänderte/neue Dateien ausdrücklich per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen.
- Bruce-AI (Release): dort neu gebaut; 117/117 Testgruppen erfolgreich (97,27 Sekunden). `./build/bruce --version` bestätigt 0.94.0; dieselben 106 CLI-Beispiele und Dokumentationsprüfungen erfolgreich.
- Installation und Paket: lokal und remote über `verify_install.py --package` erfolgreich geprüft; lokal jeweils 26 historische Fixtures für Installation und Archiv nachgewiesen.
- Grenzfälle: exakt ausreichendes Prüfpunktlimit, ein Punkt zu wenig, Limit 0, real zugestelltes SIGTERM sowie Callback-Abbrüche innerhalb der Verarbeitung getestet. Bei erkanntem Transferabbruch entsteht keine neue Ausgabedatei. Ausreichend budgetierte native Ausgaben stimmen bytegleich mit dem bisherigen Weg überein.

Es wurden keine produktiven Regeln aktiviert oder geändert. Diese Nachweise gelten für das beschriebene Paket; sie sind keine Gesamtfreigabe des Blueprints.
