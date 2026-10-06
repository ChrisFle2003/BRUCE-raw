# Abnahme nichtlineare Forschung / Core 0.4 – 22.09.2026

## Lieferung

Neue Strategie `power-affine` im bestehenden Forschungsablauf: lazy `a*x^p+b`, p=1..8, exakte BigInt-/rationale Werte, getrennte Vorwärts- und Gegenrechnung, Hold-out, ehrliche Mehrdeutigkeit und unvollständige Befunde, kanonischer Replay und Z-Import mit tatsächlich ausgewertetem Exponenten. Keine neue Datenbank oder Laufzeitabhängigkeit.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/integration/test_power_research.py build/bruce -v
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
python3 scripts/demo.py
```

**Sieben CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Build weiterhin mit `-Wall -Wextra -Werror`. Acht neue CLI-Tests und ergänzte direkte API-Prüfungen. Die erste CLI-Abnahme wurde vor Implementierung rot mit `UnknownOption: --strategy` beobachtet.

Abgedeckte Fälle:

- quadratische Formel 2*x²+3, negative Hold-out-Eingaben, Export und Replay nach Löschen ursprünglicher Eingabedateien;
- kubische Formel mit rationalen Koeffizienten und Eingaben sowie BigInt-Kubik bei x ab 10^80;
- Exponent 8 und Ablehnung ungültiger Exponentengrenzen/Strategieoptionen;
- Konstanten ohne doppelte Exponentenkandidaten oder unnötige Großzahlexpansion;
- mehrere auf x=0/1 passende Potenzen bleiben `AMBIGUOUS`;
- Hold-out-Gegenbeispiel ergibt `REFUTED`, zu kleine vollständig untersuchte Familie `NO_CANDIDATE`;
- vollständige Kandidatenmengen gegen eine unabhängige Python-Fraction-Referenz;
- ein nach bereits gefundenem Kandidaten erreichtes Zahlenlimit ergibt insgesamt `INCOMPLETE`;
- Kandidaten-, Operations-, Ereignis- und Bytebudgets, genau ausreichende Grenzen, kontrollierter Abbruch samt identischem Replay;
- direkte Gegenprüfung akzeptiert korrekte Werte und lehnt falsche ab; ungekürzte API-Brüche werden normalisiert;
- Z-Projektion setzt tatsächlich den Exponenten ein; Auswahl-Replay funktioniert mit eingebettetem Ursprung ohne Quelldateien;
- rehashte Änderungen an Exponent, Exponentengrenze oder Regelversion werden zurückgewiesen;
- tatsächlicher historischer Core-0.3-Export bleibt identisch replaybar; bisherige Core-0.1-/Core-0.2-Fixtures bestehen weiter.

Bei der ergänzten Abbruchprüfung wurde eine inkonsistente Testvorgabe berichtigt: Für a=b=1 und x=2 lautet der quadratische Zielwert 5. Danach bestand die Prüfung ohne Produktcodeänderung.

## Unabhängiges Review

Keine konkreten kritischen oder wichtigen Befunde. Der Reviewer führte zusätzlich 140 zufällige Referenzfälle gegen Python `Fraction` aus: Exponenten 1–8, vollständige Kandidatenmengen, Reihenfolge/IDs, Hold-out-Status und Replay. Weitere 100 Fälle variierten Budgets gleichzeitig und bestätigten Stopstatus, Grenzen und Replay. Keine Dateien durch den Reviewer verändert.

Die historischen Fixtures prüfen konkrete archivierte Läufe. Ohne gültige Git-Historie ist damit keine pauschale Kompatibilitätsaussage für sämtliche jemals erzeugten Pakete belegt.

## Sichtbare Demo

Alle bisherigen affinen/Z-/BigInt-/Bruchabläufe bleiben enthalten. Neu findet die Demo aus den Lernpaaren 2*x²+3, prüft 1105 Kandidaten für Exponenten 1–4 und meldet mit Hold-out `VERIFIED_ON_SAMPLES`. Replay stimmt überein. Die Z-Projektion bei x=-3 ergibt 21 und `SELECTED`; das Gegenbeispiel (3,99) ergibt `REFUTED`.

## Grenzen

Ein Suchraum aus `a*x^p+b` ist keine allgemeine Formelsynthese. Kein allgemeiner mathematischer Beweis und keine automatische Lern-/Veröffentlichungsfreigabe. Adaptive Planung, weitere Formelklassen, Grid-Cache/BigInt-Z-Prädikate, Experience, Sprache, NanoCell, Cockpit und Phase-DAG bleiben offen. [Benutzung und genaue Verträge](../12-power-research-usage.md).
