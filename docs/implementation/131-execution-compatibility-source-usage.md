# Core 0.63: Forschungsimport der Z-Auswahl abbrechen

Beim Ableiten von Kandidaten aus Forschung kann jetzt auch der Erstlauf intern auf einen Abbruch reagieren:

```bash
./build/bruce compatibility --rules regeln.txt --from-run forschung.bruce --at 2 \
  --verification replay \
  --execution-cancellation compatibility-source-checkpoints \
  --verification-cancellation compatibility-checkpoints \
  --serialization-cancellation digest-checkpoints --output auswahl.bruce
```

Der neue Modus erreicht klassische, exakte und Grid-basierte Forschungsquellen sowie darin eingebettete Erfahrungsnachweise. Er prüft auch zwischen den Hypothesen, deren Werte als Kandidaten projiziert werden.

Ohne Abbruch bleiben Quellenprüfung, Auswahl, Zähler und native Bundlebytes identisch. Der Sitzungsbericht verwendet V12. Ein Quellenimport ersetzt weiterhin keinen allgemeinen Wahrheitsbeweis.

Bei Strg+C während des Imports endet die Sitzung mit `ExecutionCompatibilitySourceCancelled` in EXECUTING ohne neue Auswahl und ohne Export. Die eigentliche Kandidatenbewertung beginnt dann nicht. Ein späterer nativer Kandidatenabbruch ist davon getrennt und behält seine bisherigen Regeln.

Der Modus gilt nur für `--from-run`. Bei manuellen Kandidaten wird er abgelehnt. Einzelne Projektionen, Snapshotaufbau, Hashing, Kopien und Dateieinlesen bleiben atomar. Die späteren Replay- und Serialisierungsphasen brauchen weiterhin ihre eigenen Optionen; eine maximale Abbruchlatenz wird nicht zugesagt.

## Weitere Roadmap

Auch der Forschungsimport der Z-Auswahl besitzt nun eigene Abbruchpunkte im Erstlauf. Native Fortsetzungsimporte, weitere Payloadfamilien, blockweises Kopieren großer Inhalte und gemeinsame Operationsbudgets bleiben offen. Dies ist kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](130-execution-compatibility-source-contract.md) · [Abnahme](acceptance/execution-compatibility-source-v1-2026-09-23.md)
