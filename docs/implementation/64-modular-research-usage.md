# Primzahlbeweise beim Forschen nutzen – Core 0.30

Bruce kann die stärkere Prüfung rationaler Definitionsbereiche jetzt direkt beim Zusammenführen gefundener Formeln verwenden. Der neue Modus `rational-q-modular` funktioniert in der Ausdruckssuche und im Fünferportfolio einschließlich Erfahrungsplanung.

## Beispiel mit kubischem Nenner

```bash
cmake --build build
./build/bruce research --strategy expression \
  --planner cached-grammar --equivalence rational-q-modular \
  --operators '-^/' --min 3 --max 3 --max-nodes 9 \
  --train examples/q-domain-zero-training.csv \
  --holdout examples/q-domain-zero-holdout.csv \
  --max-operations 10000000 --output modular-ausdruecke.bruce
./build/bruce replay modular-ausdruecke.bruce
```

Bruce findet unter anderem Nullausdrücke mit dem Nenner x³−3. Dieser Nenner hat keine rationale Nullstelle. Solche Ausdrücke können deshalb mit einer Nullfunktion ohne diese formale Ausschlussbedingung zusammengeführt werden. Mit `rational-q` bleibt der kubische Bereichsvergleich noch offen; der neue Modus bildet hier fünf statt sechs Gruppen. Im geprüften Cache-Lauf sinkt die logische Arbeit von 465169 auf 461889 Einheiten, weil spätere Treffer weniger getrennte Gruppen vergleichen müssen. Das ist eine Messung dieses begrenzten Beispiels, kein allgemeines Laufzeitversprechen.

**Das Beispiel bleibt `INCOMPLETE`: Es erreicht die Grenze von 128 gespeicherten Originaltreffern.** Alle Originaltreffer bleiben erhalten; präzisere Gruppierung erhöht diese Grenze nicht. `domain_modular_certificates` zeigt die Anzahl erfolgreicher modularer Seitenprüfungen, nicht die Anzahl unterschiedlicher Formeln oder vollständiger Beweise der Lernaufgabe.

Für das Portfolio ersetze im Aufruf `--strategy expression --planner cached-grammar` durch:

```text
--strategy portfolio-expression --expression-planner cached-grammar --max-power 3 --max-degree 3
```

Auch hier wird die kubische Gruppe zusammengeführt. Der Gesamtstatus lautet `INCOMPLETE` mit `ChildIncomplete`; im Ausdrucksteilrun steht `ResultBudgetExceeded`. Das Portfolio behält die Teilbefunde und deren Herkunft.

Ein einzelner Formelvergleich liefert zusätzlich die konkrete Primzahl und die vollständige Restwerttabelle:

```bash
./build/bruce compare \
  --left-numerator 0 --left-denominator '-3,0,0,1' \
  --right-numerator 0 --right-denominator 1 --domain-policy modular
```

Dieser Aufruf beweist die Gleichheit der beiden vorgegebenen Funktionen **über Q**. Er beweist keine allgemeine Beziehung unbekannter Daten.

## Vollständiger Ablauf mit Erfahrung

```bash
./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q-modular --expression-planner cached-grammar \
  --planner adaptive-order --min 1 --max 1 \
  --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv \
  --holdout examples/expression-holdout.csv \
  --output modular-quelle.bruce

./build/bruce experience add --from-run modular-quelle.bruce \
  --output modular-erfahrung.experience

./build/bruce research --strategy portfolio-expression \
  --equivalence rational-q-modular --expression-planner cached-grammar \
  --planner experience-order --experience modular-erfahrung.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv \
  --output modular-ziel.bruce
./build/bruce replay modular-ziel.bruce
```

Die vollständige Quelle vergibt `[0,0,0,0,1]` Stimmen. Die Ausdruckssuche läuft zuerst; alle fünf Familien bleiben enthalten, und die vollständigen Gruppen stimmen mit dem adaptiven Referenzlauf überein. Die Quellenprüfung wird zusätzlich als `experience_audit_operations` ausgewiesen. Die Stimmen belegen nützliche Suchwege im passenden Kontext, kein allgemeines Lernen und keine mathematische Wahrheit.

Erfahrungen mit anderem Gleichheitsmodus oder Ausdrucksplaner wirken hier nicht. Alte und neue Quellen können gemeinsam gespeichert werden, ohne ihre bisherigen Kennungen zu ändern. Ein erfahrungsbasierter V28-Lauf darf keine neue Erfahrungsquelle werden.

```bash
./build/bruce experience inspect modular-erfahrung.experience
./build/bruce experience revoke --store modular-erfahrung.experience \
  --id QUELLEN_ID_AUS_INSPECT --reason "Quelle widerrufen" \
  --output modular-widerrufen.experience
```

Der Widerruf entfernt künftige Stimmen. Bereits exportierte Läufe behalten ihren damaligen eingebetteten Speicherstand und bleiben reproduzierbar.

## Auswahl und Grenzen

```bash
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run modular-ziel.bruce --at 0 --output modular-auswahl.bruce
./build/bruce replay modular-auswahl.bruce
```

Im Beispiel wird Wert 0 ausgewählt. Die Polstelle −1 bleibt gesperrt. Unvollständige Forschung darf nicht als abgeschlossene Auswahl verwendet werden. Forschungs- und Auswahl-Replay benötigen die ursprünglichen Quelldateien nach dem Export nicht mehr. Vorhandene Dateien werden nicht überschrieben.

Alle drei Ausdrucksplaner sowie feste, adaptive und erfahrungsbasierte Portfolio-Reihenfolge sind unterstützt. Frühere Gleichheitsmodi bleiben unverändert. Ein höhergradiger Vergleich kann auch im neuen Modus offen bleiben; die Gruppen werden dann nicht zusammengeführt.

Die neue Prüfung ist jetzt in Suche, Portfolio und Erfahrungsplanung integriert. Vollständigere Nullstellenverfahren, effizientere Gruppenvergleiche, externe Evidenz und weitergehende Korrekturketten bleiben offen. Dies ist kein vollständiger M2-/M4-/Gesamtprojektabschluss.

[Vertrag](63-modular-research-contract.md) · [Abnahme](acceptance/modular-research-v1-2026-09-22.md)
