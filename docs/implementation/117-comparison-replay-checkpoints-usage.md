# Core 0.56: Bruchvergleich während der Replayprüfung abbrechen

Auch die zusätzliche Prüfung eines exakten Bruchvergleichs kann jetzt innerhalb der Berechnung auf einen Abbruch reagieren:

```bash
./build/bruce compare --left-numerator '-1,0,1' --left-denominator '-1,1' \
  --right-numerator '1,1' --right-denominator '1' --domain-policy modular-witness \
  --verification replay --verification-cancellation comparison-checkpoints \
  --output vergleich.bruce
```

Die Koeffizienten stehen in aufsteigender Potenzreihenfolge. Der Vergleich berücksichtigt weiterhin die ursprünglichen Definitionslücken; ein Kürzen entfernt sie nicht aus der Aussage.

Ohne Abbruchsignal erzeugt Bruce denselben mathematischen Befund und dieselben nativen Bundlebytes wie bisher. Der Sitzungsbericht verwendet V6 und nennt `comparison-checkpoints`. Der Modus gilt für alle drei Vergleichsverfahren und lässt sich mit `--max-replay-bytes` kombinieren.

Bei längeren Abläufen fordert Strg+C den Abbruch an. Die zusätzliche Replayprüfung erkennt ihn an den vorhandenen Rechencheckpoints und beendet die Sitzung mit `VerificationCancelled`, FAILED und ohne Export. Sie gibt weder einen neuen Teilbefund noch eine Aussage über Gleichheit oder Verschiedenheit aus.

Einzelne Rechenoperationen, Koeffizienteneinlesen und Serialisierung bleiben atomar. Ein sofortiger Stopp wird nicht zugesagt. Ohne die neue Option bleiben die bisherigen Abbruch- und Exportregeln erhalten.

## Weitere Roadmap

Der Bruchvergleichsreplay ist intern angebunden. Z-Kompatibilitätsreplay, Erfahrungs-Quellaudits, unterbrechbare Serialisierung und gemeinsame Operationsbudgets bleiben offen. Dies ist kein vollständiger Architekturabschluss.

[Vertrag](116-comparison-replay-checkpoints-contract.md) · [Abnahme](acceptance/comparison-replay-checkpoints-v1-2026-09-23.md)
