# Abnahme Z-Kompatibilitätsreplay / Core 0.57 – 23.09.2026

## Lieferung

Optionaler innerer Replaycallback für Z-Kompatibilität einschließlich Int64-, exakter und Grid-Forschungsursprünge; CLI-Modus `compatibility-checkpoints` und Erfolgsbericht V7. Native Auswahlregeln und V1-Bundles bleiben erhalten.

## Prüfungen

Vor Umsetzung scheiterte der neue CLI-Test mit `UnknownVerificationCancellation`. Nach Umsetzung bestehen alle fünf gezielten Compatibility-/Sitzungstestgruppen. Ein anfänglich ungültiger Testaufbau mit gleichzeitiger Gridquelle und expliziten Trainingssamples wurde nach der erwarteten Ablehnung korrigiert.

Der neue Unittest prüft vollständige Auswahl, Kandidaten-/Prüfungs-/Tracebudgets, native Abbrüche bei null und einem Kandidaten sowie alle drei Forschungsursprungstypen. An jedem erreichten Callbackpunkt wird eine NumericLimit-Exception ausgelöst und auf unmittelbare Weitergabe im Originaltyp geprüft. Ein zusätzlicher Vergleich mit den inneren Replayaufrufzahlen erkennt fehlende Weiterleitung an Forschungsursprünge. Manipulierte Payloads mit neuem Digest und veränderte Forschungsprojektionen werden weiterhin abgelehnt.

Die Sitzung wird deterministisch am ersten Replay-Kandidaten abgebrochen: FAILED in REPLAYING, getrennte native Callbackzahl, kein Rückgaberesultat. Fehlender Callback und Konflikte mit jedem der drei anderen inneren Modi werden abgelehnt. CLI-Tests prüfen vollständige/unvollständige V7-Ergebnisse, bytegleiche Exporte, Replay und Ablehnung eines falschen Aufgabentyps.

Die vollständige Debug-Suite besteht mit 92/92 Testgruppen (64,86 Sekunden). Das unabhängige Review meldet keine konkreten Fehler und bestätigt für acht Budgetkombinationen identische Ergebnisse und bytegleiche Exporte gegenüber Core 0.56.

Die vollständige UBSan-Suite besteht ebenfalls mit 92/92 Testgruppen (117,41 Sekunden). Nach Ergänzung der strengeren Weiterleitungsassertion besteht der betroffene Unittest erneut in Debug und UBSan. Die vollständige Demo einschließlich des neuen V7-Falls besteht. Alle 26 historischen Fixtures liefern exakt dieselben Replayausgaben wie Core 0.56. Die Linkprüfung über 197 Markdown-Dateien findet keine ungültigen lokalen Ziele.

## Grenzen

Deterministische Callbacktests belegen kooperative Unterbrechung, keine garantierte Signallatenz. Einzelne Kandidatenbewertungen, Einlesen, Hashing, Serialisierung und der native Forschungsimport bleiben innerhalb ihrer bisherigen Grenzen atomar. Kein vollständiger Meilensteinabschluss.

[Vertrag](../118-compatibility-replay-checkpoints-contract.md) · [Benutzung](../119-compatibility-replay-checkpoints-usage.md)
