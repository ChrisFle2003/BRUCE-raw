# Core 0.61: Erfahrungsnachweise bereits im Erstlauf abbrechen

Die Quellenprüfung der Erfahrung lässt sich jetzt bereits während der ersten Ausführung unterbrechen:

```bash
./build/bruce research --train training.csv --holdout holdout.csv \
  --domain rational --strategy portfolio-rational --planner experience-order \
  --experience wissen.experience --verification replay \
  --execution-cancellation experience-checkpoints \
  --verification-cancellation research-checkpoints \
  --serialization-cancellation digest-checkpoints --output forschung.bruce
```

Die drei Optionen greifen an verschiedenen Stellen: beim Erfahrungsimport des Erstlaufs, bei der zusätzlichen Replayprüfung und bei der äußeren Exportprüfsumme. Der neue Ausführungsmodus gilt auch für erfahrungsbasierte Grid-Forschung.

Ohne Abbruch bleiben mathematische Ergebnisse, Erfahrungsstimmen und native Bundlebytes identisch. Der Sitzungsbericht verwendet V10. Die Erfahrung beeinflusst weiterhin nur die Strategiereihenfolge und wird nicht zum mathematischen Wahrheitsbeweis.

Erkennt Bruce Strg+C während des Erfahrungsimports, endet die Sitzung mit `ExecutionExperienceCancelled` in EXECUTING und ohne Export. Die Suche startet dann nicht. Ein erst nach dem Import eintretender Suchabbruch behält seine bisherigen nativen Regeln; das ist eine getrennte Abbruchart.

Die vorgelagerten Grid-Eingangsquellen, Dateieinlesen und einzelne atomare Operationen erhalten durch diese Option keine neuen Prüfpunkte. Für Replay und Serialisierung bleiben die beiden zusätzlichen Optionen im Beispiel erforderlich. Eine maximale Abbruchlatenz wird nicht zugesagt.

## Weitere Roadmap

Der Erstlauf kann nun seine Erfahrungs-Quellaudits unterbrechen. Weitere Eingangsquellen des Erstlaufs, andere Payloadfamilien, blockweises Kopieren großer Inhalte und gemeinsame Operationsbudgets bleiben offen. Dies ist kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](126-execution-experience-checkpoints-contract.md) · [Abnahme](acceptance/execution-experience-checkpoints-v1-2026-09-23.md)
