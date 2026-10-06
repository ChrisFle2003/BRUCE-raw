# Core 0.64: Vorgängerprüfung und Cacheaufbau einer Fortsetzung abbrechen

Bei einer Grid-Fortsetzung kann jetzt bereits die Vorbereitung im Erstlauf intern auf einen Abbruch reagieren:

```bash
./build/bruce grid-resume --from-run teil.bruce --additional-queries 20 \
  --verification replay --execution-cancellation resume-import-checkpoints \
  --verification-cancellation grid-query-boundaries \
  --serialization-cancellation grid-record-checkpoints --output fortsetzung.bruce
```

Der neue Modus erreicht das Replay der bisherigen Evidenz und den anschließenden Wiederaufbau des Caches. Auch mehrstufige Fortsetzungsketten werden geprüft. Erst danach beginnen die neuen Abfragen.

Ohne Abbruch bleiben Cacheverhalten, Zähler, Ergebnisse und native Bundlebytes identisch. Der Sitzungsbericht verwendet V13. Der Vorgänger muss weiterhin ein gültiger, unvollständiger Lauf sein; Größen- und Generationsgrenzen bleiben erhalten.

Erkennt Bruce Strg+C während der Vorbereitung, endet die Sitzung mit `ExecutionResumeImportCancelled` in EXECUTING ohne neue Abfragen und ohne Export. Ein späterer nativer Abbruch während neuer Abfragen behält seine bisherige replaybare Abfragegrenze.

Dateieinlesen, Quellenhashes, Elternkopien und einzelne Grid-Auswertungen bleiben atomar. Replay und Serialisierung behalten ihre separaten Optionen. Eine maximale Abbruchlatenz wird nicht zugesagt.

## Weitere Roadmap

Die nativen Fortsetzungsimporte besitzen jetzt eigene Abbruchpunkte. Weitere Payloadfamilien, blockweises Kopieren großer Inhalte und gemeinsame Operationsbudgets bleiben offen. Dies ist kein vollständiger Serialisierungs- oder Architekturabschluss.

[Vertrag](132-execution-resume-import-contract.md) · [Abnahme](acceptance/execution-resume-import-v1-2026-09-23.md)
