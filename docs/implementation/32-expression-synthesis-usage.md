# Core 0.14: Kleine Formeln aus Rechenoperationen zusammensetzen

BRUCE kann jetzt kleine **Ausdrucksbäume aus `x`, rationalen Konstanten und `+`, `−`, `×`, `÷`** durchsuchen. Damit ist erstmals eine begrenzte Kombination der Rechenoperationen möglich, ohne vorher eine einzelne Polynom- oder Bruchfamilie festzulegen.

## Das erste Beispiel

```bash
cmake --build build
./build/bruce research --strategy expression \
  --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output ausdruck-forschung.bruce
./build/bruce replay ausdruck-forschung.bruce
```

Die Lernwerte sind `(1,1/2)`, `(2,4/3)`, `(3,9/4)`, `(4,16/5)`. Die getrennten Prüfpaare lauten `(0,0)` und `(5,25/6)`. Als Bausteine stehen hier `x` und die Konstante `1` zur Verfügung. BRUCE untersucht alle **2802** kanonischen Bäume bis sieben Knoten und benötigt **103126 logische Operationseinheiten**, davon **69064 für die Erzeugung und Planung**.

Gefundene Lernkandidaten:

| Ausdruck | Hold-out-Befund | Definitionsbedingungen |
| --- | --- | --- |
| `x − x/(x+1)` | `VERIFIED_ON_SAMPLES` | `x+1 != 0` |
| `x * (x/(x+1))` | `VERIFIED_ON_SAMPLES` | `x+1 != 0` |
| `x / (1+1/x)` | `REFUTED` | unter anderem `x != 0` |
| `x / ((x+1)/x)` | `REFUTED` | unter anderem `x != 0` |
| `(x*x)/(x+1)` | `VERIFIED_ON_SAMPLES` | `x+1 != 0` |

Die beiden widerlegten Kandidaten scheitern an der endlichen Ausgabe bei `x=0`. Die anderen drei Schreibweisen bleiben als eigene Bäume erhalten: **Gesamtstatus `AMBIGUOUS`**. BRUCE behauptet hier keine automatische allgemeine Gleichheitsprüfung zwischen ihnen. Der Replay reproduziert auch diese Mehrdeutigkeit und ihre Prüfspur.

## Eigene Suchgrenzen

```bash
./build/bruce research --strategy expression \
  --min -1 --max 2 --max-denominator 2 --max-nodes 5 \
  --train meine-lernpaare.csv --holdout meine-pruefpaare.csv \
  --max-candidates 10000 --max-operations 1000000
```

Eine CSV-Datei beginnt mit `x,y`; Zahlen und exakte Brüche wie `3/7` sind erlaubt. Lern- und Hold-out-x-Werte müssen getrennt sein. Die Grenzen bedeuten:

- `--min` und `--max`: Zähler der gekürzten Konstanten, nicht Grenzwerte sämtlicher Zwischen- oder Endergebnisse. Standard `[0,1]`.
- `--max-denominator`: positive Nenner der Konstanten; Standard 1, höchstens 16. Das Produkt aus Zählerbreite und Nennergrenze darf höchstens 64 sein.
- `--max-nodes`: gesamte Baumgröße einschließlich Blättern. Erlaubt sind 1, 3, 5, 7 oder 9; Standard 7. `x+1` hat drei Knoten, `(x*x)/(x+1)` sieben.
- `--max-bits`: Bitgrenze für rationale Werte und für die ganzzahligen Zwischenrechnungen der Gegenprüfung; Standard 4096.
- Die vorhandenen Kandidaten-, Operations-, Ergebnis- und Tracegrenzen gelten weiter. Eine Überschreitung ergibt `INCOMPLETE`.

Planer `grammar` und Zahlenbereich `rational` werden automatisch gewählt. Grad- und Potenzoptionen passen nicht zu dieser Strategie und werden abgewiesen. `--max-nodes` gehört ausschließlich zur Ausdrucksstrategie.

Mehr Knoten vergrößern den Raum stark. Die maximale Konfiguration beschreibt 1327858375545 Bäume, erzeugt aber jeweils nur den nächsten benötigten Baum. Sie kann mit den Arbeitsbudgets normalerweise nicht vollständig untersucht werden. Diese Zahl ist weder Speicherverbrauch noch Geschwindigkeitsnachweis.

## Was zusammengeführt wird – und was erhalten bleibt

Bei Addition und Multiplikation entfällt die reine Vertauschung: `x+1` und `1+x` werden einmal erzeugt. Andere algebraische Umformungen werden nicht angewandt. Unterschiedliche Klammerungen oder Verteilungen können weiterhin mehrere Treffer liefern.

Divisionen behalten sämtliche Bedingungen: `x/x` ist bei `x=0` undefiniert, auch wenn sein Wert an anderen Stellen `1` ist. Ebenso bleibt `0*(1/x)` bei `x=0` undefiniert. `domain_conditions` nennt alle Divisoren, einschließlich der inneren Bedingungen verschachtelter Brüche. Die Ausgabe enthält zusätzlich den konkreten Baum unter `expression`.

Vorwärtsauswertung und Gegenrechnung gehen getrennte Wege. Die zweite Rechnung verwendet ganzzahlige Zähler-/Nennerpaare. Falls ein Kreuzprodukt ihr Bitlimit überschreitet, bleibt die Suche unvollständig – selbst wenn eine gekürzte rationale Rechnung kleinere Werte hätte. Es wird weder gerundet noch eine erforderliche Gegenprüfung ausgelassen.

`grammar_expressions` beschreibt den vollständig geplanten Raum; bei einer bereits unterbrochenen Grammatikplanung ist diese Zahl noch nicht verfügbar und steht auf 0. `candidates_executed` zählt begonnene Kandidaten. Ohne Hold-out bleiben Treffer Kandidaten. `NO_CANDIDATE` schließt nur die vollständig untersuchte Grammatik aus, nicht alle denkbaren Formeln.

## Projektion und Demo

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run ausdruck-forschung.bruce --at 0 \
  --output ausdruck-auswahl.bruce
./build/bruce replay ausdruck-auswahl.bruce
python3 scripts/demo.py
```

Die drei positiv geprüften Ausdrücke liefern bei `x=0` jeweils `0`; die Auswahl bleibt bei diesen Regeln mehrdeutig. Bei `x=-1` wird die Division durch null ausdrücklich abgewiesen. Nichtganzzahlige Werte passen weiterhin nicht in die bestehende Int64-Z-Projektion.

Forschungs-Replay funktioniert ohne ursprüngliche CSV-Dateien, Auswahl-Replay auch ohne die ursprüngliche Forschungsdatei. Exportdateien dürfen noch nicht existieren; vorhandene Ergebnisse werden nicht überschrieben.

## Weitere Roadmap

Eine erste feste, begrenzte Ausdrucksgrammatik ist implementiert. Portfolio-/Experience-Einbindung dieser Strategie, allgemeinere Operatoren und Grammatiken, effizientere Suchplanung sowie algebraische Gleichheitsprüfung mit erhaltener Definitionsmenge bleiben offen. Ebenso weitergehende Experience-Korrekturketten, externe Evidenz, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Sampleprüfungen liefern weiterhin keinen allgemeinen Beweis; dies ist kein vollständiger M2-/Gesamtprojektabschluss.

Fortsetzung: [Core 0.15](34-expression-equivalence-usage.md) ergänzt inzwischen die optionale rationale Normalisierung mit erhaltenen Ausschlussbedingungen. Der hier beschriebene Standardmodus bleibt unverändert.
