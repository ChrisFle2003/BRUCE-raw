# Core 0.52: Zusätzliche Prüfung an Phasengrenzen abbrechen

Die gemeinsame Prüfsitzung kann jetzt optional auf Abbruchsignale reagieren:

```bash
./build/bruce research --train examples/affine-training.csv \
  --verification replay --verification-cancellation phase-boundaries \
  --output geprueft.bruce
```

Bei längeren Läufen fordert Strg+C den Abbruch an. Bruce prüft die Anforderung vor der Serialisierung, vor dem Replay und nach dem Replayvergleich vor der Freigabe. Wird sie erkannt, endet die Sitzung mit `VerificationCancelled`, terminalem FAILED und Exitcode 2. Es wird kein Ergebnisbundle exportiert.

Ein Signal während einer laufenden Serialisierung oder Replayberechnung wird erst an deren nächster Prüfgrenze berücksichtigt. Die Option verspricht deshalb keinen sofortigen Stopp. Nach der letzten Grenze besteht ebenfalls keine Garantie mehr, dass ein Signal die Ausgabe verhindert.

Ohne Signal entsteht der normale geprüfte Befund. Der Sitzungsbericht verwendet V3 und nennt `verification_cancellation: "phase-boundaries"`. Das native Exportformat bleibt unverändert. Die Option lässt sich mit `--max-replay-bytes` kombinieren.

Ohne die neue Option oder mit `--verification-cancellation none` bleibt das bisherige Verhalten bestehen. Insbesondere kann ein nativ abgebrochener Suchlauf weiterhin als INCOMPLETE-Befund replaygeprüft und exportiert werden. Mit aktivierten Prüfgrenzen beendet dieselbe Signalanforderung anschließend auch die Prüfsitzung und verhindert diesen Export.

## Weitere Roadmap

Abbruchpunkte zwischen atomaren Phasen sind implementiert. Unterbrechung innerhalb von Replay und Serialisierung, begrenzte Abbruchlatenz, gemeinsame Operationsbudgets, persistente Fortsetzung und allgemeine Phasengraphen bleiben offen. FAILED bezeichnet hier einen gestoppten Ablauf, keine mathematische Aussage über das untersuchte Problem.

[Vertrag](108-verification-cancellation-contract.md) · [Abnahme](acceptance/verification-cancellation-v1-2026-09-23.md)
