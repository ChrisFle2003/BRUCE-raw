# Core 0.8: rationale Bruchfunktionen

BRUCE untersucht jetzt zusätzlich die begrenzte Familie

```text
y = (a*x+b)/(x+c), mit x != -c
```

Dazu gehören Kehrwerte wie `1/x`, verschobene Kehrwerte wie `1/(x+1)` und allgemein lineare Zähler über monischen linearen Nennern. Die Koeffizienten werden aus den aktuellen Lernpaaren ermittelt; eine Aufgabendatenbank wird nicht benötigt.

## Starten

```bash
cmake --build build
./build/bruce research --strategy rational-linear \
  --train examples/fraction-training.csv \
  --holdout examples/fraction-holdout.csv
```

Die drei Lernpaare `(0,1)`, `(1,1/2)`, `(2,1/3)` bestimmen `a=0, b=1, c=1`. Die zurückgehaltenen Paare `(-2,-1)` und `(3,1/4)` bestätigen den Kandidaten. Ergebnis: `VERIFIED_ON_SAMPLES`, genau ein ausgeführter Kandidat, ausgeschlossener Wert `-1`.

```bash
python3 scripts/demo.py
```

Die Demo enthält diesen Ablauf, identischen Replay, einen widerlegenden Polstellenfall und die Projektion nach Z.

## Eigene Eingaben und Grenzen

CSV-Dateien enthalten `x,y` als Kopfzeile und exakte Ganzzahlen oder Brüche, etwa `-3/7`. Lern- und Hold-out-Eingaben müssen verschieden sein; doppelte x-Werte werden abgewiesen. Beide Dateien werden tatsächlich gelesen und geprüft. Hold-out-Werte beeinflussen die Koeffizientenableitung nicht.

| Option | Standard / Bedeutung |
| --- | --- |
| `--strategy rational-linear` | explizite Wahl dieser Familie |
| `--domain rational` | Standard und einzige zugelassene Zahlendomäne dieser Strategie |
| `--planner solve` | exaktes lineares Gleichungssystem, bei Rangmangel vollständige begrenzte Suche |
| `--min -8 --max 8` | inklusive Grenzen der **gekürzten Zähler** von a, b und c; Intervallbreite höchstens 4096 |
| `--max-denominator 8` | positive Nenner von a, b und c, maximal 16 |
| `--max-bits 4096` | Bitlimit auch für arithmetische Zwischenergebnisse, maximal 16384 |
| `--max-candidates 100000` | höchstens 100000 tatsächlich ausgeführte Kandidaten |
| `--max-operations 1000000` | logische Arbeit einschließlich Planung |
| `--max-results 128` | höchstens 128 gespeicherte Lernkandidaten |
| `--max-trace 100002` | Ereignisbudget einschließlich Start/Ende |
| `--max-trace-bytes 1048576` | Bytebudget der Prüfspur |

`--max-power` und `--max-degree` gehören nicht zu dieser festen Familie. Große Ganzzahlen sind als rationale Zahlen mit Nenner 1 möglich. BigInt-Werte im Grid oder in Z-Prädikaten werden dadurch nicht implementiert.

## Ableitung, Prüfung und Definitionsbereich

Für jedes Lernpaar bildet BRUCE `a*x+b-c*y=x*y`. Der exakte Solver verarbeitet die Zeilen bis zu einem Widerspruch oder bis alle Lernpaare einbezogen sind. Rang 3 bestimmt höchstens ein Tripel; liegt es außerhalb der angegebenen Koeffizientenfamilie, gibt es dort keinen Kandidaten. Bei geringerem Rang werden die kanonischen Koeffiziententripel lazy durchsucht. Ein widersprüchliches System schließt diese begrenzte Familie aus.

Jeder erzeugte Kandidat wird anschließend erneut an allen Lernpaaren geprüft: Quotient mit vorab geprüftem Nenner und getrennte expandierte Gegenrechnung `y*x+y*c=a*x+b`. Danach folgen die zurückgehaltenen Prüfpaare. Begrenzte Zwischenergebnisse können die Rechnung stoppen; dann lautet der Gesamtstatus `INCOMPLETE`.

**Nennernullstellen bleiben ausgeschlossen, auch bei Kürzbarkeit.** `x/x` liefert bei `x=0` keinen Wert. Die Kandidaten `x/x` und `(x+1)/(x+1)` haben unterschiedliche Definitionslücken und bleiben getrennt, selbst wenn beide auf bisherigen Daten konstant 1 ergeben. Dadurch ist eine ehrliche Mehrdeutigkeit möglich.

```bash
./build/bruce research --strategy rational-linear \
  --train examples/fraction-training.csv --holdout examples/fraction-pole.csv
```

Das Prüfdatum `(-1,0)` verlangt eine endliche Ausgabe an der Nennernullstelle. Es widerlegt den zuvor gefundenen Kandidaten: `REFUTED`, mit `holdout[0] pole` in der Prüfspur.

Die Ausgabe enthält `a,b,c`, die Koeffizientenlisten `numerator=[b,a]`, `denominator=[c,1]`, `excluded_x=-c`, Status und Trace. `NO_CANDIDATE` gilt nur für die vollständig untersuchte Familie. `VERIFIED_ON_SAMPLES` ist kein allgemeiner Beweis; Learn-/PublishGate bleiben geschlossen.

## Export, Replay und Z

```bash
./build/bruce research --strategy rational-linear \
  --train examples/fraction-training.csv --holdout examples/fraction-holdout.csv \
  --output bruchfunktion.bruce
./build/bruce replay bruchfunktion.bruce
./build/bruce compatibility --rules examples/fraction-selection.rules \
  --from-run bruchfunktion.bruce --at 0 --output bruchfunktion-auswahl.bruce
./build/bruce replay bruchfunktion-auswahl.bruce
```

Exportdateien dürfen noch nicht existieren. Der Forschungscontainer `BRUCE_RATIONAL_LINEAR_BUNDLE_V1` enthält Eingaben, Grenzen, Versionen, Befund und Trace. Replay führt die Forschung erneut aus; die Quelldateien sind dafür nicht nötig. Die Auswahl enthält ihr Ursprungspaket und funktioniert ebenfalls ohne ursprüngliche Dateien. Alte Forschungs-/Auswahlformate bleiben unterstützt.

Z wertet die tatsächliche Bruchfunktion aus: bei `--at 0` entsteht Wert 1. Bei `--at -1` folgt `RationalFunctionPole`; bei `--at 1` ist der Wert 1/2 und wird von der weiterhin ganzzahligen Z-Projektion abgewiesen. Es gibt keine Rundung oder stilles Entfernen eines Kandidaten.

Das Zweistrategien-Portfolio aus Core 0.7 bleibt unverändert: Es umfasst Potenz- und Polynomforschung. Diese neue Familie wird separat gewählt. Eine spätere Zusammenführung muss insbesondere Definitionslücken erhalten.

## Weitere Roadmap

Die begrenzte Bruchfunktionsfamilie `(a*x+b)/(x+c)` ist implementiert. [Core 0.9](22-rational-portfolio-usage.md) ergänzt inzwischen die gemeinsame Planung mit Potenz- und Polynomstrategie. Allgemeine rationale Funktionen mit höhergradigen Nennern, freie Ausdruckssynthese, gelerntes Strategieranking, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.
