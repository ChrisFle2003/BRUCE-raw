# Abnahme native Fortsetzungsimporte / Core 0.64 – 23.09.2026

## Lieferung

Getrennte Import- und Fortsetzungsprüfpunkte für Grid-Resume. Der neue Erstlaufcallback erreicht Vorgängerreplay und Cache-Präfixaufbau, aber keine neuen Abfragen. CLI-Modus `resume-import-checkpoints`, V13-Bericht und Importabbruch in EXECUTING.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung mit `UnknownExecutionCancellation`. Nach Umsetzung bestehen acht fokussierte Testgruppen.

Die neue Unit prüft jeden erreichten Importpunkt für direkte und mehrstufige Vorgänger, unterschiedliche zusätzliche Abfragemengen, Cache-Bypass sowie leere und abgebrochene Präfixe. Die Anzahl der Importabfragen muss dem Quellenreplay plus Cache-Präfixeinträgen entsprechen; neue Abfragen und spätere Sitzungsprüfung dürfen den Importcallback nicht wiederverwenden. Weiterlaufende Callbacks liefern identische Bundlebytes.

Jeder Importabbruch muss vor dem nativen Abfragecallback in EXECUTING scheitern. Exceptions am letzten Cache-Wiederaufbaupunkt behalten ihren Typ. Ein separater nativer Abbruch nach abgeschlossenem Import behält seinen replaybaren Teilbefund und die Anzahl bereits bearbeiteter Queries. Beschädigte Vorgänger, null zusätzliche Abfragen, vollständige Quellen, überschrittene Generationsgrenze und unpassende Aufgabentypen werden weiterhin abgelehnt.

CLI-Tests prüfen V13, Kombination mit Grid-Replay und Recordserialisierung, bytegleiche Exporte sowie Ablehnung fehlender Replayprüfung und falscher Aufgaben. Das unabhängige Review meldet keine relevanten Fehler und bestätigt zehn gezielte Testgruppen.

Vollständige Testsuiten: Debug 99/99 (65,76 Sekunden), UBSan 99/99 (117,79 Sekunden). Elf Baselinefälle über alle acht Auftragstypen und bisherigen Replaymodi bleiben unverändert zu Core 0.63, ebenso alle 26 historischen Fixtures. Die vollständige Demo einschließlich V13-Fortsetzung, erhaltenem Cache-Hit und Replay besteht.

## Grenzen

Deterministische Callbacktests belegen keine maximale Signallatenz. Quellenhashes, Elternkopien, einzelne Cache-/Grid-Auswertungen und Dateieinlesen bleiben atomar. Kein neues gemeinsames Operationsbudget und kein vollständiger Architekturabschluss.

[Vertrag](../132-execution-resume-import-contract.md) · [Benutzung](../133-execution-resume-import-usage.md)
