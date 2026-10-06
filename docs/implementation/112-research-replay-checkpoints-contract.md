# Forschungsreplay mit Suchcheckpoints – Vertrag V1

Core 0.54 erweitert die internen Replay-Prüfpunkte auf Int64-Forschung und exakte Formelforschung einschließlich Strategien und Portfolios. Der Prüfcallback bleibt von den gespeicherten nativen Abbruchgrenzen getrennt und erzeugt keinen neuen Teilbefund.

## Replay und Suchalgorithmus

`replay` nimmt einen optionalen `ReplayCheckpoint` an. `replay_exact_research` erhält ihn als vierten optionalen Parameter hinter dem unveränderten Operationslimit und der Auftragszulassung. `replay_application` leitet den Callback an diese Forschungsreplays oder die bereits unterstützten Grid-Stapel-/Fortsetzungsreplays weiter.

Prüfpunkte liegen nach erfolgreicher Digestprüfung, zwischen dem Einlesen der Lern-/Prüfpaare, vor der Forschung, an ihren vorhandenen Suchcheckpoints und nach der Forschung vor dem vollständigen Evidenzvergleich. Die bisherigen Limits und die Auftragszulassung werden weiterhin vor der eigentlichen exakten Forschung geprüft.

Die vorhandenen Fachalgorithmen erhalten bei gesetztem Replaycallback einen Observer, der nach normaler Callback-Rückkehr immer false liefert. Er fordert damit niemals einen nativen Suchabbruch an. Gespeicherte `cancel_after_candidates`-/`cancel_after_checkpoints`-Grenzen bleiben wirksam. Ohne Replaycallback bleibt auch der Observer leer.

Callbackexceptions werden beim Eintritt in den Fachalgorithmus in einer privaten Weitergabehülle gehalten und unmittelbar am Forschungsausgang als ursprüngliche Exception erneut geworfen. Dadurch können numerische Fehlerbehandlungen beispielsweise eine vom Callback geworfene `NumericLimit` nicht als normalen mathematischen Grenzfall verschlucken. Bei Unterbrechung kehrt kein teilweise geprüfter Forschungsbefund zurück.

## Sitzung und CLI

`--verification replay --verification-cancellation research-checkpoints` ist für die Int64- und exakten Forschungsaufträge verfügbar. Die native Ausführung bleibt erhalten; die zusätzliche Replayprüfung fragt an ihren inneren Prüfpunkten die bestehende SIGINT-/SIGTERM-Anforderung ab. Ein erkannter Abbruch führt zu `VerificationCancelled`, Sitzungsphase REPLAYING, terminalem FAILED und keinem Export.

In der Bibliothek aktiviert `VerificationControl.research_replay_checkpoints=true` diesen Modus und verlangt einen Prüfcallback. Gleichzeitige Grid- und Forschungsmodi werden mit `ConflictingReplayCheckpoints` abgelehnt. Nicht passende Auftragstypen werden vor der nativen Ausführung zurückgewiesen. Ein fehlender Callback wird nicht stillschweigend ignoriert.

Erfolgreiche Sitzungen verwenden `application-execution-v5` und `verification_cancellation: "research-checkpoints"`. Die bisherigen Erfolgsfelder und ein optionales Replay-Bytebudget bleiben erhalten. Ohne neuen Modus bleiben V1–V4 und die nativen Exportformate unverändert. Der Modus ist nicht im Bundle persistiert.

## Grenzen

Die Prüfpunkte folgen den bestehenden Suchalgorithmen; ihre Anzahl ist keine allgemeine Kostenmetrik. Einzelne Rechen-/Planungsoperationen, Digestberechnung, Kopieren und Serialisierung bleiben atomar. Erfahrungsprüfung und darin geschachtelte Quellreplays erhalten in diesem Paket keine zusätzlichen inneren Prüfpunkte. Abbruchlatenz ist deshalb nicht allgemein begrenzt.

Grid-Forschung als zusammengesetzter Auftrag, Einzel-Grid, Bruchvergleich und Z-Kompatibilität unterstützen diesen Modus nicht. Grid-Stapel/Fortsetzungen behalten ihren eigenen Modus. Der eigenständige CLI-Befehl `replay` erhält keine neue Option. Keine Wiederaufnahme abgebrochener Prüfungen, kein allgemeines Operationsbudget und kein vollständiger Meilensteinabschluss.

[Benutzung](113-research-replay-checkpoints-usage.md) · [Abnahme](acceptance/research-replay-checkpoints-v1-2026-09-23.md)
