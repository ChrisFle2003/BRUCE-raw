# BRUCE Core 0.5 – datenabhängige Suchplanung

BRUCE kann in seiner Potenzfamilie die Koeffizienten jetzt aus Lernpaaren ableiten und danach vollständig prüfen. Der optionale Planer wählt pro Exponent zwischen direkter Ableitung und bisheriger Kandidatensuche. Er nutzt keine gespeicherten Antworten und keine Hold-out-Daten zur Erzeugung.

## Ausprobieren

```bash
cmake --build build
python3 scripts/demo.py
./build/bruce research --strategy power-affine --planner adaptive \
  --train examples/power-training.csv --holdout examples/power-holdout.csv
```

Das Beispiel liefert weiterhin **`2*x^2+3`**, Status `VERIFIED_ON_SAMPLES`, Hypothesen-ID 470. Bei gleichen Suchgrenzen und Prüfpaaren wurden folgende Arbeitszähler gemessen:

| Verfahren | Ausgeführte Kandidaten | Logische Operationen | Trace-Textbytes |
| --- | ---: | ---: | ---: |
| `--planner enumerate` | 1105 | 13440 | 61995 |
| `--planner adaptive` | 4 | 226 | 503 |

Die 226 Operationseinheiten enthalten die Planung. Dies ist eine Messung der ausgeführten logischen Arbeit für dieses Beispiel, kein universeller Laufzeitfaktor. Bei ungeeigneten Lernpaaren kann die Planung Zusatzarbeit verursachen. Der Standard bleibt `enumerate`.

## Wie BRUCE die Suche einschränkt

Für jeden Exponenten p=1..`max-power` setzt BRUCE `u=x^p`. Zwei Lernpaare mit unterschiedlichen u bestimmen innerhalb dieser Familie höchstens ein Koeffizientenpaar:

```text
a = (y1-y0)/(u1-u0)
b = y0-a*u0
```

Der Planer verwendet das erste Lernpaar und sucht danach das erste mit abweichendem u. Er berechnet a und b exakt, kontrolliert die vorgegebenen Zähler-/Nennergrenzen und gibt höchstens diesen Kandidaten zur vollständigen Prüfung weiter. Bei BigInt müssen die Koeffizienten ganzzahlig sein. Für die Ableitung selbst werden begrenzte rationale Zahlen verwendet, damit ein nichtganzzahliger Quotient sichtbar als außerhalb der Ganzzahlfamilie erkannt wird.

Das ist eine notwendige Einschränkung innerhalb `a*x^p+b`: andere Koeffizienten können diese beiden Paare nicht gleichzeitig erfüllen. Daraus folgt kein Beweis, dass die gefundene Formel unbekannte Daten beschreibt. Alle erzeugten Kandidaten müssen weiterhin sämtliche Lernpaare und die getrennte Gegenrechnung bestehen. Erst danach werden Hold-out-Paare ausgewertet.

## Nachvollziehbare Planungsentscheidungen

Die Trace enthält je abgeschlossenem Planungsschritt ein `PLAN`-Ereignis:

| Entscheidung | Bedeutung |
| --- | --- |
| `derived` | ein Koeffizientenpaar ist aus zwei verschiedenen Projektionen bestimmt und liegt im Suchraum |
| `outside_family` | das einzige ableitbare Paar liegt außerhalb der Koeffizientengrenzen; auch bereits bei p=1 vertretene Konstanten werden hier bei höheren p ausgeschlossen |
| `inconsistent_projection` | gleiche u-Werte verlangen unterschiedliche y; kein Kandidat dieses Exponenten kann beide erfüllen |
| `fallback` | keine zwei verschiedenen u gefunden; bestehende vollständige Koeffizientensuche wird verwendet |

Die Ereignisse nennen p, die verwendeten nullbasierten Lernpaar-Indizes und bei Ableitung die Koeffizientenslots. Bei `fallback` bezeichnet `[0,0]` das Ausgangspaar ohne zweiten unabhängigen Anker; die übrigen Lernpaare wurden auf gleiche Projektion geprüft. Ein Planungslimit erhält eine eigene `stop`-Zeile und setzt den Gesamtstatus auf `INCOMPLETE`.

Beispiel: `(-2,4)` und `(2,4)` ergeben bei p=2 dieselbe Projektion. Diese beiden Paare allein bestimmen a,b nicht eindeutig; BRUCE darf nicht durch null dividieren und verwendet die vollständige Suche. Bei `(-2,1)` und `(2,2)` schließen dieselben Projektionen eine gemeinsame quadratische Formel aus. Ein einzelnes Lernpaar führt ebenfalls zur vollständigen Suche.

Die Lernpaarreihenfolge bestimmt die Wahl des Ankers und damit Trace sowie Rechenaufwand. Bei vollständig abgeschlossenen Läufen muss der fachliche Kandidatenbestand erhalten bleiben. Bei knappen Zahlen-/Arbeitsgrenzen können andere Anker unterschiedliche Abbrüche bewirken.

