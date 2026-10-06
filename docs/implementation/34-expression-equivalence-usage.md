# Core 0.15: Gleiche Formeln zusammenführen, Definitionslücken erhalten

BRUCE kann jetzt unterschiedliche Schreibweisen einer rationalen Funktion als gemeinsame Gruppe erkennen. Die Originalausdrücke bleiben nachvollziehbar. Stellen, an denen eine ursprüngliche Division ungültig war, werden beim Kürzen nicht vergessen.

## Ausführen

```bash
cmake --build build
./build/bruce research --strategy expression --equivalence rational \
  --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output ausdruck-normalisiert.bruce
./build/bruce replay ausdruck-normalisiert.bruce
```

Ohne `--equivalence rational` gilt weiterhin der bisherige Modus `syntax`. Die Option gehört ausschließlich zur Ausdrucksstrategie; die [bisherigen Suchgrenzen](32-expression-synthesis-usage.md) gelten weiter.

## Das sichtbare Ergebnis

Im Beispiel werden dieselben **2802 Bäume** untersucht. Die fünf passenden Lernkandidaten bilden zwei Gruppen:

| Gruppe | Ursprüngliche Ausdrücke | Befund |
| --- | --- | --- |
| x²/(x+1), ausgeschlossen x=-1 | `x-x/(x+1)`, `x*(x/(x+1))`, `(x*x)/(x+1)` | Alle drei bestehen Lern- und Hold-out-Prüfung |
| x²/(x+1), ausgeschlossen x=0 und x=-1 | `x/(1+1/x)`, `x/((x+1)/x)` | Beide scheitern am Prüfpunkt x=0 |

Damit wechselt der Gesamtstatus von `AMBIGUOUS` zu **`VERIFIED_ON_SAMPLES`**: Eine Gruppe bleibt übrig. Es wurden keine weiteren unabhängigen Datenbelege erzeugt. Die gefundene Datenbeziehung ist weiterhin nicht allgemein bewiesen.

Der Beispiellauf benötigt **105518 logische Operationseinheiten**, davon **2392 für Normalisierung und zusätzliche Samplevergleiche** und **69064 für Planung/Erzeugung**. Die Zusammenführung reduziert hier die Zahl der Ergebnisgruppen, nicht den durchsuchten Raum. Sie kostet zusätzliche Rechenarbeit.

## Was die Ausgabe enthält

- `expression_origins`: sämtliche ursprünglichen Bäume mit ID, Formel und Befund. `formula` und `expression` der Gruppe zeigen deren ersten Repräsentanten.
- `normal_form.numerator` und `denominator`: exakte Polynomkoeffizienten vom konstanten Glied aufwärts. `["0","0","1"]` ist x²; `["1","1"]` ist 1+x.
- `normal_form.excluded_polynomial`: dieses Polynom muss ungleich null sein. In der ersten Gruppe `["1","1"]`, in der zweiten `["0","1","1"]`, also x+x². Wiederholte Faktoren werden entfernt, ihre Nullstellen bleiben ausgeschlossen.
- `stored_source_hypotheses`: im Beispiel fünf gespeicherte Ursprünge; `hypotheses` enthält zwei Gruppen.
- `normalization_operations`: Teilkosten der Normalisierung einschließlich zusätzlicher Auswertungsvergleiche, bereits in `operations_used` enthalten.

Die Normalform dient der Gleichheitsprüfung. `x/x` wird deshalb nicht mit `1` zusammengeführt: Der Wert stimmt außerhalb null überein, aber die Definitionsbereiche unterscheiden sich. Auch `0*(1/x)` behält die Lücke bei null.

Die Prüfung ist bewusst hinreichend, nicht vollständig: `0/(x²+1)` und `0` können getrennte Gruppen bleiben, weil Faktoren ohne rationale Nullstellen noch nicht entfernt werden. Unterschiedliche Gruppen beweisen keine allgemeine Ungleichheit.

## Budgets und unvollständige Ergebnisse

`--max-results` begrenzt gespeicherte Originalkandidaten samt Herkunft. Es begrenzt nicht nur die bereits zusammengeführten Gruppen. Operations-, Bit-, Trace- und Abbruchgrenzen gelten auch während der Polynomnormalisierung.

Eine symbolische Zwischenrechnung kann das Bitlimit überschreiten, obwohl einzelne Samplewerte klein bleiben. Beispielsweise ist x+x bei x=0 gleich 0, sein normalisierter Koeffizient ist aber 2. Ein zu kleines Bitlimit führt dann zu `INCOMPLETE`. Ein vorhandener Treffer oder eine Teilnormalform darf den unvollständigen Gesamtbefund nicht ersetzen. Solche Abbrüche sind ebenfalls replaybar.

## Auswahl und Demo

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run ausdruck-normalisiert.bruce --at 0 \
  --output ausdruck-normalisiert-auswahl.bruce
./build/bruce replay ausdruck-normalisiert-auswahl.bruce
python3 scripts/demo.py
```

Bei x=0 bleibt eine positiv geprüfte Gruppe mit Wert 0: `SELECTED`. Bei x=-1 wird die Polstelle ausdrücklich zurückgewiesen. Nichtganzzahlige Werte passen weiterhin nicht in die Int64-Z-Projektion. Forschungs-Replay benötigt die ursprünglichen CSV-Dateien nicht; Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht. Vorhandene Exportdateien werden nicht überschrieben.

Die Demo zeigt den bisherigen und den neuen Modus nacheinander. Historische V1-Exporte bleiben unverändert reproduzierbar; der neue Modus schreibt ein versioniertes V2-Paket.

## Weitere Roadmap

Die optionale algebraische Zusammenführung rationaler Ausdrücke mit erhaltenen Ausschlussbedingungen ist implementiert. Portfolio-/Experience-Einbindung dieser Strategie, allgemeinere Operatoren und Grammatiken, effizientere Suchplanung und vollständigere Definitionsbereichsvergleiche bleiben offen. Ebenso weitergehende Experience-Korrekturketten, externe Evidenz, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.

Fortsetzung: [Core 0.16](36-expression-portfolio-usage.md) integriert die Ausdruckssuche inzwischen in ein Fünferportfolio mit festen Suchreihenfolgen und gemeinsamer Normalisierung. Die Experience-Einbindung bleibt der nächste Ausbau.
