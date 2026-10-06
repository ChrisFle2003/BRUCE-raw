# Core 0.59: Bundle-Prüfsumme während der Erzeugung abbrechen

Die äußere Prüfsumme eines geprüften Exports kann jetzt blockweise auf einen Abbruch reagieren:

```bash
./build/bruce grid-batch --paths '2,6;4,3' \
  --verification replay \
  --serialization-cancellation digest-checkpoints \
  --verification-cancellation grid-query-boundaries \
  --output stapel.bruce
```

Die neue Option gilt für alle acht Auftragstypen. Sie ergänzt den bisherigen Replaymodus: Im Beispiel kann Strg+C sowohl während der äußeren Prüfsummenberechnung als auch an den inneren Grid-Replaygrenzen erkannt werden. Ohne `--verification-cancellation` bleiben im Replay die Phasengrenzprüfungen aktiv.

Ohne Abbruch entstehen dieselben mathematischen Ergebnisse und dieselben nativen Bundlebytes wie bisher. Der Sitzungsbericht verwendet V8 und nennt die zusätzliche Unterbrechungsmöglichkeit. Die Option kann mit `--max-replay-bytes` kombiniert werden.

Bei einem Abbruch während der äußeren Prüfsummenberechnung endet die Sitzung mit `VerificationCancelled` in SERIALIZING und ohne Export. Die Verarbeitung wird vor jedem Block von höchstens 64 KiB und vor dem Abschluss der Prüfsumme auf einen Abbruch geprüft.

Der Aufbau des Inhalts vor dieser Prüfsumme, verschachtelte Prüfsummen und das anschließende Zusammenfügen bleiben ununterbrechbar. Es gibt daher keine Zusage eines sofortigen Stopps oder einer maximalen Wartezeit.

## Weitere Roadmap

Ein Teil der Evidenzerzeugung ist nun unterbrechbar. Schrittweiser Payloadaufbau, weitere innere Prüfsummen, Unterbrechung des Erstlaufs und gemeinsame Operationsbudgets bleiben offen. Dies ist kein vollständiger Architekturabschluss.

[Vertrag](122-serialization-digest-checkpoints-contract.md) · [Abnahme](acceptance/serialization-digest-checkpoints-v1-2026-09-23.md)
