# Abnahme exakte Zahlendomänen / Core 0.3 – 22.09.2026

## Lieferung

GMP-basierte BigInt-/Brucharithmetik im bestehenden C++-Kern, Rechenkommando, lazy affine Forschung mit ganzzahligen oder rationalen Koeffizienten, Vorwärts-/Rückprüfung, Hold-out, Zahlen-/Arbeitsbudgets und eigenem kanonischem Replayformat. Z-Kompatibilität konsumiert neue Forschungsexporte bei exakt darstellbarer int64-Projektion. Persistenz bleibt optional.

## Ausgeführte Prüfungen

```bash
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/integration/test_exact_domains.py build/bruce -v
python3 scripts/demo.py
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure
```

Ergebnis: **sechs CTest-Suiten bestanden**, sowohl im Debug-Build als auch mit UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. C++ weiterhin mit `-Wall -Wextra -Werror`. Neun neue CLI-Tests und eine direkte C++-Suite zusätzlich zu den bisherigen Tests. Die ursprünglichen neuen CLI-Abnahmen schlugen vor der Implementierung mit `UnknownCommand: calculate` fehl.

Abgedeckte neue Fälle:

- große Produkte, Potenzen, modulare Potenzen und alle Vorzeichenfälle der euklidischen Division;
- 80 Bruchrechnungen aus deterministischen Zufallswerten gegen Python `Fraction`, plus modulare Potenz gegen Python-Ganzzahlen;
- vollständige Forschung `2*x+3` mit Eingaben ab `10^100` und Hold-out;
- rationale Formel `(1/2)*x+(1/3)`, rationale Eingaben und echtes Hold-out-Gegenbeispiel;
- Parameterintervall mit großem Offset `10^50`, ohne Durchlaufen aller kleineren Zahlen;
- Normalisierung, ungültige Zahlen und kanonisch gleiche bzw. überlappende x-Werte;
- Zahlen- und Tracebyte-Limit ergeben `INCOMPLETE`, keine falsche Widerlegung;
- direkte API-Prüfung von kontrolliertem Abbruch samt Replay, exakt ausreichenden Budgets, knappen Operations-/Ergebnisbudgets und ungekürzten Operanden;
- neue Replay-Pakete funktionieren nach Löschen der Eingabedateien; ein geändertes Ergebnis wird auch bei neu berechnetem Digest erkannt;
- BigInt-/rationale Forschungsimporte, explizite Ablehnung zu großer oder gebrochener int64-Projektionen, selbstständiger Z-Replay mit eingebettetem Ursprung;
- historische Core-0.1- und Core-0.2-Fixtures bleiben unverändert replaybar. Die Z-Fixture wurde vor dem Neubau mit dem damaligen Core-0.2-Binary erzeugt.

## Review

Ein unabhängiger Reviewer bestätigte einen Fehler bei direkt übergebenen ungekürzten Bibliothekswerten: Division und Gegenprüfung validierten normalisierte Kopien, verwendeten anschließend aber die ursprünglichen Zähler. Beide Stellen verwenden jetzt die kanonischen Werte. Die Regressionen wurden vor der Korrektur rot und danach grün beobachtet; der Reviewer bestätigte zusätzlich mit seinem separaten Reproduktionsprogramm die Behebung. CLI-Eingaben waren durch ihre vorherige Normalisierung nicht betroffen.

Seine zusätzliche unabhängige Prüfung umfasste 110 rationale affine Wiederfindungen gegen Python `Fraction` und vier Budget-/Replay-Fälle. Alle 114 Prüfungen bestanden. Abschließend keine weiteren wichtigen Befunde im geprüften Umfang.

## Sichtbare Demo

Die vorhandenen Core-0.1-/Z-Abläufe bleiben erhalten. Neu: `1/3 + 1/6 = 1/2`, rationale Forschung mit `VERIFIED_ON_SAMPLES` und BigInt-Forschung mit demselben Status. Beide neuen Läufe ergeben in einem neuen Prozess identische Ergebnisse und Traces; für BigInt fehlen dabei bereits die ursprünglichen CSV-Dateien.

## Verbleibende Grenzen

Exakte, aber konfigurierte Bitgrenzen; begrenzte affine Kandidatenfamilie, keine allgemeine Formelsynthese. `VERIFIED_ON_SAMPLES` bestätigt die angegebenen Beispiele, keinen allgemeinen Beweis. LearnGate/PublishGate bleiben geschlossen. Z-Prädikate und das Grid behalten ihre bisherigen Zahlenbereiche. Experience Store, adaptive Planung, Sprache, NanoCell, Cockpit und Phase-DAG sind nicht Teil dieser Abnahme. [Benutzung und genaue Verträge](../10-exact-domains-usage.md).
