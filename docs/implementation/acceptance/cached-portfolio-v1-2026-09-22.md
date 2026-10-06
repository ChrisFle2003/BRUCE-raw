# Abnahme Teilbaumcache im Portfolio / Core 0.25 – 22.09.2026

## Lieferung

Der vorhandene Cacheplaner ist im festen, adaptiven und erfahrungsbasierten Fünferportfolio verfügbar. Operatorauswahl einschließlich literaler Potenzen, gemeinsame Budgets, V20–V22-Replays, getrennte Erfahrungskontexte, gemischte V7-Speicher, Widerruf und Z-Projektion sind integriert. Die Cache- und Mathematikimplementierung selbst bleibt unverändert.

## Prüfkommandos und Ergebnis

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**48 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Die erweiterte Gesamtdemo besteht. Alle **24 historischen Fixtures** wurden zusätzlich separat mit ihren gespeicherten JSON-Ergebnissen verglichen: identisch.

## Neue Prüffälle

Die neue Portfolio-Anforderung wurde vor Produktänderung mit `UnknownExpressionPlanner` als fehlende Funktion nachgewiesen. Vier CLI-Testfälle und eine direkte C++-Suite prüfen:

- Die Operatorauswahlen `+-*/`, `+*/`, `+^` und `+-*/^` bei fester und adaptiver Reihenfolge: gleiche vollständige Hypothesen und Statuswerte wie mit dem Indexplaner. Die übrigen vier Teilruns stimmen vollständig überein. Das Ausdruckskind verwendet Engine V6, tatsächliche Cachetreffer und höchstens 64 Einträge.
- Für die vollständige Grundrechengrammatik entspricht die Einsparung im Gesamtportfolio genau der Einsparung an Ausdrucksplanung. Bei exakt ausreichendem neuen Operationsbudget ist der Cachelauf vollständig, der bisherige Indexlauf unvollständig.
- Adaptive V21-Quellen mit und ohne Potenzen liefern passende Stimmen `[0,0,0,0,1]`. Die Zielsuche beginnt mit dem Ausdruckskind und führt alle fünf Familien aus. Hypothesen stimmen mit der adaptiven Referenz überein. Feste V20-Quellen werden ebenfalls erfolgreich gespeichert und verwendet.
- Eigene Lernpaare bleiben vom Einfluss ausgeschlossen. Ein anderer Ausdrucksplaner erhält keine Cache-Stimmen. V22 ist als neue Erfahrungsquelle gesperrt. Widerruf entfernt den Einfluss und stellt die Formreihenfolge wieder her.
- Gemischter V7-Speicher mit historischer ungecacheter Quelle, neuer Cachequelle und anderer Operatorauswahl: alte Records bleiben vollständig unverändert, jeder Zielauftrag nutzt genau seinen Kontext. Deduplizierung wirkt auch über Planergrenzen hinweg. Ein neu signiertes Downgrade auf V6 wird zurückgewiesen.
- Forschungs-Replay nach Entfernen der ursprünglichen Quell-, Speicher- und Regeldateien sowie Auswahl-Replay nach zusätzlichem Entfernen der Forschungsdatei: identische Ergebnisse. Auswahl bei x=0 liefert 0; die ursprüngliche Polstelle x=−1 bleibt gesperrt.
- Operations-, Kandidaten-, Trace-, Tracebyte- und Ergebnisgrenzen unter allen drei Reihenfolgen: INCOMPLETE mit identischem Replay.
- Manipulierte Ausdrucksplaner-, Cache- und Exponentenpolicy, veränderte Operatorauswahl und Cachezähler werden trotz neu berechnetem Digest abgewiesen. Historisches V19 darf den Cacheplaner nicht nachträglich erhalten.
- Direkte C++-Prüfung für x²+2 mit reiner Ausdrucksherkunft sowie x²+x mit familienübergreifender Herkunft, jeweils unter allen drei Reihenfolgen: stabile Gruppenidentität und Herkunft, aktive Cachetreffer, exakte globale Kosten-, Trace- und Checkpointabrechnung. Jeder Ausdrucksteilrun entspricht einem separat frisch gestarteten Lauf; andere Kinder besitzen keine Cachetreffer.
- Dieselben sechs C++-Fälle prüfen alle kleinen Checkpoint-/Kandidaten-Abbruchpositionen, Callback-Abbruch, jede kleinere positive Operationsgrenze und exakt ausreichende Budgets, jeweils mit Replay.

Die frühere Ablehnung von `cached-grammar` als Ausdrucksplaner im Portfolio wurde entsprechend der absichtlichen Erweiterung auf einen weiterhin unbekannten Planer angepasst. Die vor der Änderung mit Core 0.24 erzeugte Ausdrucks-V6-Fixture und die historische Portfolio-V19-Fixture sind direkt in den neuen CLI-Tests enthalten.

## Unabhängiges Review

Das lesende Review verglich die Produktionsänderungen mit einer vor der Änderung gesicherten Kopie und fand keine handlungsbedürftigen Befunde. Unabhängig geprüft wurden:

- alle 31 nichtleeren Operatorteilmengen unter fester und adaptiver Reihenfolge: 62 Cache-/Indexvergleiche mit gleichen Befunden, identischen anderen Kindern und exaktem Replay;
- korrekte Z-Herkunftskennungen für V20, V21 und V22;
- neun einzelne Kontextänderungen: Planer, Operatoren, Minimum, Maximum, Bitgrenze, Nennergrenze, Potenzgrenze, Polynomgrad und Knotengrenze verhindern jeweils unpassenden Erfahrungseinfluss;
- historische V13-, V16- und V19-Decoder lehnen `subtree-cache-v1` ab.

Die unveränderte Mathematik-/Cacheimplementierung wurde nicht erneut separat reviewt. Vollständige Regression, UBSan und Dokumentation wurden im Hauptlauf geprüft. Ein Git-Commit-Diff steht hier nicht zur Verfügung; die Produktionskopie diente als Vergleichsbasis.

## Gemessene Demo

Konstante 1, sieben Knoten, Potenz-/Polynomfamiliengrenze 2, vier Grundrechenarten, Lernpaare für x²/(x+1):

| Messgröße | Indexplaner | Cacheplaner |
| --- | ---: | ---: |
| Ausdruckskandidaten | 2802 | 2802 |
| Feste Portfolioarbeit | 93523 | 87086 |

Die anderen vier Teilruns und die vollständigen Hypothesen sind identisch. Die neue erfahrungsbasierte Zielsuche benötigt 83996 logische Einheiten; die separat ausgewiesene Quellenprüfung 87133. Die Demo prüft passende Stimmen, Widerruf, V22-Replay und Auswahl-Replay nach Entfernen ursprünglicher Dateien.

Erfahrungsprüfung verursacht zusätzlichen Aufwand; das Ranking verspricht keine niedrigeren Gesamtkosten. Die Cachemessung ist keine CPU-Zeitmessung oder allgemeine Beschleunigungsgarantie. Weitergehende Optimierungen, allgemeines Lernen und vollständige M2-/M4-Abnahme bleiben offen.
