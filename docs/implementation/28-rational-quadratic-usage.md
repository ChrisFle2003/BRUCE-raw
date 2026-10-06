# Core 0.12: Bruchfunktionen mit quadratischem Nenner

BRUCE untersucht jetzt auch **`(a*x+b)/(x²+c*x+d)`**, beispielsweise `1/(x²+1)`. Die vier Koeffizienten werden exakt aus deinen Lernpaaren abgeleitet. Bei Mehrdeutigkeit durchsucht BRUCE den angegebenen Koeffizientenbereich innerhalb der Arbeitsgrenzen.

## Ausführen und erneut prüfen

```bash
cmake --build build
./build/bruce research --strategy rational-quadratic \
  --train examples/quadratic-fraction-training.csv \
  --holdout examples/quadratic-fraction-holdout.csv \
  --output quadratische-forschung.bruce
./build/bruce replay quadratische-forschung.bruce
```

Die Lernpaare sind `(0,1)`, `(1,1/2)`, `(2,1/5)`, `(3,1/10)`. Zwei zurückgehaltene Paare `(-2,1/5)` und `(4,1/17)` prüfen den gefundenen Kandidaten. Ergebnis: `a=0, b=1, c=0, d=1`, also `1/(x²+1)`, mit Status `VERIFIED_ON_SAMPLES`.

Im mitgelieferten Beispiel untersucht BRUCE **einen Kandidaten** mit **238 logischen Operationseinheiten**, davon **94 für die Ableitung**. Die Standardgrenzen beschreiben 342102016 Rohkombinationen; sie werden weder vorab gespeichert noch bei eindeutigem Gleichungssystem durchlaufen. Diese Zahl ist kein Laufzeit- oder Beschleunigungsnachweis.

Der Replay funktioniert auch ohne die ursprünglichen CSV-Dateien. Die Exportdatei darf noch nicht existieren; wähle für einen weiteren Export einen neuen Namen.

## Eigene Werte und Grenzen

Eingabedateien besitzen den Kopf `x,y`. Ganze Zahlen und exakte Brüche wie `3/7` sind erlaubt. Lern- und Hold-out-Datei müssen verschiedene x-Werte enthalten. Vier verschiedene Lernpaare ermöglichen Rang 4, garantieren ihn aber nicht. Mehr Daten können Widersprüche aufdecken.

```bash
./build/bruce research --strategy rational-quadratic \
  --train meine-lernpaare.csv --holdout meine-pruefpaare.csv \
  --min -4 --max 4 --max-denominator 4 \
  --max-candidates 10000 --max-operations 1000000
```

`--min` und `--max` begrenzen die Zähler der gekürzten Koeffizienten; `--max-denominator` ihre positiven Nenner. Diese Grenzen gelten für alle vier Koeffizienten. Standard: Zähler `[-8,8]`, Nenner bis 8, rationale Zahlen, Planer `solve`. Die feste Formelfamilie verwendet keine Grad- oder Potenzoption; `--max-degree` und `--max-power` werden hier ausdrücklich abgewiesen.

Maximale Zählerbreite 4096, Nennergrenze 16; zusätzlich muss das Produkt aus Breite und Nennergrenze höchstens 32768 sein. Das hält Kandidaten- und Z-Adressen innerhalb der vorhandenen 64-Bit-Darstellung. Große vollständige Suchräume können dennoch am Arbeitsbudget enden: Der Status ist dann `INCOMPLETE`, keine negative Gesamtaussage.

Ohne Hold-out bleibt ein einzelner Treffer `CANDIDATE`. Mehrere überlebende Tupel bleiben `AMBIGUOUS`. Widersprüchliche Lerndaten oder ein eindeutig abgeleitetes Tupel außerhalb deiner Familie ergeben bei abgeschlossener Suche `NO_CANDIDATE`.

## Nennernullstellen bleiben sichtbar

Die Ausgabe enthält `numerator: [b,a]`, `denominator: [d,c,1]` und `domain_condition`, zum Beispiel `x^2+(0)*x+(-2) != 0`. Die Bedingung bleibt exakt, auch wenn die Nullstellen irrational sind. BRUCE rundet sie nicht und entfernt keine Definitionslücken durch Kürzen.

```bash
./build/bruce research --strategy rational-quadratic \
  --train examples/quadratic-pole-training.csv \
  --holdout examples/quadratic-pole-holdout.csv
```

Hier passen die Lernwerte zu `1/(x²−1)`. Der Hold-out verlangt an `x=1` eine endliche Ausgabe. Dort ist der Nenner null: Ergebnis `REFUTED`, im Trace ausdrücklich als `pole` erklärt. Die Beispiele prüfen einen Definitionsbereich; das Scheitern dieses Kandidaten beweist keine Aussage über alle denkbaren Funktionen.

## Ergebnis in der Z-Auswahl verwenden

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run quadratische-forschung.bruce --at 0 \
  --output quadratische-auswahl.bruce
./build/bruce replay quadratische-auswahl.bruce
```

Für `1/(x²+1)` liefert die Projektion bei `x=0` den Wert `1`. Der Auswahl-Replay enthält den Ursprungsrun. Bei `x=1` ist das Ergebnis `1/2`; die bisher ganzzahlige Z-Auswahl weist es ab. Nennernullstellen werden ebenfalls als Fehler gemeldet.

Die Gesamtdemo zeigt Forschung, Hold-out, Replay, Auswahl und einen Polstellen-Gegenfall:

```bash
python3 scripts/demo.py
```

## Weitere Roadmap

Quadratische Nenner sind als eigenständige begrenzte Strategie implementiert. Ihre Einbindung in Portfolio und Erfahrungsranking, höhere rationale Familien und freie Ausdruckssynthese bleiben offen. Ebenso offen bleiben vollständige Experience-Korrekturketten/externe Evidenz, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung sowie BigInt-Werte im Grid und in Z-Prädikaten. Sampleprüfungen sind kein allgemeiner Beweis; dies ist kein vollständiger M2-/Gesamtprojektabschluss.

Fortschreibung: [Core 0.13](30-quadratic-portfolio-usage.md) bindet diese Familie nun über `portfolio-quadratic` ins gemeinsame Portfolio und dessen Erfahrungsranking ein. Die hier beschriebenen Einzelaufrufe bleiben gültig.
