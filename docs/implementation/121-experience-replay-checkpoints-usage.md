# Core 0.58: Erfahrungsnachweise im Forschungsreplay unterbrechen

Die vorhandene Option `research-checkpoints` erreicht jetzt auch die Quellenprüfung eingebetteter Erfahrungsdaten:

```bash
./build/bruce research --train training.csv --holdout holdout.csv \
  --domain rational --strategy portfolio-rational --planner experience-order \
  --experience wissen.experience \
  --verification replay --verification-cancellation research-checkpoints \
  --output forschung.bruce
```

Bruce prüft dabei weiterhin die ursprünglichen Forschungsnachweise, ihre Korrekturen und Widerrufe. Die daraus gewonnenen Stimmen verändern nur die Suchreihenfolge. Ohne Abbruch entstehen dieselben Befunde und Exporte wie bisher; der Sitzungsbericht bleibt V5.

Während der zusätzlichen Replayprüfung erreicht Strg+C nun auch die inneren Prüfpunkte der Erfahrungsquellen. Das gilt ebenso für Grid-Forschung und für erfahrungsbasierte Forschung innerhalb eines Z-Kompatibilitätsreplays mit `compatibility-checkpoints`. Ein Prüfungsabbruch beendet die Sitzung ohne Export; er wird nicht als widerlegte Hypothese oder beschädigter Erfahrungsnachweis ausgegeben.

Der native Erstlauf, einzelne Rechenoperationen, Hashing und Serialisierung behalten ihre bisherigen Unterbrechungsgrenzen. Ein sofortiger Stopp wird nicht zugesagt. Für `experience add`, `replace`, `revoke` oder `inspect` führt dieses Paket keine neue CLI-Option ein.

## Weitere Roadmap

**Fortschreibung bis Core 0.67:** [Äußere Prüfsummen](123-serialization-digest-checkpoints-usage.md), [Grid-Inhaltsaufbau](125-grid-serialization-records-usage.md), [Erfahrungsimporte im Erstlauf](127-execution-experience-checkpoints-usage.md) und [Grid-Eingangsquellen](129-execution-grid-source-checkpoints-usage.md) besitzen inzwischen begrenzte Abbruchpunkte. Auch [Forschungsimporte der Z-Auswahl](131-execution-compatibility-source-usage.md) sind inzwischen im Erstlauf angebunden. Hinzu kommen [native Fortsetzungsimporte](133-execution-resume-import-usage.md). Seit Core 0.67 besitzt die [Elternkopie im Fortsetzungs-Payload](140-resume-copy-blocks-usage.md) Kontrollpunkte zwischen 64-KiB-Blöcken. Ein [gemeinsames Prüfpunktbudget](135-session-work-budget-usage.md) begrenzt jetzt die vorhandenen Kontrollstellen über alle Sitzungsphasen; es ist kein einheitlicher mathematischer Operationszähler.

Die eingebetteten Erfahrungs-Quellaudits bleiben an den Replaycallback angeschlossen. Noch offen sind weitere Payloadfamilien, blockweises Kopieren großer Inhalte und gemeinsame Operationsbudgets. Die vorhandenen Abbruchpunkte garantieren keine maximale Signallatenz. Dies ist kein allgemeines Lernen und kein vollständiger Architekturabschluss.

[Vertrag](120-experience-replay-checkpoints-contract.md) · [Abnahme](acceptance/experience-replay-checkpoints-v1-2026-09-23.md)
