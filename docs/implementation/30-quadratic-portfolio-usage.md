# Core 0.13: Vier Formelfamilien gemeinsam untersuchen

BRUCE kann jetzt Potenzen, Polynome sowie Bruchfunktionen mit linearem und quadratischem Nenner **in einem Lauf** untersuchen. Das neue Viererportfolio teilt die Arbeitsgrenzen, erklärt seine Reihenfolge und kann passende Erfahrungen aus früheren geprüften Läufen berücksichtigen.

## 1. Gemeinsam suchen

```bash
cmake --build build
./build/bruce research --strategy portfolio-quadratic --max-degree 2 \
  --planner adaptive-order \
  --train examples/quadratic-fraction-training.csv \
  --holdout examples/quadratic-fraction-holdout.csv \
  --output vierer-forschung.bruce
./build/bruce replay vierer-forschung.bruce
```

Die Daten passen zu `1/(x²+1)`. BRUCE beginnt hier mit `rational-quadratic` und untersucht danach auch Potenzen, Polynome und lineare Bruchfunktionen. Der vollständige Lauf liefert `VERIFIED_ON_SAMPLES` mit erhaltener Nennerbedingung.

| Planer | Verhalten |
| --- | --- |
| `portfolio` (Standard) | feste Reihenfolge: Potenz, Polynom, linearer Nenner, quadratischer Nenner |
| `adaptive-order` | begrenzte Formhinweise aus den ersten Lernpaaren bestimmen die Reihenfolge |
| `experience-order` | passende belegte Erfahrungen gewichten den datenabhängigen Plan |

`strategy_runs` zeigt die tatsächlich gestarteten Familien samt Einzelbefunden. Bei den beiden datenabhängigen Planern zeigt `strategy_order` den vorgesehenen Gesamtplan. Ein Budgetabbruch kann verhindern, dass alle geplanten Familien starten. Ein früher Treffer schließt die offene Suche nicht ab: Der Gesamtstatus bleibt `INCOMPLETE`.

Die Heuristik prüft Steigungen und zweite dividierte Differenzen, direkt und an Kehrwerten. Bei gleichzeitigem quadratischem Direkt- und Kehrwerthinweis hat der Kehrwerthinweis Vorrang. Das kann die Suchreihenfolge verbessern oder verschlechtern; es ersetzt keine Kandidatenprüfung. Hold-out-Werte beeinflussen die Planung nicht.

## 2. Erfahrung aus einem früheren Lauf übernehmen

```bash
./build/bruce research --strategy portfolio-quadratic --max-degree 2 \
  --train examples/quadratic-experience-source-training.csv \
  --holdout examples/quadratic-experience-source-holdout.csv \
  --output vierer-quelle.bruce
./build/bruce experience add --from-run vierer-quelle.bruce \
  --output vierer-erfahrung.snapshot
./build/bruce experience inspect vierer-erfahrung.snapshot
```

Die Quelle untersucht `(x+2)/(x²+1)`. Vor der Aufnahme führt BRUCE ihren Replay aus. Die Erfahrung enthält Stimmen `[0,0,0,1]` – eine Stimme für die vierte Familie, die ihre Hold-out-Prüfung bestanden hat.

Die Stimmen stehen immer in der Reihenfolge **Potenz, Polynom, linearer Nenner, quadratischer Nenner**. Quelle, Lernpaare und Grenzen bleiben nachvollziehbar. Unvollständige Läufe oder Läufe ohne Hold-out werden nicht aufgenommen.

## 3. Neue Aufgabe anhand dieser Erfahrung planen

```bash
./build/bruce research --strategy portfolio-quadratic --max-degree 2 \
  --planner experience-order --experience vierer-erfahrung.snapshot \
  --train examples/quadratic-experience-target-training.csv \
  --holdout examples/quadratic-experience-target-holdout.csv \
  --output vierer-ziel.bruce
./build/bruce replay vierer-ziel.bruce
```

Die neue Aufgabe folgt `(2*x+3)/(x²+1)`. Ohne Erfahrung beginnt ihre Formheuristik mit Potenzen; mit passender Erfahrung beginnt sie mit quadratischen Bruchfunktionen. Alle vier Familien werden weiter geprüft. Im Beispiel bleibt der vollständige fachliche Befund gleich.

