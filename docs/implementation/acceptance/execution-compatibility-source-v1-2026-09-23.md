# Abnahme Forschungsimport der Z-Auswahl / Core 0.63 – 23.09.2026

## Lieferung

Separater Quellenimportcallback für Z-Kompatibilität mit Forschungsursprung. Native Kandidatenbewertung bleibt unabhängig. CLI-Modus `compatibility-source-checkpoints`, V12-Erfolgsbericht, Quellenabbruch in EXECUTING ohne neuen Teilbefund.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung mit `UnknownExecutionCancellation`. Nach Umsetzung bestehen acht fokussierte Testgruppen.

Die neue Unit prüft klassische, exakte und Grid-Forschungsquellen sowie erfahrungsbasierte exakte und Grid-Forschung. An jedem erreichten Vorbereitungspunkt wird abgebrochen; die Sitzung muss in EXECUTING scheitern, bevor der native Kandidatencallback aufgerufen wird. Weiterlaufende Callbacks liefern identische Bundlebytes und werden bei der zusätzlichen Sitzungsprüfung nicht erneut verwendet. NumericLimit-Exceptions bleiben erhalten. Eine gespeicherte Kandidatenabbruchgrenze null überspringt den Import nicht.

Nach abgeschlossenem Import behält ein nativer Kandidatenabbruch seinen Cancelled-Teilbefund. Widersprüchliche Projektionen, unvollständige Forschungsquellen, manuelle Kandidaten ohne Ursprung und falsche Aufgabentypen werden abgelehnt. CLI-Tests prüfen V12, die Kombination mit Replay-/Digestmodus, bytegleiche Exporte und fehlenden Export bei falscher Konfiguration.

Die vollständige Debug-Suite besteht mit 98/98 Testgruppen (65,89 Sekunden). Das unabhängige Review meldet keine relevanten Befunde, bestätigt sechs fokussierte Gruppen und einen bytegleichen Vergleich von Standardausgabe und Bundle gegen Core 0.62.

Elf weitere Baselinefälle über alle acht Auftragstypen und bisherigen Replaymodi bleiben unverändert; ebenso alle 26 historischen Fixtures.

Die vollständige UBSan-Suite besteht ebenfalls mit 98/98 Testgruppen (118,49 Sekunden). Die vollständige Demo einschließlich V12-Quellenimport und anschließendem Replay besteht. Die Prüfung über 215 Markdown-Dateien findet keine ungültigen lokalen Links. Beide Builds melden keine Compilerwarnungen.

## Grenzen

Deterministische Abbruchtests belegen keine maximale Signallatenz. Einzelne Projektionen, Snapshotaufbau, Hashing, Kopien und Dateieinlesen bleiben atomar. Native Kandidatenbewertung und nachgelagerte Prüfphasen behalten getrennte Abbruchmechanismen. Kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](../130-execution-compatibility-source-contract.md) · [Benutzung](../131-execution-compatibility-source-usage.md)
