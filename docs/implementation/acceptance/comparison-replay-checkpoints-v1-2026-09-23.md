# Abnahme Bruchvergleichsreplay-Checkpoints / Core 0.56 – 23.09.2026

## Lieferung

Interne Replay-Prüfpunkte für die drei nativen Bruchvergleichsverfahren. Der bestehende geschützte Observer erhält Originalexceptions und setzt keine neue native Abbruchgrenze. CLI-Modus `comparison-checkpoints`, Erfolgsbericht V6; vorherige Modi und native Bundleformate bleiben erhalten.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung am unbekannten Modus. Alle elf fokussierten Vergleichs-/Session-/Adapter-/Replaygruppen bestehen. Die neue Unit prüft 18 Grundfälle über die drei Vergleichsverfahren: gleiche/verschiedene Werte, Definitionslücken, kubische Restpolynome und leere Definitionsbereiche, zusätzlich Operationsgrenzen und native Abbrüche.

Jeder erreichbare Callbackpunkt dieser Fälle wird mit einer NumericLimit-Exception unterbrochen; der Originaltyp muss erhalten bleiben. Eine ExpressionNormalizationLimit-Exception am ersten Rechencheckpoint prüft denselben Schutz gegen die zweite mathematische Fehlerbehandlung. Manipulierte Payloads mit neuem Digest müssen weiterhin am vollständigen Evidenzvergleich scheitern.

Ein Sitzungsfall bricht beim ersten Rechencheckpoint innerhalb des Replay ab, ohne den nativen Callback erneut abzufragen. Fehlende Callbacks und widersprüchliche innere Modi werden abgelehnt. Die CLI-Integration prüft V6, Replay-Bytebudget, unveränderte native Exporte in allen drei Verfahren und Typablehnung bei Einzel-Grid.

Die vollständige Debug-Suite besteht mit 91/91 Testgruppen (64,53 Sekunden), ebenso die UBSan-Suite mit 91/91 Testgruppen (117,60 Sekunden). Die vollständige Demo einschließlich des neuen V6-Sitzungsfalls besteht.

Die Kompatibilitätsprüfung gegen Core 0.55 bestätigt für 19 Strategie-/Planerfälle unveränderte Ergebnisse, bytegleiche Exporte und historisches Replay. Alle 26 historischen Fixtures liefern ihre gespeicherten Ergebnisse. Die Prüfung der 194 Markdown-Dateien findet keine ungültigen lokalen Links.

Das unabhängige Code-Review meldet keine konkreten Fehler im Lieferumfang. Zusätzlich geprüfte 120 Baseline-Vergleiche sind bytegleich; 30 Exporte mit dem neuen V6-Sitzungsmodus entsprechen exakt den jeweiligen nativen V1–V3-Bundles.

## Grenzen

Interne Unterbrechung wird mit deterministischen Callbacks geprüft. Keine garantierte Signallatenz, keine Unterbrechung innerhalb einzelner Arithmetikoperationen oder der Serialisierung. Kein vollständiger Meilensteinabschluss.

[Vertrag](../116-comparison-replay-checkpoints-contract.md) · [Benutzung](../117-comparison-replay-checkpoints-usage.md)
