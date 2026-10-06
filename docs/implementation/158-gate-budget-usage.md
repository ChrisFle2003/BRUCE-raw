# Core 0.78: CLI-Budget für Zulassungsprüfungen

```bash
bruce gate-run lauf.bruce --max-session-checkpoints 100000
```

Die optionale Grenze wird mit dem bestehenden `SessionUsage`-Zähler umgesetzt. Replay und Erfahrungsquellenaudit teilen sich das Konto; alle Prüfpunkte werden unter REPLAYING verbucht. Es gibt keine neue mathematische Budgeteinheit.

Mit Option ist die Ausgabe `application-gates-budget-v1` mit `result` (unverändertes `application-gates-v1`) und `usage` (`session-checkpoints-v1`). Ohne Option bleibt die bisherige Ausgabe unverändert. Die Grenze ist eine vorzeichenlose 64-Bit-Zahl. Null ist zulässig und stoppt am ersten unterstützten Prüfpunkt. Eine exakt ausreichende Grenze erlaubt den Abschluss; erst der nächste Prüfpunkt würde scheitern.

Bei Überschreitung liefert die CLI Exit 2 mit `SessionCheckpointBudgetExceeded`, Phase, Verbrauch und Grenze. Es wird kein Teilbericht ausgegeben. Insbesondere werden Budgetabbrüche im Erfahrungsquellenaudit nicht als fachliche Quellenablehnung dargestellt.

Dateilesen (weiter höchstens 32 MiB), JSON-Darstellung und nicht instrumentierte Restarbeit sind nicht Teil des Kontos. Es gibt keine Laufzeit-, Speicher- oder maximale Signallatenzgarantie. Reine Grid-Bundles unterstützen Replaycallbacks weiterhin nicht; mit Option erhalten sie `ReplayCheckpointsUnsupported`, ohne Option bleiben sie inspizierbar. Ein zusätzliches `--verification replay` ist für diesen Befehl nicht erforderlich und wird nicht akzeptiert.

Die Tests prüfen unveränderte Resultate, Verbrauch, Nullgrenze, exakt ausreichende Grenze, eine zu kleine Grenze, ungültige Argumente und einen Abbruch im Quellenaudit einer zulässigen Portfolioquelle.

Signalsteuerung, weitere Payloadunterstützung und anwendungsübergreifende Weiterleitung bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und auf Bruce-AI bestanden jeweils 108/108 Testgruppen. Release-Build, Installation und TGZ-Paket für 0.78.0 wurden remote erfolgreich geprüft. Die CLI-Prüfung bestand lokal und remote mit 90 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.

## Korrektur des Nutzer-Builds

Die ursprüngliche Remote-Abnahme verwendete `/tmp/bruce072-owner-p8uriP/bruce`. Das vom Nutzer aufgerufene `/home/chris/Desktop/Bruce_raw/build/bruce` blieb dabei auf 0.67.0; sein CMake-Cache verwies auf den früheren lokalen Quellpfad. Die Quellenübertragung hatte diesen Build nicht aktualisiert.

Der alte Build wurde nach `build-before-078-OdCUMf/build` verschoben und aufbewahrt. Anschließend wurde `build` auf Bruce-AI frisch als Release-Build konfiguriert und gebaut. Genau `/home/chris/Desktop/Bruce_raw/build/bruce` meldet jetzt 0.78.0 und bestand 108/108 Testgruppen sowie 90 CLI-Beispiele.

Der direkt geprüfte Aufruf `./build/bruce gate-run tests/fixtures/research-v1.bruce --max-session-checkpoints 100000` war erfolgreich und verbrauchte 24 Prüfpunkte. `lauf.bruce` in den allgemeinen Beispielen ist ein Platzhalter für eine tatsächlich vorhandene Datei.
