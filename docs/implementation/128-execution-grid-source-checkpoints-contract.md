# Grid-Eingangsquellen im Erstlauf unterbrechen / Core 0.62

## Vertrag

`ExecutionControl::grid_source_cancelled` ist ein optionaler, von Such- und Erfahrungsimportcallback unabhängiger Callback. Am gemeinsamen Auftragseinstieg wird er ausschließlich für GridResearchTask akzeptiert. Andere Aufgaben scheitern vor Callbackaufruf mit `ExecutionGridSourceCheckpointsRequireGridResearch`.

`research_from_grid` reicht den neuen optionalen `grid_checkpoint` an die vorhandene Quellenprüfung und Sampleprojektion weiter. Das Quellenreplay erreicht damit auch eingebettete Fortsetzungsketten. Trainings- und Holdoutwerte werden an ihren bestehenden Projektionspunkten geprüft. Der Callback bleibt auf diese Eingangsphase beschränkt; er wird weder an die eigentliche Forschung noch an die spätere Sitzungs-Replayprüfung weitergegeben.

`false` setzt fort; `true` wird in `ExecutionGridSourceCancelled` übersetzt. Andere Callbackexceptions bleiben unverändert. Ein Abbruch beendet den Auftrag ohne neuen nativen Teilbefund. Die bisherigen nativen Suchabbruchgrenzen bleiben erhalten.

## Zulassung und Kombination

Die bestehenden Prüfungen für explizite Samples, Achse, fehlenden Erfahrungssnapshot und Gesamt-/Importbudget bleiben vor dem Quellenreplay. Prüfsummen, Vollständigkeit, exakte Werte, Spezifikationsgleichheit, feste Grid-Scheibe und überlappungsfreie Eingaben werden weiterhin geprüft. Ein weiterlaufender Callback verändert weder Samplewerte noch Budgets oder Bundlebytes.

Grid- und Erfahrungsimportcallback dürfen gemeinsam aktiv sein. Die Reihenfolge bleibt Gridquellen → Erfahrungsnachweise → native Forschung. Ein früher Gridabbruch darf den Erfahrungsimport nicht mehr erreichen; ein späterer Erfahrungsabbruch darf die Suche nicht starten.

## Sitzung und CLI

Mit `--verification replay` sind folgende Ausführungsmodi möglich:

- `--execution-cancellation grid-source-checkpoints`: nur Grid-Eingangsquellen und ihre Sampleprojektion.
- `--execution-cancellation grid-and-experience-checkpoints`: zusätzlich Erfahrungsimporte; erfordert Grid-Forschung mit `planner=experience-order`.

Der bestehende Modus `experience-checkpoints` bleibt unverändert. Neue erfolgreiche Sitzungsberichte verwenden `application-execution-v11` und nennen den aktiven Ausführungsmodus. Bestehende Berichtsversionen ohne Grid-Importcallback bleiben erhalten. Replay-/Serialisierungsmodi und das Replay-Bytebudget sind separat kombinierbar.

Ein Gridimportabbruch erzeugt FAILED in EXECUTING; nachgelagerte Serialisierung, Replay und CLI-Export werden nicht erreicht. Native Grid-Forschungsbundles bleiben unverändert.

## Grenzen

Einzelne Grid-Auswertungen, Quellhashes, Kopien, Dateieinlesen und einzelne Projektionsoperationen bleiben atomar. Die Projektion prüft zwischen Samples, nicht innerhalb ihrer Adress-/Wertkonvertierung. Quellenprüfung anderer Auftragstypen, etwa des nativen Z-Kompatibilitätsimports oder einer eigenständigen Fortsetzung, wird durch dieses Paket nicht erweitert. Keine maximale Signallatenz und kein neues gemeinsames Operationsbudget.

[Benutzung](129-execution-grid-source-checkpoints-usage.md) · [Abnahme](acceptance/execution-grid-source-checkpoints-v1-2026-09-23.md)
