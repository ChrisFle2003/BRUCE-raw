# Unterbrechbare äußere Bundle-Prüfsumme / Core 0.59

## Vertrag

`sha256(value, checkpoint={})` verwendet ohne Callback weiterhin die bisherige einmalige Digestberechnung. Mit Callback wird ein eigener OpenSSL-Digestkontext angelegt und per RAII auch bei Exceptions freigegeben. Die Eingabe wird in Blöcken bis 65.536 Bytes verarbeitet. Der Callback wird vor jedem Block und vor der Finalisierung abgefragt; bei leerer Eingabe einmal vor der Finalisierung. Exceptions werden unverändert weitergegeben.

Alle acht nativen Bundle-Packer und `pack_application` akzeptieren denselben optionalen Callbacktyp. Sie reichen ihn ausschließlich an die äußere Bundle-Prüfsumme weiter. Der Payloadaufbau, darin verschachtelte Packer/Digests und das Zusammenfügen des finalen Bundles bleiben atomar. Für weiterlaufende Callbacks müssen Digest und Bundlebytes exakt dem bisherigen Format entsprechen.

Bestehende direkte C++-Aufrufe bleiben durch Defaultargumente gültig. Die Funktionszeigertypen der erweiterten Funktionen enthalten jedoch den zusätzlichen Parameter; Aufrufe über solche Zeiger müssen ihn ausdrücklich übergeben. Es wird keine binäre ABI-Kompatibilität zugesagt.

## Geprüfte Sitzung

`VerificationControl::serialization_digest_checkpoints` ist unabhängig von den gegenseitig exklusiven inneren Replaymodi. Es erfordert einen Abbruchcallback; eine fehlende Funktion wird in EXECUTING mit `SerializationCheckpointsRequireCallback` abgelehnt.

Die CLI-Option `--serialization-cancellation digest-checkpoints` erfordert `--verification replay`. Sie aktiviert die bestehenden Phasengrenzabfragen sowie die äußere Prüfsummenabfrage in SERIALIZING. Sie lässt sich mit den bisherigen `--verification-cancellation`-Modi und dem Replay-Bytebudget kombinieren. Ohne expliziten inneren Replaymodus bleiben im Replay nur die Phasengrenzen aktiv.

Erfolgreiche Berichte verwenden `application-execution-v8` und `serialization_cancellation: digest-checkpoints`. Das Feld `verification_cancellation` nennt weiterhin den gewählten Replaymodus beziehungsweise `phase-boundaries`. Frühere Berichte ohne die neue Option und sämtliche native Bundleformate bleiben unverändert.

Ein Abbruch an den neuen Prüfpunkten erzeugt `VerificationCancelled`, FAILED in SERIALIZING und keinen Sitzungsrückgabewert. Der CLI-Export wird somit nicht erreicht. Abbruch- und Budgetfehler in späteren Phasen behalten ihre bisherige Zuordnung.

## Grenzen

Dies ist ein erster unterbrechbarer Teil der Evidenzerzeugung, keine vollständig unterbrechbare Serialisierung. Payload, innere Prüfsummen, Rendern, Speicherallokationen und Kopien bleiben atomar. Die Blockgröße begrenzt Bytes zwischen den neuen Prüfpunkten, nicht deren Laufzeit. Kein neues Speicherbudget, kein gemeinsames Operationsbudget und keine maximale Signallatenz. Replay-Digestprüfungen bleiben unverändert.

[Benutzung](123-serialization-digest-checkpoints-usage.md) · [Abnahme](acceptance/serialization-digest-checkpoints-v1-2026-09-23.md)
