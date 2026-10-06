# Core 0.57: Z-Kompatibilität während der Replayprüfung abbrechen

Die zusätzliche Prüfung einer Z-Auswahl kann jetzt auch innerhalb ihres Ablaufs auf einen Abbruch reagieren:

```bash
./build/bruce compatibility --rules regeln.txt --candidates kandidaten.csv \
  --verification replay --verification-cancellation compatibility-checkpoints \
  --output auswahl.bruce
```

Für aus Forschung abgeleitete Kandidaten gilt derselbe Modus:

```bash
./build/bruce compatibility --rules regeln.txt --from-run forschung.bruce --at 2 \
  --verification replay --verification-cancellation compatibility-checkpoints \
  --output auswahl.bruce
```

Ohne Abbruch entstehen dieselben Auswahlbefunde und nativen Exporte wie bisher. Der Sitzungsbericht verwendet V7. Auch unvollständige native Befunde werden unverändert reproduziert; eine erfolgreiche Wiederholungsprüfung macht sie nicht vollständig.

Strg+C fordert den Abbruch an. Während der Replayprüfung wird er an Kandidatengrenzen und bei eingebetteter Forschung an deren bestehenden Prüfpunkten erkannt. Die Sitzung endet dann mit `VerificationCancelled`, FAILED und ohne Export. Ein Prüfungsabbruch liefert keine neue Auswahl und keinen neuen Teilbefund.

Einzelne Kandidatenbewertungen, Einlesen, Hashing und Serialisierung bleiben atomar. Der native Erstlauf besitzt weiterhin seine bisherigen Abbruchgrenzen; insbesondere wird sein Forschungsimport durch diese Option nicht intern unterbrechbar. Ein sofortiger Stopp wird nicht zugesagt.

## Weitere Roadmap

Die Z-Kompatibilitätsprüfung ist an die inneren Replayprüfpunkte angeschlossen. Erfahrungs-Quellaudits, unterbrechbare Serialisierung und gemeinsame Operationsbudgets bleiben offen. Dies ist kein vollständiger Architekturabschluss.

[Vertrag](118-compatibility-replay-checkpoints-contract.md) · [Abnahme](acceptance/compatibility-replay-checkpoints-v1-2026-09-23.md)
