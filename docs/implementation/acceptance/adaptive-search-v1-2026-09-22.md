# Abnahme adaptive Koeffizientensuche / Core 0.5 – 22.09.2026

## Lieferung

Optionaler datenabhängiger Planer für die vorhandene Familie `a*x^p+b`: exakte Ableitung aus zwei verschiedenen Lernprojektionen, Ausschluss unmöglicher/außerhalb liegender Paare oder vollständige Suche bei fehlender Eindeutigkeit. Bestehende Kandidatenprüfung, Gegenrechnung, Hold-out, IDs und Z-Projektion werden weiterverwendet. Planung zählt zum Arbeits-/Tracebudget und erhält ein eigenständiges versioniertes Replayformat.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
python3 scripts/demo.py
```

**Acht CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung; weiterhin `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und ergänzte direkte API-Prüfungen. Die erste neue CLI-Abnahme wurde vor Implementierung mit `UnknownOption: --planner` rot beobachtet.

Neue Prüffälle:

- gleicher quadratischer Befund und gleiche Hypothesen-ID wie vollständige Enumeration bei deutlich weniger geprüften Kandidaten;
- 15 deterministische rationale Referenzfälle: adaptive/enumerative Hypothesen gleich, vollständiger Kandidatenbestand unabhängig gegen Python `Fraction` geprüft;
- einzelne Lernpaare, singuläre Potenzprojektionen, widersprüchliche gleiche Projektionen und mehrdeutige Potenzen;
- exakte Familienzugehörigkeit: ein Bruchkoeffizient wird in BigInt ausgeschlossen und in passender rationaler Familie zugelassen;
- Hold-out verändert keine Kandidatenerzeugung oder Planungsentscheidungen, kann aber einen Kandidaten widerlegen;
- Planungslimits für Operationen, Ereignisse, Tracebytes und Zahlengröße bleiben `INCOMPLETE`;
- zu knappes Kandidatenbudget und Replay unvollständiger Planungsabläufe;
- alle Checkpointgrenzen des Referenzlaufs werden mit Abbruch exportiert und identisch wiederholt;
- Abbruchcallback nach dem ersten Plan, aber vor dem ersten Kandidaten: Replay bewahrt den Plan trotz gleicher Kandidatenzahl null;
- genau ausreichende Operations-/Kandidaten-/Ereignisbudgets gegen eine um eins zu geringe Operationsgrenze;
- Replay mit eingebettetem adaptivem Forschungsursprung nach Löschen der ursprünglichen Quelldateien;
- veränderte Planerversion wird auch mit neu berechnetem Digest abgewiesen;
- tatsächlicher Core-0.4-Export vor der Änderung gesichert und unverändert replaybar; bestehende Fixtures aus Core 0.1–0.3 bleiben grün.

## Unabhängiges Review

Keine konkreten kritischen oder wichtigen Befunde. Zusätzlich 160 zufällige adaptive/enumerative Vergleiche mit unterschiedlichen Ganzzahl-/Bruchfamilien, 1–4 Samples, Exponenten bis 8, singulären und gestörten Daten: vollständige Hypothesen, IDs und Status identisch. Weitere 150 kombinierte Budgetfälle hielten Grenzen und identischen Replay ein.

Die Anmerkung zur Operationszählung ist in der Anleitung präzisiert: feste Familien-/Slotbuchführung zählt nicht als logische Rechenoperation. Sie ist begrenzt und erzeugt keine unbudgetierte Suchschleife. Der Reviewer änderte keine Dateien.

## Tatsächlich gemessener Vergleich

`examples/power-training.csv`, `examples/power-holdout.csv`, Standardparameter:

| Planer | Kandidaten | Operationseinheiten gesamt | Trace-Textbytes |
| --- | ---: | ---: | ---: |
| enumerate | 1105 | 13440 | 61995 |
| adaptive | 4 | 226 | 503 |

Beide liefern `(2)*x^2+(3)`, ID 470, `VERIFIED_ON_SAMPLES`. Die 226 Einheiten enthalten 68 Planungseinheiten. Dies ist kein allgemeiner Geschwindigkeitsnachweis oder Wallclock-Benchmark. Die Demo vergleicht die tatsächlichen Ergebnisse und exportiert/replayt auch den adaptiven Lauf.

## Grenzen

Keine allgemeine Formelsynthese, kein gelerntes Strategieranking, keine allgemeine Aussage über unbekannte Daten. Numerische Grenzen können adaptive und enumerative Rechenwege unterschiedlich früh stoppen. Aussagen zur Gleichheit vollständiger Befunde gelten für tatsächlich vollständig abgeschlossene Vergleichsläufe. Familie, Koeffizientenraum und Gates bleiben unverändert. [Benutzung und genaue Verträge](../14-adaptive-search-usage.md).
