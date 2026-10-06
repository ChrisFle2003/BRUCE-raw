# Abnahme quadratische Bruchfunktionen / Core 0.12 – 22.09.2026

## Lieferung

Eigenständige Strategie `rational-quadratic` für `(a*x+b)/(x²+c*x+d)` im vorhandenen Kern. Exakte Ableitung mit vier Unbekannten, begrenzte lazy Enumeration bei Rangmangel, explizite Nennerbedingung, getrennte Vorwärts-/Gegenrechnung, Hold-out, deterministischer Replay und Z-Projektion. Der gemeinsame Solver und Runner erhalten das bisherige Verhalten der linearen Bruchfunktionen.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**22 CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`); keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine neue direkte C++-Suite. Die neue CLI-Anforderung wurde vor Implementierung mit `UnknownResearchStrategy` rot nachgewiesen. Die Gesamtdemo ist erfolgreich durchgelaufen.

Geprüfte Fälle:

- `1/(x²+1)` aus vier Lernpaaren mit zwei unabhängigen Hold-out-x-Werten, exakt ein ausgeführter Kandidat;
- zwölf reproduzierbare rationale Koeffizientenfälle gegen Python `Fraction`, zusätzlich kleine rangarme Familien gegen vollständige Enumeration;
- einfache, doppelte, irrationale Nennernullstellen sowie erhaltener Definitionsausschluss bei kürzbarem `x/x²`;
- Polstellen widersprechen endlichen Hold-out-Ausgaben; separate Z-Projektion weist Pole und nichtganzzahlige Werte ab;
- eindeutige Lösung außerhalb der Koeffizientenfamilie und überbestimmtes widersprüchliches System: kein Kandidat, vollständiger Replay;
- Kandidaten-, Operations-, Ergebnis-, Traceereignis-, Tracebyte- und Bitgrenzen mit unvollständigem Status;
- jeder Checkpoint sowie jede kleinere positive Operationsgrenze der beiden direkten Referenzläufe (eindeutig und rangarm), mit identischem Replay;
- Callback-Abbruch und exakt ausreichende Kandidaten-/Operations-/Traceereignisbudgets;
- Solver normalisiert rationale Eingaben; getrennte Gegenprüfung weist einen falschen Wert ab;
- Replay ohne ursprüngliche CSV-Dateien und Auswahl-Replay ohne Ursprungsrun/Regeldatei;
- manipulierte Planerversion oder Ergebniskoeffizienten werden auch bei neuem Digest abgewiesen;
- falsche Domain-/Planer-/Grad-/Potenzoptionen und Überschreitung der zusätzlichen Slotgrenze scheitern ausdrücklich;
- maximal erlaubte 32768 Koeffizientenslots (`2^60` Rohkombinationen) mit abgeleiteter ID größer `2^53`, erfolgreicher Z-Projektion und Replay; der Raum wird nicht materialisiert;
- tatsächliches vor Änderungen gesichertes Core-0.11-V4-Paket mit eingebetteter Erfahrung und alle bisherigen historischen Fixtures bleiben identisch replaybar.

## Unabhängiges Review

Das lesende Review prüfte Gleichungssolver, Fallback, Definitionsbereich, Budgets, Replay, Validierung und Z-Adressen. Zusätzlich verglich der Reviewer 120 zufällige Lernbestände mit vollständiger Python-Fraction-Enumeration über `[-1,1]^4`; alle Mengen stimmten überein. Keine blockierenden Befunde. Die empfohlenen zusätzlichen Fälle für Lösungen außerhalb der Familie und widersprüchliche zusätzliche Lernzeilen wurden als dauerhafte Tests aufgenommen und bestanden in beiden Builds.

## Sichtbares Ergebnis

`examples/quadratic-fraction-training.csv` und `quadratic-fraction-holdout.csv` liefern `1/(x²+1)` als `VERIFIED_ON_SAMPLES`. Standardgrenzen: 342102016 Rohslots, **ein tatsächlich geprüfter Kandidat, 238 logische Operationseinheiten, davon 94 Planung**. Z-Projektion an `x=0` liefert `1`; das separate Beispiel `1/(x²−1)` mit geforderter endlicher Ausgabe an `x=1` wird `REFUTED`.

## Grenzen

Ein linearer Zähler, ein monischer quadratischer Nenner, beschränkte rationale Koeffizienten und Arbeitsbudgets. Definitionsbedingung als exaktes Polynom, keine allgemeine Nullstellenauflösung. Die neue Familie ist noch nicht im Dreistrategien-Portfolio oder dessen Experience-Ranking. Kein allgemeiner Formelbeweis, keine freie Synthese, kein vollständiger M2-Abschluss. [Benutzung](../28-rational-quadratic-usage.md), [Vertrag](../27-rational-quadratic-contract.md).
