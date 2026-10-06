# Core 0.46: Gemeinsame Forschungs-, Strategie- und Gate-Verträge

Der erste Architektur-Konsolidierungsschritt vereinheitlicht die vorhandene Formelforschung. Die öffentliche Bedienung bisheriger Forschungsaufträge bleibt erhalten.

```bash
./build/bruce contracts
```

Der neue Befehl zeigt die zehn eingebauten Strategievarianten mit Standardplaner und erlaubten Planern. Diese Liste stammt aus demselben Katalog, den CLI und Engine tatsächlich verwenden. Die zehn IDs umfassen Einzelstrategien und Portfoliovarianten; sie bedeuten nicht zehn unabhängige Formelfamilien.

## Was sich im Kern verbessert

- **Aufgaben:** Die vollständige Prüfung exakter Forschungsaufträge liegt in `validate_exact_task`. Dazu gehören Zahlenbereiche, Samples, Dubletten, Fähigkeitskombinationen und Budgets. CLI, Grid-Forschung und Replay benutzen sie über denselben Engine-Einstieg.
- **Strategien:** Standardplaner, erlaubte Planer und Auswahl des zuständigen Executors stammen aus einem gemeinsamen Katalog. Doppelte Strategie-/Planerprüfungen in CLI und Engine entfallen.
- **Gates:** Z-Auswahl und Experience-Aufnahme beziehen ihre Freigaben aus gemeinsamen Forschungsregeln. Unvollständige oder unbekannte Status erhalten keine abschließende Auswahlfreigabe. Experience verlangt zusätzlich unabhängige abgeschlossene Portfolio-Evidenz mit Holdout und bestätigter Hypothese.

Die Quellen werden weiterhin vollständig replaygeprüft. Ein Gate darf nicht mit bloß behaupteten Statusfeldern geöffnet werden. Lernpromotion und Veröffentlichung bleiben geschlossen; eine lokale Exportdatei darf trotzdem gespeichert werden. Bestandene Samples sind kein Allgemeinbeweis.

## Architekturstand nach der Konsolidierung

| Bereich | Jetzt nachgewiesen | Weiter offen |
| --- | --- | --- |
| M0 Aufgabenvertrag | Gemeinsame Exact-Auftragsprüfung und Strategie-/Planerzulassung | Übergreifender Vertrag für Grid, Vergleich, Legacy und künftige NanoCell-Aufträge |
| M0/M1 Gates | Gemeinsame Entscheidung an den Übergängen Forschung → Z/Experience | Allgemeine Gate-Zustandsmaschine mit Run-, Technik- und Astgeltung |
| M1 Reproduzierbarkeit | Bestehende Exporte und alte Replays bleiben kompatibel | Release-/Artefaktverwaltung und systematische Erstinstallation |
| M2 Strategien | Eingebauter Katalog steuert reale Ausführung | Allgemeiner registrierbarer Strategie-Port und neue Problemklassen |
| M2 Orchestrierung | Bestehende Portfolios, Gridanbindung und Budgets erhalten | Unterbrechbare Auditphasen, fortsetzbare Forschung und allgemeine Phasensteuerung |

Diese Konsolidierung ersetzt verstreute Entscheidungen im laufenden Code. Sie ist ein erster gemeinsam verwendeter Vertrag, keine vollständige M0–M2-Abnahme. Sprache, NanoCell, Cockpit, Phase-DAG und GPU bleiben weiterhin offen.

[Architekturentscheidung](../adr/010-research-contract-and-gates.md) · [Plan](96-research-contract-consolidation-plan.md) · [Abnahme](acceptance/research-contract-v1-2026-09-22.md)
