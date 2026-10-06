# Core 0.51: Prüfdaten vor dem Replay begrenzen

Die zusätzliche Replayprüfung kann jetzt ein eigenes Größenlimit erhalten:

```bash
./build/bruce grid-batch --paths '2,6;4,3' --max-query-steps 1 \
  --verification replay --max-replay-bytes 10000 --output teil.bruce
```

Bruce berechnet und serialisiert den Befund. Passt das vollständige Bundle in die angegebenen 10000 Bytes, führt er die Replayprüfung durch und exportiert nach deren Erfolg. Der Bericht zeigt unter `execution.verification_budget` das Limit und die tatsächliche Bundlegröße. Der Befund bleibt wegen der einen ausgeführten Abfrage INCOMPLETE.

Ein bewusst zu kleines Budget lässt sich so vorführen:

```bash
./build/bruce grid --path 2,6 --verification replay --max-replay-bytes 0
```

Die Berechnung und Serialisierung gelingen, aber die Zulassungsprüfung stoppt in REPLAYING mit FAILED. Die Ursache `ReplayByteBudgetExceeded` nennt Größe und Limit. Es wird kein Ergebnisbundle exportiert; vorhandene Zieldateien bleiben erhalten. Das gilt auch ohne `--output`.

Ohne Größenlimit bleibt der bisherige Ablauf erhalten. Die Option ist nur zusammen mit `--verification replay` gültig. Ein Budget exakt in Größe des Bundles genügt; ein Byte weniger wird abgelehnt.

## Grenzen und weitere Roadmap

Dies ist eine Grenze für die Prüfdaten, nicht für Rechenzeit oder Gesamtspeicher. Insbesondere ist die Serialisierung bereits erfolgt, bevor die Bundlegröße geprüft wird. Ein zugelassener Replay läuft weiterhin vollständig. Gemeinsame Operationsbudgets, unterbrechbare Prüfphasen und allgemeine Phasengraphen bleiben offen; die Lieferung schließt M0–M2 nicht pauschal ab.

[Vertrag](106-replay-byte-budget-contract.md) · [Abnahme](acceptance/replay-byte-budget-v1-2026-09-23.md)