Der gemessene neue Suchlauf benötigt **662 logische Operationseinheiten**; die separate Nachweisprüfung benötigt **648**. Das ist keine pauschale Beschleunigung. Mit `--max-candidates 1` wird der passende Bruchkandidat zwar früh positiv geprüft, die offene Gesamtuntersuchung bleibt aber `INCOMPLETE`.

`experience_votes`, `experience_records_used`, `experience_digest` und `experience_audit_operations` zeigen die verwendete Erfahrung und ihre Kosten. Nur gleiche Familienmenge, gleicher Formhinweis und gleiche Zahlen-/Koeffizienten-/Grad-/Potenzgrenzen passen. Die gleichen Lernpaare liefern keine Stimme für sich selbst.

## 4. Erfahrung widerrufen oder alte Speicher weiterführen

```bash
./build/bruce experience revoke --store vierer-erfahrung.snapshot \
  --id EINTRAGS_ID --reason "Erfahrung nach Gegenpruefung zurueckgezogen" \
  --output vierer-widerrufen.snapshot
```

Die ID steht in der `inspect`-Ausgabe. Verwende den neuen Snapshot für folgende Suchen. Die widerrufene Quelle bleibt dokumentiert, zählt aber nicht mehr fürs Ranking. Frühere Exporte enthalten ihren damaligen Zustand und bleiben ohne ursprüngliche Quell- oder Storedateien replaybar.

Vorhandene Dreier-Erfahrungsspeicher kannst du mit `experience add --store ALT --from-run NEUE_QUELLE --output NEU` erweitern. Der neue Snapshot erhält beim ersten Viererquelllauf die Version V2. Bestehende Ereignisse und IDs bleiben erhalten; die alte Datei wird nicht verändert. Gemischte Speicher zeigen je Quelle drei oder vier Stimmen. Dreierquellen beeinflussen nur Dreieraufgaben, Viererquellen nur Viereraufgaben. Der bestehende Aufruf `--strategy portfolio-rational` behält weiterhin drei Familien.

## Auswahl und Grenzen

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run vierer-forschung.bruce --at 0 \
  --output vierer-auswahl.bruce
./build/bruce replay vierer-auswahl.bruce
python3 scripts/demo.py
```

Für `1/(x²+1)` ergibt die Projektion an `x=0` den Wert `1`. Nennernullstellen, nichtganzzahlige Werte und Werte außerhalb Int64 bleiben ausdrücklich abgewiesen.

- Koeffizienten standardmäßig Zähler `[-8,8]`, Nenner bis 8. Zählerbreite höchstens 4096 und Nennergrenze höchstens 16; zusätzlich muss ihr Produkt höchstens 32768 sein.
- `--max-power` begrenzt die Potenzfamilie, `--max-degree` die Polynomfamilie. Die beiden Bruchfamilien behalten ihre festen Formen.
- Unterschiedliche Nennerdarstellungen bleiben getrennt; Kürzung oder allgemeine Funktionsgleichheit werden nicht zur Zusammenführung verwendet. Dadurch können auch gleichwertige Darstellungen als mehrere Kandidaten erhalten bleiben.
- Erfahrung bleibt optional: ohne Experience-Aufruf oder `--experience` ist kein Erfahrungsspeicher erforderlich.
- Maximal 1 MiB, acht Quellen und 16 Ereignisse pro Snapshot. Quellbudget höchstens 1000000 Operationseinheiten. Mit Erfahrung geplante V4-/V7-Läufe dürfen nicht selbst als neue Quellen dienen.
- Exportdateien dürfen noch nicht existieren. Digests prüfen Integrität, sie sind keine externen Signaturen.

## Weitere Roadmap

Das Viererportfolio einschließlich datenabhängiger Reihenfolge, Erfahrungsranking, Widerruf und Replay ist implementiert. Höhergradige rationale Familien, allgemeine Ausdruckssynthese, algebraische Gleichheitsprüfung bei erhaltener Definitionsmenge, weitergehende Experience-Korrekturketten und externe Evidenz bleiben offen. Ebenso Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung sowie BigInt im Grid und in Z-Prädikaten. Sampleprüfungen sind kein allgemeiner Beweis; dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

Fortschreibung: [Core 0.14](32-expression-synthesis-usage.md) ergänzt eine erste kleine Ausdrucksgrammatik als eigenständige Strategie. Sie ist noch nicht Teil dieses Viererportfolios oder dessen Erfahrungsranking.
