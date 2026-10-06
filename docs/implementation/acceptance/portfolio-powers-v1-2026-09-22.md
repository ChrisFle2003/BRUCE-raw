# Abnahme Potenzausdrücke im Portfolio / Core 0.23 – 22.09.2026

## Lieferung

Literale Potenzausdrücke sind im Fünferportfolio integriert: beide Ausdrucksplaner und feste, adaptive sowie erfahrungsbasierte Reihenfolge. Neue V17–V19-Forschungsexporte, passende V6-Erfahrungskontexte, gemischte Speicher, Widerruf und Z-Projektion sind verfügbar. Mathematik und historische Grammatiken ohne Potenzen bleiben unverändert.

## Prüfkommandos und Ergebnisse

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**44 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), ohne Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Die erweiterte Gesamtdemo besteht. Alle **22 historischen Fixtures** wurden zusätzlich separat gegen ihr gespeichertes JSON-Ergebnis geprüft: identisch.

## Neue Prüffälle

Die neue Portfolio-Anforderung wurde vor Produktänderung mit `PowerOperatorRequiresStandaloneExpression` als fehlende Funktion nachgewiesen. Vier CLI-Testfälle und eine neue direkte C++-Suite prüfen die folgenden Verträge:

- x³+x wird mit fünf Knoten und `+^` über das Ausdruckskind gefunden. Beide Ausdrucksplaner liefern bei fester und adaptiver Reihenfolge gleiche vollständige Hypothesen, deklarierte Exponentenpolicy und identischen Replay. Die anderen vier Teilruns entsprechen vollständig dem Referenzlauf ohne Potenzoperator.
- V17- und V18-Quellen sind als Erfahrung zulässig. Verschiedene Operatorauswahlen und Ausdrucksplaner wirken nur im passenden Kontext. Stimmen `[0,0,0,0,1]` stellen den Ausdrucksweg voran; alle fünf Familien werden ausgeführt. Vollständige Hypothesen stimmen mit adaptiver Reihenfolge überein.
- Gleiche Lernpaare werden auch bei anderer Operatorauswahl dedupliziert und als eigene Stimme ausgeschlossen. V19 darf keine neue Erfahrungsquelle werden. Widerruf entfernt den Planungseinfluss und führt auf die ursprüngliche Formreihenfolge zurück.
- Ein bestehender V5-Speicher und neue Potenzquellen lassen sich zu V6 zusammenführen. Der alte Record bleibt vollständig unverändert. Eine alte Quelle wirkt nicht auf den neuen Potenzkontext. Ein mit neuem Digest versehenes Downgrade auf V5 wird als nichtkanonisch zurückgewiesen.
- Forschungs-Replay nach Entfernen sämtlicher ursprünglicher Eingabe-, Regel-, Quell- und Speicherdateien und anschließender Auswahl-Replay nach Entfernen der Forschungsdatei: identische Ergebnisse. Projektion bei x=−2 liefert −10.
- `(1/x)^2` behält x≠0 als Ausschlussbedingung. Beide Ausdrucksplaner liefern bei x=−1 den Wert 1; x=0 bleibt gesperrt. Ein durch Ergebnislimit abgebrochener Lauf bleibt INCOMPLETE, ist replaybar und darf nicht zur Auswahl verwendet werden.
- Operations-, Kandidaten-, Trace- und Tracebytegrenzen werden in allen sechs Planerkombinationen geprüft. Ein einzelner vollständig verarbeiteter Treffer darf bei Ergebnislimit 1 vollständig sein; der Abbruchtest verwendet deshalb einen Fall mit mehreren Ursprungsbäumen.
- Nichtkanonische Operatoren, entfernter Potenzoperator, veränderte Exponentenpolicy, unbekannter Ausdrucksplaner und manipulierte Kandidatenzählung werden trotz neuem Digest abgewiesen. Historisches V16 darf nicht durch nachträgliches Einfügen von `^` erweitert werden.
- Direkte C++-Tests prüfen x² mit reiner Ausdrucksherkunft und familienübergreifender Herkunft unter allen sechs Planerkombinationen: stabile Gruppenidentität und Herkunft, exakte globale Kosten-/Trace-/Checkpointabrechnung, alle kleinen Checkpoint- und Kandidaten-Abbruchpositionen, Callback-Abbruch, jede kleinere positive Operationsgrenze und exakt ausreichende Budgets, jeweils einschließlich Replay.

Die bestehende Ablehnung von `^` im Portfolio wurde entsprechend der absichtlichen Schnittstellenerweiterung durch weiterhin ungültige Operatoren beziehungsweise weiterhin unzulässige Strategien ersetzt. Die vor der Änderung mit Core 0.22 erzeugte Ausdrucks-V5-Fixture und die historische Portfolio-V16-Fixture sind zusätzlich Teil des neuen Integrationstests.

## Unabhängiges Review

Das lesende Review verglich die aktuellen Produktionsdateien mit einer vor der Änderung gesicherten Kopie. Es fand keine konkreten behebungsbedürftigen Befunde. Unabhängig ausgeführt wurden:

- die vier neuen CLI-Tests und die C++-Suite mit zwölf Kombinationen;
- 28 CLI-Proben für Exponenten 2–8, beide Ausdrucksplaner und feste/adaptive Reihenfolge, jeweils mit negativem Hold-out und Replay;
- drei Proben ohne zulässiges Exponentenliteral;
- 412 zusätzliche Tracebudget-Grenzproben ohne Überschreitung;
- vier Compatibility-Proben mit vollständiger Operatorauswahl einschließlich Potenzoperator, Versionsherkunft und eingebettetem Replay.

Der Mathematikordner blieb gegenüber der Kopie unverändert. Vollständige Regression, UBSan und Dokumentation wurden im Hauptlauf geprüft. Es liegt kein Git-Repository mit verfügbarem Commit-Diff vor.

## Gemessene Demo

Indizierter Ausdrucksplaner, Konstanten 1–3, Potenz-/Polynomfamiliengrenze 2, Lernpaare für x³+x:

| Messgröße | Produkte `+*`, sieben Knoten | Potenzen `+^`, fünf Knoten |
| --- | ---: | ---: |
| Ausdruckskandidaten | 1884 | 130 |
| Gesamte feste Portfolioarbeit | 62278 | 3717 |

Die anderen vier Teilruns bleiben identisch. Beide Aufträge bestätigen dieselbe Formelgruppe. Bei getrennten Zielpaaren benötigt die erfahrungsbasierte Suche 3532 logische Einheiten; die separat ausgewiesene Quellenprüfung 3764. Die Demo prüft passenden Einfluss, unveränderte vollständige Hypothesen, Widerruf, V19-Replay und Auswahl-Replay ohne ursprüngliche Forschungsdatei.

Diese Messung vergleicht unterschiedliche Grammatiken und Knotengrenzen, keine allgemeine CPU-Beschleunigung. Erfahrungsstimmen sind keine zusätzliche mathematische Evidenz. Allgemeinere Synthese, weitergehendes Erfahrungslernen und der vollständige M2-/M4-Abschluss bleiben offen.
