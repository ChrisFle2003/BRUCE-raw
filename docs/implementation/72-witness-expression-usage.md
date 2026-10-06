# Definitionslücken während der Formelforschung prüfen – Core 0.34

Die Ausdruckssuche kann jetzt mit `--equivalence rational-q-witness` kleine rationale Gegenstellen suchen. Damit werden manche bisher offenen Bereichsvergleiche nachweislich verschieden. Die gefundenen Werte erscheinen in der Prüfspur zusammen mit den verglichenen Ausdruckskennungen.

## Einen tatsächlichen Gegenzeugen finden

```bash
cmake --build build
./build/bruce research --strategy expression --planner cached-grammar \
  --equivalence rational-q-witness --operators '-^/' \
  --min 3 --max 3 --max-nodes 9 --max-results 256 \
  --max-operations 10000000 \
  --train examples/witness-zero-training.csv \
  --holdout examples/witness-zero-holdout.csv \
  --output ausdruckszeugen.bruce
./build/bruce replay ausdruckszeugen.bruce
```

Der Referenzlauf speichert **256 Originaltreffer in sechs Gruppen** und findet **vier abgeschlossene Gegenbeispiele aus der begrenzten Suche**. Er verbraucht 558976 logische Arbeitseinheiten. Der Status bleibt `INCOMPLETE / ResultBudgetExceeded`, weil die Originalgrenze erreicht ist.

Ein Treffer lautet:

```text
domain_witness=0 witness_against=2 excluded_side=left
```

Hier besitzt `((x-x)/(x-(3/x)))` bei 0 eine echte Definitionslücke, während der Vertreter `(x-x)` dort definiert ist. Bruce hat beide ursprünglichen Bereichsbedingungen exakt gegengeprüft. Danach kann der Kandidat noch mit einem anderen Vertreter zusammengeführt werden, der dieselbe Lücke besitzt.

Die neue Obergrenze 256 muss ausdrücklich gewählt werden. Ohne `--max-results` bleiben es 128 Originale. Alle bisherigen Modi behalten ihre Obergrenze 128.

## Vollständige Forschung mit Auswahl

Ein zweiter, kleinerer Auftrag läuft vollständig durch:

```bash
./build/bruce research --strategy expression --planner cached-grammar \
  --equivalence rational-q-witness --min 1 --max 1 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output zeugen-vollstaendig.bruce
./build/bruce replay zeugen-vollstaendig.bruce
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run zeugen-vollstaendig.bruce --at 0 --output zeugen-auswahl.bruce
./build/bruce replay zeugen-auswahl.bruce
```

Die Forschung liefert `VERIFIED_ON_SAMPLES`, die Auswahl bei 0 den Wert 0. Die Polstelle −1 bleibt gesperrt. Auswahl-Replay benötigt die ursprüngliche Forschungsdatei nicht mehr. Das unvollständige erste Beispiel darf nicht als abgeschlossene Forschungsquelle für die Auswahl dienen.

## Zähler und Grenzen

`domain_bounded_witnesses` zählt abgeschlossene Vergleiche mit ausgewähltem Suchzeugen, `domain_witness_candidates` alle vollständig geprüften Suchstellen abgeschlossener Bereichsvergleiche und `domain_different` alle bewiesenen Bereichsunterschiede. Die verbrauchte Arbeit steckt bereits in `domain_comparison_operations` und `operations_used`; diese Werte nicht addieren.

Die Suche verwendet Zählerbeträge und positive Nenner bis 16. Kein Treffer bedeutet weiterhin `UNKNOWN`, nicht bewiesene Gleichheit. Jeder Kandidat zeigt höchstens seinen ersten begrenzten Gegenzeugen in der Prüfspur. Arbeits- und Zahlengrenzen können die Untersuchung unterbrechen.

Alle drei Ausdrucksplaner werden unterstützt. Die neue Option gilt zunächst für eigenständige Ausdruckssuche; Portfolio, Erfahrung und `--domain-cache equal-64` folgen in einem eigenen Integrationspaket. Der Teilbaumcache von `cached-grammar` ist bereits nutzbar. Neue Dateien tragen die Ausdrucksversion V10; ältere Dateien bleiben replaybar.

## Weitere Roadmap

Die begrenzte Zeugensuche ist in der eigenständigen Forschung angekommen. Ihre Integration in Portfolio und Erfahrungsplanung, passende Bereichscaches, vollständigere Nullstellenverfahren, weitere Suchoptimierungen und Experience-Korrekturketten bleiben offen. Sampleprüfungen ersetzen keinen allgemeinen Beweis einer Datenbeziehung.

[Vertrag](71-witness-expression-contract.md) · [Abnahme](acceptance/witness-expression-v1-2026-09-22.md)
