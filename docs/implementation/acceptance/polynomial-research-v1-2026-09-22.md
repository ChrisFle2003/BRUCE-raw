# Abnahme mehrtermige Polynomforschung / Core 0.6 – 22.09.2026

## Lieferung

Weitere exakte Strategie im bestehenden `research`-Kommando: Polynome c0+c1*x+...+cd*x^d, d=0..8, mit begrenzten BigInt-/Bruchkoeffizienten. Newton-Ableitung aus Lernankern, exakte Umrechnung, vollständige Lernprüfung über Horner und getrennte Potenzsumme, Hold-out, budgetierte Abbrüche, versionierter Replay und echte Z-Projektion. Keine neue Abhängigkeit oder Datenbank.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
python3 scripts/demo.py
```

**Zehn CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Build mit `-Wall -Wextra -Werror`. Sechs neue CLI-Tests und eine neue direkte C++-Suite. Vor Implementierung schlug der neue Polynomablauf mit `UnknownResearchStrategy` fehl.

Abgedeckte Fälle:

- mehrtermiges x²+x+1, negative Hold-out-Werte, Z-Auswertung und Replay ohne ursprüngliche CSV-Dateien;
- rationale Polynome mit unabhängiger Python-Fraction-Referenz und zurückgehaltenen rationalen x-Werten;
- Grad 8 mit Eingaben ab 10^12, exakte Rückgewinnung aller neun Koeffizienten;
- Konstanten und Grad 0, Entfernung doppelter höherer Darstellungen mit führender Null;
- weniger als D+1 Lernpaare führen insgesamt zu `INCOMPLETE / InsufficientTrainingSamples`, trotz passender niedrigerer Teilhypothese;
- außerhalb liegende Koeffizienten, nichtganzzahlige BigInt-Kandidaten, zu geringe Gradgrenze und echte Hold-out-Widerlegung;
- Operations-/Kandidaten-/Ereignis-/Bytebudgets, begrenzte Interpolations-Zwischenergebnisse und Replay unvollständiger Läufe;
- jeder Planungs-/Kandidatencheckpoint des Referenzlaufs mit identisch wiederholtem Abbruch;
- Abbruchcallback zwischen Ableitung und erster Auswertung, genau ausreichende Budgets und um eins zu geringe Operationsgrenze;
- direkte API-Normalisierung, Ablehnung kanonisch gleicher Interpolations-x und getrennte exakte Rechenwege;
- rehashte Planerversion oder veränderte Koeffizientenliste werden beim Replay abgelehnt;
- vor der Änderung mit Core 0.5 erzeugtes adaptives Forschungspaket bleibt unverändert replaybar; bisherige historische Fixtures bleiben grün.

## Unabhängiges Review

Keine wichtigen oder kritischen Befunde. 512 zusätzliche unabhängige Prüfungen bestanden:

- 90 zufällige rationale Polynome von Grad 0 bis 8, genaue Koeffizienten und Replay;
- 120 Vergleiche gegen vollständig enumerierte kleine Koeffizientenfamilien;
- 302 Budgetgrenzen mit Replay und überprüften Operations-/Trace-/Byte-/Kandidatenobergrenzen.

Der Reviewer las neue Mathematik- und Runnerdateien sowie CLI-, Replay- und Z-Integration und nahm keine Änderungen vor.

## Sichtbarer Ablauf

Die Demo enthält weiterhin sämtliche bisherigen Forschungsschritte. Neu findet sie `(1)+(1)*x^1+(1)*x^2`, bestätigt die Hold-out-Paare und reproduziert den gesamten Lauf. Die Z-Projektion bei x=-3 ergibt 7 und `SELECTED`. Das Gegenbeispiel (4,99) ergibt `REFUTED`.

## Grenzen

Newton-Interpolation erzeugt einen Kandidaten, keinen allgemeinen Beweis. Fehlende Lernpaare werden nicht durch Hold-out aufgefüllt. Grad-, Koeffizienten-, Bit- und Arbeitsgrenzen bleiben explizit; über sie hinausgehende Behauptungen sind nicht Teil der Abnahme. Freie Ausdruckssynthese, rationale Funktionen, Strategieorchestrierung und weitere Roadmap-Module bleiben offen. [Benutzung und Verträge](../16-polynomial-research-usage.md).
