# Phase-DAG V3 — Begrenzter Parallel-Scheduler: Plan

**Ziel:** Bis zu vier unabhängige V3-Knoten deterministisch in Wellen ausführen; V2 und V3-Bundleformat bleiben unverändert.

**Spec:** [2026-10-13-phase-dag-v3-bounded-parallel-scheduler-design.md](../specs/2026-10-13-phase-dag-v3-bounded-parallel-scheduler-design.md)

## Grenzen

- V3-Control-Standard bleibt 1; nur 1 bis 4 sind zulässig.
- Ergebnisse, Receipts, Trace und Bundlebytes müssen bei 1 und 2 bis 4 gleich sein.
- Keine Änderungen an V2, Cockpit, Adapterkatalog oder Bundlecodec.
- CLI-Schalter nur für V3 execute; docs-Generatorartefakte nur über den Generator in einer Vollkopie.

## Task 1: Deterministischer Wellen-Scheduler

**Dateien:**

- Ändern: include/bruce/phase_dag/phase_dag_v3.hpp
- Ändern: src/phase_dag/phase_dag_v3.cpp
- Ändern: tests/unit/phase_dag_v3_tests.cpp

**Schritte:**

1. Schreibe zunächst Tests mit instrumentierten, thread-sicheren Fakeadaptern: Standard 1, erlaubte Grenzen 1/4, Ablehnung 0/5, maximale Gleichzeitigkeit, mehrwellige Ordnung, Bytegleichheit, Fehler einer Welle und Cancellation vor der nächsten Welle.
2. Führe den V3-Kernvertrag rot gegen den seriellen Scheduler aus.
3. Ergänze `max_parallel_nodes` im Control, validiere 1 bis 4 und ersetze nur die Adapterstartphase durch begrenzte Wellen mit C++20-Threads/Futures. Sammle und übernimm alle Ergebnisse sortiert nach Wellenreihenfolge.
4. Prüfe V3-, Payload-Gate- und V2-Verträge in einer vollständigen /tmp-Kopie.

## Task 2: V3-CLI-Parallelitätsoption

**Dateien:**

- Ändern: apps/phase_dag_v3_cli.hpp
- Ändern: tests/mods/nanocell/test_phase_dag_v3_cli.py

**Schritte:**

1. Ergänze Tests für fehlenden Standard, 2 und 4, ungültige Werte und Option bei inspect/replay; erzeuge eine mehrwellige Spec und verlange bytegleiche seriell/parallele Exporte.
2. Belege RED gegen die alte Argumentbehandlung.
3. Implementiere ausschließlich `execute SPEC --output BUNDLE [--max-parallel-nodes N]`, parse strikt und reiche den Wert im `PhaseDagControlV3` weiter.
4. Prüfe V2-/V3-CLI, NanoCell-off und Kernverträge.

## Task 3: Dokumentation und Integrationsabnahme

**Dateien:**

- Ändern: docs/cli/README.md; docs/cli/commands.md; docs/cli/formats.md; docs/architecture/03-blueprint-consolidation.md
- Ändern: tests/mods/nanocell/test_phase_dag_v3_cli.py
- Nur Generator: docs/cli/source-index.md; docs/cli/examples.json; docs/cli/validation.md

**Schritte:**

1. Ergänze erst Dokuassertions für Standard 1, Grenze 4, deterministische Wellen und Replay.
2. Aktualisiere nur implementierte Aussagen; keine Performancebehauptung.
3. Generiere Artefakte ausschließlich in einer vollständigen temporären Kopie und übernimm exakt die drei Generatorartefakte.
4. Prüfe Links, gezielte Verträge und vollständigen CTest-Lauf.

## Abnahmefokus

- Kein Thread-Timing darf Bundlebytes oder Trace ändern.
- Keine fünfte gleichzeitige Adapterausführung.
- Fehler und Cancellation dürfen keine halb übernommene Welle erzeugen.
- V2 und Cockpit bleiben unverändert.
