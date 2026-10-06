# Genauere Ausdrucksgruppen über Q – Core 0.27

Bruce kann jetzt den exakten Vergleich rationaler Definitionsbereiche direkt während der Ausdruckssuche verwenden. Der neue optionale Modus heißt `rational-q`. Er führt gleiche Wertefunktionen nur zusammen, wenn auch ihre Definitionsbereiche über rationalen Zahlen nachweislich gleich sind. Bei einem offenen Vergleich bleiben die Treffer getrennt.

## Vollständiger Forschungsablauf

```bash
cmake --build build
./build/bruce research --strategy expression \
  --planner cached-grammar --equivalence rational-q \
  --min 1 --max 1 --max-nodes 7 \
  --train examples/quadratic-fraction-training.csv \
  --holdout examples/quadratic-fraction-holdout.csv \
  --output q-ausdruecke.bruce
./build/bruce replay q-ausdruecke.bruce
```

Das Beispiel findet `1/(1+x²)` und schließt die deklarierte Suche mit `VERIFIED_ON_SAMPLES` ab. Die Gruppierung bezieht sich auf Q; die Bestätigung der vermuteten Datenbeziehung bezieht sich weiterhin auf die geprüften Lern- und Hold-out-Paare. `general_proof` bleibt false.

`grammar` und `indexed-grammar` funktionieren ebenfalls. Die Operatorauswahl und literalen Potenzen von 2 bis 8 bleiben verwendbar. Der Cache startet wie bisher bei jedem Such-/Replaylauf leer.

## Die drei Gleichheitsmodi

| `--equivalence` | Was zusammengeführt wird |
| --- | --- |
| `syntax` | Keine zusätzliche algebraische Zusammenführung; bisheriger Standard |
| `rational` | Gleiche gekürzte Brüche mit identischem quadratfreiem Ausschlusspolynom |
| `rational-q` | Gleiche gekürzte Brüche mit nachgewiesen gleichem rationalem Definitionsbereich |

Der neue Modus kann mehr Gleichheiten erkennen, benötigt dafür zusätzliche logische Arbeit. Das ist kein Versprechen einer schnelleren Suche.

Die erste gefundene Formel bleibt Repräsentant. Alle ursprünglichen Bäume, Kennungen und Statuswerte stehen in `expression_origins`. Ihre ursprünglichen Nennerbedingungen werden nicht aus den Bäumen entfernt. Beispielsweise bleibt `x/x` von `1` getrennt: Bei `0` unterscheiden sich ihre Definitionsbereiche.

## Zusätzliche Zusammenführung sichtbar machen

```bash
./build/bruce research --strategy expression \
  --planner cached-grammar --equivalence rational-q \
  --operators '-^/' --min 2 --max 2 --max-nodes 9 \
  --train examples/q-domain-zero-training.csv \
  --holdout examples/q-domain-zero-holdout.csv \
  --max-operations 10000000 --output q-nullfunktion.bruce
./build/bruce replay q-nullfunktion.bruce
```

Für einen direkten Vergleich denselben Befehl mit `--equivalence rational` und einem anderen Exportnamen ausführen.

Im geprüften Beispiel reduziert der neue Modus die bisherigen drei Gruppen auf zwei. Unter anderem wird `(2−2)/(x²−2)` mit der Nullfunktion zusammengeführt: Die scheinbaren Lücken ±√2 sind keine rationalen Eingaben. Eine tatsächliche rationale Lücke bleibt getrennt. Alle 128 gespeicherten Originaltreffer und ihre Statuswerte bleiben identisch erhalten.

**Dieses größere Beispiel endet in beiden Modi mit `INCOMPLETE` und `ResultBudgetExceeded`.** Die Grenze von 128 Originaltreffern wird nicht durch die Zusammenführung umgangen. Das Beispiel belegt die Gruppierung im erhaltenen Teilbefund, keine vollständige Suche. Ein solcher Export darf weiterhin nicht zur Auswahl verwendet werden.

Mit `--min 3 --max 3` entsteht auch der Ausschluss `x³−3`. Diese begrenzte Nullstellenprüfung entscheidet den verbleibenden kubischen Faktor nicht. Die Gruppe bleibt getrennt; `domain_unknown` zeigt die offenen Vergleiche. Daraus folgt keine mathematische Ungleichheit.

## Nachvollziehbare zusätzliche Arbeit

- `domain_comparison_operations`: Arbeit für Gruppendurchlauf, Koeffizientenvergleich und Bereichsprüfung; bereits in `operations_used` enthalten.
- `domain_comparisons`: begonnene Bereichsvergleiche bei gleicher Wertefunktion.
- `domain_unknown`: abgeschlossene, aber unentschiedene Bereichsvergleiche.

Die Kandidatenspur nennt `domain_checks`, `domain_unknown` und gegebenenfalls `merge_into`. Die vollständigen Berechnungen werden beim Replay anhand der gespeicherten Ursprünge wiederholt. Bei Arbeits-, Bit- oder Abbruchgrenzen bleibt der Gesamtlauf unvollständig. Ein Kandidat, dessen Gruppenvergleich unterbrochen wurde, wird mit seinem Ursprung separat als `INCOMPLETE` erhalten.

Die Suche behält höchstens 128 Originaltreffer. Höhere Restgrade können gleiche Funktionen in verschiedenen Gruppen belassen. Der Vergleich arbeitet ausschließlich über Q; Ergebnisse dürfen nicht ohne Weiteres auf reelle Eingaben übertragen werden.

## Auswahl und Replay

```bash
./build/bruce compatibility --rules examples/compatibility.rules \
  --from-run q-ausdruecke.bruce --at 0 --output q-auswahl.bruce
./build/bruce replay q-auswahl.bruce
```

Die geladenen Regeln entscheiden über die Auswahl. V7-Forschung kann importiert werden; die Herkunft enthält `expression-q-`. Polstellen, Bruchergebnisse außerhalb der Ganzzahlprojektion, Werte außerhalb Int64 und unvollständige Forschung bleiben gesperrt. Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht mehr. Vorhandene Exportdateien werden nicht überschrieben.

Neue Läufe dieses Modus schreiben `BRUCE_EXPRESSION_BUNDLE_V7`. Alte Suchmodi und historische Replay-Dateien bleiben unverändert.

## Weitere Roadmap

Die eigenständige Ausdruckssuche nutzt jetzt den genaueren rationalen Bereichsvergleich. Portfolio und Erfahrungsplanung verwenden weiterhin ihre bisherigen Gleichheitsregeln. Deren Einbindung, allgemeinere Nullstellenentscheidungen und effizientere Gruppenvergleiche bleiben offen; ebenso weitere Suchoptimierungen, externe Evidenz, Experience-Korrekturketten, Sprache, NanoCell, Cockpit, Phase-DAG, GPU-Ausführung und BigInt im Grid/Z. Kein vollständiger M2-/Gesamtprojektabschluss.

[Vertrag](57-q-expression-contract.md) · [Abnahme](acceptance/q-expression-v1-2026-09-22.md)

Nächster Ausbau: [Core 0.28 – Q-Gruppen im Portfolio und mit Erfahrung nutzen](60-q-portfolio-usage.md).
