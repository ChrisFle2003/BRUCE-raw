# Core 0.42: Grid-Stapel fortsetzen

Ein gespeicherter Teilbefund kann jetzt weitere Abfragen bearbeiten. Bruce prüft zuerst die Vorgeschichte, baut den Cache wieder auf und setzt mit der nächsten offenen Abfrage fort. Die neue Datei enthält ihre gesamte Herkunft.

```bash
./build/bruce grid-batch --paths '2,6;02,06;0,0;4,3;2,6' \
  --zero-zero stop --max-query-steps 1 --output teil.bruce
./build/bruce grid-resume --from-run teil.bruce \
  --additional-queries 2 --output weiter.bruce
./build/bruce grid-resume --from-run weiter.bruce \
  --additional-queries 2 --output fertig.bruce
./build/bruce replay fertig.bruce
```

Die erste Fortsetzung bearbeitet einen Cachetreffer und einen lokalen Stopp: drei von fünf Abfragen sind fertig, Status `INCOMPLETE`, keine neue Potenzberechnung. Für Prüfung und Cacheaufbau wurden zwei Potenzberechnungen benötigt (`audit_evaluations: 2`).

Die zweite Fortsetzung beendet die Liste: `COMPLETED`, Generation 2, eine neue Berechnung und drei Prüf-/Aufbauberechnungen. Der enthaltene `batch` zählt für die logische Gesamtsequenz zwei Berechnungen. Replay der letzten Datei funktioniert auch ohne die beiden Vorgängerdateien.

Mit Strg+C kann die neue Ausführung zwischen Abfragen erneut einen Teilbefund liefern. Die Prüfung der Vorgeschichte und der Cacheaufbau laufen davor vollständig. Insgesamt sind höchstens acht aufeinanderfolgende Fortsetzungen zulässig. Ein fertiger Stapel lässt sich nicht weiter fortsetzen; die ursprünglichen Rechenregeln und Pfade lassen sich dabei nicht ändern.

Ein fertig fortgesetzter Stapel ist auch für `research --grid-train` oder `--grid-holdout` zulässig, sofern er deren bisherige Bedingungen erfüllt. Die vollständige Herkunft bleibt im Forschungsexport eingebettet. Ein unvollständiger Stapel wird weiterhin abgelehnt.

## Weitere Roadmap

Grid-Fortsetzung mit geprüfter Herkunft und rekonstruiertem Cache ist implementiert. Allgemeine Aufgaben-/Strategie-/Gate-Verträge, fortsetzbare Forschung, NanoCell, Sprache, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z bleiben weitere Arbeit. Stichproben sind kein allgemeiner Beweis; dies ist kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](88-grid-resume-contract.md) · [Abnahme](acceptance/grid-resume-v1-2026-09-22.md)
