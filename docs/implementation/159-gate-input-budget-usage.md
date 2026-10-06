# Core 0.79: Eingabegrenze für gate-run

```bash
./build/bruce gate-run tests/fixtures/research-v1.bruce --max-replay-bytes 1048576 --max-session-checkpoints 100000
```

`--max-replay-bytes N` begrenzt die beim Dateilesen akzeptierten Bundlebytes. Die effektive Grenze ist das Minimum aus N und der vorhandenen festen Grenze von 32 MiB. N ist eine vorzeichenlose 64-Bit-Zahl; auch UINT64_MAX hebt die feste Grenze nicht auf. Eine Datei mit exakt N Bytes darf gelesen werden; eine größere Datei wird mit `InputByteLimit` und Exit 2 abgewiesen, bevor Replay beginnt.

Der vorhandene Dateileser liest in 8192-Byte-Blöcken. Die Grenze beschränkt den aufgebauten Eingabestring, nicht exakt die vom Betriebssystem gelesene Menge. Sie ist keine Obergrenze für den gesamten Prozessspeicher, verschachtelte Rekonstruktionen oder Laufzeit. Eine leere Datei bei N=0 besteht die Größenprüfung, scheitert aber am Bundleparser.

Ohne Prüfpunktbudget bleibt das Ergebnis `application-gates-v1`; mit `--max-session-checkpoints` bleibt es `application-gates-budget-v1`. Das Bytebudget erzeugt keinen zusätzlichen Verbrauchsbericht. Beide Optionen gelten direkt für `gate-run`, ohne `--verification replay`. Die engeren Grenzen des Erfahrungsquellenaudits bleiben erhalten.

Tests prüfen exakte Grenze, Unterschreitung um ein Byte, Null, negative Eingabe, Kombination beider Budgets, Größenablehnung vor Parsing und Beibehaltung der 32-MiB-Grenze selbst bei UINT64_MAX.

Remote-Abnahmen dieses Pakets verwenden ausdrücklich `/home/chris/Desktop/Bruce_raw/build/bruce`, also den Nutzerpfad. Allgemeine Speicherbudgets, Signalsteuerung und weitere Architekturbausteine bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und remote bestanden 108/108 Testgruppen. Auf Bruce-AI wurde ausdrücklich `./build/bruce` gebaut und als 0.79.0 geprüft, einschließlich des obigen kombinierten Aufrufs (24 Prüfpunkte). Installation und TGZ-Paketprüfung bestanden. Die CLI-Prüfung bestand lokal und remote mit 91 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
