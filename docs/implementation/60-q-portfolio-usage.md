# Rationale Definitionsbereiche gemeinsam nutzen – Core 0.28

`rational-q` funktioniert jetzt im Fünferportfolio und mit erfahrungsbasierter Suchplanung. Bruce kann dadurch auch Treffer verschiedener Formelfamilien zusammenführen, deren ursprüngliche Nennerbedingungen unterschiedlich aussehen, aber über rationalen Zahlen dieselben Eingaben zulassen.

## Vollständiges Beispiel: drei Gruppen werden eine

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q --expression-planner cached-grammar \
  --operators '+/' --min 0 --max 1 \
  --max-power 1 --max-degree 1 --max-nodes 3 \
  --train examples/q-domain-zero-training.csv \
  --holdout examples/q-domain-zero-holdout.csv \
  --output q-portfolio.bruce
./build/bruce replay q-portfolio.bruce
```

Die Suche läuft vollständig durch und liefert eine gemeinsame Gruppe mit `VERIFIED_ON_SAMPLES`. Darin liegen unter anderem die Nullfunktion und quadratische Brüche wie `0/(x²+1)`: Ihr Nenner hat keine rationale Nullstelle. Ein echtes Loch, etwa bei `0/x`, würde weiterhin getrennt bleiben; in diesem Beispiel schließt schon das Lernpaar bei 0 solche Ausdrücke aus.

Mit `--equivalence rational` entstehen bei sonst gleichen Einstellungen drei Gruppen und `AMBIGUOUS`. Der Q-Modus erkennt ihre Gleichheit über Q und führt sie zusammen. **Alle sieben Originaltreffer bleiben erhalten**: Familienherkunft steht in `origins`, vollständige Ausdrucksursprünge in den Teilruns.

Der geprüfte feste Lauf benötigt 3564 statt 3147 logischer Einheiten, davon 417 für die zusätzliche Gruppen-/Bereichsprüfung. Das ist präzisere Gruppierung gegen zusätzliche Arbeit, kein Geschwindigkeitsversprechen. Der Status bestätigt weiterhin nur die Datenpaare; `general_proof` bleibt false.

## Suchreihenfolge und Zusammenführung

```bash
./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q --expression-planner cached-grammar \
  --planner adaptive-order --min 1 --max 1 \
  --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output q-quelle.bruce
```

`--planner` wählt `portfolio`, `adaptive-order` oder `experience-order`. Unabhängig davon wählt `--expression-planner` zwischen `grammar`, `indexed-grammar` und `cached-grammar`. Operatorauswahl und literale Potenzen bleiben verwendbar.

Die Familien suchen in der gewählten Reihenfolge. Danach werden ihre vorhandenen Befunde immer in derselben Familienreihenfolge zusammengeführt. Bei identischen vollständig berechneten Quellen bleiben Gruppen und Repräsentanten dadurch gleich. Bei Abbruch können verschiedene Suchreihenfolgen unterschiedliche Teilbefunde liefern.

Die nachgelagerte Zusammenführung zählt zum gemeinsamen Arbeitsbudget. Auch fünf vollständige Teilruns können deshalb noch einen unvollständigen Gesamtbefund ergeben. Bisher nicht vereinigte Quellen stehen weiterhin in `strategy_runs`. Bei `INCOMPLETE` ist die abschließende Auswahl gesperrt.

## Passende Erfahrung, Widerruf und Replay

```bash
./build/bruce experience add --from-run q-quelle.bruce \
  --output q-erfahrung.experience

./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q --expression-planner cached-grammar \
  --planner experience-order --experience q-erfahrung.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output q-ziel.bruce
./build/bruce replay q-ziel.bruce
```

Die passende Quelle vergibt `[0,0,0,0,1]` Stimmen und lässt die Ausdruckssuche zuerst laufen. Alle fünf Familien bleiben enthalten. Die vollständigen Formelgruppen stimmen mit dem adaptiven Referenzlauf überein. Die Quellenprüfung ist zusätzlicher Aufwand und wird unter `experience_audit_operations` separat ausgewiesen.

Erfahrung wirkt nur bei passendem Gleichheitsmodus, Ausdrucksplaner, Operatoren und weiteren Kontextgrenzen. Eine Quelle aus dem bisherigen `rational`-Modus oder mit anderem Ausdrucksplaner beeinflusst diesen Q-Kontext nicht. Die eigenen Lernpaare vergeben keine Stimmen; doppelte Lernquellen werden auch über Modusgrenzen hinweg abgelehnt.

```bash
./build/bruce experience inspect q-erfahrung.experience
./build/bruce experience revoke --store q-erfahrung.experience \
  --id QUELLEN_ID_AUS_INSPECT --reason "Quelle widerrufen" \
  --output q-widerrufen.experience
```

Ein Widerruf entfernt den Planungseinfluss der Quelle. Frühere Forschungsexporte behalten ihren eingebetteten damaligen Speicherstand und bleiben reproduzierbar. Alte und neue Quellen können gemeinsam gespeichert werden, ohne alte Kennungen neu zu vergeben. Ein bereits erfahrungsbasierter Lauf darf weiterhin keine neue Erfahrungsquelle werden.

## Auswahl

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run q-ziel.bruce --at 0 --output q-auswahl.bruce
./build/bruce replay q-auswahl.bruce
```

Die Beispielauswahl liefert `SELECTED` mit Wert 0. Die Polstelle −1 bleibt gesperrt. Nichtganzzahlige Ergebnisse, Werte außerhalb Int64 und unvollständige Forschungsbefunde werden weiterhin zurückgewiesen. Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht mehr; Erfahrungs-Replay benötigt die ursprünglichen Quellen-/Speicherdateien nicht mehr.

Q-Portfolios schreiben V23/V24/V25, Q-Erfahrungsspeicher V8. Bestehende Suchmodi und historische Exporte bleiben unverändert. Exporte überschreiben keine bestehenden Dateien.

## Kosten und Grenzen lesen

- `union_operations` enthält die gesamte Familienvereinigung einschließlich ihrer Q-Vergleiche.
- `domain_comparison_operations` enthält Q-Vergleiche aus Ausdrucksteilrun **und** Vereinigung. Es überschneidet sich daher teilweise mit `union_operations`.
- `normalization_operations` enthält Normalisierung und übrige Vereinigungsverwaltung, keine neue Q-Vergleichsarbeit.
- `domain_comparisons` zählt begonnene Bereichsprüfungen; `domain_unknown` zählt abgeschlossene offene Entscheidungen.

Diese Zähler nicht pauschal addieren. Für die Gesamtarbeit gilt: Planung der Reihenfolge plus Teilrun-Operationen plus `union_operations`.

Höhere Restgrade können weiterhin unentschieden bleiben. Die Prüfung behauptet dann keine Ungleichheit und führt die betreffenden Gruppen nicht zusammen. Die Grenze von 128 gespeicherten Originaltreffern gilt über alle Familien; genaueres Gruppieren schafft keine zusätzlichen Ursprungsplätze. Quellenprüfung und Stimmenranking sind kein allgemeines Lernen.

## Weitere Roadmap

Q-Gruppierung ist in Portfolio und Erfahrungsplanung integriert. Allgemeinere Nullstellenentscheidungen, effizientere Gruppenvergleiche, weitere Suchoptimierungen, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](59-q-portfolio-contract.md) · [Abnahme](acceptance/q-portfolio-v1-2026-09-22.md)
