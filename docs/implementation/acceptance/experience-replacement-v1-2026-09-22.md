# Abnahme Experience-Korrekturketten / Core 0.44 – 22.09.2026

## Lieferung

`experience replace` ersetzt einen aktiven Datensatz durch eine neue, vollständig geprüfte unabhängige Portfolioquelle. Ein einziges Ereignis speichert Ziel, Grund und Nachfolgerquelle. V13 erhält Verknüpfungen und historische Quellen. Gleiche Lernpaare innerhalb der Vorgängerkette sind zulässig; andere Zweige bleiben dedupliziert. Nur aktive Nachfolger liefern Stimmen.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**79 von 79 Testgruppen bestanden**, sowohl Debug (64,78 s) als auch UndefinedBehaviorSanitizer (115,82 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 147 Markdown-Dateien geprüft. Alle sechs gezielten Experience-Testgruppen bestehen ebenfalls.

Die ersten drei neuen Integrationstests scheiterten vor Umsetzung am unbekannten Befehl. Die fertige Integration umfasst vier Tests:

- Zweifache Ersetzung gleicher Lernpaare mit geänderten Prüfwerten; genau ein aktiver Nachfolger, eindeutige Verweise, unveränderte vorherige Datei und anschließender Widerruf.
- Späterer Forschungslauf verwendet genau eine Erfahrung; sein Export replayt ohne sämtliche ursprünglichen Dateien.
- Fehlendes/inaktives Ziel, identische direkte Quelle, leerer oder mehrzeiliger Grund, fremder doppelter Fingerprint und ungeeignete Ersatzquelle werden ohne neuen Export abgewiesen.
- Manipulierter Zielverweis bleibt trotz neuem äußerem Digest ungültig.
- Nach acht historischen Datensätzen wird eine weitere Ersetzung abgelehnt.
- Grid-Forschungsquellen lassen sich ebenfalls ersetzen; vollständige Grid-Markierung und Herkunft bleiben nach Entfernung der Originaldateien prüfbar.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Vier Replace-Integrationstests bestanden unabhängig. Bisheriges `add` und `revoke` ergaben identische Ausgaben und bytegleiche Stores gegenüber Core 0.43. Ein gleicher Fingerprint in einem inaktiven fremden Zweig wurde abgewiesen. Rückkehr zu einer früheren Quelle der eigenen Vorfahrenkette und erneutes Einlesen funktionierten. Aktive Ziele, vollständige Quellenprüfung, V13-Verweise und historische Grenzen wurden im Code geprüft.

## Grenzen

Maximal acht historische Datensätze, 16 Ereignisse und 1 MiB. Keine Löschung oder Kompaktierung der Vorgeschichte. Ein Austausch dokumentiert Routing-Evidenz und erklärt nicht automatisch einen früheren mathematischen Befund für falsch. Audit bleibt separat vom Suchbudget; keine automatische Korrekturentscheidung, externe Evidenzprüfung oder allgemeines Lernen.

[Vertrag](../92-experience-replacement-contract.md) · [Benutzung](../93-experience-replacement-usage.md)
