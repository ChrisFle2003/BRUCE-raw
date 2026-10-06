# Core 0.43: Aus Grid-Forschung Erfahrung gewinnen

Bruce kann jetzt auch geprüfte Portfolio-Forschung aus Grid-Quellen für spätere Strategieentscheidungen verwenden. Die Quelle bleibt vollständig im Erfahrungsspeicher enthalten.

```bash
./build/bruce grid-batch --paths '0,2;1,2;2,2' --output lernen.bruce
./build/bruce grid-batch --paths '3,2' --output pruefen.bruce
./build/bruce research --strategy portfolio-rational --max-degree 2 \
  --min -2 --max 2 --max-power 2 --max-denominator 1 \
  --grid-train lernen.bruce --grid-holdout pruefen.bruce --output forschung.bruce
./build/bruce experience add --from-run forschung.bruce --output erfahrung.bruce
./build/bruce experience inspect erfahrung.bruce
```

Der Snapshot zeigt einen aktiven Datensatz mit `grid_origin: true`. Seine Stimmen stehen für Formelfamilien, die die Lern- und separaten Prüfwerte bestanden haben. Bei einer späteren passenden Forschung werden sie mit `--planner experience-order --experience erfahrung.bruce` berücksichtigt. Bei gleichen Stimmen bleibt die bisherige Reihenfolge bestehen.

Auch Forschung aus vollständig fortgesetzten Grid-Stapeln ist zulässig. Der Snapshot und ein späterer Forschungsexport bleiben nach Entfernung der ursprünglichen Quelldateien replaybar. Vorhandene Ausgabedateien werden nicht überschrieben.

Gleiche Lernpaare zählen nicht mehrfach – auch wenn sie einmal aus CSV und einmal aus dem Grid kommen. Ein Widerruf über `experience revoke` entfernt die Stimmen für künftige Planung. Ein unvollständiger Lauf, fehlende Prüfwerte, Einzelstrategien und bereits erfahrungsgeleitete Quellen werden abgewiesen.

Der Erfahrungsspeicher bleibt bewusst begrenzt: maximal 1 MiB und acht Datensätze. Große Portfolios können diese Importgrenze überschreiten. Die Auditkosten sind ein logischer Rahmen aus Forschung und Quellbytes, keine Laufzeitmessung.

## Weitere Roadmap

Der Übergang Grid → Forschung → Erfahrung → spätere Strategieplanung ist implementiert. Allgemeine Aufgaben-/Strategie-/Gate-Verträge, fortsetzbare Forschung, externe Evidenz und weitergehende Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU und BigInt im Grid/Z. Stichprobenbestätigung ist kein allgemeiner Beweis und kein M2-/M4-Gesamtabschluss.

[Vertrag](90-grid-experience-contract.md) · [Abnahme](acceptance/grid-experience-v1-2026-09-22.md)
