# Abnahme Erfahrungs-Quellaudits / Core 0.58 – 23.09.2026

## Lieferung

Innerer Replaycallback durch Erfahrungsereignisse, exakte/Grid-Forschungsquellen und deren Auswertung. Bestehende Forschungs- und Z-Kompatibilitätsreplaymodi nutzen die Erweiterung ohne neue CLI-Option oder neue Bundleversion.

## Prüfungen

Der neue Regressionstest scheiterte vor Umsetzung an der fehlenden Callbackweiterleitung in den Quellaudit. Nach Umsetzung bestehen sieben fokussierte Gruppen. Der neue Unittest vergleicht die erreichten inneren Prüfpunkte mit dem eigenständigen Quellenreplay und erkennt dadurch eine fehlende Weiterleitung.

Direkte Erfahrungsprüfungen decken Hinzufügen, Ersetzen, Widerruf und Gridquellen ab. Jeder erreichte Callbackpunkt wird unterbrochen; ein geworfener `ReplayOperationLimit` bleibt im Original erhalten. Die mathematischen Forschungsreplays werden zusätzlich an jedem erreichten Punkt mit NumericLimit unterbrochen. Stimmen, Ketten und Auditzähler bleiben bei weiterlaufendem Callback identisch. Beschädigte Snapshotdigests werden vor der ersten Callbackabfrage abgelehnt; echte Quellenbudgetüberschreitungen behalten ihre bisherige Fehlerklassifikation.

Eine geprüfte Sitzung mit gespeichertem nativen Suchabbruch wird gezielt innerhalb des Quellaudits in REPLAYING unterbrochen. Der native Suchcallback wird dabei nicht erneut abgefragt. Das unabhängige Review findet keine konkreten Fehler und bestätigt 13 erfolgreiche Experience-/Replay-Testgruppen.

Vollständige Testsuiten: Debug 93/93 (70,10 Sekunden), UBSan 93/93 (117,70 Sekunden).

Zwölf Quellen-/Snapshot-/Forschungsfälle über Drei-, Vier- und Fünffamilienportfolios liefern identische Ergebnisse und bytegleiche Exporte gegenüber Core 0.57. Alle 26 historischen Fixtures bleiben im Replay unverändert. Die vollständige Demo besteht einschließlich eines getrennten Folgeauftrags, der tatsächlich eine Erfahrungsstimme verwendet. Ein erster Demoentwurf verwendete dieselben Trainingsdaten und wurde entsprechend der bestehenden Schutzregel korrigiert. Die Prüfung über 200 Markdown-Dateien findet keine ungültigen lokalen Links.

## Grenzen

Deterministische Abbruchtests belegen keine maximale Signallatenz. Einzelne Rechenoperationen, Hashing, Kopien, Hypothesenprüfung innerhalb einer Quellstrategie und Serialisierung bleiben atomar. Keine neue CLI-Abbruchoption für die Experience-Verwaltungsbefehle und keine neue Unterbrechung des nativen Erstlaufs. Kein vollständiger Architektur- oder Lernmeilensteinabschluss.

[Vertrag](../120-experience-replay-checkpoints-contract.md) · [Benutzung](../121-experience-replay-checkpoints-usage.md)
