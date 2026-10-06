# Core 0.9: Potenzen, Polynome und Bruchfunktionen gemeinsam untersuchen

Der neue Modus `portfolio-rational` führt drei vorhandene Forschungsstrategien in einem Lauf zusammen: adaptive Potenzsuche, Polynomforschung und die Bruchfunktionsfamilie `(a*x+b)/(x+c)`. Jede Strategie untersucht dieselben Lernpaare unter einem gemeinsamen Budget. Die Ausgabe zeigt die Teilbefunde und die Herkunft jedes Kandidaten.

## Ausprobieren

```bash
cmake --build build
./build/bruce research --strategy portfolio-rational \
  --train examples/fraction-training.csv \
  --holdout examples/fraction-holdout.csv --max-degree 2
```

Die Lernpaare sind `(0,1)`, `(1,1/2)`, `(2,1/3)`. Die zurückgehaltenen Prüfpunkte sind `(-2,-1)` und `(3,1/4)`.

| Strategie | Ergebnis dieses Laufs |
| --- | --- |
| Potenzfamilie `a*x^p+b` | kein passender Lernkandidat |
| Polynome bis Grad 2 | ein Polynom passt zu den Lernpaaren, scheitert aber an Hold-out |
| Bruchfunktion `(a*x+b)/(x+c)` | `1/(x+1)` besteht Lern- und Hold-out-Prüfung |
| Gesamtbefund | `VERIFIED_ON_SAMPLES` mit erhaltenem ausgeschlossenen Wert `-1` |

Dieser Lauf prüft insgesamt acht Kandidaten und verbraucht 436 logische Operationseinheiten einschließlich 136 Planungseinheiten. Das widerlegte Polynom bleibt mit seinem Status sichtbar. Ohne Hold-out sind das Polynom und die Bruchfunktion noch gleichrangige Kandidaten.

```bash
python3 scripts/demo.py
```

Die Demo zeigt jetzt auch diesen Vergleich und wiederholt den gemeinsamen Lauf aus seinem Export.

## Einstellungen und Arbeitsgrenzen

| Option | Standard / Bedeutung |
| --- | --- |
| `--strategy portfolio-rational` | alle drei Familien in fester Reihenfolge |
| `--domain rational` | Standard und einzige zugelassene Domäne dieses Modus |
| `--planner portfolio` | Standard; versionierte Reihenfolge Potenz → Polynom → Bruchfunktion |
| `--max-power 4` | Exponenten 1 bis zur Grenze, maximal 8 |
| `--max-degree 3` | Polynomgrade 0 bis zur Grenze, maximal 8 |
| `--min -8 --max 8` | Grenzen der gekürzten Koeffizientenzähler |
| `--max-denominator 8` | Koeffizientennenner, maximal 16 |
| `--max-bits 4096` | Grenze exakter Zahlen und arithmetischer Zwischenergebnisse |

Die bisherigen Optionen `--max-candidates`, `--max-operations`, `--max-results`, `--max-trace`, `--max-trace-bytes` und `--output` gelten für den gemeinsamen Lauf. Es entsteht **kein neues Budget beim Strategiewechsel**. Das Ergebnisbudget zählt gespeicherte Quellhypothesen vor Zusammenführung. Ein erfolgreicher Teilbefund hebt einen Abbruch nicht auf: Der Gesamtstatus bleibt `INCOMPLETE`.

Für einen vollständigen Polynomlauf bis Grad d werden mindestens d+1 Lernpaare benötigt. Deshalb verwendet das Beispiel mit drei Lernpaaren `--max-degree 2`. Mit dem Standardgrad 3 bleibt der Gesamtbefund unvollständig, obwohl die Bruchfunktion bereits gefunden wird.

## Gleiche Werte sind nicht immer dieselbe Funktion

Polynomtreffer aus Potenz- und Polynomstrategie werden weiterhin zusammengeführt; beide Herkünfte bleiben erhalten. Bruchfunktionen behalten ihre Definitionslücken:

- `1` ist überall definiert;
- `x/x` ist bei `x=0` nicht definiert;
- `(x+1)/(x+1)` ist bei `x=-1` nicht definiert.

Diese drei Kandidaten bleiben getrennt. Für die Lernwerte `(2,1)` und `(3,1)` mit Koeffizientenzählern 0..1, Nennergrenze 1, Potenzgrenze 1 und Polynomgrad 1 lautet der Befund deshalb `AMBIGUOUS`. Ein Hold-out-Paar `(0,1)` widerlegt nur `x/x`; die anderen beiden Kandidaten bleiben gleichrangig.

Die Hypothesen tragen `kind=polynomial` oder `kind=rational-linear`. Bruchfunktionen enthalten zusätzlich `a,b,c`, `numerator`, `denominator` und `excluded_x`. Jeder Treffer besitzt seine ursprünglichen Strategie-IDs und Status unter `origins`.

## Export und Auswahl

```bash
./build/bruce research --strategy portfolio-rational \
  --train examples/fraction-training.csv --holdout examples/fraction-holdout.csv \
  --max-degree 2 --output gemeinsamer-bruchlauf.bruce
./build/bruce replay gemeinsamer-bruchlauf.bruce
./build/bruce compatibility --rules examples/fraction-selection.rules \
  --from-run gemeinsamer-bruchlauf.bruce --at 0 \
  --output gemeinsame-bruchauswahl.bruce
./build/bruce replay gemeinsame-bruchauswahl.bruce
```

Exportdateien dürfen noch nicht existieren. Das neue Paket `BRUCE_PORTFOLIO_BUNDLE_V2` enthält Eingaben, Parameter, vollständige Teilbefunde und Traces. Replay benötigt die ursprünglichen Eingabedateien nicht. Die Auswahl enthält ihr Ursprungspaket und kann ebenfalls ohne ursprüngliche Regeln/Exportdatei wiederholt werden.

Bei `--at 0` entsteht für die bestätigte Bruchfunktion Wert 1. Bei `--at -1` wird der Import mit `RationalFunctionPole` abgewiesen. Eine Polstelle wird auch dann nicht still übersprungen, wenn andere Kandidaten am selben Punkt definiert sind. Bei `--at 1` ist der Wert 1/2 und kann nicht in die weiterhin ganzzahlige Z-Projektion übernommen werden.

Der bisherige Modus `--strategy portfolio` bleibt das zweistrategische Portfolio aus Core 0.7; seine historischen Pakete und IDs bleiben unterstützt. `rational-linear` bleibt auch als Einzelstrategie verfügbar.

## Weitere Roadmap

Die gemeinsame Planung über Potenz-, Polynom- und begrenzte Bruchfunktionsstrategie ist implementiert. [Core 0.10](24-adaptive-portfolio-usage.md) ergänzt eine optionale datenabhängige Reihenfolge anhand eines kurzen Lernpräfixes. Höhergradige rationale Funktionen, freie Ausdruckssynthese, weitergehende adaptive Suchplanung, gelerntes Ranking, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Sampleprüfungen sind kein allgemeiner Beweis; dies ist kein vollständiger M2-/Gesamtprojektabschluss.
