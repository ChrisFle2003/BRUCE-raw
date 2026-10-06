# Abnahme Grid-/Experience-Importrahmen / Core 0.45 – 22.09.2026

## Lieferung

Neue erfahrungsgeleitete Grid-Forschung mit Gesamtrahmen zählt vollständige Gridquellbytes und Experience-Snapshotbytes vor der neuen Suche. V4 erhält Policy, ursprünglichen Rahmen, Bytebindung, Teilbefunde und Herkunft. Historische V3-Replays behalten ihre Abrechnung. Z-Auswahl akzeptiert vollständige V4-Befunde; Experienceimport bleibt wegen der erfahrungsgeleiteten Herkunft ausgeschlossen.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**80 von 80 Testgruppen bestanden**, sowohl Debug (64,18 s) als auch UndefinedBehaviorSanitizer (114,97 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 150 Markdown-Dateien geprüft. 17 gezielte Grid-/Experience-Testgruppen und ergänzte Admission-/Abbruchprüfungen bestehen ebenfalls.

Die neuen Integrationstests zeigten vor Umsetzung die fehlende Experience-Abrechnung. Drei Tests decken den Ablauf ab:

- Grenzwerte mit einer, einer zu wenig, exakt genügend und einer zusätzlichen Suchoperation; Verbrauch und Rest korrekt, Teil-/Vollbefunde identisch replaybar ohne Originaldateien.
- Reiner Gridrahmen ohne genügend Platz für Experience, exakt volle Importkosten, ungültige Gesamtgrenzen und manipulierter Bytezähler/Policy werden abgewiesen. Separates kleineres Suchlimit bleibt wirksam.
- Vollständiger V4-Befund fließt in Z-Auswahl und deren Replay ohne Originale; rekursiver Experienceimport wird abgewiesen.

Die C++-Suite prüft zusätzlich V4-Abbruch, Y-Projektion und dass die neue Zulassungsprüfung den bereits geparsten Task sieht, bevor ein ungültiger eingebetteter Snapshot ausgeführt werden könnte.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Drei Integrationstests bestanden unabhängig. Ein mit Core 0.44 erzeugtes gelerntes V3-Bundle lieferte beim neuen Replay identische Ausgabe. Gelernte Läufe ohne Gesamtrahmen und ungelernte V3-Läufe blieben bytegleich. Manipulierte äußere Experience-Bytezahl wurde gegen den inneren Task abgewiesen. Ausführungsreihenfolge, Verbrauchssumme und Z-/Experience-Beschränkungen wurden im Code geprüft.

## Grenzen

Logische Bytezählung statt tatsächlicher Audit-/CPU-Zeitmessung. Quellenprüfung und Snapshotprüfung bleiben atomar; die Auditbilanz bleibt separat. Keine allgemeine Phasenbudgetierung, keine fortsetzbare Formelforschung und kein M2-/M4-Gesamtabschluss.

[Vertrag](../94-grid-experience-budget-contract.md) · [Benutzung](../95-grid-experience-budget-usage.md)
