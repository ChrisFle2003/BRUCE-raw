# Core 0.53: Grid-Prüfung auch zwischen Abfragen abbrechen

Grid-Stapel und ihre Fortsetzungsketten können jetzt während der zusätzlichen Replayprüfung auf einen Abbruch reagieren:

```bash
./build/bruce grid-batch --paths '2,6;4,3;3,2' --max-query-steps 1 \
  --verification replay --verification-cancellation grid-query-boundaries \
  --output teil.bruce

./build/bruce grid-resume --from-run teil.bruce --additional-queries 1 \
  --verification replay --verification-cancellation grid-query-boundaries \
  --output fortsetzung.bruce
```

Ohne Abbruchsignal entstehen dieselben nativen Ergebnisse und Exporte wie bisher. Der Sitzungsbericht nennt V4 und den Modus `grid-query-boundaries`. Ein fachlicher Teilbefund bleibt ausdrücklich unvollständig.

Bei längeren Abläufen fordert Strg+C den Abbruch an. Bruce prüft die Anforderung jetzt zusätzlich zwischen Grid-Abfragen, beim Einlesen und Validieren der Pfade sowie beim Wiederaufbau des Caches in der Replayprüfung. Auch die Vorfahren einer Fortsetzungskette erhalten diese Prüfpunkte. Ein erkannter Abbruch endet mit `VerificationCancelled`, FAILED und ohne Export. Es wird kein verkürzter Replay als bestandene Prüfung ausgegeben.

Einzelne Rechenoperationen, Hashing, Serialisierung und Aufbau der Ergebnisdarstellung bleiben atomar. Die neue Option verspricht daher keinen sofortigen Stopp. Die erste Berechnung behält ihre bisherigen nativen Abbruchregeln; die neuen inneren Prüfpunkte betreffen die zusätzliche Replayprüfung.

Für andere Laufarten wird dieser Modus ausdrücklich abgelehnt. Dort kann weiterhin `--verification-cancellation phase-boundaries` verwendet werden. Ohne neue Option bleibt das bisherige Verhalten erhalten. `--max-replay-bytes` lässt sich weiterhin kombinieren.

## Weitere Roadmap

Interne Grid-Replaypunkte sind implementiert. Unterbrechbare mathematische Forschungs-Replays, unterbrechbare Serialisierung, gemeinsame Operationsbudgets und persistente Fortsetzung abgebrochener Prüfungen bleiben offen. Dies ist kein vollständiger Architektur- oder Meilensteinabschluss.

[Vertrag](110-grid-replay-checkpoints-contract.md) · [Abnahme](acceptance/grid-replay-checkpoints-v1-2026-09-23.md)
