# Abnahme Grammatikindex / Core 0.18 – 22.09.2026

## Lieferung

Optionaler Planer `indexed-grammar` für die vorhandene Ausdruckssuche, mit maximal 32 vorbereiteten Rangintervallen und binärer Blockauswahl. Grammatik, Reihenfolge, IDs, Bäume, Gegenrechnung, Definitionslücken und Normalisierung bleiben unverändert. Beide Gleichheitsmodi, Budgetabbrüche, V3-Replay und Z-Projektion sind integriert. Der bisherige Planer bleibt Standard.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**34 CTest-Suiten bestanden**, Debug und UndefinedBehaviorSanitizer (`-fsanitize=undefined -fno-sanitize-recover=all`), keine Sanitizer-Meldung. C++ mit `-Wall -Wextra -Werror`. Zwei neue CLI-Tests und eine neue direkte C++-Suite. Die neue CLI-Anforderung wurde vor Implementierung mit `UnknownSearchPlanner` rot nachgewiesen. Gesamtdemo erfolgreich.

Geprüfte Fälle:

- Beide Gleichheitsmodi liefern auf dem Referenzlauf identische Hypothesen einschließlich Bäumen, IDs, Status und gegebenenfalls Normalformen und Herkunft.
- Planungsarbeit sinkt; sämtliche übrige Rechenarbeit bleibt exakt gleich. Kandidatenanzahl bleibt 2802.
- Bei exakt ausreichendem neuen Operationsbudget ist der indizierte Lauf vollständig, der bisherige Lauf unvollständig.
- Sämtliche Bäume bis neun Knoten mit nur x beziehungsweise x und Konstante 1 sowie alle Bäume bis sieben Knoten mit Konstanten −1, 0, 2/3 stimmen knotenweise mit dem alten Generator überein.
- Maximale Grammatik mit 65 Terminals und neun Knoten: erste und letzte Ränge sowie 10000 reproduzierbare große Zufallsränge stimmen knotenweise überein. Der letzte Rang bleibt lazy erzeugbar.
- Reine Terminals benötigen keinen Index; sieben Knoten benötigen 20 und neun Knoten 32 Einträge. Ungültige Ränge werden abgewiesen.
- Jede kleinere positive Operationsgrenze, sämtliche Checkpoints und Kandidaten-Abbruchpositionen zweier kleiner Aufgaben unter beiden Gleichheitsmodi sind unvollständig und identisch replaybar.
- Callback-Abbruch sowie exakt ausreichende Operations-/Kandidaten-/Traceereignisbudgets funktionieren.
- Trace-, Tracebyte- und Ergebnislimits bleiben wirksam; andere Strategien weisen den neuen Planer zurück.
- Manipulierte Planer-/Gleichheitsversionen und Indexmetadaten werden trotz neu berechnetem Digest zurückgewiesen.
- Z-Auswahl bei x=0 liefert SELECTED mit Wert 0; x=-1 bleibt gesperrt. Auswahl-Replay benötigt die ursprüngliche Forschungs- und Regeldatei nicht.
- Ausdrucks-V1/V2, Portfolio-V8 und das vor Änderung tatsächlich gesicherte Core-0.17-V10 mit eingebettetem Erfahrungssnapshot bleiben unverändert replaybar. Alle übrigen historischen Regressionstests bestehen ebenfalls.

## Unabhängiges Review

Das lesende Review fand keine blockierenden Probleme in Blockaufteilung, Dreiecksrangfolge, Ganzzahlgrenzen, Kosten, V3-Versionen oder Projektion. Ein unabhängiger Prüfhelfer verglich zusätzlich **99410 Ränge** über Konfigurationen mit 0, 1, 2, 16 und 64 Konstanten sowie Knotengrenzen 1–9, einschließlich Blockgrenzen und großen Zufallsrängen. Keine Baumabweichung.

Der neue V10-Fixturetest musste den äußersten Ergebnisblock auswählen: Ein Erfahrungsexport enthält bereits einen eingebetteten Quell-Replay mit eigener RESULT-Markierung. Die erwartete JSON-Ausgabe wird deshalb nach der letzten Markierung gelesen. Der Produkt-Replay selbst war davon nicht betroffen.

## Gemessener Referenzlauf

Bestehende Ausdrucks-Lern-/Hold-out-Paare, einzige Konstante 1, maximal sieben Knoten:

| Messgröße | Bisheriger Planer | Indizierter Planer |
| --- | ---: | ---: |
| Kandidaten | 2802 | 2802 |
| Planungsarbeit | 69064 | 56843 |
| Gesamtarbeit, Syntaxmodus | 103126 | 90905 |
| Gesamtarbeit, rationale Zusammenführung | 105518 | 93297 |

**12221 logische Einheiten weniger**. Die rationell zusammengeführten Hypothesen bleiben VERIFIED_ON_SAMPLES; getrennte Syntaxhypothesen bleiben AMBIGUOUS. Bei Operationsbudget 93297 ist der neue rationale Lauf vollständig, der bisherige INCOMPLETE. Die Demo führt diesen Vergleich, Replay und Auswahl tatsächlich aus.

## Grenzen

Logische Arbeitsersparnis auf dem gemessenen Referenzfall; keine gemessene CPU-/Laufzeitbeschleunigung und keine Garantie für jede kleine Konfiguration. Suchraum und Kandidatenzahl werden nicht reduziert. Der Index ist vorerst nur in der eigenständigen Ausdruckssuche auswählbar. Portfolio-/Experience-Einbindung, allgemeinere Grammatiken und M2-Gesamtabschluss bleiben offen.

[Vertrag](../39-indexed-expression-contract.md), [Benutzung](../40-indexed-expression-usage.md).
