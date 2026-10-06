# Core 0.16: Fünf mathematische Suchwege in einem Lauf

BRUCE kann die Ausdruckssuche jetzt gemeinsam mit den vier bisherigen Formelfamilien ausführen. Der Lauf teilt sich Arbeitsgrenzen, führt gleiche Formeln zusammen und erhält deren Herkunft und Definitionslücken.

## Das neue Beispiel starten

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output fuenfer-forschung.bruce
./build/bruce replay fuenfer-forschung.bruce
```

Die Beispiele beschreiben x²/(x+1). Der gemeinsame Lauf untersucht nacheinander:

| Strategie | Ergebnis in diesen Grenzen |
| --- | --- |
| Potenzformeln `a*x^p+b` | Kein passender Kandidat |
| Polynome bis Grad 2 | Kein passender Kandidat |
| Brüche mit linearem Nenner | Kein passender Kandidat |
| Brüche mit quadratischem Nenner | Kein passender Kandidat |
| Kleine zusammengesetzte Ausdrücke | Eine Gruppe besteht Lern- und Hold-out-Prüfung |

Gesamtstatus: **`VERIFIED_ON_SAMPLES`**. Der Lauf untersucht **2802 Kandidaten** und benötigt **105744 logische Operationseinheiten**. Fünf ursprüngliche Ausdruckskandidaten bilden zwei Gruppen: drei Schreibweisen bestehen die Prüfpaare; zwei weitere scheitern an ihrer zusätzlichen Lücke bei x=0.

Damit erweitert die Ausdruckssuche den tatsächlich untersuchten Raum. Das Ergebnis behauptet weiterhin keinen allgemeinen Beweis der Datenbeziehung.

## Gleiche Funde aus unterschiedlichen Strategien

```bash
./build/bruce research --strategy portfolio-expression \
  --min 0 --max 1 --max-power 2 --max-degree 2 --max-nodes 5 \
  --train examples/fraction-training.csv \
  --holdout examples/fraction-holdout.csv
```

Hier finden sowohl die rationale Familie als auch die Ausdruckssuche `1/(x+1)`. BRUCE führt sie zu einer Gruppe mit erhaltener Bedingung `x+1 != 0` zusammen.

`origins` nennt die beteiligten Strategien und Quell-IDs. Unter `strategy_runs` stehen die vollständigen Einzelbefunde. Eine Ausdrucksgruppe enthält dort wiederum `expression_origins` mit ihren Originalbäumen. Mehrere Rechenwege auf denselben Beispielen sind keine zusätzlichen unabhängigen Datenbelege.

`normal_form` beschreibt den gemeinsamen gekürzten Bruch und sein Ausschlusspolynom. Die Koeffizienten stehen wie bisher vom konstanten Glied aufwärts: `["1","1"]` bedeutet 1+x. Der erste Quellbefund bestimmt die Gruppen-ID.

Gleiche Werte allein reichen nicht: `1`, `x/x` und `(x+1)/(x+1)` können getrennte Gruppen bleiben. An x=0 beziehungsweise x=-1 sind die Bruchausdrücke undefiniert. Auch Faktoren ohne rationale Nullstellen bleiben vorerst in der Ausschlussbedingung; die Gleichheitserkennung ist damit bewusst unvollständig.

## Suchgrenzen und Kosten

Der neue Modus verwendet automatisch rationale Arithmetik, rationale Gleichheitsprüfung und eine feste Reihenfolge der fünf Familien. Standard sind Zähler 0–1, Nennergrenze 1, Potenzgrenze 4, Polynomgrad 3 und sieben Baumknoten. `--planner portfolio` und `--equivalence rational` dürfen ausdrücklich angegeben werden.

Die Parametergrenzen gelten gemeinsam für die bisherigen begrenzten Koeffizientenfamilien und die Konstanten der Ausdrucksgrammatik. Die Polynominterpolation behält ihren bisherigen Vertrag; ihre berechneten Koeffizienten sind nicht auf diesen Zählerbereich begrenzt. Es sind höchstens 64 Konstanten-Rohslots erlaubt: Zählerbreite mal Nennergrenze. Die übrigen [Ausdrucksgrenzen](32-expression-synthesis-usage.md) bleiben wirksam.

Alle Familien teilen sich die Budgets. Zusätzlich kostet die Normalisierung und Zusammenführung Rechenarbeit:

- `union_operations` zählt die Arbeit zwischen den Teilruns.
- `operations_used` enthält sämtliche Teilrun-Arbeit plus diese Zusammenführung.
- `normalization_operations` umfasst sowohl die interne Ausdrucksnormalisierung als auch die Zusammenführungsarbeit; der Wert ist bereits in `operations_used` enthalten.
- `stored_source_hypotheses` und `--max-results` zählen die gespeicherten Ursprünge einschließlich aller Ausdrucksbäume, nicht nur fertige Gruppen.

Abbrüche während Suche oder Zusammenführung ergeben `INCOMPLETE`. Die Teilbefunde und ihr Replay bleiben verfügbar, die abschließende Z-Auswahl bleibt gesperrt. Sehr kleine Tracebyte-Budgets können bereits vor dem ersten Teilrun stoppen, weil Platz für die übergeordnete Prüfspur reserviert wird.

Die neue Familienmenge unterstützt noch kein `adaptive-order` oder `experience-order`. Entsprechende Aufrufe und das Hinzufügen eines neuen Fünfer-Exports zum Erfahrungsspeicher werden ausdrücklich abgewiesen. Die bestehende Erfahrungsplanung für drei beziehungsweise vier Familien bleibt nutzbar.

## Ergebnis auswählen und Demo ausführen

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run fuenfer-forschung.bruce --at 0 \
  --output fuenfer-auswahl.bruce
./build/bruce replay fuenfer-auswahl.bruce
python3 scripts/demo.py
```

Bei x=0 liefert die überlebende Gruppe den Wert 0 und `SELECTED`; bei x=-1 bleibt die Polstelle gesperrt. Die bestehende Z-Projektion verlangt weiterhin Int64-Werte. Forschungs-Replay benötigt keine ursprünglichen CSV-Dateien; Auswahl-Replay benötigt keine ursprüngliche Forschungs- oder Regeldatei. Vorhandene Exportdateien werden nicht überschrieben.

## Weitere Roadmap

Die Ausdruckssuche ist jetzt in ein gemeinsames Fünferportfolio mit Normalformen, Herkunft, Budgets und Replay integriert. Adaptive Reihenfolge und Experience-Einbindung dieser fünf Familien sind der nächste Ausbau. Allgemeinere Operatoren und Grammatiken, effizientere Suche, vollständigere Definitionsbereichsvergleiche, externe Evidenz, Experience-Korrekturketten, Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z bleiben offen. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.

Fortsetzung: [Core 0.17](38-five-experience-usage.md) ergänzt inzwischen adaptive Reihenfolge und Erfahrung auch für diese fünf Familien. Der hier beschriebene feste Modus und seine V8-Replays bleiben unverändert.
