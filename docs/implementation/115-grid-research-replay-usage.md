# Core 0.55: Grid-Forschung durchgängig im Replay abbrechen

Die Prüfsitzung einer Grid-Forschung kann jetzt dieselbe Abbruchanforderung durch ihre Quellen und die mathematische Suche weiterreichen:

```bash
./build/bruce grid-batch --paths '0,2;1,2;2,2' --output training.bruce
./build/bruce grid-batch --paths '3,2' --output holdout.bruce
./build/bruce research --grid-train training.bruce --grid-holdout holdout.bruce \
  --domain rational --strategy polynomial --planner interpolate --max-degree 2 \
  --verification replay --verification-cancellation research-checkpoints \
  --output forschung.bruce
```

Bruce prüft die Grid-Quellen, bildet daraus Lern-/Prüfpaare und reproduziert die Formelforschung. Der Callback ist jetzt auch in den Quellenreplays, deren Fortsetzungsketten und der Zuordnungsprüfung aktiv. Strg+C wird an den vorhandenen Prüfpunkten berücksichtigt. Ein erkannter Abbruch beendet die Prüfsitzung mit FAILED und ohne Export.

Ohne Abbruchsignal entstehen dieselben nativen Ergebnisse und Bundlebytes wie bisher. Der Bericht bleibt V5 mit `research-checkpoints`. X-/Y-Projektion, Quellen- und Experiencebudgets sowie gespeicherte Teilbefunde behalten ihre Bedeutung. Auch vollständig fortgesetzte Gridquellen werden unterstützt; unvollständige Quellen bleiben unzulässig.

Die zusätzliche Prüfung kann damit innerhalb ihrer zusammengesetzten Arbeit anhalten. Einzeloperationen, Hashing, Serialisierung und interne Erfahrungs-Quellaudits bleiben weiterhin atomar. Ein sofortiger Stopp wird nicht zugesagt. Ohne die Option bleibt das bisherige Verhalten erhalten.

## Weitere Roadmap

Der zusammengesetzte Grid-Forschungsreplay ist angebunden. Weitere Prüffamilien, unterbrechbare Erfahrungs-Quellaudits und Serialisierung sowie allgemeine Operationsbudgets bleiben offen. Diese Lieferung schließt M0–M2 nicht pauschal ab.

[Vertrag](114-grid-research-replay-contract.md) · [Abnahme](acceptance/grid-research-replay-v1-2026-09-23.md)
