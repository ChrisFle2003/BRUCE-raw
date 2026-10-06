# Core 0.47: Gemeinsamer Einstieg für Laufaufträge

Die vorhandenen Berechnungsabläufe nutzen jetzt eine gemeinsame typisierte Anwendungsschnittstelle. CLI-Befehle und ihre Ergebnisformate bleiben erhalten.

```bash
./build/bruce task-contracts
./build/bruce contracts
```

`task-contracts` zeigt die acht integrierten Auftragstypen. `contracts` zeigt weiterhin die Strategie-/Planerkombinationen innerhalb der exakten Forschung.

| Auftrag | Fachlicher Ablauf |
| --- | --- |
| `research-int64` | Bisherige Int64-Formelforschung |
| `research-exact` | Exakte Forschung und Portfolios |
| `grid` | Einzelne Grid-Auswertung |
| `grid-batch` | Grid-Stapel mit Cache und Teilbefunden |
| `grid-resume` | Geprüfte Fortsetzung eines Grid-Stapels |
| `grid-research` | Forschung mit vollständiger Grid-Herkunft |
| `fraction-comparison` | Exakter Vergleich von Polynombrüchen |
| `compatibility` | Numerische Z-Auswahl |

## Gemeinsame Schnittstelle

C++-Anwendungen können die bisherigen fachlichen Parameter über `ApplicationTask` an `execute_application` übergeben. Das Ergebnis bleibt in `ApplicationRun` typisiert. `render_application` und `pack_application` liefern die bisherigen JSON- und Bundleformate. Der Einstieg selbst liest oder schreibt keine Dateien.

```cpp
bruce::GridTask task;
task.path = "2,6";
auto run = bruce::execute_application(task);
auto json = bruce::render_application(run);
auto bundle = bruce::pack_application(run);
```

Der zugehörige Header ist `bruce/application/application_task.hpp`. Ein optionales `ExecutionControl` liefert nur ein Abbruchsignal. Wann es geprüft wird und wie der Abbruch gespeichert wird, bestimmt weiterhin der jeweilige Executor. Ein einzelner Gridwert wird atomar berechnet; Suchläufe und Stapel behalten ihre lokalen Checkpoints.

## Architekturfortschritt

Der gemeinsame Aufgabenvertrag aus M0 ist nun für acht bestehende Laufarten konkret als C++-Schnittstelle umgesetzt. Er verwendet die vorhandenen Validatoren und den gemeinsamen Exact-Strategievertrag aus Core 0.46. Die CLI nutzt ihn tatsächlich.

M1 erhält einen gemeinsamen Zugang zu den unveränderten Ergebnis- und Exportfunktionen. M2 besitzt damit einen Anwendungseinstieg über Forschung und Grid hinweg. Gemeinsame Budgeteinheiten, eine allgemeine Gate-/Phasen-Zustandsmaschine, dynamische Strategieregistrierung und fortsetzbare Formelforschung bleiben offen. Dies ist keine vollständige M0–M2-Abnahme.

Die Verwaltung von Experience-Snapshots, reine Koordinatenumrechnung und Taschenrechnerbefehle bleiben eigene Operationen. Sprache, NanoCell, Cockpit, Phase-DAG und GPU sind weiterhin weitere Roadmap-Arbeit.

[Architekturentscheidung](../adr/011-typed-application-entry.md) · [Plan](98-application-task-plan.md) · [Abnahme](acceptance/application-task-v1-2026-09-22.md)
