# Core 0.54: Formelforschung während der Replayprüfung abbrechen

Die zusätzliche Prüfung mathematischer Forschung kann jetzt auch an den Suchcheckpoints auf einen Abbruch reagieren:

```bash
./build/bruce research --train examples/affine-training.csv \
  --holdout examples/affine-holdout.csv --domain rational \
  --verification replay --verification-cancellation research-checkpoints \
  --output geprueft.bruce
```

Der Modus gilt für Int64- und exakte Forschung, einschließlich Potenz-, Polynom-, Bruchfunktions-, Ausdrucks- und Portfoliostrategien. Ohne Signal erzeugt Bruce dieselben nativen Ergebnisse und Bundlebytes wie bisher. Der Sitzungsbericht nennt V5 und `research-checkpoints`.

Strg+C fordert bei längeren Abläufen den Abbruch an. Während des zusätzlichen Replay erkennt Bruce ihn an den vorhandenen Suchcheckpoints und beendet die Sitzung mit FAILED und `VerificationCancelled`. Es wird keine Ergebnisdatei geschrieben. Die Prüfung liefert keinen neuen verkürzten Suchlauf als geprüften Befund.

Ein gespeicherter INCOMPLETE-Befund bleibt dagegen bei ungestörter Prüfung unverändert reproduzierbar. Sein damaliger Suchabbruch wird nicht mit dem aktuellen Prüfungsabbruch vermischt.

Einzelne Rechenoperationen, Erfahrungsimporte und Serialisierung bleiben atomar. Der Modus verspricht daher keine sofortige Reaktion. Ohne ihn bleibt das bisherige Verhalten erhalten; `--max-replay-bytes` lässt sich weiterhin kombinieren.

## Weitere Roadmap

Interne Replaypunkte für die eigenständige mathematische Forschung sind implementiert. Zusammengesetzte Grid-Forschung, Erfahrungs-Quellaudits, weitere Prüffamilien, unterbrechbare Serialisierung und gemeinsame Operationsbudgets bleiben offen. Diese Lieferung schließt M0–M2 nicht pauschal ab.

[Vertrag](112-research-replay-checkpoints-contract.md) · [Abnahme](acceptance/research-replay-checkpoints-v1-2026-09-23.md)
