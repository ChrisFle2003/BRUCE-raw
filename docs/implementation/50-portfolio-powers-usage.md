# Core 0.23: Potenzausdrücke gemeinsam planen und Erfahrung nutzen

Das Fünferportfolio kann jetzt Potenzen wie `x^3` direkt in seinen Ausdrucksweg einbauen. Geprüfte frühere Läufe können diesen Weg bei passenden neuen Aufgaben priorisieren. Alle fünf Suchfamilien werden weiterhin ausgeführt.

## Gemeinsam suchen

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar --operators '+^' \
  --min 1 --max 3 --max-power 2 --max-degree 2 --max-nodes 5 \
  --train examples/expression-power-training.csv \
  --holdout examples/expression-power-holdout.csv --output portfolio-potenzen.bruce
./build/bruce replay portfolio-potenzen.bruce
```

Die Beispielpaare folgen `x³+x`. In diesem begrenzten Auftrag findet das Ausdruckskind die passende Formel mit fünf Baumknoten. Der Gesamtstatus lautet `VERIFIED_ON_SAMPLES`: Lern- und Prüfpaare stimmen; ein allgemeiner Beweis der Datenbeziehung folgt daraus nicht.

`--operators` gilt ausschließlich für das Ausdruckskind. Die separate Potenzfamilie untersucht hier durch `--max-power 2` nur Exponenten bis 2; das Ausdruckskind darf trotzdem die deklarierte Konstante 3 als Exponenten verwenden. Seine feste Regel lautet weiterhin: literale ganzzahlige Exponenten von 2 bis 8 aus dem Konstantenvorrat. Variable, negative oder gebrochene Exponenten sind nicht enthalten.

`--expression-planner grammar` verwendet den bisherigen Generator; `indexed-grammar` nutzt den Grammatikindex. `--planner` wählt unabhängig davon die Reihenfolge des Portfolios: `portfolio`, `adaptive-order` oder `experience-order`. Ohne Operatoroption bleibt die Grammatik `+-*/` aktiv.

## Gemessener Beispielaufwand

Bei fester Reihenfolge und indiziertem Ausdrucksplaner:

| Messgröße | `+*`, sieben Baumknoten | `+^`, fünf Baumknoten |
| --- | ---: | ---: |
| Ausdruckskandidaten | 1884 | 130 |
| Gesamte logische Portfolioarbeit | 62278 | 3717 |

Beide Aufträge finden dieselbe bestätigte Formelgruppe. Die anderen vier Teilruns sind identisch. Die Messung vergleicht unterschiedliche Grammatiken und Knotengrenzen; sie belegt keine allgemeine CPU-Beschleunigung.

## Erfahrung für eine neue Aufgabe verwenden

```bash
./build/bruce experience add --from-run portfolio-potenzen.bruce \
  --output portfolio-potenzen.experience

./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar --operators '+^' \
  --planner experience-order --experience portfolio-potenzen.experience \
  --min 1 --max 3 --max-power 2 --max-degree 2 --max-nodes 5 \
  --train examples/expression-power-target-training.csv \
  --holdout examples/expression-power-target-holdout.csv \
  --output portfolio-potenzen-erfahrung.bruce
./build/bruce replay portfolio-potenzen-erfahrung.bruce
```

Die neue Aufgabe verwendet andere Lern- und Prüfpaare. Die Quelle vergibt die Stimmen `[0,0,0,0,1]`, sodass die Ausdruckssuche zuerst läuft. Alle fünf Familien bleiben Teil der Prüfung; die vollständigen Hypothesen stimmen mit der adaptiven Suche derselben Grammatik überein. Die Demo misst 3532 logische Einheiten für die neue Suche und separat 3764 für die Quellenprüfung.

Erfahrung wirkt nur bei passender Operatorauswahl, passendem Ausdrucksplaner, gleichen Suchgrenzen und den übrigen Kontextbedingungen. Quellen ohne Potenzoperator beeinflussen diesen Auftrag nicht. Alte und neue Quellen dürfen gemeinsam in einem V6-Speicher liegen; alte Record-IDs bleiben erhalten. Gleiche Lernpaare werden weder mehrfach aufgenommen noch als eigene Erfahrungsstimmen verwendet.

Ein erfahrungsbasierter Lauf darf nicht wieder als neue Erfahrungsquelle dienen. Mit `experience revoke` kann eine Quelle widerrufen werden. Frühere Forschungsdateien behalten ihren damaligen Snapshot und bleiben reproduzierbar.

## Auswahl und Nachvollziehbarkeit

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run portfolio-potenzen-erfahrung.bruce --at 0 \
  --output portfolio-potenzen-auswahl.bruce
./build/bruce replay portfolio-potenzen-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielauswahl liefert `SELECTED` mit Wert 0. Definitionslücken bleiben erhalten: `(1/x)^2` ist bei x=0 gesperrt. Unvollständige Gesamtbefunde dürfen nicht zur Auswahl verwendet werden. Die Projektion bleibt auf Int64-Werte begrenzt.

Neue Potenzportfolios verwenden V17/V18/V19, passende Erfahrungsspeicher V6. Forschung und Auswahl lassen sich nach Entfernen der ursprünglichen Quell-, Speicher- und Eingabedateien reproduzieren. Die Demo prüft zusätzlich Widerruf und Auswahl-Replay ohne ursprüngliche Forschungsdatei. Bestehende Ausgabedateien werden nicht überschrieben.

## Weitere Roadmap

Literale Potenzausdrücke sind jetzt in Portfolio und Erfahrungsplanung integriert. Allgemeinere Operatoren und Grammatiken, weitere Suchoptimierungen, vollständigere Definitionsbereichsvergleiche, externe Evidenz und weitergehende Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Das Stimmenranking ist kein allgemeines Lernen und kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](49-portfolio-powers-contract.md) · [Abnahme](acceptance/portfolio-powers-v1-2026-09-22.md)

**Fortsetzung:** [Core 0.24](52-cached-expression-usage.md) ergänzt einen optionalen Cache für kleine Teilbäume in der eigenständigen Ausdruckssuche. Seine Portfolio-/Experience-Einbindung bleibt ein Folgepaket.
