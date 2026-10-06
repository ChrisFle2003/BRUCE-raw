# Blockweise Compatibility-Serialisierung

`pack_compatibility_records(run, digest_checkpoint, records_checkpoint)` kopiert die drei Payloadblöcke `rules_bytes`, `candidate_bytes` und `origin_bytes` in höchstens 64-KiB-Schritten.

`pack_application` und das gemeinsame Sitzungsbudget reichen den Callback für `CompatibilityRun` weiter. Die Bundleversion, Digestberechnung und Replayprüfung bleiben unverändert. Der Pfad ist eine begrenzte Kopierkontrolle und kein neuer mathematischer Beweis.
