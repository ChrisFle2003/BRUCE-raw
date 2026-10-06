# Core 0.17: Fünf Suchwege anhand von Daten und Erfahrung ordnen

BRUCE kann jetzt auch im Fünferportfolio die Suchreihenfolge anpassen. Die Form der Lernwerte gibt einen ersten Hinweis; geprüfte frühere Läufe können zusätzlich passende Suchwege vorziehen. Alle fünf Familien werden weiterhin untersucht und neue Ergebnisse erneut geprüft.

## Adaptive Reihenfolge starten

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression --planner adaptive-order \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output fuenfer-quelle.bruce
```

Die Heuristik betrachtet höchstens vier Lernpaare. Sie kann Potenz-, Polynom- oder Bruchstrategien vorziehen; die Ausdruckssuche wird zunächst hinten ergänzt. Hold-out-Werte fließen nicht in die Reihenfolge ein. `strategy_order` zeigt die gewählte Reihenfolge, `ordering_reason` den Grund und `ordering_operations` den Aufwand.

Ein Hinweis ist keine Gewissheit. Bei x²+1 können die ersten vier Kehrwerte zufällig einen quadratischen Verlauf nahelegen. BRUCE prüft trotzdem alle Familien weiter. Ohne `--planner` bleibt die bisherige feste Reihenfolge aktiv.

## Geprüfte Erfahrung verwenden

```bash
./build/bruce experience add \
  --from-run fuenfer-quelle.bruce --output fuenfer.experience
./build/bruce experience inspect fuenfer.experience
./build/bruce research --strategy portfolio-expression --planner experience-order \
  --experience fuenfer.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output fuenfer-erfahrung.bruce
./build/bruce replay fuenfer-erfahrung.bruce
```

Der Quelllauf untersucht x²/(x+1) mit Lernstellen 1–4. Die neue Aufgabe verwendet Lernstellen 2–5 und getrennte Prüfstellen 0 und 7. Im angegebenen Suchraum liefert allein die Ausdrucksfamilie einen positiv geprüften Treffer.

Der Eintrag erhält Stimmen **`[0,0,0,0,1]`**. Damit wechselt die erste Strategie für die neue Aufgabe von `power-affine` zu **`expression`**. Der vollständige Befund einschließlich Gruppen-IDs, Definitionslücken und Herkunft bleibt derselbe wie ohne Erfahrung. Die bekannten Formeln werden nicht als Antwort übernommen: BRUCE durchsucht und prüft die neue Aufgabe erneut.

Eine Familie erhält höchstens eine Stimme je Quelle. Drei Schreibweisen derselben Funktion zählen daher nicht als drei Erfolge. Bei Gleichstand bleibt die heuristische Reihenfolge erhalten.

## Wann eine Erfahrung passt

Familienmenge, Zahlenbereich, Parameter- und Bitgrenzen, maximale Potenz, Polynomgrad, Baumgröße, Gleichheitsregel und Lernpräfixklasse müssen passen. Die Zielaufgabe darf nicht dieselben Lernpaare wie die Quelle verwenden. Eine andere Zeilenreihenfolge umgeht diesen Ausschluss nicht.

Drei-, Vierer- und Fünferquellen können gemeinsam gespeichert werden; sie beeinflussen nur ihre jeweilige Familienmenge und ihren passenden Kontext. Ältere Quellen behalten ihre IDs. Feste und adaptiv geplante Fünferläufe können als Quelle dienen. Bereits erfahrungsbasiert geplante Läufe werden als neue Quelle zurückgewiesen, damit keine rekursiven Nachweise entstehen.

`experience_records_used` zählt die für diese Aufgabe verwendeten Einträge; `experience_votes` zeigt ihre Stimmen. `experience_audit_operations` beschreibt die separate Prüfung der gespeicherten Nachweise. Diese Arbeit wird vor der Zielsuche durchgeführt und fällt zusätzlich zum Suchbudget an.

Im mitgelieferten Beispiel benötigt die neue Suche **102654 logische Operationseinheiten**, die separate Quellenprüfung **105838**. Die Zahlen belegen keine Beschleunigung: Dieser vollständige Lauf untersucht weiterhin alle Familien und bezahlt zusätzliche Planung sowie Nachweisprüfung. Der unmittelbare Effekt ist die geänderte Reihenfolge.

## Einen Eintrag widerrufen

Die ID zeigt `experience inspect` unter `records` an. Setze sie anstelle von `EINTRAGS_ID` ein:

```bash
./build/bruce experience revoke --store fuenfer.experience \
  --id EINTRAGS_ID --reason 'Quelle zurückgezogen' \
  --output fuenfer-widerrufen.experience
```

Bei späteren Läufen mit dem neuen Snapshot trägt diese Quelle keine Stimme mehr bei. Ohne weitere passende Stimmen gilt wieder die heuristische Reihenfolge. Bereits exportierte Läufe enthalten ihren damaligen Speicherstand und bleiben unverändert replaybar.

## Auswahl und Gesamtdemo

```bash
./build/bruce compatibility \
  --rules examples/quadratic-fraction-selection.rules \
  --from-run fuenfer-erfahrung.bruce --at 0 \
  --output fuenfer-erfahrung-auswahl.bruce
./build/bruce replay fuenfer-erfahrung-auswahl.bruce
python3 scripts/demo.py
```

Die Beispielgruppe liefert an x=0 den Wert 0 mit `SELECTED`; ihre Polstelle x=-1 bleibt gesperrt. Die Demo führt Erfahrung, geänderte Reihenfolge, Widerruf und historischen Replay ohne ursprüngliche Speicher- und Forschungsdateien vor. Vorhandene Exportdateien werden nicht überschrieben.

Adaptive Läufe schreiben V9, erfahrungsbasierte Läufe V10 und der erweiterte Erfahrungsspeicher V3. Alte V8-Läufe bleiben unverändert reproduzierbar. Gemeinsame Such- und Herkunftsbudgets gelten weiter. Ein Abbruch bleibt `INCOMPLETE`, auch wenn eine früh ausgeführte Familie bereits einen positiven Treffer enthält.

## Weitere Roadmap

Adaptive und erfahrungsbasierte Planung sind jetzt auch für die fünf Familien implementiert. Effizientere Ausdruckssuche, allgemeinere Operatoren und Grammatiken, vollständigere Definitionsbereichsvergleiche, externe Evidenz, automatische Regelpromotion und weitergehende Experience-Korrekturketten bleiben offen. Ebenso Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Die Stimmenplanung ist kein allgemeines Lernen und kein vollständiger M2-/M4-/Gesamtprojektabschluss.

Fortsetzung: [Core 0.18](40-indexed-expression-usage.md) ergänzt einen optionalen Grammatikindex für die eigenständige Ausdruckssuche. Die Einbindung dieser Optimierung in das Fünferportfolio bleibt ein eigenes Paket.
