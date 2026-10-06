# Abnahme Fünferplanung und Erfahrung / Core 0.17 – 22.09.2026

## Lieferung

Adaptive und erfahrungsbasierte Reihenfolge für das bestehende Fünferportfolio. Erweiterung der Formheuristik, fünf kontextgebundene Erfahrungsstimmen, stabile Gruppenidentitäten unabhängig von Ausführungsreihenfolge, gemischte Erfahrungssnapshots mit unveränderten historischen Einträgen, Widerruf sowie V9/V10-Replay und Z-Auswahl.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**32 CTest-Suiten bestanden**, sowohl Debug als auch UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Vier neue CLI-Tests und eine neue direkte C++-Suite. Die neuen Anforderungen wurden vor Implementierung durch `AdaptiveOrderRequiresRationalPortfolio` und `ExperienceRequiresIndependentPortfolioSource` rot nachgewiesen. Die erweiterte Gesamtdemo ist erfolgreich.

Geprüfte Fälle:

- Adaptiver Planer führt stets alle fünf Familien auf; die bestehenden affinen, reziproken und quadratischen Präfixregeln bleiben wirksam.
- Eine vollständige unabhängige Quelle für x²/(x+1) liefert `[0,0,0,0,1]`; auf anderen Lernpaaren zieht Erfahrung die Ausdruckssuche vor.
- Vollständige fixe, adaptive und erfahrungsbasierte Läufe liefern identische Normalformen, Status, Gruppen-IDs und Herkunft, einschließlich familienübergreifender Zusammenführung.
- Änderungen der Hold-out-Werte ändern weder Formhinweis noch Erfahrungsranking; widersprechende Prüfwerte widerlegen die Kandidaten anschließend ausdrücklich.
- Gleiche Lerninputs und abweichende Baumgröße, Polynomgrad oder Potenzgrenze tragen keine Stimme bei.
- Quellen aus V8 und V9 werden geprüft und angenommen; V10-Quellen werden als rekursive Erfahrung zurückgewiesen.
- Widerruf entfernt Planungseinfluss; der historische V10-Replay bleibt mit eingebettetem damaligem Snapshot identisch.
- Gemischte Vierer-/Fünfer-Snapshots behalten ursprüngliche IDs und Eingabedateien unverändert. Neue und alte Familienkontexte erhalten jeweils nur ihre passenden Stimmen.
- Manipulierte gespeicherte Nachweise und Planerversionen werden auch bei neu berechneter äußerer Prüfsumme zurückgewiesen.
- Jede kleinere positive Operationsgrenze, jeder globale Checkpoint und jede Kandidaten-Abbruchposition von zwei kleinen Aufgaben unter beiden neuen Planern ist unvollständig und identisch replaybar.
- Callback-Abbrüche vor Planung, vor Teilruns und während Quellenübernahme sind reproduzierbar; exakt ausreichende Operations-/Traceereignisbudgets schließen ab.
- Planungs-, Teilrun-, Zusammenführungs- und Normalisierungskosten sowie Quellenanzahl, Kontrollpunkte und Tracebytes stimmen in der Buchhaltung überein.
- Kandidaten-, Operations-, Trace-, Tracebyte- und Herkunftsgrenzen bleiben wirksam. Frühe positiv geprüfte Ausdrucksgruppen lassen offene Familien unvollständig.
- Z-Auswahl aus V10 ergibt am Beispielpunkt x=0 SELECTED, erhält die Polstelle bei x=-1 und replayt nach Entfernung der ursprünglichen Dateien.
- Tatsächliche vor Änderung gespeicherte Core-0.16-V8-Fixture und sämtliche älteren Replay-/Erfahrungsfälle bleiben unverändert reproduzierbar.

## Unabhängiges Review

Das lesende Review fand keine blockierenden Probleme in V9/V10-Replay, Quellzulässigkeit, Kontexttrennung, gemischten Speichern, Widerruf, stabilen Gruppenidentitäten oder Z-Projektion. Zusätzlich wurden 16 vollständig abgeschlossene Vergleichsfälle aus affinen, quadratischen und reziproken Funktionen mit verschiedenen Koeffizienten und negativen Eingaben geprüft: feste und adaptive Planung lieferten identische Hypothesen.

Ein neuer Test erwartete bei x²+1 zunächst einen direkten quadratischen Formhinweis. Die ersten vier Kehrwerte an x=0,1,2,3 besitzen jedoch ebenfalls gleiche zweite Differenzen. Die bestehende Priorität des reziprok-quadratischen Hinweises greift korrekt. Der Test prüft nun sowohl diesen irreführenden Hinweis als auch den direkten quadratischen Fall x²; die vorhandene Heuristik wurde nicht nachträglich an den erwarteten Treffer angepasst.

## Sichtbares Ergebnis

Quelle: bestehende Ausdrucks-Beispielpaare, Zählerbereich [1,1], Nennergrenze 1, Potenz-/Polynomgradgrenze 2, sieben Knoten, adaptive Planung. Ziel: neue Dateien `expression-experience-target-training.csv` und `expression-experience-target-holdout.csv` mit Lernstellen 2–5 und Prüfstellen 0 und 7.

Die erste Strategie wechselt von power-affine zu expression. Alle fünf Familien werden vollständig ausgeführt, der fachliche Befund bleibt VERIFIED_ON_SAMPLES mit identischer Herkunft. **102654 logische Operationseinheiten für die neue Suche**, **105838 für die separat ausgewiesene Quellenprüfung**. Diese Kosten sind kein Nachweis einer Beschleunigung.

Die Gesamtdemo widerruft anschließend die Quelle, prüft die wiederhergestellte heuristische Reihenfolge und reproduziert den historischen V10-Lauf nach Entfernen der Quell- und Snapshotdateien.

## Grenzen

Begrenztes Routing aus positiv geprüften Familienbefunden; keine automatische Wahrheitsübernahme oder Regelpromotion. Die heuristische Grundplanung priorisiert die Ausdruckssuche noch nicht eigenständig anhand einer Ausdrucksform. Gleiche Trainingsdaten bleiben dieselben Belege; ein vollständiger Suchlauf mit Erfahrung kann mehr Arbeit kosten. Externe Evidenz, umfassende Korrekturketten, allgemeinere Grammatiken und der vollständige M2-/M4-Abschluss bleiben offen.

[Vertrag](../37-five-experience-contract.md), [Benutzung](../38-five-experience-usage.md).
