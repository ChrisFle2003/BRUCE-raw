# Core 0.20: Die Rechenarten einer Ausdruckssuche auswählen

BRUCE kann jetzt gezielt mit ausgewählten Rechenarten nach Formeln suchen. Wenn eine Aufgabe beispielsweise nur Addition und Multiplikation untersuchen soll, genügt `--operators '+*'`. Dadurch wird der Suchraum kleiner und die Aussage des Ergebnisses genauer eingegrenzt.

## Beispiel: x²+x finden

```bash
cmake --build build
./build/bruce research --strategy expression --planner indexed-grammar \
  --operators '+*' --equivalence rational \
  --min 1 --max 1 --max-nodes 5 \
  --train examples/selected-operators-training.csv \
  --holdout examples/selected-operators-holdout.csv \
  --output ausgewaehlte-rechenarten.bruce
./build/bruce replay ausgewaehlte-rechenarten.bruce
```

BRUCE erhält die Lernpaare `(1,2), (2,6), (3,12), (4,20)` und prüft zusätzlich `(0,0), (5,30)`. Die Ausdrücke `x*(x+1)` und `x+x*x` passen. Mit rationaler Zusammenführung entsteht eine Formelgruppe mit `VERIFIED_ON_SAMPLES`; die unterschiedlichen Ursprungsbäume bleiben sichtbar. Im Syntaxmodus bleiben sie getrennt und ergeben `AMBIGUOUS`.

## Was kleiner wird

Bei gleicher Konstante 1, fünf Knoten, indiziertem Planer und rationaler Zusammenführung misst die Demo:

| Messgröße | Alle vier Rechenarten | Nur `+*` |
| --- | ---: | ---: |
| Untersuchte Ausdrücke | 184 | 32 |
| Logische Operationseinheiten | 5206 | 1597 |

Beide Räume finden in diesem Beispiel dieselbe bestätigte Normalform. Die kleinere Grammatik enthält jedoch grundsätzlich weniger mögliche Formeln. Sie ist nur dann der passende Suchauftrag, wenn diese Einschränkung gewünscht ist. Das ist keine automatische mathematische Begründung, andere Rechenarten wegzulassen, und keine gemessene CPU-Beschleunigung.

## Auswahl und Bedeutung

Erlaubt sind alle nichtleeren Kombinationen aus `+`, `-`, `*` und `/`, jeweils einmal. Beispiele: `'+'`, `'+*'`, `'-/'`, `'+-*/'`. Die Anführungszeichen schützen die Zeichen vor der Shell. `'*+'` wird ebenso wie `'+*'` kanonisch als `'+*'` gespeichert. Doppelte oder unbekannte Zeichen werden abgewiesen.

Ohne die Option gilt weiter `'+-*/'`. Die Auswahl funktioniert mit `grammar` und `indexed-grammar` sowie mit `syntax` und `rational`. Die Kandidaten-IDs gehören jeweils zur gewählten Grammatik; gleiche Zahlen aus verschiedenen Operatorräumen bezeichnen nicht notwendigerweise denselben Ausdruck.

Ein bewusst engerer Auftrag zeigt die Aussagegrenze:

```bash
./build/bruce research --strategy expression --planner indexed-grammar \
  --operators '+' --equivalence rational \
  --min 1 --max 1 --max-nodes 5 \
  --train examples/selected-operators-training.csv \
  --holdout examples/selected-operators-holdout.csv
```

Das Ergebnis ist `NO_CANDIDATE`: Nur mit Addition, diesen Terminalen und höchstens fünf Knoten passt kein Ausdruck. Über andere Grammatiken sagt das nichts aus. Bei erschöpftem Budget lautet der Befund weiterhin `INCOMPLETE`.

Auch eine einzelne Division ist als Operatorauswahl möglich. Eine passende Formel wie `1/x` bleibt bei x=0 undefiniert; der kleinere Suchraum hebt keine Definitionsbedingungen auf. Sampleprüfungen bleiben Prüfungen an den angegebenen Punkten und sind kein allgemeiner Beweis der Datenbeziehung.

## Auswahl und Replay verwenden

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run ausgewaehlte-rechenarten.bruce --at 0 \
  --output ausgewaehlte-rechenarten-auswahl.bruce
./build/bruce replay ausgewaehlte-rechenarten-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielprojektion liefert `SELECTED` mit Wert 0. Replay enthält die Operatorauswahl und den vollständigen Forschungsnachweis; Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht. Vorhandene Exportdateien werden nicht überschrieben.

Teilgrammatiken schreiben Ausdrucks-Replay V4. Die vollständige Standardgrammatik behält ihre bisherigen Versionen und historischen Replays.

## Weitere Roadmap

Explizite Operatorauswahl ist für die eigenständige Ausdruckssuche implementiert. Ihre Einbindung in Portfolio und Erfahrungsplanung, neue Operatorarten, benutzerdefinierte Grammatiken, weitere Suchoptimierungen, vollständigere Definitionsbereichsvergleiche, externe Evidenz und Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](43-expression-operators-contract.md) · [Abnahme](acceptance/expression-operators-v1-2026-09-22.md)

**Fortschreibung Core 0.21:** Die [Operatorauswahl im Fünferportfolio und seiner Erfahrungsplanung](46-portfolio-operators-usage.md) ist inzwischen implementiert. Sie betrifft dort ausschließlich die Ausdrucksfamilie.
