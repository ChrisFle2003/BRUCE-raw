# Abnahme rationale Bruchfunktionen / Core 0.8 – 22.09.2026

## Lieferung

Die explizite Strategie `rational-linear` untersucht `(a*x+b)/(x+c)`, x != -c. Exakter Rangsolver über Lernpaare, lazy vollständige Suche bei Rangmangel, Quotientenrechnung und separate expandierte Gegenprüfung, Hold-out mit Polstellenerkennung, budgetierter Trace, Replay und Z-Import sind implementiert. Verschiedene Definitionslücken bleiben getrennt. Das historische Potenz-/Polynomportfolio bleibt unverändert.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**14 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung; C++ mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine neue C++-Suite. Der erste CLI-Test wurde vor Implementierung mit `UnknownResearchStrategy` rot nachgewiesen.

Abgedeckte Fälle:

- Ableitung von `1/(x+1)` und `1/x`, rationale Koeffizienten gegen Python Fraction;
- vollständige kleine Familien bei Rangmangel, widersprüchliche zusätzliche Lernzeilen;
- konstante Werte mit verschiedenen Definitionslücken bleiben mehrdeutig;
- echte und kürzbare Nennernullstellen, unabhängige Gegenrechnung lehnt falsche Werte ab;
- kanonische Normalisierung auch direkt über die C++-API;
- jeder Checkpoint des eindeutigen und singulären Referenzlaufs mit identischem Abbruch-Replay;
- Callback-Abbruch nach Planung, genau ausreichende Budgets sowie alle kleineren positiven Operationsbudgets beider Referenzläufe;
- Operations-, Kandidaten-, Trace- und Byteabbruch; Bitlimit bereits während der Gleichungsableitung;
- Z-Projektion bei ganzzahligem Ergebnis, ausdrückliche Ablehnung einer Polstelle oder Bruchausgabe;
- Replay ohne ursprüngliche CSV-Dateien, Auswahl-Replay ohne Regeln/Ursprungsdatei;
- manipulierte Planerversion oder Definitionslücke werden auch bei neu berechnetem Digest abgewiesen;
- tatsächlicher vor dem Umbau gesicherter Core-0.7-Portfolioexport und alle älteren Fixtures bleiben identisch replaybar.

## Unabhängiges Review

Keine offenen Befunde. Der Reviewer führte zusätzlich 180 Vergleiche gegen vollständige Python-Fraction-Enumeration von jeweils 343 Koeffiziententripeln sowie 460 Prüfungen an Budgetgrenzen mit identischem Replay aus. Die Referenzen umfassen rationale Eingaben, singuläre Systeme und widersprüchliche Paare. Keine Produktdateien wurden durch das Review verändert.

## Sichtbarer Referenzlauf

Lernpaare `(0,1)`, `(1,1/2)`, `(2,1/3)`; Hold-out `(-2,-1)`, `(3,1/4)`. Ausgabe `a=0,b=1,c=1`, `excluded_x=-1`, `VERIFIED_ON_SAMPLES`. Genau ein Kandidat ausgeführt, 41 logische Planungsoperationen und insgesamt 101 Operationseinheiten. Die 2515456 rohen Parameterslots enthalten auch nichtkanonische Bruchslots und sind keine gemessene Suchleistung. Der Solver bestimmt direkt den einzigen möglichen Koeffizientensatz.

Die Demo wiederholt den exportierten Lauf in einem separaten Prozess, widerlegt die endliche Erwartung an x=-1 und projiziert bei x=0 den Wert 1 nach Z. Alle älteren Demonstrationsfälle bestehen ebenfalls.

## Grenzen

Monischer linearer Nenner, kein allgemeiner rationaler Ausdrucksraum; eigenständige Strategie, noch keine Einbindung in das Portfolio. Exakte Sampleprüfung und Replay beweisen keine allgemeine Beziehung. Resultat-, Arbeits- oder Zahlengrenzen bleiben konservativ unvollständig. Kein Experience Store, gelerntes Ranking oder vollständiger M2-Abschluss. [Benutzung](../20-rational-linear-usage.md).
