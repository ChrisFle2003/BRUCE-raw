# Abnahme begrenzte Ausdruckssynthese / Core 0.14 – 22.09.2026

## Lieferung

Eigenständige Strategie `expression` für kleine binäre Bäume aus x, rationalen Konstanten und den vier Grundrechenarten. Größenbegrenzte kanonische Grammatik, lazy Unranking, rekursive exakte Auswertung, getrennte Ganzzahl-Zähler-/Nenner-Gegenrechnung, erhaltene Divisionsbedingungen, Hold-out, Ressourcen-/Abbruchgrenzen, eigenes Replayformat und Z-Projektion.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**26 CTest-Suiten bestanden**, Debug und UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Fünf neue CLI-Tests und eine direkte C++-Suite. Die neue CLI-Anforderung wurde vor Implementierung mit `UnknownOption: --max-nodes` rot nachgewiesen. Gesamtdemo erfolgreich.

Geprüfte Fälle:

- Verschachtelte Ausdruckssuche für x²/(x+1) mit getrennten Lern-/Hold-out-Paaren und tatsächlich regeneriertem Replay;
- unabhängige Python-Referenz: alle geordneten Bäume bis fünf Knoten erzeugen, nur Vertauschung normalisieren, Fraction-Auswertung gegen vollständige Kandidaten-/Statusmengen auf drei verschiedenen Samplekonfigurationen vergleichen;
- alle 2802 kanonischen Bäume des Bausteinsatzes x/1 bis sieben Knoten haben unterschiedliche Formeltexte; Vorwärts- und Gegenrechnung stimmen an x=-1, x=0 und x=2/3 einschließlich Polstellen überein;
- Definitionslücken bei x/x und bei Multiplikation mit null bleiben erhalten; verschachtelte innere Divisionen werden nicht weggekürzt;
- rationale Konstanten erscheinen eindeutig geklammert; Bitlimit ergibt INCOMPLETE mit identischem Replay;
- sämtliche Checkpoints, sämtliche Kandidaten-Abbruchpositionen, Callback-Abbruch und jede kleinere positive Operationsgrenze zweier kleiner Referenzläufe sind reproduzierbar;
- exakt ausreichende Kandidaten-/Operations-/Traceereignisbudgets liefern den vollständigen Befund;
- Tracebyte-, Traceereignis-, Ergebnis- und Kandidatenlimits halten den Gesamtstatus unvollständig;
- maximaler Grammatikraum mit 65 Terminals und neun Knoten korrekt gezählt; letzter Rang erzeugt einen gültigen Baum;
- ungültige Baumreferenzen, Operatorstruktur und Baumgrößen werden abgewiesen;
- falsche Domain-/Planer-/Grad-/Potenzoptionen, unzulässige Knotengrenzen sowie zu viele Konstantenslots scheitern ausdrücklich;
- manipulierte Regelversion mit neuem Digest wird abgewiesen;
- Forschungs-Replay ohne CSV-Dateien sowie Z-Auswahl-Replay ohne Ursprungsrun und Regeldatei;
- Polstellen in der Z-Projektion führen zu ausdrücklichem Fehler;
- vor Änderungen gesichertes tatsächliches Core-0.13-V6-Paket und sämtliche früheren Replay-Fixtures bleiben unverändert reproduzierbar.

## Review und Befunde

Das unabhängige lesende Review fand keine blockierenden Probleme in Grammatik, Unranking, Baumvalidierung, beiden Rechenwegen, Budgets, Replay und Projektion. Es leitete die maximale Baumzahl von **1327858375545** unabhängig her und prüfte 100000 pseudorandomisierte IDs im gesamten Raum einschließlich Randfällen. Der Hinweis, den Strategie-Runner im Header ausdrücklich als internen Einstieg mit vorheriger Taskvalidierung zu markieren, wurde umgesetzt.

Ein ursprünglicher Beispieltest erwartete nur einen positiv geprüften Ausdruck. Die tatsächliche vollständige Grammatik enthält drei passende Schreibweisen. Der Test wurde auf den vereinbarten Vertrag korrigiert: Nur Kommutativität wird normalisiert; algebraisch gleichwertige Umformungen bleiben getrennt und ergeben AMBIGUOUS. Es wurde kein gültiger Kandidat entfernt, um einen eindeutigen Befund zu erzwingen.

## Sichtbares Ergebnis

`examples/expression-training.csv` und `expression-holdout.csv`, einzige Konstante 1, maximal sieben Knoten: **2802 untersuchte Bäume, 103126 logische Operationseinheiten, davon 69064 Planung/Erzeugung**. Drei Schreibweisen bestehen die angegebenen Samples; zwei weitere Lernkandidaten scheitern an ihrer Definitionslücke bei x=0. Gesamtstatus AMBIGUOUS.

Die Demo zeigt alle fünf Lernkandidaten mit ihren Bedingungen, führt Replay aus und projiziert die drei überlebenden Ausdrücke an x=0 auf den Wert 0. Der Auswahl-Replay funktioniert anschließend ohne ursprünglichen Forschungsexport.

## Grenzen

Feste kleine Grammatik, höchstens neun Knoten und 64 Konstanten-Rohslots. Keine allgemeine Funktionsgleichheit, keine automatische Kürzung, keine unbeschränkte Suche. Die Gegenrechnung kann konservativ am Bitlimit eines ungekürzten Zwischenprodukts enden. Die neue Strategie ist noch nicht im Portfolio oder Experience-Ranking. Keine allgemeine Beweisaussage, kein vollständiger M2-Abschluss. [Vertrag](../31-expression-synthesis-contract.md), [Benutzung](../32-expression-synthesis-usage.md).
