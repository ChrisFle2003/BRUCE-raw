# Core 0.62: Gridquellen bereits im Erstlauf abbrechen

Bruce kann jetzt die Grid-Nachweise prüfen und ihre Werte in Samples übertragen, während innere Abbruchpunkte aktiv sind:

```bash
./build/bruce research --grid-train training.bruce --grid-holdout holdout.bruce \
  --domain rational --strategy polynomial --planner interpolate --max-degree 2 \
  --verification replay --execution-cancellation grid-source-checkpoints \
  --verification-cancellation research-checkpoints --output forschung.bruce
```

Auch fortgesetzte Grid-Nachweise werden dabei intern erreicht. Bei erfahrungsbasierter Grid-Forschung aktiviert `--execution-cancellation grid-and-experience-checkpoints` beide Importphasen. Dieser kombinierte Modus setzt `--planner experience-order` und einen passenden Erfahrungssnapshot voraus.

Ohne Abbruch bleiben Trainingswerte, mathematische Ergebnisse und native Bundlebytes unverändert. Neue Sitzungsberichte verwenden V11. Die bisherigen Budget- und Quellenprüfungen bleiben erhalten: Unvollständige Gridquellen und überlappende Trainings-/Holdouteingaben sind weiterhin unzulässig.

Erkennt Bruce Strg+C während der Gridquellenprüfung, endet die Sitzung mit `ExecutionGridSourceCancelled` in EXECUTING ohne neuen Teilbefund und ohne Export. Im kombinierten Modus kann anschließend auch der Erfahrungsimport mit `ExecutionExperienceCancelled` abbrechen. Der native Suchabbruch ist davon getrennt.

Dateieinlesen, einzelne Rechenoperationen, Hashing und Kopien behalten ihre bisherigen Grenzen. Für die spätere Replayprüfung und Serialisierung bleiben eigene Optionen erforderlich. Es gibt keine maximale Abbruchlatenz.

## Weitere Roadmap

Grid- und Erfahrungsimporte der Grid-Forschung besitzen jetzt getrennte Abbruchpunkte im Erstlauf. Weitere native Quellenimporte, zusätzliche Payloadfamilien, blockweises Kopieren und gemeinsame Operationsbudgets bleiben offen. Dies ist kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](128-execution-grid-source-checkpoints-contract.md) · [Abnahme](acceptance/execution-grid-source-checkpoints-v1-2026-09-23.md)
