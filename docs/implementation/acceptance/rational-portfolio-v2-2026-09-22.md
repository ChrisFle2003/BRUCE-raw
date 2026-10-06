# Abnahme Portfolio mit Bruchfunktionen / Core 0.9 – 22.09.2026

## Lieferung

`research --strategy portfolio-rational`: adaptive Potenzsuche, Newton-Polynomforschung und rationale lineare Bruchfunktionssuche in einem gemeinsamen Lauf. Restbudgets, globale Abbruchcheckpoints und Herkunft werden über alle drei Strategien erhalten. Definitionen mit ausgeschlossenen Punkten werden nicht mit totalen Polynomen zusammengeführt. Neuer V2-Replaycontainer; V1 und alle bisherigen Einzelstrategien bleiben unterstützt.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**16 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`); keine Sanitizer-Meldung. Build mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine C++-Suite. Der erste CLI-Test wurde vor Implementierung mit `UnknownResearchStrategy` rot nachgewiesen.

Abgedeckte Fälle:

- alle drei Strategien unter gemeinsamen Budgets; passendes Lernpolynom wird durch Hold-out widerlegt, Bruchfunktion bleibt bestätigt;
- Zusammenführung desselben Polynoms mit beiden Herkünften;
- konstante totale Funktion und zwei kürzbare Bruchfunktionen mit unterschiedlichen Definitionslücken bleiben drei Kandidaten;
- Hold-out an x=0 widerlegt nur den dort nicht definierten Kandidaten;
- geteilte Operations-/Kandidaten-/Ergebnis-/Tracebudgets, insbesondere kein Neustart vor der dritten Strategie;
- fehlende Polynom-Anker halten Gesamtbefund unvollständig trotz gefundenem Bruchkandidaten;
- jeder globale Checkpoint zweier Referenzläufe, Callback-Abbruch vor Beginn der dritten Strategie, genau ausreichende Budgets und sämtliche kleineren positiven Operationsgrenzen;
- identischer Replay auch nach Löschen von CSV-, Ursprungs- und Regeldateien;
- Z-Projektion der tatsächlichen Bruchfunktion, explizite Polstellen- und Bruchwertablehnung;
- große Bruchkandidaten-ID oberhalb 2^48 mit erfolgreicher Z-Projektion und verschachteltem Replay;
- manipulierte Planerversion oder Definitionslücke scheitert auch mit neu berechnetem Digest;
- historische Core-0.8-Bruchfixture und Core-0.7-Portfoliofixture sowie ältere Formate bleiben unverändert replaybar.

## Unabhängiges Review

Keine offenen Befunde. Zusätzlich wurden 70 Fälle gegen vollständige exakte Familienvereinigungen geprüft: Koeffizientenzähler -1..1, Nenner bis 2, Grad/Potenz bis 2, mit Lern-/Hold-out-Daten und erhaltener Funktionsidentität. Weitere 366 Läufe an Budgetgrenzen blieben innerhalb aller gemeinsamen Obergrenzen und wurden identisch replayt. Der Reviewer bestätigte die Grenzen der V2-IDs und Z-Koordinaten; keine Produktdateien durch das Review geändert.

## Sichtbarer Referenzlauf

Drei Lernpaare aus `fraction-training.csv`, zwei Hold-out-Paare aus `fraction-holdout.csv`, Polynomgradgrenze 2. Gesamtbefund `VERIFIED_ON_SAMPLES`; acht tatsächlich geprüfte Kandidaten, 436 logische Operationseinheiten einschließlich 136 Planungseinheiten. Die Potenzstrategie findet keinen Lernkandidaten. Das interpolierte Polynom `1-2/3*x+1/6*x²` scheitert an Hold-out. Die Bruchfunktion `1/(x+1)` besteht die Prüfpaare und behält den ausgeschlossenen Punkt -1. Die Demo wiederholt den gemeinsamen Export in einem neuen Prozess.

## Grenzen

Feste Reihenfolge, keine gelernte oder datenabhängige Auswahl verschiedener Strategien. Bruchfamilie bleibt auf `(a*x+b)/(x+c)` beschränkt. Kein allgemeiner mathematischer Beweis, keine zusätzliche unabhängige Evidenz durch wiederholte Verwendung derselben Daten, kein Experience Store und kein vollständiger M2-Abschluss. [Benutzung](../22-rational-portfolio-usage.md), [Identitäts-/Versionsvertrag](../21-portfolio-domain-contract.md).
