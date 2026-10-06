# Abnahme gemeinsame Strategieplanung / Core 0.7 – 22.09.2026

## Lieferung

`research --strategy portfolio`: adaptive Potenzsuche und Newton-Polynomforschung in einem begrenzten Lauf. Gemeinsame Restbudgets, exakte kanonische Formelzusammenführung, erhaltene Herkunft, konservative Gesamtstatus, globale Abbruchcheckpoints, eigenständiger Replay und Z-Import. Bestehende Einzelstrategien und historische Ausgaben bleiben verfügbar.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
python3 scripts/demo.py
```

**Zwölf CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Build weiterhin mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine neue C++-Suite. Die erste neue CLI-Abnahme schlug vor Implementierung mit `UnknownResearchStrategy` fehl.

Abgedeckte Fälle:

- beide Strategien finden dieselbe quadratische Formel: ein kanonischer Kandidat mit beiden Herkünften;
- mehrtermige Formel ausschließlich aus der Polynomfamilie;
- rationale Koeffizienten und gemeinsame Hold-out-Widerlegung;
- verschiedene Potenzen bleiben mehrdeutig, auch wenn eine davon zugleich in der Polynomfamilie liegt;
- eine gültige dünn besetzte Potenzformel bleibt erhalten, wenn der entsprechende innere Nullkoeffizient außerhalb der mehrtermigen Koeffizientenfamilie liegt;
- gesamte Operations-/Kandidaten-/Tracebuchhaltung stimmt mit der Summe der Teilruns und Wrapper überein;
- kein neues Budget bei Strategiewechsel, unzureichende Operations-/Kandidaten-/Ergebnis-/Ereignis-/Bytebudgets halten den Gesamtstatus unvollständig;
- fehlende Polynom-Anker machen auch den gemeinsamen Lauf unvollständig;
- jeder globale Checkpoint des Referenzlaufs wird unterbrochen und identisch replayt;
- Callback-Abbruch direkt beim Strategiewechsel sowie genau ausreichende gemeinsame Budgets und eine um eins zu geringe Operationsgrenze;
- Z-Projektion und verschachtelter Ursprungs-Replay nach Löschen der Quelldateien;
- manipulierte Planerversion und Teilbefunde werden auch mit neu berechnetem Digest abgewiesen;
- gesicherter tatsächlicher Core-0.6-Polynomexport bleibt identisch replaybar; ältere Fixtures bestehen ebenfalls.

## Unabhängiges Review

Keine wichtigen oder kritischen Fehler gefunden. Zusätzlich bestanden:

- 120 Vergleiche gegen vollständig enumerierte exakte Fraction-Familienvereinigungen, einschließlich positiver/negativer Koeffizientenintervalle, gestörter Lerndaten und guter/falscher Hold-out-Paare;
- 713 Budgetgrenzen mit bestätigten Operations-, Kandidaten-, Ergebnis-, Ereignis- und Byteobergrenzen samt identischem Replay;
- große Portfolio-ID `68717510624`, Z-Projektion mit Wert `257` und Replay mit eingebettetem Ursprung.

Die Konfliktbehandlung gleicher Formeln mit unterschiedlichen abschließenden Status wurde statisch geprüft. Ein solcher Konflikt ließ sich mit den vorhandenen deterministischen Rechenstrategien nicht natürlich erzeugen; daraus wird kein dynamischer Konflikttest behauptet. Keine Repository-Dateien durch den Reviewer verändert.

## Sichtbarer Ablauf

Die Demo verwendet vier Lernpaare für 2*x²+3 und die zurückgehaltenen Paare (-3,21), (5,53). Beide Teilstrategien schließen ab. Die Ausgabe enthält eine Formel `(3)+(0)*x^1+(2)*x^2`, beide Herkünfte, `VERIFIED_ON_SAMPLES` und 436 gemeinsame Operationseinheiten. Der vollständige gemeinsame Lauf wird in einem neuen Prozess identisch wiederholt. Die bisherigen Demos bleiben enthalten.

## Grenzen

Feste versionierte Reihenfolge, kein gelerntes Ranking oder allgemeiner Technikplaner. Keine zusätzliche unabhängige Evidenz durch wiederholte Nutzung desselben Datensatzes. Grenzen der einzelnen Familien und Rechenwege bleiben erhalten; ein Teilabbruch wird nicht durch einen anderen Treffer überstimmt. Kein allgemeiner Beweis oder M2-Gesamtabschluss. [Benutzung und Verträge](../18-strategy-portfolio-usage.md).
