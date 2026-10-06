# Gegenbeispiele im gemeinsamen Forschungsablauf – Core 0.35

Das Fünferportfolio und seine Erfahrungsplanung unterstützen jetzt `--equivalence rational-q-witness`. Die neue Prüfung sucht kleine rationale Gegenstellen sowohl im Ausdruckskind als auch bei der Zusammenführung verschiedener Familien. Die Rechnung und alle Gegenprüfungen belasten das gemeinsame Budget.

## Tatsächlicher Gegenbeispiellauf

```bash
cmake --build build
./build/bruce research --strategy portfolio-expression \
  --expression-planner cached-grammar --equivalence rational-q-witness \
  --operators '-^/' --min 3 --max 3 --max-power 3 --max-degree 3 \
  --max-nodes 9 --max-results 256 --max-operations 10000000 \
  --train examples/witness-zero-training.csv \
  --holdout examples/witness-zero-holdout.csv --output portfolio-zeugen.bruce
./build/bruce replay portfolio-zeugen.bruce
```

Der feste Referenzlauf verbraucht 561354 logische Einheiten und enthält vier abgeschlossene begrenzte Gegenzeugen bei insgesamt 256 Originaltreffern. Er bleibt `INCOMPLETE / ChildIncomplete`, weil das Ausdruckskind die Originalgrenze erreicht. Die vier Zeugen entstehen hier im Ausdruckskind; die direkte Zeugensuche der Familienvereinigung wird zusätzlich durch eigene Tests geprüft.

`domain_bounded_witnesses` zählt fertige ausgewählte Suchzeugen. `domain_witness_candidates` zählt vollständig geprüfte Suchstellen abgeschlossener Vergleiche. Beide Phasen werden zusammengezählt. Kein Treffer in der endlichen Suche ist kein Abwesenheitsbeweis.

## Vollständige Forschung und Erfahrung

```bash
./build/bruce research --strategy portfolio-expression \
  --expression-planner cached-grammar --equivalence rational-q-witness \
  --planner adaptive-order --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-training.csv --holdout examples/expression-holdout.csv \
  --output zeugen-quelle.bruce
./build/bruce experience add --from-run zeugen-quelle.bruce --output zeugen.experience
./build/bruce research --strategy portfolio-expression \
  --expression-planner cached-grammar --equivalence rational-q-witness \
  --planner experience-order --experience zeugen.experience \
  --min 1 --max 1 --max-power 2 --max-degree 2 --max-nodes 7 \
  --train examples/expression-experience-target-training.csv \
  --holdout examples/expression-experience-target-holdout.csv --output zeugen-ziel.bruce
./build/bruce replay zeugen-ziel.bruce
./build/bruce compatibility --rules examples/quadratic-fraction-selection.rules \
  --from-run zeugen-ziel.bruce --at 0 --output zeugen-auswahl.bruce
./build/bruce replay zeugen-auswahl.bruce
```

Die vollständige Quelle erzeugt Stimmen `[0,0,0,0,1]`: Das Ausdrucksverfahren wird vorgezogen, alle fünf Familien bleiben enthalten. Die Befunde stimmen mit dem vollständigen adaptiven Vergleichslauf überein. Die Auswahl bei 0 liefert 0; die Polstelle −1 bleibt gesperrt.

```bash
./build/bruce experience inspect zeugen.experience
./build/bruce experience revoke --store zeugen.experience \
  --id QUELLEN_ID_AUS_INSPECT --reason "Quelle widerrufen" --output zeugen-widerrufen.experience
```

Ein Widerruf entfernt den späteren Einfluss. Der gespeicherte Forschungs- und Auswahl-Replay bleibt unabhängig von ursprünglichen Quelldateien. Eigene Lernpaare, andere Ausdrucksplaner oder Vergleichspolitiken erzeugen keine passenden Stimmen. Ein erfahrungsbasierter Lauf darf nicht selbst zur neuen Stimmenquelle werden.

Neue Portfolioexporte verwenden V32/V33/V34, neue Erfahrungsspeicher V11. Alte Dateien bleiben lesbar. Der Standard bleibt bei 128 Originalen; bis 256 sind im neuen Modus ausdrücklich wählbar. Die ersten vier Familien behalten jeweils ihre Grenze 128, zusätzlich zum noch verfügbaren gemeinsamen Rahmen. Der Bereichscache `equal-64` unterstützt diese Politik noch nicht.

## Architektur und nächste Arbeit

Die Versionsauswahl des Fünferportfolios ist jetzt für Ausgabe, Export und Replay zentralisiert. [Aktueller Architekturstand und konkrete M0–M2-Lücken](75-current-architecture-and-milestones.md) ersetzen pauschale Fertigstellungsannahmen. Offen bleiben insbesondere die gemeinsame Grid-Anbindung, weitere Strategie-/Auftragsverträge, Bereichscaches für die neue Politik und Experience-Korrekturketten.

[Vertrag](73-witness-portfolio-contract.md) · [Abnahme](acceptance/witness-portfolio-v1-2026-09-22.md)
