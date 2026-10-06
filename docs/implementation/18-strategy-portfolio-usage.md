# BRUCE Core 0.7 – mehrere Strategien in einem Forschungslauf

BRUCE kann adaptive Potenzsuche und mehrtermige Polynomforschung jetzt gemeinsam ausführen. Ein gemeinsames Budget begrenzt beide Läufe. Ihre Kandidaten werden nach exakter Formel zusammengeführt; Herkunft, Teilstatus, Prüfspur und Rechenaufwand bleiben sichtbar.

## Ausprobieren

```bash
cmake --build build
python3 scripts/demo.py
./build/bruce research --strategy portfolio \
  --train examples/portfolio-training.csv \
  --holdout examples/power-holdout.csv
```

Aus den vier Lernpaaren `(0,3)`, `(1,5)`, `(2,11)`, `(3,21)` finden beide Strategien `2*x²+3`. Nach Prüfung von `(-3,21)` und `(5,53)` lautet der Gesamtbefund `VERIFIED_ON_SAMPLES`. Das Resultat enthält **eine** Formel mit zwei Herkünften, keine künstliche Mehrdeutigkeit durch doppelte Darstellung.

Mit den mehrtermigen Daten funktioniert derselbe Einstieg:

```bash
./build/bruce research --strategy portfolio \
  --train examples/polynomial-training.csv \
  --holdout examples/polynomial-holdout.csv
```

Hier findet die Potenzfamilie keinen passenden Kandidaten. Die Polynomstrategie findet `1+x+x²`; der gemeinsame Lauf kann damit abschließen, sofern beide Teilräume vollständig behandelt wurden.

## Plan und Familien

Der versionierte Plan `power-then-polynomial-v1` führt aus:

1. `power-affine` mit adaptiver Koeffizientensuche, p=1..`--max-power` (Standard 4, Maximum 8).
2. `polynomial` mit Newton-Ableitung, d=0..`--max-degree` (Standard 3, Maximum 8).

Beide Strategien verwenden dieselben Lern-/Hold-out-Paare, Domäne, Bitgrenzen und Koeffizientenregeln. Standarddomäne ist BigInt; `--domain rational` erlaubt Brüche. `--planner portfolio` ist der einzige Planer für diesen Modus und muss nicht ausdrücklich angegeben werden. Die früheren Strategien und ihre Standardwerte bleiben verfügbar.

Die Gesamtfamilie ist die **Vereinigung** beider deklarierten Familien. Eine Potenzformel kann beispielsweise einen höheren Grad als die Polynomgrenze haben. Bei einer Zählergrenze, die null ausschließt, kann `x²+1` in der Potenzfamilie liegen, während ihr innerer Nullkoeffizient sie aus der mehrtermigen Polynomfamilie ausschließt. Ein solcher gültiger Potenzkandidat bleibt erhalten.

Dies ist eine feste deterministische Reihenfolge mit datenabhängiger Planung innerhalb der Potenzstrategie. Es gibt noch kein gelerntes Ranking, keine parallele Ausführung und keinen allgemeinen Technikplaner.

## Gemeinsame Budgets

Die zweite Strategie erhält nur das unverbrauchte Budget der ersten. Es werden keine neuen Standardbudgets pro Strategie vergeben.

| Zähler | Bedeutung im Portfolio |
| --- | --- |
| `operations_used` | Summe beider Teilruns einschließlich ihrer Planung |
| `planning_operations` | darin enthaltener Planungsanteil |
| `candidates_executed` | tatsächlich ausgeführte Kandidaten beider Teilruns |
| `stored_source_hypotheses` | gespeicherte Teilhypothesen vor dem Zusammenführen; verbraucht `--max-results` |
| `trace_events_used` | alle Teilrun-Ereignisse plus die beiden Portfolio-Ereignisse |
| `trace_bytes` | Textbytes sämtlicher Teilrun- und Portfolio-Ereignisse |
| `checkpoints` | globale freigegebene Planungs-/Kandidatengrenzen über beide Strategien |

Das Portfolio reserviert zwei eigene Traceereignisse und 256 eigene Trace-Textbytes. Die restlichen Mittel werden den Teilruns zugewiesen. Reichen Restbudgets nicht einmal für den kleinsten zulässigen Teilrunvertrag (unter anderem zwei Ereignisse und 256 Textbytes), endet der Gesamtbefund unvollständig. Die Teilstrategien können zusätzliche konservative Reserven vor einzelnen Schritten verlangen.

Auch zusammengeführte Duplikate verbrauchen ihr ursprüngliches Ergebnisbudget. Beispiel: Bei `--max-results 1` kann ein bereits gespeicherter Potenzkandidat den Start des zweiten Teilruns verhindern. BRUCE meldet dann `INCOMPLETE`, statt den zweiten Suchraum still als geprüft zu behandeln.

Die Zähler messen deklarierte logische Arbeit, keine GMP-Instruktionen oder garantierte Laufzeit. Parser, feste Buchführung und kanonische Zusammenführung sind durch die Eingabe-/Ergebnisgrenzen beschränkt, zählen aber nicht als mathematische Rechenoperationen.

## Zusammenführung und Evidenz

Potenzformeln werden in aufsteigende Monomkoeffizienten übersetzt. Führende Nullterme werden entfernt. Exakt gleiche gekürzte Koeffizientenlisten repräsentieren dieselbe Formel und erhalten einen gemeinsamen Eintrag.

