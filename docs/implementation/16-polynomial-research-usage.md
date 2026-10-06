# BRUCE Core 0.6 – mehrtermige Polynomforschung

BRUCE untersucht jetzt auch Polynome mit mehreren unabhängigen Koeffizienten: `c0+c1*x+c2*x²+...+cd*xᵈ`. Damit gehören beispielsweise `x²+x+1` und `2*x³-x²+3*x+1` zum ausführbaren Suchraum. Die bisherige affine und Potenzforschung bleibt verfügbar.

## Ausprobieren

```bash
cmake --build build
python3 scripts/demo.py
./build/bruce research --strategy polynomial \
  --train examples/polynomial-training.csv \
  --holdout examples/polynomial-holdout.csv
```

Lernpaare: `(0,1)`, `(1,3)`, `(2,7)`, `(3,13)`. Zurückgehaltene Prüfpunkte: `(-3,7)`, `(5,31)`. BRUCE erzeugt **`1+x+x²`**, prüft beide Rechenwege und meldet `VERIFIED_ON_SAMPLES`. Im JSON lautet die Koeffizientenliste `["1","1","1"]`, aufsteigend vom konstanten Term bis zum höchsten Grad.

```bash
./build/bruce research --strategy polynomial \
  --train examples/polynomial-training.csv \
  --holdout examples/polynomial-counterexample.csv
```

Das Gegenbeispiel `(4,99)` ergibt `REFUTED`, denn `1+4+4²=21`.

## Familie und Einstellungen

| Option | Verhalten |
| --- | --- |
| `--strategy polynomial` | untersucht Grade 0 bis zum Maximalgrad |
| `--max-degree` | Standard 3, zulässig 0–8 |
| `--domain bigint` | Standard; ganzzahlige Eingaben und endgültige Koeffizienten |
| `--domain rational` | exakte Brüche als Eingaben und Koeffizienten |
| `--min`, `--max` | Zählergrenzen jedes gekürzten Koeffizienten; Standard -8 bis 8 |
| `--max-denominator` | BigInt: 1; rational: Standard 8, maximal 16 |
| `--max-bits` | Standard 4096, maximal 16384 pro Zahlenkomponente |
| `--planner interpolate` | fester Planer für diese Strategie; muss nicht angegeben werden |

Jeder Koeffizient, auch ein innerer Nullkoeffizient, muss im angegebenen Koeffizientenraum liegen. Der Koeffizient des höchsten Grades muss außer beim Grad 0 ungleich null sein. Ein konstantes Polynom wird einmal als Grad 0 geführt; zusätzliche Nullterme erzeugen keine konkurrierenden Kandidaten. Das Nullpolynom wird ebenfalls als Grad 0 dargestellt.

`--max-power` sowie die Planer `adaptive`/`enumerate` gehören nicht zur Polynomialstrategie und werden zurückgewiesen. Die Gradoption wird bei anderen Strategien ebenfalls zurückgewiesen. Die bestehenden Dateiformate (`x,y`), Sample-/Zählerbereichsgrenzen und allgemeinen Budgets gelten weiter. Konfiguration und Eingaben werden tatsächlich verarbeitet; keine fertige Antwort wird nachgeschlagen.

## Ableitung und unabhängige Auswertung

Für Grad d verwendet BRUCE die ersten d+1 unterschiedlichen Lern-x. Newtons dividierte Differenzen liefern exakt rationale Koeffizienten in der Newton-Basis; eine anschließende exakte Umrechnung liefert die Monomkoeffizienten c0..cd. Zwischenwerte dürfen auch beim BigInt-Ziel rational sein. Nichtganzzahlige endgültige Koeffizienten werden in einer BigInt-Familie als außerhalb des Suchraums behandelt.

Danach folgen drei getrennte Schritte:

1. Alle Lernpaare werden erneut mit dem abgeleiteten Polynom geprüft. Zusätzliche Lernpaare jenseits der d+1 Anker können es widerlegen.
2. Die Vorwärtsauswertung nach Horner wird durch eine getrennte Potenzsumme gegengeprüft. Die Gegenprüfung übernimmt keinen berechneten Horner-Wert.
3. Erst ein passender Lernkandidat wird an Hold-out-Paaren geprüft. Diese Daten gehen weder in die Ableitung noch in die Koeffizientenauswahl ein.

Horner arbeitet beispielsweise als `(c2*x+c1)*x+c0`. Die Gegenprüfung berechnet `c0+c1*x+c2*x²` mit eigenen Potenzen und Summenschritten. Beide verwenden dasselbe exakte GMP-Zahlenbackend. Das sind zwei Rechenwege, keine zwei externen Beweisinstanzen.

**Interpolation ist kein allgemeiner Beweis.** d+1 verschiedene x bestimmen höchstens ein Polynom vom Grad höchstens d. Daraus folgt nicht, dass ein unbekannter Prozess diesem Polynom folgt. `VERIFIED_ON_SAMPLES` bezieht sich ausschließlich auf die angegebenen geprüften Paare; `general_proof`, `learn_gate` und `publish_gate` bleiben `false`.

## Zu wenige Daten und Status

Um den gesamten Suchraum bis D abschließend zu behandeln, benötigt dieser Planer mindestens D+1 verschiedene Lern-x. Bei weniger Lernpaaren endet der Gesamtbefund mit **`INCOMPLETE / InsufficientTrainingSamples`**. Bereits passende niedrigere Grade bleiben als Teilbefunde sichtbar.

