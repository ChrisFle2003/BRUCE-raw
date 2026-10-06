# Z-Kompatibilitätsreplay: innere Prüfpunkte / Core 0.57

## Umfang

`replay_compatibility(bundle, checkpoint={})` unterstützt einen werfenden `ReplayCheckpoint`. Der typisierte Replayadapter reicht ihn weiter. Ohne Callback bleiben die bisherigen nativen Ergebnisse und V1-Bundles erhalten.

Prüfpunkte liegen nach der Digestprüfung, vor dem Snapshotaufbau, an jeder Forschungs-Hypothesenprojektion, vor der Kandidatenauswertung, an deren vorhandenen Kandidatengrenzen und vor dem abschließenden Payloadvergleich. Eingebettete Int64-, exakte und Grid-Forschungsreplays erhalten denselben Callback und nutzen ihre bestehenden inneren Prüfpunkte.

Der native `run_compatibility` verwendet weiterhin keinen Replaycallback. Ein interner gemeinsamer Helfer vermeidet eine zweite Implementierung der Auswahl. Der Beobachter an Kandidatengrenzen liefert immer `false` oder wirft: Er setzt keine neue native Abbruchgrenze. Eine gespeicherte Abbruchgrenze kann wie bisher eine spätere Kandidatenabfrage verhindern. Eine nachgelagerte Abfrage erkennt die Prüfungsunterbrechung trotzdem, bevor ein Replayresultat zurückgegeben wird.

## Sitzung und CLI

`VerificationControl::compatibility_replay_checkpoints` erfordert einen Abbruchcallback und einen `CompatibilityTask`. Alle vier inneren Replaymodi sind gegenseitig exklusiv; Fehlkonfigurationen scheitern in EXECUTING. Die CLI verwendet `--verification replay --verification-cancellation compatibility-checkpoints`.

Ein erfolgreicher Sitzungsbericht verwendet `application-execution-v7` und `verification_cancellation: compatibility-checkpoints`. Frühere Modi behalten ihre Versionen. Der native Export bleibt `BRUCE_COMPATIBILITY_BUNDLE_V1`. Das Replay-Bytebudget bleibt kombinierbar.

Ein angeforderter Abbruch wirft `VerificationCancelled`; die Sitzung endet in FAILED an der erreichten Phase und exportiert nichts. Callbackexceptions werden weitergegeben. Die vorhandenen geschützten Forschungsobserver erhalten ihren ursprünglichen Typ auch gegenüber mathematischen Fehlerbehandlungen. Snapshotdigest, Forschungsprojektion und vollständiger Payloadvergleich werden weiterhin geprüft.

## Grenzen

Regel-/Kandidateneinlesen, eine einzelne Kandidatenbewertung mit ihren Regeln, Auswahlabschluss, einzelne mathematische Operationen, Hashing und Serialisierung bleiben atomar. Experience-Quellaudits haben weiterhin keine durchgehend inneren Prüfpunkte. Es gibt keine garantierte Abbruchlatenz und kein gemeinsames phasenübergreifendes Operationsbudget. Der native Erstlauf prüft seinen Forschungsursprung weiterhin ohne diesen zusätzlichen Replaycallback. Eine erfolgreiche Auswahl ist kein allgemeiner Wahrheitsbeweis.

[Benutzung](119-compatibility-replay-checkpoints-usage.md) · [Abnahme](acceptance/compatibility-replay-checkpoints-v1-2026-09-23.md)
