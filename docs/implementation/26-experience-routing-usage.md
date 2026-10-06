# Core 0.11: Erfahrungen speichern und für neue Suchen verwenden

BRUCE kann jetzt geprüfte frühere Forschungsläufe als **optionale Erfahrung für die Suchreihenfolge** speichern. Das ist ein erster begrenzter Lernpfad: Welche Formelfamilie hat bei vergleichbaren Aufgaben bereits die Prüfdaten bestanden? Neue Aufgaben werden weiterhin selbst berechnet; die gespeicherten Ergebnisse ersetzen keine Prüfung.

## 1. Einen belegten Quelllauf erzeugen

```bash
cmake --build build
./build/bruce research --strategy portfolio-rational --max-degree 2 \
  --train examples/experience-source-training.csv \
  --holdout examples/experience-source-holdout.csv --output erfahrung-quelle.bruce
./build/bruce experience add --from-run erfahrung-quelle.bruce \
  --output erfahrung-01.snapshot
./build/bruce experience inspect erfahrung-01.snapshot
```

Der Quelllauf untersucht `(x+2)/(x+1)` und prüft ein zurückgehaltenes Paar. Beim Übernehmen führt BRUCE seinen Replay aus und berechnet die Strategie-Stimmen aus den tatsächlichen geprüften Hypothesen. Ein unvollständiger Lauf oder ein Kandidat ohne Hold-out genügt nicht.

`inspect` zeigt Ereigniszahl, aktive Einträge, unveränderliche IDs, Quell- und Eingabefingerprints, Stimmen sowie Widerrufsgründe. Auch `inspect` prüft die gespeicherten Quellläufe erneut.

## 2. Eine neue Aufgabe damit priorisieren

```bash
./build/bruce research --strategy portfolio-rational --planner experience-order \
  --experience erfahrung-01.snapshot --max-degree 2 \
  --train examples/experience-target-training.csv \
  --holdout examples/experience-target-holdout.csv --output erfahrung-ziel.bruce
./build/bruce replay erfahrung-ziel.bruce
```

Die neue Aufgabe folgt `(2*x+3)/(x+1)`. Die reine Formheuristik beginnt hier mit Potenzen. Mit der gespeicherten Erfahrung beginnt BRUCE mit der Bruchfunktionsstrategie. Alle drei Familien werden weiterhin untersucht; der vollständige fachliche Befund bleibt im Beispiel gleich.

| Ausgabefeld | Bedeutung |
| --- | --- |
| `experience_digest` | genau der verwendete Speicherstand |
| `experience_records_used` | in der Planung berücksichtigte aktive Einträge |
| `experience_votes` | Stimmen für Potenz, Polynom, Bruchfunktion, in dieser Reihenfolge |
| `experience_audit_operations` | separat ausgewiesene Kosten der Quelllaufprüfung |
| `strategy_order` | daraus tatsächlich geplante Reihenfolge |
| `ordering_reason` | Formhinweis, bei genutzter Erfahrung mit Präfix `experience_votes:` |

Im Beispiel ist die Stimmenverteilung `[0,0,1]`. Die neue vollständige Suche benötigt 495 logische Operationseinheiten; die separate Nachweisprüfung benötigt 488. Daraus folgt keine pauschale Beschleunigung. Mit `--max-candidates 1` wird der passende Bruchkandidat zuerst geprüft; der Gesamtstatus bleibt wegen der offenen Familien `INCOMPLETE`.

Nur Einträge mit gleichem Formhinweis und gleichen Zahlen-/Koeffizienten-/Grad-/Potenzgrenzen zählen. Ein bloß ähnlicher Dateiname genügt nicht. Aktuelle Hold-out-Daten beeinflussen die Planung nicht. Ohne passende aktive Einträge fällt die Reihenfolge auf die bisherige Formheuristik zurück.

## 3. Weitere Erfahrung hinzufügen

```bash
./build/bruce experience add --store erfahrung-01.snapshot \
  --from-run weiterer-quelllauf.bruce --output erfahrung-02.snapshot
```

