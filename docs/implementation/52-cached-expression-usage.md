# Core 0.24: Wiederkehrende kleine Teilbäume wiederverwenden

BRUCE kann beim Aufbau von Ausdrücken kleine Teilbäume vorübergehend zwischenspeichern. Dadurch entfällt bei Wiederholungen ein Teil der Planungsarbeit. Alle Kandidaten werden weiterhin vollständig nach den bisherigen Regeln ausgewertet und geprüft. Es entsteht keine feste Aufgaben-/Antwortdatenbank.

## Starten

```bash
cmake --build build
./build/bruce research --strategy expression --planner cached-grammar \
  --equivalence rational --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv --output ausdruck-cache.bruce
./build/bruce replay ausdruck-cache.bruce
```

Das Beispiel untersucht x²/(x+1). Die bestätigte Formelgruppe mit Bedingung x+1≠0 bleibt dieselbe wie mit `indexed-grammar`. Andere Schreibweisen mit zusätzlichen Definitionslücken behalten ihre eigenen Befunde. Der Status `VERIFIED_ON_SAMPLES` bestätigt die angegebenen Paare, keinen allgemeinen Beweis einer Datenbeziehung.

Der Speicher enthält höchstens 64 Teilbäume mit jeweils drei Knoten. Er wird beim Start jedes Laufs neu aufgebaut und danach verworfen. Gespeichert werden Rechenstrukturen; Werte aus früheren Aufgaben werden dadurch nicht übernommen.

## Gemessener Unterschied

Die Demo vergleicht dieselbe Grammatik und dieselben Prüfungen:

| Messgröße | `indexed-grammar` | `cached-grammar` |
| --- | ---: | ---: |
| Ausdruckskandidaten | 2802 | 2802 |
| Planungsarbeit | 56843 | 50406 |
| Gesamte logische Arbeit | 93297 | 86860 |

Der neue Lauf verwendet 14 Cacheeinträge, erzielt 3374 Treffer und 14 Fehlzugriffe; Verdrängungen treten in diesem Beispiel nicht auf. Bei einem gemeinsamen Operationsbudget von 86860 bleibt der bisherige Indexlauf `INCOMPLETE`, während der Cachelauf vollständig ist.

Die Werte sind logische Arbeitseinheiten einschließlich Cacheverwaltung, keine gemessenen CPU-Zeiten. Bei anderen Aufgaben kann ein Cache zusätzlichen Aufwand verursachen. Er ist deshalb optional. Ohne wiederverwendbare Teilbäume bleibt er leer.

## Rechenarten und Potenzen

```bash
./build/bruce research --strategy expression --planner cached-grammar \
  --operators '+^' --equivalence rational --min 1 --max 3 --max-nodes 5 \
  --train examples/expression-power-training.csv \
  --holdout examples/expression-power-holdout.csv --output potenz-cache.bruce
./build/bruce replay potenz-cache.bruce
```

Auch ausgewählte Rechenarten und die literalen Exponenten 2 bis 8 werden unterstützt. Ohne Operatoroption gelten weiterhin die vier Grundrechenarten. `--equivalence syntax` hält unterschiedliche Schreibweisen getrennt; `rational` verwendet die vorhandene Normalformzusammenführung mit erhaltenen Ausschlussbedingungen.

Kandidaten-IDs und Reihenfolge entsprechen innerhalb derselben Grammatik dem bisherigen Planer. Ergebnis-, Operations-, Kandidaten- und Tracegrenzen gelten weiter. Bei einem Abbruch bleibt der Befund unvollständig und replaybar. Die Ausgabe zeigt zusätzlich `cache_hits`, `cache_misses`, `cache_evictions` und `cache_entries`.

## Auswahl und Replay

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run ausdruck-cache.bruce --at 0 --output ausdruck-cache-auswahl.bruce
./build/bruce replay ausdruck-cache-auswahl.bruce
python3 scripts/demo.py
```

Die Auswahl liefert im Beispiel `SELECTED` mit Wert 0. Bei x=−1 bleibt die Formel gesperrt. Die Projektion ist weiterhin auf Int64-Werte begrenzt.

Der neue Forschungsplaner schreibt Ausdrucks-Bundles V6. Beim Replay wird der Cache aus den eingebetteten Eingaben erneut aufgebaut; ein ursprünglicher Speicherzustand oder eine Cachedatei ist nicht erforderlich. Auswahl-Replay funktioniert auch ohne die ursprüngliche Forschungsdatei. Bestehende Ausgabedateien werden nicht überschrieben.

## Weitere Roadmap

Ein begrenzter Cache für kleine Teilbäume ist in der eigenständigen Ausdruckssuche implementiert. Seine Portfolio-/Experience-Einbindung, weitere Suchoptimierungen, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](51-cached-expression-contract.md) · [Abnahme](acceptance/cached-expression-v1-2026-09-22.md)

**Fortsetzung:** [Core 0.25](54-cached-portfolio-usage.md) integriert diesen Cache in Portfolio und Erfahrungsplanung. Die obige Abgrenzung beschreibt den damaligen Einzelstrategie-Ausbau.
