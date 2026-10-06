# Core 0.21: Ausgewählte Rechenarten gemeinsam mit fünf Suchwegen nutzen

Die Operatorauswahl ist jetzt im Fünferportfolio verfügbar – mit beiden Ausdrucksplanern und fester, adaptiver oder erfahrungsbasierter Reihenfolge. Die Auswahl gilt **nur für die Ausdruckssuche**. Potenz-, Polynom- und beide Bruchfunktionsfamilien behalten ihre bisherigen Suchräume.

## Starten

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar --operators '+*' \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 5 \
  --train examples/selected-operators-training.csv \
  --holdout examples/selected-operators-holdout.csv \
  --output portfolio-rechenarten.bruce
./build/bruce replay portfolio-rechenarten.bruce
```

Das Beispiel folgt `x²+x`. BRUCE untersucht alle fünf Familien und findet die passende Formelgruppe über die Ausdruckssuche. Deren Schreibweisen `x*(x+1)` und `x+x*x` werden mit ihren Herkünften zusammengeführt. Der Status ist `VERIFIED_ON_SAMPLES`; ein allgemeiner Beweis der Datenbeziehung wird daraus nicht abgeleitet.

`--operators` legt die Rechenarten des Ausdruckskindes fest. `--expression-planner` wählt dessen Baumgenerator. `--planner` bestimmt die Reihenfolge der fünf Familien. Diese Entscheidungen sind unabhängig voneinander. Ohne Operatoroption gilt weiter `'+-*/'`; `'*+'` wird kanonisch wie `'+*'` behandelt.

## Gemessener Unterschied

Bei fester Reihenfolge und indiziertem Ausdrucksplaner liefert die Demo:

| Messgröße | Vollständige Ausdrucksgrammatik | Ausdrucksgrammatik `+*` |
| --- | ---: | ---: |
| Ausdrücke im fünften Teilrun | 184 | 32 |
| Gesamte logische Portfolioarbeit | 5431 | 1822 |

Die anderen vier Teilruns sind identisch. Beide Gesamtbefunde finden dieselbe bestätigte Normalform. Die kleinere Ausdrucksgrammatik ist ein ausdrücklich engerer Suchauftrag; sie beweist nicht, dass andere Rechenarten unnötig wären. Die Werte sind logische Arbeitseinheiten, keine gemessene CPU-Beschleunigung.

Ein Ausdruckskind mit `--operators '*'` kann beispielsweise `x+1` nicht bilden. Andere Familien können diese Formel trotzdem finden. Die Operatorauswahl verbietet dem Gesamtportfolio deshalb weder Addition noch Division außerhalb des Ausdruckskindes.

## Erfahrung speichern und einsetzen

```bash
./build/bruce experience add --from-run portfolio-rechenarten.bruce \
  --output portfolio-rechenarten.experience

./build/bruce research --strategy portfolio-expression \
  --expression-planner indexed-grammar --operators '+*' \
  --planner experience-order --experience portfolio-rechenarten.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 5 \
  --train examples/selected-operators-target-training.csv \
  --holdout examples/selected-operators-target-holdout.csv \
  --output portfolio-rechenarten-erfahrung.bruce
./build/bruce replay portfolio-rechenarten-erfahrung.bruce
```

Die passende Quelle vergibt `[0,0,0,0,1]` Stimmen. BRUCE beginnt dadurch mit der Ausdruckssuche und führt trotzdem alle fünf Familien aus. Die neue Zielsuche benötigt in der Demo 1644 logische Einheiten; die separat ausgewiesene Nachweisprüfung der Quelle benötigt 1869. Die vollständigen Hypothesen stimmen mit der adaptiven Suche derselben Grammatik überein.

Erfahrungen gelten nur bei passender Operatorauswahl, passendem Ausdrucksplaner und den übrigen bereits geltenden Kontextbedingungen. Eine Quelle mit `'+*'` beeinflusst weder `'+-*'` noch die vollständige Grammatik. Alte und neue Quellen dürfen gemeinsam im Speicher liegen; bestehende Record-IDs bleiben erhalten.

Gleiche Lernpaare werden nicht mehrfach als Erfahrung aufgenommen und nicht als eigene Erfahrungsstimme genutzt. Aus einem erfahrungsbasierten Lauf kann weiterhin keine neue Erfahrungsquelle entstehen. Ein Widerruf mit `experience revoke` entfernt den Planungseinfluss. Frühere Exporte behalten ihren damaligen Snapshot und bleiben reproduzierbar.

## Auswahl und Demo

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run portfolio-rechenarten-erfahrung.bruce --at 0 \
  --output portfolio-rechenarten-auswahl.bruce
./build/bruce replay portfolio-rechenarten-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielauswahl liefert `SELECTED` mit Wert 0. Enthält die gewählte Grammatik Division, gelten deren Definitionslücken weiter. So bleibt die Gruppe für `x²/(x+1)` bei x=−1 gesperrt. Abgebrochene Gesamtbefunde bleiben unvollständig und dürfen nicht zur Auswahl verwendet werden.

Die Demo prüft unveränderte andere Familien, Arbeitsvergleich, passende Stimmen, Widerruf und Replay nach Entfernen der ursprünglichen Quell-, Speicher- und Forschungsdateien. Neue Teilgrammatik-Portfolios schreiben V14/V15/V16, ihre Erfahrungsspeicher V5. Die vollständige Standardgrammatik behält ihre bisherigen Exportformate. Vorhandene Dateien werden nicht überschrieben.

## Weitere Roadmap

Die Operatorauswahl ist in Portfolio und Erfahrungsplanung integriert. Neue Operatorarten, freiere Grammatiken, weitere Suchoptimierungen, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](45-portfolio-operators-contract.md) · [Abnahme](acceptance/portfolio-operators-v1-2026-09-22.md)

**Fortsetzung:** [Core 0.22](48-expression-powers-usage.md) ergänzt optionale literale Potenzen in der eigenständigen Ausdruckssuche. Ihre Portfolio-/Experience-Einbindung bleibt ein separates Folgepaket.
