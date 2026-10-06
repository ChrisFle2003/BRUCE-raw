# Abnahme Erfahrungsimport im Erstlauf / Core 0.61 – 23.09.2026

## Lieferung

Separater Ausführungs-Importcallback für erfahrungsbasierte exakte und Grid-Forschung. CLI-Modus `--execution-cancellation experience-checkpoints`, V10-Bericht, Importabbruch in EXECUTING ohne neuen mathematischen Teilbefund.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung mit `UnknownOption: --execution-cancellation`. Nach Umsetzung bestehen sieben fokussierte Gruppen. Der neue Unittest prüft an jedem erreichten Importpunkt vollständige Forschung, gespeicherten Suchabbruch null, Ersatznachweise und Grid-Forschung mit und ohne Gesamtbudget.

Bei jedem Importabbruch muss die Sitzung FAILED in EXECUTING melden; der native Suchcallback darf noch nicht aufgerufen worden sein. Ein weiterlaufender Importcallback liefert dieselben Bundlebytes wie ohne Callback und wird während der zusätzlichen Replayprüfung nicht wiederverwendet. NumericLimit- und ReplayOperationLimit-Exceptions des Aufrufers bleiben unverändert. Ein separater Suchabbruch nach dem Import liefert weiterhin den nativen Cancelled-Teilbefund. Unpassende Aufgaben werden vor Aufruf des Importcallbacks abgelehnt.

Die CLI prüft einen unabhängigen Folgeauftrag mit tatsächlich verwendeter Erfahrungsstimme, V10, Kombination mit Forschungsreplay und Digestmodus sowie identische native Exporte. Fehlende Replayprüfung und nicht erfahrungsbasierte Forschung werden abgelehnt.

Die vollständige Debug-Suite besteht mit 96/96 Testgruppen (66,45 Sekunden). Das unabhängige Review bestätigt neun erfolgreiche Testgruppen und findet keine funktionalen Fehler. Eine doppelte CLI-Hilfezeile wurde bereinigt.

Die vollständige UBSan-Suite besteht ebenfalls mit 96/96 Testgruppen (118,54 Sekunden). Zwölf Quellen-/Snapshot-/Forschungsfälle über drei, vier und fünf Strategiefamilien bleiben identisch zu Core 0.60; sechs darin enthaltene vollständige/unvollständige Forschungsfälle wurden zusätzlich mit V10 und bytegleichen nativen Exporten geprüft. Alle 26 historischen Fixtures bleiben unverändert. Die vollständige Demo besteht. Die Prüfung über 209 Markdown-Dateien findet keine ungültigen lokalen Links. Nach Bereinigung der Hilfezeile wurden beide Builds aktualisiert und deren Hilfeausgabe geprüft.

## Grenzen

Deterministische Callbacktests belegen keine maximale Signallatenz. Vorgelagerte Gridquellen, Dateieinlesen, einzelne Operationen, Hashing und Kopien behalten ihre bisherigen Grenzen. Replay und Serialisierung benötigen weiterhin ihre eigenen optionalen Prüfpunkte. Kein allgemeines Lernen und kein vollständiger Erstlauf-/Architekturabschluss.

[Vertrag](../126-execution-experience-checkpoints-contract.md) · [Benutzung](../127-execution-experience-checkpoints-usage.md)
