# Gemeinsame Strategieplanung – Core 0.7

Freigegebene Fortsetzung: `research --strategy portfolio` orchestriert vorhandene adaptive Potenzsuche und Newton-Polynomforschung. Native Umsetzung im bestehenden Kern, keine neue Abhängigkeit oder Datenbank.

## Vertrag

Feste versionierte Reihenfolge: power-affine/adaptive, danach polynomial/interpolate. Gemeinsame Eingaben und Koeffizientengrenzen; `--max-power` Standard 4 und `--max-degree` Standard 3 bestimmen die Vereinigung beider Familien. Der Planer heißt `power-then-polynomial-v1`; CLI `--planner portfolio`. Int64 wird wie bei bisherigen exakten Strategien abgewiesen.

Kandidaten-, Operations-, Ergebnis- und Tracebudgets werden zwischen den Läufen weitergereicht: zweiter Lauf erhält nur unverbrauchte Mittel. Das Ergebnisbudget zählt gespeicherte rohe Teilhypothesen einschließlich später zusammengeführter Duplikate. Zwei Wrapper-Ereignisse und 256 Wrapper-Textbytes werden reserviert. Reichen die Restbudgets für die minimalen Teilrunverträge nicht aus, bleibt das Portfolio INCOMPLETE. Kein Arbeitsschritt eines Teilruns ist kostenlos oder unter eigenem Default versteckt.

Alle Kandidaten werden zu kanonischen Monomkoeffizienten normalisiert. Algebraisch identische Polynome werden einmal geführt, mit Herkunft (Strategie, ursprüngliche ID, Status). Keine Erhöhung unabhängiger Evidenz durch denselben Datensatz. Unterschiedliche Kandidaten bleiben gleichrangig. ID aus erstem Ursprung: power_id*2 oder polynomial_id*2+1. Z-Koordinaten (id/65536,id%65536) und Kontext portfolio-domain-v1.

Jeder unvollständige Teilrun hält Gesamtstatus INCOMPLETE, auch bei einem passenden anderen Teilrun. Nichtabbruchbedingte Teilstopps erlauben den zweiten Lauf, sofern Mittel reichen. Cancelled stoppt weitere Dispatches. Abschließende widersprüchliche Status derselben Formel ergeben INCOMPLETE / StrategyResultConflict; keine stille Auswahl einer Quelle. Checkpoints zählen global über alle Teilruns und sichern den exakten Replay eines Abbruchs.

Neuer Container BRUCE_PORTFOLIO_BUNDLE_V1, Engine bruce-portfolio-v1, Regel portfolio-union-v1, Policy shared-budget-v1. Export enthält Root-Aufgabe und vollständige neu berechnete Teilrun-Ausgaben im Resultat. Replay führt dieselbe begrenzte Orchestrierung aus; keine beliebige rekursive Portfolio-Verschachtelung. Alte Ausgabeversionen bleiben unverändert.

## Umsetzung

- [x] Historischen Core-0.6-Export sichern und neue CLI-Abnahmen rot beobachten.
- [x] Portfolio-Runner mit Restbudgets, globalen Checkpoints und kanonischer Zusammenführung implementieren.
- [x] CLI, Replay und Z-Projektion integrieren; komplette Teilbefunde sichtbar machen.
- [x] Familienvereinigung, Duplikate/Herkunft, Hold-out, Grenzen, fehlende Daten, Unterbrechung und Manipulation unabhängig prüfen.
- [x] Review, Debug-/UBSan-Abnahme, Demo und Benutzeranleitung abschließen.

Keine freie Ausdruckssynthese, kein Lernen oder allgemeiner Beweis. Diese erste Orchestrierung ist deterministisch; erfahrungsbasiertes Strategieranking bleibt offen.
