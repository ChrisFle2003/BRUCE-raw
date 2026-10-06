# Core 0.80: Signalabbruch für gate-run

```bash
./build/bruce gate-run tests/fixtures/research-v1.bruce --cancellation checkpoints --max-replay-bytes 1048576 --max-session-checkpoints 100000
```

`--cancellation` akzeptiert `none` (Standard) oder `checkpoints`. Mit `checkpoints` registriert die CLI Handler für SIGINT (Strg+C) und SIGTERM. Diese setzen ausschließlich das vorhandene Signalflag. Am nächsten unterstützten Prüfpunkt wird `GateInspectionCancelled` mit Exit 2 ausgegeben. Es gibt keinen Teilbericht auf stdout.

Signalprüfung und Prüfpunktbudget benutzen denselben Callback. Ein bereits beobachtetes Signal hat am Prüfpunkt Vorrang vor einer Budgetüberschreitung. Replay und Erfahrungsquellenaudit reichen Callbackabbrüche unverändert durch; fachliche Ablehnungen bleiben getrennt. Ohne Prüfpunktbudget bleibt die normale Ergebnisausgabe erhalten, mit Budget der bestehende Umschlag samt Verbrauch.

Ein Signal während des Dateilesens wird erst an einem späteren Prüfpunkt ausgewertet. Blockierendes Lesen wird dadurch nicht zuverlässig beendet. Es gibt keine maximale Signallatenzgarantie, keine Kontrolle nach dem letzten Prüfpunkt und keine neuen Prüfpunkte für bislang nicht unterstützte Payloads. Reine Grid-Bundles liefern bei aktiviertem Callback weiterhin `ReplayCheckpointsUnsupported`. Ohne Option beziehungsweise mit `none` bleibt das bisherige Betriebssystem-Signalverhalten erhalten.

Die Linux-Integrationstests warten anhand von `/proc` auf die Handlerinstallation, senden SIGINT beziehungsweise SIGTERM und geben anschließend über eine FIFO ein gültiges Bundle frei. Sie prüfen Exit 2, leeres stdout und Vorrang des Signals vor einem Nullbudget. Weitere Tests prüfen beide Optionswerte ohne Signal und ungültige Werte. Bereits vorhandene C++-Tests sichern Callbackabbrüche auch im Quellenaudit ab; der Signaltest belegt keine maximale Reaktionszeit während langer Berechnungen.

Allgemeine Speicherbudgets, weitere Payloadunterstützung und automatische Ergebnisweiterleitung bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und remote bestanden 108/108 Testgruppen. Auf Bruce-AI wurde der Nutzerpfad `./build/bruce` als 0.80.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden. Die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
