# Forschungsquellen der Z-Auswahl im Erstlauf / Core 0.63

## Vertrag

`ExecutionControl::compatibility_source_cancelled` ist ein optionaler Importcallback für CompatibilityTask mit Forschungsursprung. Andere Aufgabentypen scheitern mit `ExecutionCompatibilitySourceRequiresCompatibilityTask`; ein fehlender Forschungsursprung wird mit `ExecutionCompatibilitySourceRequiresOrigin` abgelehnt. Beide Ablehnungen erfolgen vor dem Aufruf des Importcallbacks.

`run_compatibility` akzeptiert einen optionalen dritten Parameter `source_checkpoint`. Er nutzt die bereits vorhandenen Vorbereitungspunkte: vor dem Snapshotaufbau, innerhalb des Quellenreplays, vor jeder Hypothesenprojektion und vor der abschließenden Kandidatenbewertung. Unterstützte Quellen sind Int64-, exakte und Grid-Forschung einschließlich ihrer eingebetteten Erfahrungsnachweise. Der boolesche native Kandidatenabbruchcallback bleibt separat.

`false` setzt den Import fort; `true` wird im gemeinsamen Adapter zu `ExecutionCompatibilitySourceCancelled`. Exceptions des Aufrufers behalten ihren Typ. Ein abgebrochener Quellaudit liefert keine neue Auswahl oder einen neuen nativen Teilbefund. Native Kandidatenabbrüche nach abgeschlossenem Import behalten ihre bisherigen Regeln.

## Evidenz und Sitzung

Ein weiterlaufender Callback verändert weder Regeln, projizierte Kandidaten, Auswahl, Budgets noch native Bundlebytes. Quellenreplay, Vollständigkeitsgate, Projektionsgleichheit und Int64-Projektionsgrenzen bleiben erhalten. Ein gespeicherter Kandidatenabbruch null überspringt den vorgeschalteten Quellenimport nicht.

`--execution-cancellation compatibility-source-checkpoints` erfordert `--verification replay` und `compatibility --from-run`. Eine erfolgreiche Sitzung verwendet `application-execution-v12` sowie `execution_cancellation: compatibility-source-checkpoints`. Bestehende Berichte ohne den neuen Callback und native Bundleformate bleiben unverändert. Replay-, Digest- und Replay-Bytebudgetoptionen sind separat kombinierbar.

Ein Importabbruch beendet die Sitzung in EXECUTING mit FAILED. Kandidatenbewertung, Serialisierung, Sitzungsreplay und CLI-Export werden nicht mehr erreicht. Der Ausführungs-Importcallback wird nicht für die spätere Replayprüfung wiederverwendet.

## Grenzen

Ein Snapshotaufbau, eine Hypothesenprojektion, einzelne Rechenoperationen, Hashing, Kopien und Dateieinlesen bleiben atomar. Die Abbruchpunkte begrenzen nicht die Laufzeit dieser Einzeloperationen. Manuelle Kandidaten besitzen keinen Forschungsimport und unterstützen diesen Modus nicht. Native Kandidatenbewertung, Replay und Serialisierung behalten ihre getrennten Abbruchmechanismen. Kein gemeinsames Operationsbudget und kein vollständiger Erstlaufabschluss.

[Benutzung](131-execution-compatibility-source-usage.md) · [Abnahme](acceptance/execution-compatibility-source-v1-2026-09-23.md)
