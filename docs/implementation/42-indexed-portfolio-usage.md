# Core 0.19: Den Grammatikindex gemeinsam mit fünf Suchwegen nutzen

BRUCE kann den sparsameren Ausdrucksplaner jetzt auch im Fünferportfolio verwenden – mit fester, datenabhängiger oder erfahrungsbasierter Reihenfolge. Alle fünf Familien werden weiterhin geprüft; die vollständigen Formelfunde behalten ihre Herkünfte und Definitionsbedingungen.

## Starten

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output portfolio-indiziert.bruce
./build/bruce replay portfolio-indiziert.bruce
```

`--expression-planner` steuert die Erzeugung der Ausdrucksbäume. `--planner` steuert die Reihenfolge der fünf Suchfamilien: `portfolio` ist fest, `adaptive-order` verwendet die ersten Lernpaare, `experience-order` nutzt zusätzlich passende Erfahrungsstimmen. Beide Entscheidungen sind unabhängig wählbar. Ohne die neue Option bleibt der bisherige Ausdrucksplaner aktiv.

## Gemessener Unterschied

Die mitgelieferten Paare folgen `x²/(x+1)`. Im vollständigen festen Portfolio stimmen die Hypothesen einschließlich IDs, Status, Normalformen und Herkunft mit dem bisherigen Planer überein.

| Logische Arbeit, feste Reihenfolge | Bisheriger Planer | Grammatikindex |
| --- | ---: | ---: |
| Gesamte Suche und Zusammenführung | 105744 | 93523 |
| Ersparnis | — | 12221 |

Im Ausdruckskind bleiben es 2802 Kandidaten; der Index hält nur 20 Rangintervalle für bis zu sieben Baumknoten. Bei Operationsbudget 93523 ist der neue Beispielrun vollständig und `VERIFIED_ON_SAMPLES`, der bisherige `INCOMPLETE`.

Die Zahlen sind logische Operationseinheiten. Sie belegen geringere Arbeit im verwendeten Kostenmodell, keine gemessene CPU-Beschleunigung. Bei kleinen Aufgaben kann der Indexaufbau mehr kosten, als er spart. Samplebestätigung ist weiterhin kein allgemeiner Beweis der Datenbeziehung.

## Erfahrung nutzen

Der zuvor gespeicherte vollständige Lauf kann als unabhängige Quelle dienen:

```bash
./build/bruce experience add --from-run portfolio-indiziert.bruce \
  --output portfolio-indiziert.experience

./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar --planner experience-order \
  --experience portfolio-indiziert.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output portfolio-indiziert-erfahrung.bruce
./build/bruce replay portfolio-indiziert-erfahrung.bruce
```

Die passende Erfahrung vergibt `[0,0,0,0,1]` Stimmen und lässt die Ausdruckssuche zuerst beginnen. Die neue Zielsuche benötigt in der Demo 90433 Operationseinheiten; alle fünf Familien werden vollständig ausgeführt. Die Demo verwendet eine adaptive Quelle und weist deren zusätzliche Nachweisprüfung separat mit 93617 Einheiten aus. Diese Prüfung gehört nicht zur neuen Suche und ist keine kostenlose Vorarbeit.

Erfahrungen des bisherigen Ausdrucksplaners und des Indexplaners können gemeinsam gespeichert werden, beeinflussen aber jeweils nur ihre passende Planung. Alte Record-IDs bleiben bei der Erweiterung erhalten. Identische Lernpaare dürfen nicht doppelt als Erfahrung eingetragen werden, auch nicht mit einem anderen Planer. Ein Lauf nutzt seine eigenen Lernpaare nicht als Erfahrungsstimme. Aus erfahrungsbasierten Läufen lassen sich keine neuen Erfahrungsquellen ableiten.

Widerruf funktioniert wie bisher mit `experience revoke --store DATEI --id RECORD_ID --reason TEXT --output NEUE_DATEI`. Danach fällt der betroffene Einfluss weg. Bereits gespeicherte Forschungsläufe behalten ihren damaligen Erfahrungssnapshot und bleiben ohne die ursprünglichen Quell- und Speicherdateien replaybar. Vorhandene Exportdateien werden nicht überschrieben.

## Auswahl und Prüfung

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run portfolio-indiziert-erfahrung.bruce --at 0 \
  --output portfolio-indiziert-auswahl.bruce
./build/bruce replay portfolio-indiziert-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielauswahl liefert bei x=0 den Wert 0 mit `SELECTED`. Bei x=-1 bleibt die Polstelle gesperrt. Die Demo prüft Arbeitsersparnis, gleiche Befunde, passende Erfahrungsstimmen, Widerruf und Replay ohne ursprüngliche Dateien.

Neue indizierte Portfolios schreiben V11/V12/V13; ihre Erfahrungsspeicher V4. Alte Portfolios und historische Exporte bleiben reproduzierbar. Der Index bleibt budgetiert; unvollständige Familien verhindern weiterhin einen vollständigen Gesamtbefund.

## Weitere Roadmap

Die Einbindung des Grammatikindex in Portfolio und Erfahrungsplanung ist implementiert. Weitere Suchoptimierungen, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](41-indexed-portfolio-contract.md) · [Abnahme](acceptance/indexed-portfolio-v1-2026-09-22.md)

**Fortschreibung Core 0.20:** Die eigenständige Ausdruckssuche unterstützt jetzt [auswählbare Rechenarten](44-expression-operators-usage.md). Das Fünferportfolio verwendet weiterhin alle vier Grundrechenarten.
