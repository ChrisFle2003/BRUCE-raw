# Abnahme datenabhängige Strategiereihenfolge / Core 0.10 – 22.09.2026

## Lieferung

Optionaler Planer `adaptive-order` für `portfolio-rational`: exakte Formhinweise auf höchstens vier Lernpaaren bestimmen die Ausführungsreihenfolge von Potenz-, Polynom- und Bruchfunktionsstrategie. Alle Familien bleiben erhalten. Planungskosten und Entscheidung werden gegen gemeinsame Budgets gebucht, protokolliert und über den neuen V3-Container replayt. Historische V1-/V2-Läufe bleiben unverändert.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**18 CTest-Suiten bestanden**, im Debug-Build und mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`); keine Sanitizer-Meldung. C++ weiterhin mit `-Wall -Wextra -Werror`. Fünf neue CLI-Tests und eine C++-Suite. Die erste neue CLI-Abnahme wurde vor Implementierung mit `UnknownSearchPlanner` rot nachgewiesen.

Abgedeckte Fälle:

- affine, quadratische und Kehrwert-Präfixe führen zu den drei vorgesehenen Reihenfolgen;
- vollständige fachliche Befunde bleiben gegenüber dem festen Portfolio gleich;
- zusammengeführte Polynom-IDs und Herkünfte bleiben unabhängig von der Ausführungsreihenfolge;
- geänderte Hold-out-Werte ändern weder Reihenfolge noch Planungsgrund/-kosten;
- irreführendes Kehrwert-Präfix wird durch ein späteres Lernpaar widerlegt;
- Nullausgaben und kurze Eingaben werden ohne unzulässige Kehrwertbildung verarbeitet;
- frühe verifizierte Teilhypothese bei Kandidatenbudget 1 hält den Gesamtstatus INCOMPLETE;
- Kosten und Ereignisse der Vorprüfung zählen tatsächlich im gemeinsamen Lauf;
- Operations-, Bit-, Traceereignis- und Tracebyte-Abbruch vor/während Planung sind replaybar;
- alle globalen Checkpoints in drei Referenzläufen; Callback vor Vorprüfung, vor erster Teilstrategie und an einem Strategiewechsel;
- genau ausreichende Operations-/Ereignisbudgets bei ausreichendem Kandidatenspielraum sowie jede kleinere positive Operationsgrenze der drei Referenzläufe;
- Z-Projektion, Polstellenfehler, verschachtelter Replay ohne Ursprungsdateien;
- Manipulation von Planerversion oder Planungskosten wird trotz neu berechnetem Digest erkannt;
- gesicherter tatsächlicher Core-0.9-V2-Export und alle älteren Fixtures bleiben unverändert replaybar.

Beim Entwurf des API-Tests wurde die Annahme korrigiert, dass die Summe tatsächlich ausgeführter Kandidaten stets als Startbudget genügt. Die bestehende konservative Regel verlangt ein positives Restbudget vor jedem Teilrun, auch wenn dieser anschließend keinen Kandidaten ausführt. Dies ist dokumentiert; V1/V2-Verhalten wurde dafür nicht verändert.

## Unabhängiges Review

Keine offenen Befunde. 120 generierte Datensätze mit affinen, quadratischen, rationalen und zufälligen Werten, unregelmäßigen x-Abständen und geänderter Reihenfolge: feste und adaptive vollständige Läufe lieferten gleiche Status, Hypothesen, IDs und Herkünfte. Zusätzlich bestanden 552 Prüfungen an Operations-, Kandidaten-, Ergebnis-, Ereignis- und Bytegrenzen mit eingehaltenen Gesamtbudgets und identischem Replay. Keine Produktdateien durch das Review verändert.

## Sichtbarer Vergleich

Referenz: `fraction-training.csv`, `fraction-holdout.csv`, Polynomgradgrenze 2.

| Variante | Kandidaten | logische Operationseinheiten | Ergebnis |
| --- | ---: | ---: | --- |
| fest, vollständig | 8 | 436 | VERIFIED_ON_SAMPLES |
| datenabhängig, vollständig | 8 | 454 | derselbe fachliche Befund |
| fest, Kandidatenbudget 1 | 1 | 47 | INCOMPLETE ohne Treffer |
| datenabhängig, Kandidatenbudget 1 | 1 | 119 | INCOMPLETE mit auf Samples bestätigter Bruchfunktion |

Der Planer stellt `rational-linear` voran, Grund `reciprocal_affine_prefix`. Die Vorprüfung kostet 18 Operationseinheiten. Der frühe Treffer demonstriert veränderte Priorisierung unter einem Kandidatenlimit, keine allgemeine Beschleunigung. Die Demo vergleicht beide Varianten und prüft vollständigen Replay in einem neuen Prozess.

## Grenzen

Versionierte Heuristik auf kurzem Lernpräfix, keine vollständige Formklassifikation oder Erfahrungslernen. Dateireihenfolge kann die Planung verändern. Die Vorprüfung kann selbst an Zahlen-/Arbeitsgrenzen scheitern. Alle bestehenden Familien- und Definitionsbereichsgrenzen bleiben erhalten. Kein allgemeiner Beweis oder M2-Gesamtabschluss. [Benutzung](../24-adaptive-portfolio-usage.md), [Planungsvertrag](../23-adaptive-portfolio-contract.md).
