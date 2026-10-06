# Core 0.25: Teilbäume im Portfolio wiederverwenden und Erfahrung nutzen

Der begrenzte Teilbaumcache steht jetzt im gemeinsamen Fünferportfolio zur Verfügung. Er kann mit fester, datenabhängiger oder erfahrungsbasierter Reihenfolge kombiniert werden. Alle fünf Suchfamilien bleiben Teil der Untersuchung.

## Gemeinsam suchen

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --expression-planner cached-grammar --min 1 --max 1 \
  --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv --output portfolio-cache.bruce
./build/bruce replay portfolio-cache.bruce
```

Das Beispiel findet die passende Formelgruppe für x²/(x+1). Ihre Bedingung x+1≠0 bleibt erhalten. Der Status `VERIFIED_ON_SAMPLES` bestätigt die angegebenen Lern- und Prüfpaare, keinen allgemeinen Beweis der Datenbeziehung.

`--expression-planner` wählt den Aufbau der Ausdrücke: `grammar`, `indexed-grammar` oder jetzt `cached-grammar`. `--planner` wählt unabhängig davon die Reihenfolge der fünf Familien: `portfolio`, `adaptive-order` oder `experience-order`.

Nur das Ausdruckskind verwendet den Cache. Er startet bei jedem Lauf leer und enthält höchstens 64 Teilbäume mit jeweils drei Knoten. Es werden keine Ergebnisse aus früheren Aufgaben übernommen. Die mathematischen Prüfungen bleiben vollständig aktiv.

## Gemessener Unterschied

Die Gesamtdemo vergleicht dieselbe Grammatik und dieselben Prüfungen bei fester Reihenfolge:

| Messgröße | Indexplaner | Cacheplaner |
| --- | ---: | ---: |
| Ausdruckskandidaten | 2802 | 2802 |
| Gesamte logische Portfolioarbeit | 93523 | 87086 |

Die vollständigen Hypothesen und die anderen vier Teilruns sind identisch. Die Ersparnis von 6437 Einheiten entsteht beim Aufbau der Ausdrücke. Bei einem gemeinsamen Budget von 87086 bleibt der Indexlauf unvollständig, während der Cachelauf vollständig ist.

Das sind logische Einheiten einschließlich Cacheverwaltung, keine CPU-Zeitmessungen. Bei anderen Aufgaben kann ein Cache zusätzlichen Aufwand verursachen; die Wahl bleibt optional. Treffer, Fehlzugriffe, Verdrängungen und belegte Einträge stehen im Ausdrucksteilrun.

## Passende Erfahrung verwenden

```bash
./build/bruce experience add --from-run portfolio-cache.bruce \
  --output portfolio-cache.experience

./build/bruce research --strategy portfolio-expression \
  --expression-planner cached-grammar \
  --planner experience-order --experience portfolio-cache.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output portfolio-cache-erfahrung.bruce
./build/bruce replay portfolio-cache-erfahrung.bruce
```

Die frühere Quelle vergibt `[0,0,0,0,1]` Stimmen: Die Ausdruckssuche läuft zuerst. Alle fünf Familien werden weiterhin ausgeführt. Die vollständigen Hypothesen stimmen mit der adaptiven Suche derselben Grammatik überein.

Die Demo misst 83996 logische Einheiten für die neue Zielsuche; 87133 weitere Einheiten für die Quellenprüfung werden separat ausgewiesen. Diese Prüfung ist zusätzlicher Aufwand. Erfahrung ist hier eine Hilfe zur Reihenfolge, kein Versprechen niedrigerer Gesamtkosten.

Erfahrung wirkt nur bei passender Cache-Regel, Operatorauswahl und den übrigen Kontextbedingungen. Ein ungecacheter Quelllauf beeinflusst diesen Cachekontext nicht. Alte und neue Quellen können gemeinsam gespeichert werden; alte Kennungen bleiben erhalten. Gleiche Lernpaare werden weder doppelt aufgenommen noch als eigene Erfahrungsstimme verwendet.

`experience revoke` entfernt den Planungseinfluss einer Quelle. Frühere Exporte behalten ihren damaligen Speicherstand. Ein erfahrungsbasierter Lauf darf weiterhin keine neue Erfahrungsquelle werden.

## Potenzen, Auswahl und Replay

`--operators '+^' --min 1 --max 3 --max-nodes 5` funktioniert ebenfalls mit dem Cacheplaner, beispielsweise mit den Dateien `examples/expression-power-training.csv` und `examples/expression-power-holdout.csv`. Exponenten bleiben unmittelbar deklarierte ganze Zahlen von 2 bis 8. Die anderen vier Familien behalten ihre eigenen Definitionen und Grenzen.

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run portfolio-cache-erfahrung.bruce --at 0 \
  --output portfolio-cache-auswahl.bruce
./build/bruce replay portfolio-cache-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielauswahl liefert `SELECTED` mit Wert 0. Bei x=−1 bleibt die Polstelle gesperrt. Unvollständige Gesamtbefunde dürfen nicht zur Auswahl verwendet werden; die Projektion bleibt auf Int64-Werte beschränkt.

Cacheportfolios schreiben V20/V21/V22, passende Erfahrungsspeicher V7. Forschung lässt sich ohne ursprüngliche Quell- und Speicherdateien reproduzieren; Auswahl-Replay funktioniert auch nach Entfernen der ursprünglichen Forschungsdatei. Jeder Replay baut den Teilbaumcache erneut auf. Bestehende Ausgabedateien werden nicht überschrieben.

## Weitere Roadmap

Der Teilbaumcache ist in Portfolio und Erfahrungsplanung integriert. Weitergehende Suchoptimierungen, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Das Stimmenranking ist kein allgemeines Lernen und kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](53-cached-portfolio-contract.md) · [Abnahme](acceptance/cached-portfolio-v1-2026-09-22.md)

Nächster Ausbau: [Core 0.26 – Polynombrüche und ihre rationalen Definitionsbereiche vergleichen](56-fraction-comparison-usage.md).
