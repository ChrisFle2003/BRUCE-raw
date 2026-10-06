# Grid-Replay mit internen Abbruchpunkten – Vertrag V1

Core 0.53 erweitert die zusätzliche Prüfsitzung für Grid-Stapel und Grid-Fortsetzungen um Abbruchpunkte innerhalb des Replay. Die Prüfspur und die gespeicherten nativen Suchgrenzen bleiben unverändert. Dies ist der erste begrenzte intern unterbrechbare Replaypfad, kein allgemeiner unterbrechbarer Forschungsreplay.

## Gemeinsamer Prüfeinstieg

`ReplayCheckpoint` ist ein optionaler `std::function<void()>`. Ein normal zurückkehrender Callback lässt die vollständige Prüfung weiterlaufen. Eine Exception beendet den Aufruf, ohne einen teilweise geprüften Befund zurückzugeben.

`replay_grid_batch`, `replay_grid_resume` und `replay_application` nehmen diesen zusätzlichen optionalen Parameter an. Beim gemeinsamen Einstieg sind mit gesetztem Callback ausschließlich GridBatch V1/V2 und GridResume V1 unterstützt. Andere Typen werden mit `ReplayCheckpointsUnsupported` abgelehnt; der Callback wird nicht stillschweigend ignoriert. Ohne Callback bleiben bisherige Dispatchregeln und Replayformate erhalten.

## Prüfpunkte

Beim Stapelreplay wird geprüft:

- nach erfolgreicher Digestprüfung;
- zwischen dem Einlesen der Pfade;
- zwischen den Eingangsgrößenprüfungen und gegebenenfalls den ohnehin notwendigen Pfadvalidierungen;
- vor jeder Wiederberechnung einer Abfrage bzw. Prüfung ihrer gespeicherten Ausführungsgrenze;
- vor dem abschließenden Vergleich von Aufgabe, Trace und Ergebnis.

Beim Fortsetzungsreplay wird der Callback durch alle geprüften Vorfahren weitergereicht. Er wird außerdem nach der Digestprüfung jeder Generation, zwischen den Cache-Wiederaufbauschritten, vor fortgesetzten Abfragen und vor dem abschließenden Evidenzvergleich aufgerufen. Die bisherige Generationsgrenze bleibt gültig.

Der Prüfcallback ist getrennt von den nativen booleschen Abbruchcallbacks. Er setzt insbesondere keine neue `cancel_after_queries`-Grenze und erzeugt keinen neuen INCOMPLETE-Befund. Gespeicherte Abbruchpositionen und Cacheereignisse werden weiterhin vollständig reproduziert. Anzahl und Reihenfolge der Callback-Aufrufe sind kein persistierter Evidenzvertrag und keine Operationskosteneinheit.

## Geprüfte Sitzung und CLI

`VerificationControl.grid_replay_checkpoints=true` aktiviert den Pfad für `GridBatchTask` und `GridResumeTask` und verlangt einen vorhandenen `cancelled`-Callback. Fehlender Callback oder ein anderer Auftragstyp führen vor der nativen Ausführung zu einem typisierten Sitzungsfehler.

In der CLI aktiviert `--verification replay --verification-cancellation grid-query-boundaries` sowohl die bisherigen drei Phasengrenzen als auch die internen Grid-Replaypunkte. Die vorhandene SIGINT-/SIGTERM-Anforderung wird dort abgefragt. Ein erkannter Abbruch wirft `VerificationCancelled`; innerhalb des Replay enthält der Sitzungsfehler `failed_at: REPLAYING` und endet in FAILED. Es gibt weder Freigabe noch Export.

Ein erfolgreicher Bericht verwendet `application-execution-v4` und `verification_cancellation: "grid-query-boundaries"`. Ein optionales Replay-Bytebudget bleibt erhalten und wird vor dem Replay geprüft. Die Modi `none` und `phase-boundaries`, V1–V3-Berichte sowie native Bundlebytes bleiben unverändert.

## Grenzen

Digestberechnung, Speicherallokationen und Kopien, einzelne Gridberechnungen, Aufbau/Vergleich kompletter Payloads und Serialisierung besitzen weiterhin keine inneren Prüfpunkte. Eine Abbruchanforderung kann deshalb warten; eine maximale Reaktionszeit wird nicht zugesagt. Die erste native Ausführung einschließlich ihres Quellenreplay und Cacheaufbaus nutzt weiterhin ihre bisherigen Kontrollpunkte. Die neuen zusätzlichen Punkte gelten für den nachgeschalteten Grid-Replay der Prüfsitzung.

Grid-Forschung, Einzel-Grid, Formelforschung, Bruchvergleich und Z-Kompatibilität unterstützen diesen neuen Modus nicht. Für sie bleibt die optionale Abbruchprüfung an Phasengrenzen verfügbar. Der eigenständige CLI-Befehl `replay` erhält in diesem Paket keine neue Option. Unterbrechung bewahrt keinen Wiederaufnahmepunkt; es gibt keine Aussage über die mathematische Wahrheit eines abgebrochenen Befunds.

[Benutzung](111-grid-replay-checkpoints-usage.md) · [Abnahme](acceptance/grid-replay-checkpoints-v1-2026-09-23.md)
