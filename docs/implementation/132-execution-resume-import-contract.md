# Native Fortsetzungsimporte unterbrechen / Core 0.64

## Vertrag

`ExecutionControl::resume_import_cancelled` ist ein optionaler Importcallback ausschließlich für GridResumeTask. Andere Aufgaben werden vor Callbackaufruf mit `ExecutionResumeImportRequiresResumeTask` abgelehnt.

`resume_grid_batch` akzeptiert nach dem nativen Abbruchcallback einen optionalen `import_checkpoint`. Er wird an das Replay der bisherigen Grid-Evidenz einschließlich verschachtelter Fortsetzungen und an den Wiederaufbau des verifizierten Cache-Präfixes weitergereicht. Die danach neu auszuführenden Abfragen erhalten diesen Callback nicht.

Intern sind Import- und Fortsetzungsprüfpunkte getrennt. Das bestehende Fortsetzungsreplay nutzt wie zuvor denselben Replaycallback für beide Bereiche. Der native Erstlauf verwendet den neuen Callback nur für Vorgängerprüfung und Cacheaufbau. Der bisherige boolesche Suchcallback bleibt für neue Abfragen zuständig.

`false` setzt fort; `true` wird im Adapter zu `ExecutionResumeImportCancelled`. Exceptions des Aufrufers werden unverändert weitergegeben. Ein Importabbruch startet keine neuen Abfragen und erzeugt keinen neuen nativen Teilbefund. Native Abbrüche nach abgeschlossenem Import behalten ihre bisherigen gespeicherten Abfragegrenzen.

## Invarianten

Eine positive und zulässige Zahl zusätzlicher Abfragen, Bundlegrößen und Generationsgrenzen werden weiterhin geprüft. Der Vorgänger muss vollständig replaybar und selbst INCOMPLETE sein. Digestfehler und bereits vollständig abgeschlossene Quellen werden weiterhin abgelehnt.

Ein weiterlaufender Importcallback verändert weder Cache-Hits/-Misses/-Bypasses, Audit-/Neuberechnungszähler, Trace noch Bundlebytes. Die Quellenprüfung stellt den Cache aus dem verifizierten Präfix wieder her. Die neue Callbacktrennung darf diese Reihenfolge nicht verändern.

## Sitzung und CLI

`--execution-cancellation resume-import-checkpoints` erfordert `--verification replay` und `grid-resume`. Erfolgreiche Sitzungsberichte verwenden `application-execution-v13` und nennen den neuen Ausführungsmodus. Frühere Berichte und native Bundleformate bleiben unverändert.

Ein Importabbruch führt zu FAILED in EXECUTING; Serialisierung, Sitzungsreplay und CLI-Export werden nicht erreicht. Der Importcallback wird nicht bei der nachgelagerten Sitzungsprüfung wiederverwendet. Replay-, Grid-Record-/Digestmodus und Replay-Bytebudget bleiben separat kombinierbar.

## Grenzen

Quellenhashes, Elternkopien, einzelne Grid-/Cache-Auswertungen und Dateieinlesen bleiben atomar. Prüfpunkte liegen zwischen Cache-Präfixeinträgen, nicht innerhalb ihrer Berechnung. Keine maximale Signallatenz und kein neues gemeinsames Operationsbudget. Die gespeicherten Importdaten werden nicht verändert oder überschrieben.

[Benutzung](133-execution-resume-import-usage.md) · [Abnahme](acceptance/execution-resume-import-v1-2026-09-23.md)
