# V3 Cockpit Scheduler-Erklärung — Plan

**Spec:** [2026-10-13-phase-dag-v3-cockpit-scheduler-explanation-design.md](../specs/2026-10-13-phase-dag-v3-cockpit-scheduler-explanation-design.md)

## Task 1: Replaybasierte Scheduler-Projektion

- Ändern: mods/cockpit/src/cockpit_inspector.cpp
- Ändern: tests/mods/cockpit/cockpit_inspector_tests.cpp

1. Schreibe zuerst C++-Tests für direkte mehrstufige Blockierungsursachen, READY/FAILED/CANCELLED ohne Blockierungsbehauptung, Abhängigkeitsreihenfolge sowie Ausschluss von Rohbytes und Timingfeldern.
2. Ergänze ausschließlich die erfolgreiche V3-JSON-Projektion um `scheduler_explanation` gemäß Spec. Erzeuge sie aus Spec und Receipts nach erfolgreichem Replay.
3. Prüfe V2-/V3-Cockpit und V3-Kernverträge in Vollkopie.

## Task 2: Textsichere UI und Dokumentation

- Ändern: mods/cockpit/src/cockpit_inspector_assets.cpp
- Ändern: tests/mods/cockpit/test_cockpit_inspector.py
- Ändern: docs/architecture/03-blueprint-consolidation.md
- Ändern: docs/cli/README.md
- Ändern: docs/cli/commands.md

1. Schreibe DOM-/Dokumentationstests für die sechste Scheduler-Sektion, textContent-only und fehlende Timing-/Parallelitätsbehauptungen.
2. Rendere ausschließlich die neuen Projektionsfelder und den klaren Hinweis auf kanonischen Trace/fehlende Parallelitätsaufzeichnung.
3. Dokumentiere nur das bestehende read-only Verhalten; keine Bundles, CLI oder Routen ändern.
4. Prüfe HTTP-/DOM-Workflows, Links und relevante CTests in Vollkopie.