Beispiel: `(0,1)` und `(1,3)` bestimmen die Gerade `1+2*x`. Bei `--max-degree 3` ist damit die Suche nach allen zugelassenen höheren Graden nicht abgeschlossen. BRUCE bezeichnet die Gerade deshalb nicht als eindeutiges Gesamtergebnis. Mit `--max-degree 1` kann der auf Geraden und Konstanten begrenzte Lauf abschließen. Hold-out-Paare werden nicht heimlich zu zusätzlichen Lernankern umgewandelt.

Bei ausreichend vielen Lernpaaren kann innerhalb des maximalen Grades höchstens ein kanonisches Polynom alle Lernpaare erfüllen. Die üblichen Status lauten dann `CANDIDATE`, `VERIFIED_ON_SAMPLES`, `REFUTED` oder `NO_CANDIDATE`. Ein fehlender Kandidat bezieht sich nur auf die vorgegebenen Grad- und Koeffizientengrenzen. Ressourcenstopps ergeben `INCOMPLETE`.

## Budgets, Trace und Replay

Die Ableitung wird schrittweise gezählt: jede Addition/Subtraktion, Multiplikation und Division reserviert eine logische Operationseinheit vor Ausführung. Pro Sampleprüfung reserviert die Auswertung konservativ `6*d+4` Einheiten für beide Rechenwege. Parser, Größenprüfung, Vergleiche und feste Buchführung zählen nicht als GMP-Instruktionsmessung. `planning_operations` ist ein Anteil von `operations_used`.

`plans_completed` zählt abgeschlossene Ableitungen, `candidates_executed` tatsächlich an Samples geprüfte Polynome. Für diese Strategie wird kein erfundener Rohraumzähler aus der früheren Zwei-Koeffizienten-Familie ausgegeben. Höchstens neun Grade werden behandelt; die Interpolation hält höchstens neun Anker und Koeffizienten. Sie legt keinen riesigen Koeffizientenproduktraum an.

Die Trace nennt Grad, verwendete Anker, Ableitung, Ausschluss durch Koeffizientengrenzen, übersprungenen niedrigeren Grad und erste Widersprüche oder Stopgründe. Vor Plan- und Kandidatenereignissen werden je 512 freie Trace-Textbytes für Ereignis und Abschluss verlangt; JSON-Struktur zählt wie bisher nicht zu `trace_bytes`. Ereignisse zählen außerdem zum bestehenden Trace-Ereignisbudget.

Jeder relevante arithmetische Zwischenschritt unterliegt dem Bitlimit. Ein zu großer Wert in Ableitung, Umrechnung, Horner oder Gegenprüfung stoppt als `INCOMPLETE / IntegerBitLimit`. Unterschiedliche Anker-/Rechenreihenfolgen können unterschiedliche Zwischenwerte haben; die Grenze ist bewusst konservativ. Bereits passende Kandidaten machen einen später gestoppten Gesamtlauf nicht vollständig.

Abbruch wird an Planungs- und Kandidatengrenzen kontrolliert. Der gespeicherte Checkpoint stellt sicher, dass Replay auch einen Stop zwischen Ableitung und Auswertung exakt wiederholt.

```bash
./build/bruce research --strategy polynomial \
  --train examples/polynomial-training.csv --holdout examples/polynomial-holdout.csv \
  --output polynom-forschung.bruce
./build/bruce replay polynom-forschung.bruce
./build/bruce compatibility --rules examples/polynomial-selection.rules \
  --from-run polynom-forschung.bruce --at -3 --output polynom-auswahl.bruce
./build/bruce replay polynom-auswahl.bruce
```

Die Z-Projektion berechnet tatsächlich das Polynom und erhält bei x=-3 den Wert 7. Im lokalen Forschungslauf entspricht die Kandidaten-ID dem Grad; die Koordinaten sind `(Grad,0)`. Der Kontext lautet `polynomial-bigint-v1` beziehungsweise `polynomial-rational-v1`. Z-Werte müssen unverändert exakt ganzzahlig und int64-darstellbar sein. Die Auswahl behält den mathematischen Ursprungsstatus bei.

`BRUCE_POLYNOMIAL_BUNDLE_V1` enthält Engine `bruce-polynomial-v1`, Regel `polynomial-v1`, Policy `polynomial-samples-v1`, Planer `newton-v1`, Gradgrenze, Eingaben, Zahlenraum, Budgets, Abbruchposition und das vollständige Ergebnis. Replay berechnet alles neu; ursprüngliche CSV-Dateien werden nicht benötigt. Ein Z-Paket bettet seinen Ursprung vollständig ein. Historische Versionen bleiben unterstützt. Nur `--output` speichert; vorhandene Dateien werden nicht überschrieben.

## Weitere Roadmap

Mehrtermige Polynome bis Grad 8 sind als weitere exakte Strategie implementiert. [Core 0.7](18-strategy-portfolio-usage.md) ergänzt inzwischen einen gemeinsamen Lauf aus Potenz- und Polynomstrategie mit Restbudgets und erhaltener Formelherkunft. Allgemeine Formelsynthese mit freier Ausdrucksgrammatik, rationale Funktionen, gelerntes Strategieranking, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.
