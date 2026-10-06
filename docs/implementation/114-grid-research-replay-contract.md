# Unterbrechbarer zusammengesetzter Grid-Forschungsreplay – Vertrag V1

Core 0.55 erweitert `research-checkpoints` auf Grid-Forschungsaufträge. Der bestehende Prüfcallback wird durch Quellenreplay, Projektion, mathematischen Replay und Bindungsprüfung weitergereicht. Native Abbruchpositionen, Quellenidentitäten und Kostenregeln bleiben erhalten.

## Schnittstellen und Ablauf

`replay_grid_source` erhält einen optionalen `ReplayCheckpoint` und reicht ihn an GridBatch- oder GridResume-Replay weiter. `replay_grid_research` erhält denselben optionalen Callback als vierten Parameter hinter Operationslimit und Forschungszulassung. `replay_application` unterstützt ihn nun ebenfalls für GridResearch V1–V4.

Zusätzliche Prüfpunkte liegen nach den erfolgreichen Digestprüfungen des äußeren und gegebenenfalls inneren Forschungsbundles, in den Grid-Quellprüfungen einschließlich Fortsetzungsketten, zwischen projizierten Grid-Abfragen, im mathematischen Replay und zwischen den Vergleichen von Quellen- und Forschungs-Samples. Vor den abschließenden Payloadvergleichen wird ebenfalls geprüft.

Ein normal zurückkehrender Callback setzt die vollständige Evidenzprüfung fort. Eine Exception beendet den Replay ohne Ergebnisrückgabe. Die vorhandenen Grid- und Forschungsreplays bewahren den Originaltyp des Callbackfehlers; ein Prüfungsabbruch wird nicht in einen neuen nativen INCOMPLETE-Befund umgewandelt.

## Grenzen und Zulassung bleiben wirksam

Die Reihenfolge der fachlichen Schranken bleibt erhalten:

- Quellen-/Experience-Bytebudget wird vor der Quellenprojektion zugelassen.
- Der bestehende `admit_research`-Hook kann die eingebettete Forschung vor dem Quellenreplay ablehnen.
- Das Operationslimit und die Prüfung der V4-Experiencebindung erfolgen vor der mathematischen Suche.
- Die projizierten Gridwerte müssen mit den gespeicherten Lern- und Prüfwerten übereinstimmen.
- Der vollständige Payloadvergleich bleibt erforderlich.

V1/V2-Achsenprojektion, V3-Quellenkosten und V4-Experiencekosten werden nicht verändert. Historische V3-Bundles mit Erfahrungsplanung behalten ausdrücklich ihre frühere Kostenregel ohne zusätzliche Experience-Bytebelastung. Die Steuerungs-Callbacks werden nicht als neue mathematische Operationen in bestehende Evidenzzähler eingetragen.

## Sitzung und CLI

`VerificationControl.research_replay_checkpoints=true` unterstützt nun zusätzlich `GridResearchTask`. In der CLI gilt derselbe Modus:

`--verification replay --verification-cancellation research-checkpoints`

Bei einem erkannten Abbruch innerhalb des zusätzlichen Replay meldet die Sitzung `VerificationCancelled`, REPLAYING und FAILED. Sie erreicht nicht READY und startet keinen Export. Erfolgreiche Aufrufe verwenden weiterhin `application-execution-v5` mit `research-checkpoints`; es wird kein neues Erfolgsformat benötigt. Die Erweiterung fügt einen zuvor abgelehnten Auftragstyp hinzu. Native Bundles und ältere Modi bleiben unverändert.

## Verbleibende atomare Bereiche

Die erste native Grid-Forschung einschließlich ihres Quellenimports verwendet weiterhin die bisherigen nativen Checkpoints. Die neuen Punkte gelten für die zusätzliche Prüfsitzung bzw. direkte Bibliotheks-Replayaufrufe mit Callback.

Erfahrungs-Quellaudits innerhalb der Strategierangfolge erhalten hier noch keine zusätzlichen inneren Prüfpunkte. Einzelarithmetik, Hashing, Blockkopien, Payloadaufbau und Serialisierung bleiben atomar. Der Modus garantiert keine maximale Abbruchlatenz und erzeugt keinen Wiederaufnahmepunkt. Einzel-Grid, Bruchvergleich und Z-Kompatibilität bleiben außerhalb dieses Modus. Kein vollständiger Meilensteinabschluss.

[Benutzung](115-grid-research-replay-usage.md) · [Abnahme](acceptance/grid-research-replay-v1-2026-09-23.md)