`weiterer-quelllauf.bruce` muss ein eigener vollständiger V2-/V3-Portfoliolauf mit Hold-out sein. Die vorherige Datei bleibt erhalten; alle bisherigen Ereignisse werden unverändert in den neuen Snapshot übernommen. Die gleichen kanonischen Lernpaare zählen nicht erneut, auch wenn ihre Reihenfolge anders ist oder ein anderer Hold-out verwendet wurde.

Ein Lauf mit `experience-order` erzeugt ein V4-Paket und wird in dieser ersten Version **nicht** wieder als neue Erfahrung aufgenommen. So entstehen keine verschachtelten Selbstbestätigungsketten. Für weitere Quellläufe stehen die feste Reihenfolge oder `adaptive-order` zur Verfügung.

## 4. Eine Erfahrung nachvollziehbar zurückziehen

```bash
./build/bruce experience revoke --store erfahrung-01.snapshot \
  --id EINTRAGS_ID --reason "Erfahrung nach Gegenpruefung zurueckgezogen" \
  --output erfahrung-widerrufen.snapshot
./build/bruce experience inspect erfahrung-widerrufen.snapshot
```

`EINTRAGS_ID` durch die ID aus `records` der inspect-Ausgabe ersetzen. Der Widerruf ergänzt ein Ereignis; er löscht den ursprünglichen Eintrag nicht. Neue Suchen, die den neuen Snapshot verwenden, berücksichtigen ihn nicht mehr. Der Text begründet die Entscheidung, wird aber nicht als maschinell geprüfter externer Gegenbeweis ausgegeben.

Ein früher exportierter Forschungslauf enthält seinen damaligen Speicherstand und bleibt identisch replaybar. Ein Widerruf ändert ihn nicht rückwirkend. Es gibt keinen automatisch aktualisierten globalen Speicher: Den gewünschten Stand gibst du bei jeder Suche ausdrücklich an.

## Grenzen und Integrität

- Maximal 1 MiB pro Snapshot, acht Quellläufe und 16 Ereignisse; ein Widerruf verbraucht ein Ereignis und gibt keinen Quellplatz frei.
- Jeder Quelllauf muss ein V2-/V3-Portfoliopaket mit Operationsbudget höchstens 1000000 sein. Das ist der bisherige Standard; die Grenze wird vor seiner Replay-Ausführung geprüft.
- Quellprüfungen erfolgen vor der neuen Suche und zählen separat. Die eigentliche Erfahrungsgewichtung zählt zum neuen Suchbudget.
- Exportdateien dürfen noch nicht existieren; vorhandene Dateien werden nicht überschrieben.
- Digests, Ereigniskette, Versionen, Ursprungsbefund und Trace werden geprüft. Digests sind keine Signatur eines externen Gutachters.
- Ohne Experience-Befehle und ohne `--experience` arbeitet BRUCE weiterhin ohne Erfahrungsspeicher. Es gibt keine automatische Aufnahme persönlicher Aufgaben.
- Lerngate und Veröffentlichungsgate bleiben geschlossen. Dieser Speicher beeinflusst nur die Reihenfolge; er erklärt keine Formel zur allgemeinen Wahrheit.

Die vorhandene Z-Projektion funktioniert auch mit V4-Paketen. Sie berechnet die neu gefundene Formel und lehnt Nennernullstellen oder nichtganzzahlige Projektionswerte weiterhin ausdrücklich ab.

```bash
python3 scripts/demo.py
```

Die Demo verwendet temporäre Dateien und zeigt Aufnahme, neue Planung, Widerruf und historischen Replay nach Entfernen der ursprünglichen Experience-Dateien.

## Weitere Roadmap

Ein begrenzter, optionaler Routing-Experience-Store mit replay-geprüften Quellen, Stimmenranking, Duplikaterkennung und Widerrufsereignissen ist implementiert. Der vollständige ExperienceRecord-Vertrag mit externen Nachweisen, automatischer Regelpromotion und weitergehenden Korrekturketten bleibt offen. Ebenso offen sind allgemeinere rationale Funktionen, freie Ausdruckssynthese, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung sowie BigInt-Werte im Grid und in Z-Prädikaten. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

Fortschreibung: [Core 0.12](28-rational-quadratic-usage.md) ergänzt quadratische Nenner als separate Forschungsstrategie. Diese Familie ist noch nicht Teil des hier beschriebenen Erfahrungsrankings.