## Unveränderte Familie, exakte Grenzen

`--planner adaptive` ist ausschließlich mit `--strategy power-affine` erlaubt. BigInt, rationale Daten, Exponenten 1–8 und alle Koeffizienten-/Samplegrenzen entsprechen [Core 0.4](12-power-research-usage.md). Der Planer erweitert weder die Familie noch den Koeffizientenraum. IDs, Exponenten, Formeln und endgültige Kandidatenstatus bleiben bei vollständig abschließbaren Vergleichsläufen mit `enumerate` gleich.

Gleichrangige Formeln bleiben `AMBIGUOUS`. Auch ein guter Kandidat macht einen später durch Budget oder Bitgrenze gestoppten Gesamtlauf nicht vollständig. `NO_CANDIDATE` bedeutet weiterhin nur: kein passender Kandidat in der vollständig abgearbeiteten deklarierten Familie. Die Ableitung allein setzt keinen Verifikationsstatus; allgemeiner Beweis, Lernen und Veröffentlichung bleiben `false`.

## Budgets und Abbruch

Planung nutzt dasselbe Operationsbudget wie Kandidatenprüfungen. Eine Transformation x^p reserviert konservativ 2*p Einheiten; der Vergleich eines weiteren projizierten Paars reserviert zwei Einheiten. Die beiden Differenzen, Division, Multiplikation und Subtraktion zur Ableitung reservieren jeweils eine Einheit. Begrenzte strukturelle Arbeit wie Familienzuordnung, Slotberechnung, Parser und Buchführung zählt wie bei der bisherigen Kandidatenerzeugung nicht als Rechenoperation. Es handelt sich nicht um GMP-Instruktions- oder Zeitmessung.

`planning_operations` ist der Planungsanteil von `operations_used`, keine zusätzliche ungezählte Arbeit. `plans_completed` zählt abgeschlossene Exponentenpläne. `candidates_executed` zählt nur die danach tatsächlich geprüften Kandidaten. `raw_parameter_combinations` beschreibt weiterhin den ursprünglichen Rohsuchraum.

Die Planungsarithmetik unterliegt derselben Bitgrenze. Auch wenn eine spätere Kürzung/Rechnung ein Ergebnis wieder kleiner machen könnte, stoppt ein unzulässiger Zwischenschritt als `INCOMPLETE / IntegerBitLimit`. Der Planer kann deshalb konservativ früher abbrechen als ein anderer Rechenweg. Er wandelt solche Fälle nicht in Ausschlüsse um.

Planungsentscheidungen zählen zum Ereignis- und Bytebudget. Vor einem Plan werden 512 freie Trace-Textbytes für die Entscheidung und den Abschluss reserviert; wie bisher zählt dies keine JSON-Strukturbytes. Mit kleinem Tracebudget kann ein Lauf bereits vor der Planung unvollständig enden.

`Ctrl+C` wirkt an Planungs- oder Kandidatengrenzen, nicht mitten in einer GMP-Operation. `checkpoints` zählt die freigegebenen Grenzen. Ein Export hält die genaue Abbruchgrenze fest: Ein Stop nach der Planung, aber vor dem ersten Kandidaten, wird beim Replay nicht versehentlich vor die Planung verschoben.

## Export und regelbasierte Auswahl

```bash
./build/bruce research --strategy power-affine --planner adaptive \
  --train examples/power-training.csv --holdout examples/power-holdout.csv \
  --output adaptive-forschung.bruce
./build/bruce replay adaptive-forschung.bruce
./build/bruce compatibility --rules examples/power-selection.rules \
  --from-run adaptive-forschung.bruce --at -3 --output adaptive-auswahl.bruce
./build/bruce replay adaptive-auswahl.bruce
```

Das neue `BRUCE_ADAPTIVE_BUNDLE_V1` speichert Engine `bruce-adaptive-v1`, Regel `power-affine-v1`, Policy `adaptive-samples-v1` und Planer `two-point-v1` zusammen mit dem vollständigen Aufgaben-/Ergebniskontext. Replay berechnet Planung, Kandidaten und Trace erneut. Quellen werden nicht nachgeladen. Z-Auswahlpakete enthalten den vollständigen Ursprung; ihre Projektions-/int64-Grenzen bleiben unverändert.

Historische Pakete aus Core 0.1 bis 0.4 werden weiterhin unterstützt. Die Standard-Einstellung und alten Ausgabeversionen bleiben erhalten. Speichern ist weiterhin nur mit `--output` vorgesehen; vorhandene Exporte werden nicht überschrieben.

## Weitere Roadmap

Die datenabhängige Auswahl zwischen direkter Koeffizientenableitung und vollständiger Suche ist für `a*x^p+b` implementiert. [Core 0.6](16-polynomial-research-usage.md) ergänzt inzwischen mehrtermige Polynome bis Grad 8 als weitere exakte Strategie. Allgemeine Formelsynthese, Planung über mehrere unterschiedliche Strategien, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen ebenfalls aus. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.
