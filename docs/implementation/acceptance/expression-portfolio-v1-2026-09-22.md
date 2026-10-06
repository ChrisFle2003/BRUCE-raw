# Abnahme Fünferportfolio / Core 0.16 – 22.09.2026

## Lieferung

Neue Strategie `portfolio-expression` im bestehenden Portfolio-Runner: Potenz-, Polynom-, lineare Bruch-, quadratische Bruch- und Ausdruckssuche in fester Reihenfolge. Gemeinsame Budgets, familienübergreifende rationale Normalformen mit erhaltenen Ausschlüssen, nachvollziehbare Originalherkünfte, V8-Replay und Z-Projektion. Die älteren Portfoliomodi bleiben unverändert.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**30 CTest-Suiten bestanden**, sowohl Debug als auch UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Vier neue CLI-Tests und eine neue direkte C++-Suite. Die CLI-Anforderung wurde vor Implementierung mit `MaxNodesRequiresExpressionStrategy` rot nachgewiesen. Die erweiterte Gesamtdemo ist erfolgreich.

Geprüfte Fälle:

- Alle fünf Strategien werden ausgeführt; `x²/(x+1)` wird im angegebenen Referenzraum ausschließlich über die Ausdruckssuche gefunden.
- `1/(x+1)` aus rationaler Familie und Ausdrucksgrammatik wird zusammengeführt; die globale Gruppe verweist auf beide vollständigen Teilbefunde.
- Die Bedingungen von `1`, `x/x` und `(x+1)/(x+1)` bleiben getrennt; der Prüfpunkt x=0 widerlegt die zusätzliche Null-Lücke.
- Die Buchhaltung für Teilrun-Arbeit, zusätzliche Zusammenführung, Normalisierung, ursprüngliche Ergebnisanzahl, Kontrollpunkte und Tracebytes stimmt exakt überein.
- Jede kleinere positive Operationsgrenze, jede Kandidaten-Abbruchposition und jeder Checkpoint zweier kleiner Fünferläufe ergibt einen unvollständigen, identisch replaybaren Befund.
- Callback-Abbrüche vor und während der Zusammenführung sind reproduzierbar; exakt ausreichende Operations-/Traceereignisbudgets schließen vollständig ab.
- Die Ergebnisgrenze zählt ursprüngliche Hypothesen einschließlich Ausdrucksherkünften. Kleinere Grenzen stoppen unvollständig, ohne durch Gruppierung umgangen zu werden.
- Kandidaten-, Operations-, Trace-, Tracebyte- und Ergebnislimits sowie ungültige Familienoptionen werden geprüft. Unvollständige globale Befunde können nicht in die Z-Auswahl übernommen werden.
- Neue Bruchnormalisierung erhält entfernbare Nennernullstellen, normalisiert skalierte/trailing-zero-Koeffizienten, verarbeitet Grad 8 und leere Definitionsbereiche und weist leere Polynomvektoren zurück.
- Die maximal mögliche Ausdrucks-ID lässt sich mit dem Faktor fünf und dem Koordinatensplit weiterhin sicher adressieren; der letzte Baum wird lazy erzeugt.
- V8-Replay prüft erneut globale Normalformen und Versionen; manipulierte Ausschlüsse und Regelversionen werden auch mit neu berechnetem Digest abgewiesen.
- Z-Projektion von 1/(x+1) bei x=0 liefert SELECTED mit Wert 1; x=-1 bleibt gesperrt. Auswahl-Replay funktioniert nach Entfernen aller ursprünglichen Eingabe-, Forschungs- und Regeldateien.
- Das Hinzufügen einer V8-Quelle zum bisherigen Erfahrungsspeicher wird ausdrücklich zurückgewiesen; seine alten unterstützten Versionen bleiben funktionsfähig.
- Vor Änderung gesicherte tatsächliche Core-0.15-Ausdrucks-V2-Fixture sowie sämtliche bisherigen historischen Fixtures bleiben unverändert replaybar.

## Review und Befunde

Das unabhängige lesende Review fand keine blockierenden Probleme in Normalisierung, Ausschlüssen, Herkunft, Budgetreservierung, Abbruch, Versionsprüfung oder Projektion. Ein zusätzlicher unabhängig kompilierter Prüfhelfer bestätigte den maximalen globalen ID-Wert **6639291877724** samt Koordinaten-Roundtrip und die erhaltene Null-Lücke bei Kürzung von 2*x²/(2*x²).

Eine anfängliche Testannahme erwartete für konstante Eins einen eindeutigen Befund. Die Untersuchung zeigte korrekt eine weitere positiv geprüfte Gruppe `(x+1)/(x+1)` mit Lücke bei x=-1, die in diesen Samples nicht vorkam. Die Erwartung wurde auf AMBIGUOUS korrigiert; keine Definitionseinschränkung wurde entfernt.

## Sichtbares Ergebnis

Unveränderte Ausdrucks-Beispielpaare, Zählerbereich [1,1], Nennergrenze 1, Potenz-/Polynomgradgrenze 2 und sieben Baumknoten:

- vier ältere Familien: NO_CANDIDATE in ihren angegebenen Grenzen;
- Ausdruckssuche: 2802 untersuchte Bäume, fünf ursprüngliche Lernkandidaten, zwei Gruppen;
- eine Gruppe besteht die Samples, die andere scheitert an x=0;
- Gesamtstatus VERIFIED_ON_SAMPLES;
- **105744 logische Operationseinheiten**, davon **69288 Planung**, **2394 Normalisierung einschließlich globaler Zusammenführung**; die globale Übernahme selbst kostet **2** Einheiten und ist in diesen Zahlen enthalten.

Die Demo zeigt außerdem die gemeinsame Gruppe von rationaler Familie und Ausdruckssuche für 1/(x+1), Forschungs-Replay, Z-Auswahl und Auswahl-Replay ohne Forschungsdatei.

## Grenzen

Feste Reihenfolge der fünf Familien, höchstens 64 Konstanten-Rohslots und neun Baumknoten. Die normalisierte Zusammenführung ist hinreichend, aber keine vollständige Prüfung rationaler Nullstellenmengen. Keine zusätzlichen unabhängigen Datenbelege, keine allgemeine Beziehung aus Samples bewiesen. Adaptive Reihenfolge und Experience für diese fünf Familien folgen als eigenes versioniertes Paket. Kein vollständiger M2-Abschluss.

[Vertrag](../35-expression-portfolio-contract.md), [Benutzung](../36-expression-portfolio-usage.md).
