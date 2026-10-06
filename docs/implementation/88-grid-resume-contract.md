# Grid-Fortsetzung V1 – Vertrag

Core 0.42 setzt unvollständige Grid-Stapel mit unverändertem Auftrag fort. Eingabe ist ein replaygültiges GridBatch-V2- oder GridResume-V1-Bundle und eine Zahl zusätzlicher Abfragen von 1 bis 1024. Vollständige Quellen werden abgewiesen. Die ursprüngliche Liste, Reihenfolge, GridSpec und Cachegrenze bleiben erhalten.

## Ausführung und Herkunft

1. Gesamte Elternquelle einschließlich ihrer Herkunft replayen.
2. Den FIFO-Cache durch Auswertung ihres bereits geprüften Präfixes rekonstruieren.
3. Die nächsten höchstens `additional_queries` Abfragen mit diesem Cache ausführen.
4. Elternquelle vollständig einbetten und neue Grenze, Trace und Ergebnis exportieren.

Die alte Endmarke wird durch die neue ersetzt. Frühere QUERY-Ereignisse und Ergebnisse bleiben erhalten. Das neue absolute Schrittende ist `min(Anzahl aller Pfade, bisher bearbeitet + zusätzliche Abfragen)`. Ein früherer Abbruch wird aufgehoben. Ein neuer Callback kann vor jeder neuen Abfrage abbrechen, auch unmittelbar vor dem ersten neuen Schritt. Die CLI verwendet SIGINT/SIGTERM. Quellenprüfung und Cacheaufbau sind atomar; ein dort eintreffendes Signal wird an der nächsten Abfragegrenze berücksichtigt.

## Zähler

`batch.evaluations` zählt Potenzberechnungen für die logische Gesamtsequenz mit ihrem Cache. `new_evaluations` zählt nur die beim neuen Suffix hinzugekommenen Berechnungen. Cachetreffer und lokale Stopps sind keine neuen Potenzberechnungen.

`audit_evaluations` zählt die tatsächlichen Potenzberechnungen dieses Aufrufs für Eltern-Replay und Cacheaufbau. Bei einer nativen Quelle sind das zweimal ihre `evaluations`. Bei einer fortgesetzten Quelle sind es deren `audit_evaluations + new_evaluations + batch.evaluations`. Insgesamt fallen in diesem Aufruf `audit_evaluations + new_evaluations` Potenzberechnungen an. Hashing, Pfadprüfung, Cachezugriffe, Kopieren und Export sind darin nicht enthalten. Die Zähler sind weder Laufzeit- noch RAM-Grenzen.

## Export und Replay

`BRUCE_GRID_RESUME_BUNDLE_V1` verwendet die feste Policy `verified-prefix-cache-rebuild-v1`. Nach SHA-256 stehen zusätzliche Abfragezahl, gespeicherte neue Abbruchgrenze und bytegenau eingerahmte Elternquelle. Anschließend folgen Trace und JSON-Ergebnis. Replay rekonstruiert denselben Ablauf und vergleicht den vollständigen kanonischen Inhalt. Ein neu berechneter Digest allein legitimiert keine geänderten Ergebnisse oder Metadaten.

Maximal acht Fortsetzungen werden ineinander eingebettet. Eine neunte wird zurückgewiesen. Es gelten weiterhin 32 MiB pro Bundle, 1024 Pfade und höchstens 1 MiB logisches Cachebudget. Export überschreibt keine bestehende Datei. Die API verarbeitet nur replaygeprüfte Elternquellen; der interne Cacheaufbau setzt diesen geprüften Zustand voraus.

## Forschung und Grenzen

Vollständige Fortsetzungen werden als Grid-Forschungsquellen akzeptiert, mit den bisherigen Achsen-, Exaktheits-, Stichproben- und 2-MiB-Quellgrenzen. Unvollständige Fortsetzungen bleiben unzulässig. Das Forschungsbudget berechnet weiterhin die vollständigen Quellbytes als logische Importkosten; es ist kein Zähler der rekursiven Potenzberechnungen.

Dies ist eine begrenzte Wiederaufnahme für Grid-Stapel. Allgemeine Phasensteuerung, fortsetzbare Formelsuche, persistente Cache-Snapshots ohne Rekonstruktion und Gesamtprojektabschluss bleiben offen.

[Benutzung](89-grid-resume-usage.md) · [Abnahme](acceptance/grid-resume-v1-2026-09-22.md)
