# Core 0.22: Potenzen direkt in Ausdrücken verwenden

BRUCE kann jetzt bei der eigenständigen Ausdruckssuche Potenzen wie `x^3`, `(x+1)^2` oder `(1/x)^2` zusammensetzen. Der neue Operator `^` ist optional. Als Exponenten dienen deklarierte ganzzahlige Konstanten von 2 bis 8.

## Starten

```bash
cmake --build build
./build/bruce research --strategy expression --planner indexed-grammar \
  --operators '+^' --equivalence rational --min 1 --max 3 --max-nodes 5 \
  --train examples/expression-power-training.csv \
  --holdout examples/expression-power-holdout.csv --output ausdruck-potenz.bruce
./build/bruce replay ausdruck-potenz.bruce
```

Die Lernpaare folgen `x³+x`: 1→2, 2→10, 3→30, 4→68. Die getrennten Prüfpaare sind 0→0 und −2→−10. BRUCE findet die passende Formel mit `VERIFIED_ON_SAMPLES`. Das bestätigt diese Paare, nicht eine allgemeine Gesetzmäßigkeit unbekannter Daten.

Der Konstantenvorrat 1 bis 3 erlaubt hier die Exponenten 2 und 3. `x^3+x` benötigt fünf Baumknoten; `x*x*x+x` benötigt sieben. Für `(x+1)^2` müssen sowohl 1 als auch 2 im Konstantenvorrat liegen. `--max-power` verändert die Exponenten dieser Ausdruckssuche nicht.

## Gemessener Unterschied

Die Gesamtdemo vergleicht die indizierte Suche mit rationaler Zusammenführung:

| Messgröße | `+*`, sieben Knoten | `+^`, fünf Knoten |
| --- | ---: | ---: |
| Ausdruckskandidaten | 1884 | 130 |
| Logische Operationseinheiten | 62045 | 3484 |
| Ergebnis | VERIFIED_ON_SAMPLES | VERIFIED_ON_SAMPLES |

Mit `+*` und nur fünf Knoten findet die Suche keinen passenden Kandidaten. Die Messung vergleicht ausdrücklich unterschiedliche Grammatiken und Knotengrenzen. Sie ist kein allgemeiner Geschwindigkeitsnachweis; logische Einheiten sind keine CPU-Zeitmessung.

Der bisherige Planer `--planner grammar` unterstützt dieselben Kandidaten. `--equivalence syntax` hält verschiedene Schreibweisen getrennt; `rational` kann sie bei übereinstimmender Normalform und Ausschlussbedingung zusammenführen. Ohne `--operators` bleiben die bisherigen vier Grundrechenarten aktiv.

## Grenzen und Definitionslücken

- Der Exponent muss unmittelbar eine deklarierte Zahl von 2 bis 8 sein. `x^x`, gebrochene oder negative Exponenten und berechnete Exponentenausdrücke gehören nicht zur Grammatik.
- Enthält der Konstantenvorrat keine Zahl von 2 bis 8, werden keine Potenzknoten erzeugt. Andere zugelassene Ausdrücke und einzelne Terminals bleiben durchsuchbar.
- Negative und gebrochene Basen werden exakt ausgewertet. Definitionslücken bleiben erhalten: `(1/x)^2` und `(x/x)^2` sind bei x=0 weiterhin gesperrt.
- Verschachtelte Potenzen können die Bitgrenze oder den maximalen Normalformgrad 64 überschreiten. Der Status bleibt dann `INCOMPLETE`, mit genauer Abbruchursache und Replay.
- Das Fünferportfolio unterstützt `^` noch nicht. Die übrigen Grenzen für Suche, Trace und Ergebnisanzahl gelten unverändert.

## Auswahl und Demo

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run ausdruck-potenz.bruce --at 0 --output ausdruck-potenz-auswahl.bruce
./build/bruce replay ausdruck-potenz-auswahl.bruce
python3 scripts/demo.py
```

Die Auswahl liefert für das Beispiel `SELECTED` mit Wert 0. Neue Potenzausdrücke schreiben Ausdrucks-Bundles V5. Forschungs-Replay benötigt die ursprünglichen Eingabedateien nicht; Auswahl-Replay enthält zusätzlich den geprüften Forschungsnachweis. Bestehende Dateien werden nicht überschrieben. Die Projektion verwendet weiterhin den Int64-Wertebereich; größere exakte Forschungsergebnisse passen gegebenenfalls nicht hinein.

## Weitere Roadmap

Begrenzte literale Potenzen sind in der eigenständigen Ausdruckssuche implementiert. Ihre Portfolio-/Experience-Einbindung, allgemeinere Operatoren und Grammatiken, weitere Suchoptimierungen, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](47-expression-powers-contract.md) · [Abnahme](acceptance/expression-powers-v1-2026-09-22.md)

**Fortsetzung:** [Core 0.23](50-portfolio-powers-usage.md) integriert diese Potenzausdrücke in Portfolio und Erfahrungsplanung. Die obigen Grenzen beschreiben den damaligen Einzelstrategie-Ausbau.
