# Abnahme Gridquellen im Erstlauf / Core 0.62 – 23.09.2026

## Lieferung

Separater Grid-Importcallback für Grid-Forschung einschließlich fortgesetzter Quellen und Sampleprojektion. Kombinierter Grid-/Erfahrungsmodus, V11-Bericht, Quellenabbruch in EXECUTING ohne nativen Teilbefund.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung mit `UnknownExecutionCancellation`. Nach Umsetzung bestehen sieben fokussierte Gruppen sowie beide separat ausgeführten CLI-Importtests.

Die neue Unit prüft jeden erreichten Quellen-/Projektionspunkt für X- und Y-Projektion, Gesamtbudget und fortgesetzte Trainingsquellen. Weiterlaufende Callbacks müssen identische Bundlebytes liefern und dürfen nicht in die Forschung oder spätere Sitzungsprüfung gelangen. An jedem Punkt führt ein angeforderter Abbruch zu EXECUTING-Fehlern, bevor der native Suchcallback startet. NumericLimit-Exceptions des Aufrufers bleiben erhalten.

Unzureichendes Gesamtbudget scheitert vor der ersten Quellenabfrage. Unvollständige Quellen und überlappende Trainings-/Holdoutwerte werden auch mit Callback abgelehnt. Im kombinierten Modus beendet ein Gridabbruch den Auftrag vor dem Erfahrungsimport; ein Erfahrungsabbruch beendet ihn vor der Suche. Erfolgreiche kombinierte Aufträge erreichen alle drei Phasen in der vorgesehenen Reihenfolge.

Die CLI prüft Einzel- und Kombinationsmodus, V11, bytegleiche Exporte und Ablehnung eines unpassenden Aufgabentyps. Die vollständige Debug-Suite besteht mit 97/97 Testgruppen (65,69 Sekunden). Das unabhängige Review findet keine relevanten Befunde, bestätigt fünf gezielte Testgruppen sowie unveränderte Ausgaben und Exporte gegenüber Core 0.61.

Elf weitere Baselinefälle über alle acht Auftragstypen und bisherigen Replaymodi bleiben identisch zu Core 0.61; ebenso alle 26 historischen Fixtures.

Die vollständige UBSan-Suite besteht ebenfalls mit 97/97 Testgruppen (117,20 Sekunden). Die vollständige Demo einschließlich V11-Gridimport und anschließendem Replay besteht. Die Linkprüfung über 212 Markdown-Dateien findet keine ungültigen lokalen Ziele. Beide Builds melden keine Compilerwarnungen.

## Grenzen

Deterministische Callbacktests belegen keine maximale Signallatenz. Einzeloperationen, Hashing, Kopien und Dateieinlesen bleiben innerhalb ihrer bisherigen Grenzen. Andere native Quellenimporte und gemeinsame Operationsbudgets bleiben offen. Kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](../128-execution-grid-source-checkpoints-contract.md) · [Benutzung](../129-execution-grid-source-checkpoints-usage.md)
