# Lokale menschliche Regelprüfung – Core 0.93

Stand: 26.09.2026. Grundlage ist [ADR-013](../adr/013-human-rule-authority.md). [Historischer Text](05-human-rule-approval-history.md).

| Vertrag | Wirkung | Bindung / Grenze |
| --- | --- | --- |
| `rules-propose` / `rules-approve` | Vorschlag und nach Terminalbestätigung exklusiver Export | Beide kanonischen Fassungen und wörtlicher Ausgabepfad; Basis erneut geprüft, aber keine gemeinsame Dateitransaktion und kein Journal |
| `rules-stage` / `rules-activate` / `rules-status` | Verwalteter aktiver numerischer Speicher | Zielpfad, Generation, Journalbasis, beide Fassungen; unter Schreibsperre erneut geprüft und gemeinsam atomar ersetzt |
| `compatibility --rule-store` | Aktive Fassung einmal in Auftrag übernehmen | Bundle bewahrt Regelbytes und Digest; keine Journalgeneration im mathematischen Bundle |
| `compatibility-plan` / `compatibility-apply` | Inhaltsgebundene Auswahlübergabe | Quelle, kanonische Regeln, Stelle und Budgets; keine Aktivierung und keine generationengebundene Zustimmung |

Im verwalteten Weg werden nur bestätigte Wechsel gespeichert. Initialisierung und Rückwechsel sind neue Entscheidungen. Ein konkurrierender bestätigter Wechsel macht einen wartenden Vorschlag ungültig, auch wenn später wieder dieselben Regelbytes aktiv sind. Dagegen bindet ein Auswahlplan absichtlich den Regelinhalt: nach Rückkehr zu gleichen Bytes kann er erneut verwendet werden.

Regelaktivierung schreibt einen gemeinsamen Journal-/Zustandsinhalt mit Dateisynchronisierung, atomarem Ersatz und anschließender Verzeichnissynchronisierung. Scheitert die letzte Synchronisierung, kann der Wechsel bereits erfolgt sein; zuerst Status prüfen. Leser sehen die alte oder neue vollständige Datei. Die Sperre wirkt zwischen kooperierenden Prozessen; sie verhindert keine direkten Eingriffe anderer Programme.

Es gibt keinen automatischen Standardspeicher, keine Signatur und keinen Identitätsnachweis. Direkte Regeldateiaufrufe bleiben möglich. Bei normalen Exportfehlern wird keine vorhandene Zieldatei überschrieben; nach bereits erfolgtem atomarem Ersatz ist eine allgemeine Zusage „Fehler bedeutet unverändert“ unzutreffend.

Verbindliche Bedien-/Formatdetails: [Einzeldateiexport](../cli/rule-review.md), [aktiver Speicher](../cli/active-rules.md), [Z-Auswahlübergabe](../cli/compatibility-transfer.md). Noch offen sind zusätzliche Regelarten, Journalmigration/-rotation, expliziter Vorschlagswiderruf und Protokollierung von Ablehnungen. Getrennte Konten und Signierschlüssel sind keine Voraussetzung des vereinbarten lokalen Profils.
