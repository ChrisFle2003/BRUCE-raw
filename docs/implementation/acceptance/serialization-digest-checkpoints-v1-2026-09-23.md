# Abnahme äußere Bundle-Prüfsumme / Core 0.59 – 23.09.2026

## Lieferung

Optionale SHA-256-Abbruchpunkte vor 64-KiB-Blöcken und vor der Finalisierung. Alle acht nativen Bundle-Packer sind angebunden; Sitzungsmodus `--serialization-cancellation digest-checkpoints`, Erfolgsbericht V8, Abbruchzuordnung zu SERIALIZING.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung mit `UnknownOption: --serialization-cancellation`. Nach Umsetzung bestehen fünf gezielte Gruppen. Die neue Digest-Unit verwendet unabhängige SHA-256-Erwartungswerte für leere Eingabe, `abc`, exakt 64 KiB, 64 KiB plus ein Byte und 256 KiB binäre Daten einschließlich Nullbytes. Sie prüft jeden erreichbaren Abbruchpunkt, unveränderte Exceptiontypen sowie anschließende erneute Digestberechnung.

Die Adaptertests prüfen alle acht Auftragstypen: identische Bundlebytes bei weiterlaufendem Callback, sofortiger Abbruch an jedem beobachteten Digestpunkt, erfolgreiche V8-Sitzung und deterministischer Abbruch innerhalb SERIALIZING. Fehlende Callbackkonfiguration wird vor Ausführung abgelehnt. CLI-Tests prüfen Exportgleichheit, Kombination mit Grid-Replay sowie Ablehnung ungültiger Optionen und fehlender Replayprüfung.

Die vollständige Debug-Suite besteht mit 94/94 Testgruppen (67,85 Sekunden). Das unabhängige Review findet keine konkreten Fehler und bestätigt vier gezielte Testgruppen.

Elf zusätzliche Baselinefälle decken alle acht Auftragstypen, vollständige/unvollständige Ergebnisse, alle bisherigen Replaymodi, alle drei Vergleichsverfahren und das Replay-Bytebudget ab. Ausgaben ohne neue Option sind identisch zu Core 0.58; mit V8 sind native Ergebnisse und Bundlebytes weiterhin identisch. Alle 26 historischen Fixtures bleiben unverändert.

Die vollständige UBSan-Suite besteht ebenfalls mit 94/94 Testgruppen (118,55 Sekunden). Die vollständige Demo einschließlich V8-Export und anschließendem Replay besteht. Die Prüfung über 203 Markdown-Dateien findet keine ungültigen lokalen Links. Beide Builds melden keine Compilerwarnungen.

## Grenzen

Tests belegen kooperative Callback-Unterbrechung und unveränderte Prüfsummen, keine garantierte Signallatenz. Payloadaufbau, innere Prüfsummen und Kopien bleiben atomar. Kein vollständiger Serialisierungs- oder Architekturabschluss.

[Vertrag](../122-serialization-digest-checkpoints-contract.md) · [Benutzung](../123-serialization-digest-checkpoints-usage.md)
