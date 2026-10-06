# Core 0.18: Ausdrucksbäume mit weniger Planungsarbeit erzeugen

BRUCE kann Ausdruckskandidaten jetzt über einen kleinen Grammatikindex erzeugen. Dabei bleiben dieselben Formeln in derselben Reihenfolge erhalten. Im bestehenden Beispiel sinkt die logische Planungsarbeit, während die mathematischen Prüfungen unverändert bleiben.

## Ausführen

```bash
cmake --build build
./build/bruce research --strategy expression --planner indexed-grammar \
  --equivalence rational --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output ausdruck-indiziert.bruce
./build/bruce replay ausdruck-indiziert.bruce
```

Ohne die neue Planeroption gilt weiterhin `grammar`. Beide Gleichheitsmodi funktionieren: `syntax` erhält getrennte Schreibweisen, `rational` führt gleiche Normalformen mit erhaltenen Definitionslücken zusammen.

## Gemessener Unterschied

Beide Planer untersuchen im Beispiel exakt **2802 Kandidaten**. Mit rationaler Zusammenführung bleibt der Befund `VERIFIED_ON_SAMPLES`, einschließlich derselben Originalausdrücke und Herkunft.

| Logische Arbeit | Bisheriger Planer | Grammatikindex |
| --- | ---: | ---: |
| Planung und Erzeugung | 69064 | 56843 |
| Auswertung, Gegenprüfung und Normalisierung | 36454 | 36454 |
| Gesamt | 105518 | 93297 |

Das sind **12221 logische Operationseinheiten weniger**. Der Index enthält für sieben Knoten lediglich 20 Bereichseinträge. Er speichert keine vollständige Liste der 2802 Bäume.

Ein direkter Budgetvergleich ist möglich:

```bash
./build/bruce research --strategy expression --planner grammar \
  --equivalence rational --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv --max-operations 93297

./build/bruce research --strategy expression --planner indexed-grammar \
  --equivalence rational --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv --max-operations 93297
```

Bei diesem gemeinsamen Budget endet der bisherige Planer mit `INCOMPLETE`, der neue mit `VERIFIED_ON_SAMPLES`. Das ist eine konkrete Verbesserung innerhalb des logischen Arbeitsbudgets. Daraus wird keine gemessene CPU-Beschleunigung oder allgemeine Laufzeitgarantie abgeleitet.

Im Syntaxmodus sinkt die Gesamtarbeit entsprechend von 103126 auf 90905 Einheiten; die getrennten passenden Schreibweisen bleiben `AMBIGUOUS`.

## Was der Index verändert

Bisher geht BRUCE beim Erzeugen jedes Teilbaums die möglichen Grammatikblöcke nacheinander durch. Der neue Planer berechnet einmal deren Rangbereiche und sucht den passenden Bereich anschließend binär. Die eigentlichen Baumränge bleiben gleich.

Für die maximal erlaubten neun Knoten reichen 32 Indexeinträge. Die Größe des beschreibbaren Suchraums ändert sich nicht; BRUCE erzeugt weiterhin jeweils nur den benötigten Baum. Mehr Knoten können deshalb trotz Index weiterhin ein sehr großes Suchproblem ergeben.

`index_entries` zeigt die fertige Indexgröße an. Ist bereits die Grammatikplanung unterbrochen, steht dort 0. Aufbau und Benutzung zählen in `planning_operations` und sind bereits in `operations_used` enthalten. Bei kleinen Aufgaben kann der Aufbau mehr kosten, als er später spart; die Beispielersparnis gilt nicht pauschal für jede Konfiguration.

Alle bisherigen Suchgrenzen gelten weiter. Der Index überspringt weder Kandidaten noch unabhängige Gegenprüfungen. Insbesondere bleiben `x/x` und die innere Division von `0*(1/x)` an x=0 undefiniert. Abbrüche bleiben replaybare Teilbefunde.

## Auswahl und Demo

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run ausdruck-indiziert.bruce --at 0 \
  --output ausdruck-indiziert-auswahl.bruce
./build/bruce replay ausdruck-indiziert-auswahl.bruce
python3 scripts/demo.py
```

Die geprüfte Beispielgruppe liefert bei x=0 den Wert 0 mit `SELECTED`; bei x=-1 bleibt die Polstelle gesperrt. Die Demo prüft gleiche Befunde, die Arbeitsersparnis, den Budgetvergleich und Auswahl-Replay ohne ursprüngliche Forschungsdatei. Vorhandene Exportdateien werden nicht überschrieben.

Der neue Modus schreibt Ausdrucks-Replay V3. Alte Ausdrucks- und Portfolio-Replays bleiben unverändert reproduzierbar.

## Weitere Roadmap

Ein optionaler Index für die eigenständige Ausdruckssuche ist implementiert. Seine Einbindung in Portfolio und Erfahrungsplanung, weitere Suchoptimierungen, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Die bestehenden Portfolios verwenden weiterhin ihren bisherigen Ausdrucksplaner. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.

**Fortschreibung Core 0.19:** Die [Einbindung in Portfolio und Erfahrungsplanung](42-indexed-portfolio-usage.md) ist inzwischen implementiert; der vorstehende Abschnitt beschreibt den Stand von Core 0.18.
