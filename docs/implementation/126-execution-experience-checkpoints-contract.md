# Erfahrungsimporte im Erstlauf unterbrechen / Core 0.61

## Vertrag

`ExecutionControl::experience_cancelled` ist ein optionaler, vom nativen Suchcallback `cancelled` unabhängiger Callback. Er ist am gemeinsamen Auftragseinstieg nur für ExactResearchTask und GridResearchTask mit `planner=experience-order` zulässig. Andere Aufträge werden vor Callbackaufruf mit `ExecutionExperienceCheckpointsRequireLearnedResearch` abgelehnt.

Der Adapter übergibt den Callback als werfenden Erfahrungsprüfpunkt an den vorhandenen Importpfad. `false` setzt die Prüfung fort, `true` wirft `ExecutionExperienceCancelled`. Exceptions des Aufrufers behalten Typ und Nachricht, auch gegenüber mathematischen Fehlerbehandlungen und der Übersetzung echter Quellenbudgetfehler.

`research_from_grid` nimmt hierfür nach dem Gesamtbudget einen optionalen `experience_checkpoint` entgegen und reicht ihn ausschließlich an den Erfahrungsimport der exakten Forschung weiter. Die vorgelagerte Prüfung der Grid-Trainings-/Holdoutquellen erhält diesen Callback nicht. Die bisherige Reihenfolge von Zulassungs-, Importbudget- und Sampleprüfungen bleibt erhalten.

Ohne Callback bleibt der native Ablauf unverändert. Ein weiterlaufender Callback ändert weder Evidenz noch Stimmen, Auditkosten, Budgets oder Suchabbruchgrenzen. Der Import bleibt vor der eigentlichen Suche; auch eine gespeicherte Suchabbruchgrenze null überspringt ihn nicht. Ein gestoppter Quellaudit darf keinen neuen nativen Teilbefund erzeugen. Ein nach abgeschlossenem Import ausgelöster nativer Suchabbruch behält dagegen seine bisherigen Regeln.

## Sitzung und CLI

`--execution-cancellation experience-checkpoints` erfordert `--verification replay`. Erfolgreiche Sitzungen verwenden `application-execution-v10` und `execution_cancellation: experience-checkpoints`. Die bisherigen Replay-/Serialisierungsoptionen und das Replay-Bytebudget sind kombinierbar; deren Berichtsfelder bleiben erhalten. Native Bundleformate ändern sich nicht.

Ein Importabbruch im Erstlauf führt zu FAILED in EXECUTING, ohne SERIALIZING oder REPLAYING zu erreichen. Die Sitzung wird nicht zurückgegeben und der CLI-Export nicht ausgeführt. Der Ausführungs-Importcallback wird bei der späteren Replayprüfung nicht wiederverwendet; dafür bleiben die separaten VerificationControl-Einstellungen zuständig.

## Grenzen

Die neue Option unterbricht nur die Erfahrungsprüfung des Erstlaufs. Grid-Eingangsquellen vor dem Erfahrungsimport, Dateieinlesen, Hashing, Kopien, einzelne Rechenoperationen und die Hypothesenprüfung innerhalb einer Quellstrategie behalten ihre bisherigen Grenzen. Ohne zusätzlich gewählten Replay-/Serialisierungsmodus aktiviert diese Option dort keine neuen Abbruchpunkte. Keine maximale Signallatenz und kein neues gemeinsames Operationsbudget.

[Benutzung](127-execution-experience-checkpoints-usage.md) · [Abnahme](acceptance/execution-experience-checkpoints-v1-2026-09-23.md)
