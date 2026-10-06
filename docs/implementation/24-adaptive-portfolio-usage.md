# Core 0.10: datenabhängige Strategieplanung

BRUCE kann die Reihenfolge seiner drei Forschungsstrategien jetzt aus den Lernpaaren ableiten. Der neue optionale Planer `adaptive-order` erkennt einfache Hinweise auf affine, quadratische und Kehrwert-Beziehungen. Er ordnet die vorhandenen Strategien; alle Familien bleiben Teil der Suche.

## Starten

```bash
cmake --build build
./build/bruce research --strategy portfolio-rational --planner adaptive-order \
  --train examples/fraction-training.csv \
  --holdout examples/fraction-holdout.csv --max-degree 2
```

Für die Lernpaare `(0,1)`, `(1,1/2)`, `(2,1/3)` sind die Kehrwerte der Ausgaben `1,2,3`. Der Planer erkennt deren linearen Verlauf und beginnt mit der Bruchfunktionssuche. Ergebnisreihenfolge: `rational-linear`, `power-affine`, `polynomial`. Die eigentliche Bruchstrategie leitet anschließend `1/(x+1)` ab und prüft es separat an Lern- und Hold-out-Paaren.

Die Ausgabe enthält zusätzlich:

| Feld | Bedeutung |
| --- | --- |
| `strategy_order` | geplante Reihenfolge; bei abgebrochener Vorprüfung leer |
| `ordering_reason` | nachvollziehbarer Musterhinweis; vor erfolgreicher Planung `not_started` |
| `ordering_operations` | Arbeit der Vorprüfung, bereits in der Gesamtarbeit enthalten |
| `strategy_runs` | tatsächlich gestartete Teilstrategien in Ausführungsreihenfolge |
| `trace` | Entscheidung und Gesamtstatus; innere Prüfspuren stehen in den Teilruns |

## Was der Planer prüft

Der Planer nutzt höchstens die ersten vier Lernpaare in Dateireihenfolge. Er vergleicht exakte dividierte Differenzen, funktioniert also auch bei ungleichen Abständen der Eingaben.

| Hinweis | Ausführungsreihenfolge |
| --- | --- |
| erste drei Paare liegen auf einer Geraden | Potenz → Polynom → Bruchfunktion |
| sonst: Kehrwerte der ersten drei Ausgaben liegen auf einer Geraden, alle drei Ausgaben ungleich null | Bruchfunktion → Potenz → Polynom |
| sonst: erste vier Paare haben gleiche zweite dividierte Differenzen | Polynom → Potenz → Bruchfunktion |
| kein solcher Hinweis oder weniger als drei Paare | Potenz → Polynom → Bruchfunktion |

Die Prüfungen erfolgen in dieser Reihenfolge. Die Eingabe wird nicht umsortiert; eine andere Dateireihenfolge kann einen anderen Hinweis erzeugen. Hold-out-Daten werden nicht zur Planung verwendet. Ein Musterhinweis darf an weiteren Lernpaaren scheitern: Keine Familie wird deswegen ausgeschlossen, kein Ergebnis als bewiesen markiert.

## Gemessener Unterschied im Beispiel

```bash
./build/bruce research --strategy portfolio-rational --planner adaptive-order \
  --train examples/fraction-training.csv --holdout examples/fraction-holdout.csv \
  --max-degree 2 --max-candidates 1
```

| Lauf | tatsächlich geprüfte Kandidaten | logische Operationseinheiten | erreichter Befund |
| --- | ---: | ---: | --- |
| feste Reihenfolge, Kandidatenbudget 1 | 1 | 47 | kein Treffer, Gesamtstatus INCOMPLETE |
| datenabhängige Reihenfolge, Kandidatenbudget 1 | 1 | 119 | Bruchfunktion besteht Hold-out, Gesamtstatus weiterhin INCOMPLETE |
| feste Reihenfolge, vollständiger Beispiellauf | 8 | 436 | VERIFIED_ON_SAMPLES |
| datenabhängige Reihenfolge, vollständiger Beispiellauf | 8 | 454 | derselbe fachliche Befund |

Die Vorprüfung kostet hier 18 Operationseinheiten. Der Vorteil ist der frühere passende Kandidat im begrenzten Lauf; eine allgemeine Beschleunigung wird daraus nicht abgeleitet. Die vollständige Suche erledigt weiterhin alle Familien und benötigt in diesem Beispiel zusätzliche Planungsarbeit. Die Werte sind logische Arbeitseinheiten, keine Laufzeitmessung.

Ein früher Treffer schließt andere Familien nicht ab. `INCOMPLETE` bleibt deshalb erhalten, und ein solcher Lauf kann nicht als abgeschlossene Z-Auswahl importiert werden.

## Grenzen, Export und Replay

Alle bisherigen Optionen und Familiengrenzen gelten weiter. Der Planer ist nur für `--strategy portfolio-rational` zugelassen. `--planner portfolio` bleibt die feste Reihenfolge und der Standard. Die Vorprüfung zählt gegen Operations-, Zahlen- und Tracegrenzen; ein Abbruch ist replaybar. Reicht das Bitlimit schon für eine Differenz oder einen Kehrwert nicht aus, endet der Lauf mit `IntegerBitLimit` und `INCOMPLETE`.

Das Portfolio benötigt wie bisher positive Restbudgets zum Start einer Teilstrategie. Auch wenn diese später ohne Kandidatenausführung einen Widerspruch ableiten könnte, startet sie bei bereits ausgeschöpftem Kandidatenbudget nicht. Reservierter Tracespeicher kann größer als die anschließend ausgegebene Textlänge sein.

```bash
./build/bruce research --strategy portfolio-rational --planner adaptive-order \
  --train examples/fraction-training.csv --holdout examples/fraction-holdout.csv \
  --max-degree 2 --output geplanter-lauf.bruce
./build/bruce replay geplanter-lauf.bruce
./build/bruce compatibility --rules examples/fraction-selection.rules \
  --from-run geplanter-lauf.bruce --at 0 --output geplante-auswahl.bruce
./build/bruce replay geplante-auswahl.bruce
```

Exportdateien dürfen noch nicht existieren. Das V3-Paket enthält die Eingaben und den vollständigen Befund. Replay berechnet auch Reihenfolge, Begründung und Planungskosten erneut. Historische Pakete bleiben unterstützt. Z wertet die Funktion aus: bei x=0 entsteht Wert 1; Nennernullstellen und nichtganzzahlige Projektionswerte bleiben ausdrücklich unzulässig. Definitionslücken werden nicht weggekürzt.

```bash
python3 scripts/demo.py
```

Die Demo zeigt vollständigen Replay und den Vergleich unter einem Kandidatenbudget von 1.

## Weitere Roadmap

Die erste datenabhängige Reihenfolge über drei begrenzte Formelfamilien ist implementiert. Es handelt sich um eine versionierte Heuristik auf einem kurzen Lernpräfix, nicht um gelerntes Ranking. [Core 0.11](26-experience-routing-usage.md) ergänzt einen begrenzten Routing-Experience-Store mit geprüftem Stimmenranking und Widerrufen. Umfassendes Erfahrungslernen, allgemeinere rationale Funktionen, freie Ausdruckssynthese, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Sampleprüfungen sind kein allgemeiner Beweis; dies ist kein vollständiger M2-/Gesamtprojektabschluss.
