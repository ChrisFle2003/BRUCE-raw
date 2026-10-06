# Abnahme gemeinsames Prüfpunktbudget / Core 0.65 – 23.09.2026

## Lieferumfang

Optionales gemeinsames Prüfpunktkonto über Ausführung, passende native Importe, Serialisierung und Replay aller acht Auftragstypen. CLI `--max-session-checkpoints` mit Replayprüfung. Erfolgsbericht V14 und Fehlerbericht V2 mit Gesamt- und Phasenverbrauch. Kein Export nach Budgetfehler; bestehende native Teilbefunde bleiben bei ausreichendem Sitzungsbudget erhalten.

Die Architekturübersicht wurde auf den aktuellen Code konsolidiert. Die M0–M2-Matrix benennt vorhandene Teilverträge und offene Gesamtabnahmen.

## Nachweise

Die beiden neuen CLI-Tests scheiterten vor der Implementierung erwartungsgemäß an `UnknownOption: --max-session-checkpoints`. Nach Umsetzung bestehen zwölf fokussierte Gruppen.

Die Unit-Matrix prüft acht Auftragstypen, direkte und mehrstufige Fortsetzungen, alle drei Vergleichsverfahren, manuelle und forschungsbasierte Z-Kandidaten sowie verschachtelte Grid-/Erfahrungsquellen. Unabhängig instrumentierte vorhandene Callbacks dienen als Zählreferenz. In den kleinen Fällen wird jedes unzureichende Budget geprüft; größere verschachtelte Fälle prüfen ausgewählte Grenzen. Exakt ausreichendes Budget erhält das Bundle. Phasensummen, Fehlerhistorie, unabhängige Signalcallbacks, native Teilbefunde, kopierte Budgetzulassung und numerische Exception-Typen werden geprüft.

Die CLI prüft 0, exakte Grenzen, UInt64-Maximum, ungültige Werte, fehlende Replayprüfung, Phasenabrechnung, unveränderte Exportbytes und Schutz einer vorhandenen Zieldatei bei Fehlern.

Das unabhängige Review fand keine handlungsrelevanten Fehler. Es bestätigte zusätzlich 20 CLI-Baselinevergleiche, 15 budgetierte Ergebnisvergleiche und gezielte numerische Fehlerinjektionen einschließlich verschachtelter Importe.

Vollständige Suiten: **Debug 101/101** (246,71 Sekunden), **UBSan 101/101** (451,41 Sekunden; `-fsanitize=undefined -fno-sanitize-recover=all`). Beide Builds ohne Compilerwarnungen/-fehler. Die Laufzeiten enthalten gleichzeitige Prüfprozesse und sind keine Performanceabnahme.

Elf eigene Baselinefälle über alle acht Auftragstypen und bisherigen Replaymodi bleiben zu Core 0.64 in JSON und Bundlebytes unverändert. Dieselben Fälle mit ausreichendem Sitzungsbudget liefern ebenfalls identische native Befunde und Exporte. Alle 26 historischen Fixtures werden unverändert replayt.

Die vollständige Demo einschließlich V14 besteht: der neue Grid-Stapel verbraucht 22 Punkte (3 Ausführung, 9 Serialisierung, 10 Replay); mit 21 Punkten erfolgt der terminale Fehler vor READY. 223 Markdown-Dateien geprüft, keine ungültigen lokalen Links.

## Grenzen

Gezählt werden Prüfpunkte, keine universellen mathematischen Operationen. Die verbleibenden atomaren Arbeiten, Prozess-RAM, CPU-Zeit, Einlesen vor der Sitzung und Export nach READY sind dadurch nicht allgemein begrenzt. Keine maximale Signallatenz, keine allgemeine Forschungsfortsetzung, keine Release- oder vollständige M0–M2-Abnahme.

[Vertrag](../134-session-work-budget-contract.md) · [Benutzung](../135-session-work-budget-usage.md) · [Abnahmematrix](../136-m0-m2-acceptance-matrix.md)
