# Abnahme Grid-Fortsetzung / Core 0.42 – 22.09.2026

## Lieferung

`grid-resume` setzt unvollständige Grid-Stapel mit unveränderten Pfaden und Rechenregeln fort. Es replayt die Elternquelle, rekonstruiert den Cache aus ihrem Präfix und bearbeitet den nächsten Abschnitt. Ein eigener V1-Export enthält die gesamte Herkunft. Fertige Fortsetzungen werden als Forschungsquellen akzeptiert. Audit- und neue Potenzberechnungen werden getrennt gezählt; die bisherige Cachebilanz bleibt nachvollziehbar.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

Debug: **77 von 77 Testgruppen bestanden** (61,32 s). UndefinedBehaviorSanitizer: ebenfalls **77 von 77 Testgruppen bestanden** (111,62 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 141 Markdown-Dateien geprüft.

Die drei neuen Integrationstests scheiterten vor der Umsetzung am unbekannten Befehl. Danach bestehen sie. Die bestehende C++-Suite wurde um Fortsetzungs- und Callbackprüfungen ergänzt.

- Gemischte Liste mit normalisiertem Cachetreffer und lokalem Stopp: fortgesetzte Ergebnisse und Cachebilanz stimmen mit dem vollständigen Lauf überein.
- Callback-Abbruch an jeder neuen Grenze, einschließlich sofortigem Abbruch und vollständiger Liste; identisches Replay.
- Cache mit Verdrängung: rekonstruierter FIFO liefert denselben fertigen Stapel wie ununterbrochene Ausführung.
- Acht Generationen erlaubt, neunte abgelehnt; null oder mehr als 1024 zusätzliche Abfragen abgelehnt, kein Fehlerexport.
- Geänderte zusätzliche Abfragezahl, Policy und Ergebniszähler trotz neuem Digest abgelehnt.
- Replay funktioniert nach Entfernen der Vorgängerdateien. Vollständig fortgesetzte Quellen durchlaufen Forschung mit Gesamtbudget und Holdout; auch deren Replay benötigt keine Originalquellen.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Drei Resume-Integrationstests und Grid-Cache-Unit bestanden im Review. 27 zusätzliche fortgesetzte Generationen bestätigten Präfixergebnisse, Cachezustand, Audit-/neue Berechnungen und Replay. 36 alte Fälle gegen Core 0.41 lieferten identische Ausgaben und bytegleiche Bundles. Generationenlimit, Abbruchgrenzen, Manipulationsabwehr und Forschungsübergang wurden geprüft.

## Grenzen

Abbruch wird zwischen neuen Abfragen geprüft. Elternprüfung und Cacheaufbau sind atomar. Reale Signalzustellung unter Last wurde für dieses Paket nicht erneut gemessen; die Callback-Grenzen sind deterministisch getestet. Zähler messen Potenzberechnungen, nicht die gesamte Prozessarbeit. Allgemeine Phasensteuerung, fortsetzbare Forschung und M2-Gesamtabschluss bleiben offen.

[Vertrag](../88-grid-resume-contract.md) · [Benutzung](../89-grid-resume-usage.md)
