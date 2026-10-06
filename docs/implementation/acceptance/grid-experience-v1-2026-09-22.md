# Abnahme Grid-Erfahrung / Core 0.43 – 22.09.2026

## Lieferung

Grid-Forschungsbundles V1/V2/V3 mit unabhängigem, abgeschlossenem Portfolio und erfolgreichem Holdout sind Experiencequellen. Vollständige Herkunft einschließlich fortgesetzter Grid-Stapel bleibt im Snapshot. V12 kennzeichnet Grid-Provenienz; native Stores behalten ihre Formate. Stimmen, Kontextabgleich, Deduplizierung und Widerruf folgen den bestehenden Regeln.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**78 von 78 Testgruppen bestanden**, sowohl Debug (65,29 s) als auch UndefinedBehaviorSanitizer (117,14 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 144 Markdown-Dateien geprüft. Die 15 gezielten Grid-/Experience-Testgruppen bestehen ebenfalls.

Die ersten drei neuen Integrationstests scheiterten vor der Umsetzung an der fehlenden Grid-Zulassung. Die fertige Integration umfasst vier Tests:

- Grid → Portfolio → Snapshot → späterer Forschungslauf; passende Stimmen werden übernommen, Widerruf deaktiviert sie, der fertige Forschungsexport replayt ohne sämtliche ursprünglichen Dateien.
- Beide Projektionsachsen und gemeinsamer V3-Rahmen; vollständiger Quelldigest und `grid_origin` bleiben sichtbar. Mehr als eine Million deklarierte Forschungsoperationen und unvollständige Befunde werden abgelehnt.
- Doppelte Lernpaare werden auch über CSV/Grid und andere Zeilenreihenfolgen hinweg abgewiesen. Gemischter Snapshot mit neuem CSV-Datensatz wird korrekt erneut eingelesen. Einzelstrategie und fehlender Holdout sind unzulässig.
- Bereits erfahrungsgeleitete Grid-Forschung wird abgewiesen. Manipulierte Quelle bleibt trotz neuem äußerem Digest ungültig.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Die ersten drei Integrationstests wurden unabhängig ausgeführt. Zwei bisherige Experience-Fälle gegen Core 0.42 ergaben identische Ausgaben und bytegleiche Stores. Vier X/Y-Fälle mit und ohne V3-Rahmen bestätigten vollständigen Quelldigest, Auditbilanz, Stimmen, Kontexte und Wiedereinlesen. Zulassung unabhängiger Portfolioheader vor innerem Replay, Operationslimit und V12-Auswahl wurden im Code geprüft.

## Grenzen

Auditkosten sind logische Forschungs-/Formanalyseoperationen plus Gridquellbytes, keine CPU-Messung und kein Gesamtprozessbudget. Die Auditbilanz bleibt separat vom späteren Suchbudget. Snapshotprüfung und Quellenreplay bleiben atomar. Größen-, Datensatz- und Ereignisgrenzen bleiben unverändert. Keine automatische Promotion mathematischer Aussagen, kein allgemeines Lernen, kein M2-/M4-Gesamtabschluss.

[Vertrag](../90-grid-experience-contract.md) · [Benutzung](../91-grid-experience-usage.md)