Jede Hypothese enthält `origins` mit Strategie, ursprünglicher Kandidaten-ID und Status. Eine mehrfach gefundene Formel ist damit nachvollziehbar, erhält aber keinen höheren Wahrheitsscore. **Dieselben Eingaben sind keine zusätzlichen unabhängigen Datenbelege.** `general_proof`, `learn_gate` und `publish_gate` bleiben `false`.

Verschiedene Formeln bleiben gleichrangig. Bei Lernpaaren `(0,0)` und `(1,1)`, `--max-degree 1` und `--max-power 4` passen `x`, `x²`, `x³`, `x⁴`: Der gemeinsame Befund ist `AMBIGUOUS`. Die Gerade wird dabei nur einmal geführt, obwohl beide Strategien sie finden.

Portfolio-IDs werden aus dem ersten Ursprung gebildet: Potenz-ID mal 2, beziehungsweise Polynom-ID mal 2 plus 1. Sie sind innerhalb des Runs stabil und entscheiden keine Wahrheit. Beim Z-Import lauten die Koordinaten `(id/65536,id%65536)`; damit bleiben auch größere Ursprungs-IDs als begrenzte Koordinaten darstellbar.

## Unvollständige und widersprüchliche Ergebnisse

Ein abgeschlossener Teilrun mit `NO_CANDIDATE` verhindert keinen passenden Kandidaten aus der anderen Familie. Ein **unvollständiger** Teilrun hält dagegen den gesamten Befund auf `INCOMPLETE`. Ein Treffer der zweiten Strategie kann einen nicht vollständig untersuchten ersten Raum nicht nachträglich vollständig machen.

Das betrifft insbesondere fehlende Polynom-Anker: Mit weniger als `max-degree+1` verschiedenen Lern-x endet die Polynomstrategie unvollständig, auch wenn die Potenzstrategie bereits eine passende Formel gefunden hat. Die Gesamtgrenzen werden nicht anhand eines erfolgreichen Treffers verkleinert.

Nach einem Teilstopp durch Zahlengrenzen oder fehlende Daten kann die andere Strategie noch ausgeführt werden, sofern Restbudgets reichen. Nach `Cancelled` werden keine weiteren Strategien gestartet. Bei unterschiedlichen abschließenden Status derselben kanonischen Formel wäre der Befund `INCOMPLETE / StrategyResultConflict`; eine Quelle würde nicht still bevorzugt. Ein unvollständiger Herkunftsstatus bleibt ebenfalls sichtbar.

`strategy_runs` enthält vollständige Teilbefunde einschließlich der jeweiligen ursprünglichen Engine-/Regelversionen und Traces. Der Gesamtstopgrund `ChildIncomplete` wird dort durch den konkreten Teilstopgrund erläutert. Gemeinsame Budgetstopps nennen die erschöpfte Ressource.

## Replay und Regelauswahl

```bash
./build/bruce research --strategy portfolio \
  --train examples/portfolio-training.csv --holdout examples/power-holdout.csv \
  --output gemeinsamer-lauf.bruce
./build/bruce replay gemeinsamer-lauf.bruce
./build/bruce compatibility --rules examples/power-selection.rules \
  --from-run gemeinsamer-lauf.bruce --at -3 --output gemeinsame-auswahl.bruce
./build/bruce replay gemeinsame-auswahl.bruce
```

Z wertet die zusammengeführte Formel exakt aus und erhält im Beispiel 21. Der Kontext heißt `portfolio-bigint-v1` beziehungsweise `portfolio-rational-v1`. Die bisherigen int64-Grenzen und die Ablehnung unvollständiger Ursprungsruns bleiben bestehen. Die Auswertung der kanonischen Koeffizienten erfolgt nach Horner unter dem Ursprungs-Bitlimit; sie kann bei anderen Zwischenwerten konservativ stoppen.

`BRUCE_PORTFOLIO_BUNDLE_V1` verwendet Engine `bruce-portfolio-v1`, Regel `portfolio-union-v1`, Policy `shared-budget-v1` und Planer `power-then-polynomial-v1`. Replay führt beide begrenzten Teilstrategien neu aus und vergleicht das gesamte kanonische Ergebnis einschließlich Herkunft und Teiltraces. Beliebige rekursive Portfolios werden nicht eingelesen. Globale Checkpoints reproduzieren auch Abbrüche genau zwischen beiden Strategien.

Nur `--output` speichert, vorhandene Dateien werden nicht überschrieben. Ursprüngliche CSV-Dateien sind zum Replay nicht nötig. Historische Exporte bleiben unterstützt; SHA-256 und Neuberechnung sind weiterhin Konsistenzprüfung, keine externe Beglaubigung.

## Weitere Roadmap

Die erste gemeinsame Planung über Potenz- und Polynomstrategie ist implementiert. [Core 0.8](20-rational-linear-usage.md) ergänzt die rationale Familie `(a*x+b)/(x+c)`; [Core 0.9](22-rational-portfolio-usage.md) bindet sie in einen neuen gemeinsamen Portfoliomodus ein. Freie Ausdruckssynthese, allgemeine rationale Funktionen, gelerntes Strategieranking, Experience Store/Lernen, Token-/Sprachverständnis, NanoCell, Cockpit, Phase-DAG und GPU-Ausführung bleiben offen. BigInt-Werte im Grid und in Z-Prädikaten stehen weiterhin aus. Dies ist kein vollständiger M2-/Gesamtprojektabschluss.
