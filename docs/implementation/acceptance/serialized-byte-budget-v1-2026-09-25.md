# Abnahme serialisiertes Sitzungsbudget / Core 0.68 – 25.09.2026

## Lieferung

Eigenständiges \`SerializationBudget\` für die Bundlegröße nach SERIALIZING. CLI-Option \`--max-serialized-bytes\`, Bericht V15 und Fehlerbericht V3. Keine Änderung der mathematischen Bundles oder Replayformate.

## Nachweise

Der neue CLI-Test scheiterte vor Umsetzung erwartungsgemäß an \`UnknownOption: --max-serialized-bytes\`. Nach Umsetzung bestehen sieben fokussierte Testgruppen einschließlich früherer Sitzungs-, Fortsetzungs- und Budgettests.

Der Test prüft exakte Grenze, Überschreitung in SERIALIZING, Fehlervertrag, tatsächliche Bytezahl, fehlenden Ergebnisbefund und fehlende Replayfreigabe. Ohne \`--verification replay\` wird die Option abgelehnt.

Das bestehende \`VerificationBudget\` blieb unverändert, damit alte C++-Aggregate und API-Aufrufe ohne neue Initialisierungswarnungen kompatibel bleiben. Das neue Budget ist ein eigener sechster Sitzungsparameter.

## Grenzen

Gemessen werden erzeugte Bundlebytes, nicht Speicherverbrauch, CPU-Zeit oder mathematische Operationen. Der Payloadaufbau vor der Prüfung bleibt atomar. Keine maximale Signallatenz, keine Prozess-RAM-Grenze und kein allgemeiner Abschluss von M0–M2.

[Vertrag](../141-serialized-byte-budget-contract.md) · [Benutzung](../142-serialized-byte-budget-usage.md)
