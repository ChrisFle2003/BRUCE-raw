# Abnahme Grid-Teilbefunde / Core 0.41 – 22.09.2026

## Lieferung

Grid-Stapel lassen sich über `--max-query-steps` und einen Abfragegrenzen-Callback begrenzen. Die CLI verbindet SIGINT/SIGTERM mit diesem Callback. V2 exportiert ursprüngliche Liste, bearbeitetes Präfix, Cacheentscheidungen und Grund; Replay rekonstruiert die Abbruchposition. Forschung akzeptiert nur vollständig abgearbeitete Quellen. Unbeschränkte, nicht abgebrochene Läufe bleiben bei V1.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**76 von 76 Testgruppen bestanden**, sowohl Debug (61,05 s) als auch UndefinedBehaviorSanitizer (111,87 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 138 Markdown-Dateien geprüft.

Die drei neuen Integrationstests scheiterten zunächst an der unbekannten Option. Nach Umsetzung bestehen sie; die C++-Suite wurde um Callback-/Grenzprüfungen erweitert.

- Alle Präfixlängen 0–5 einer gemischten Liste in drei Modi liefern dieselben Einzelergebnisse wie der entsprechende Anfang des vollständigen Laufs; Cachetreffer und lokale Stopps bleiben erhalten.
- Jede Callback-Grenze 0–N wurde einschließlich Aufrufzahl, gespeicherter Position, Status und Replay geprüft. Nach der letzten Abfrage wird kein zusätzlicher Callback ausgelöst.
- Bei gleichzeitigem Callback-Abbruch und Schrittbudgetende gewinnt `Cancelled`. Nullbudget berechnet keine Werte und belegt keinen Cache.
- Ungültige Grenzen und fehlerhafte unbesuchte Pfade werden abgewiesen. Ungültige Eingaben erzeugen keinen Export.
- Unvollständige Trainings- oder Holdout-Stapel werden als Quellen abgelehnt. Vollständige V2-Stapel durchlaufen Forschung und Gesamtbudget-Replay auch ohne ursprüngliche Dateien.
- Geänderte Grenzen, Policy, Status und Fortschrittszähler werden trotz neuem Digest abgewiesen.
- Ein reales SIGINT wurde bei einem Auftrag mit 1024 Abfragen erfasst. Es ergab `INCOMPLETE / Cancelled` vor der ersten Abfrage; der exportierte Teilbefund replayte identisch. Weitere Positionen sind deterministisch durch Callback-Tests abgedeckt, nicht durch zeitabhängige Signaltests behauptet.

## Unabhängiges Review

Keine konkreten Fehlerbefunde. Drei neue Integrationstests und Grid-Cache-Unit bestanden im Review. Drei Baseline-Fälle gegen Core 0.40 hatten identische Ausgaben und bytegleiche V1-Exporte.

78 zusätzliche Präfix-/Replay-Prüfungen bestätigten Ergebnisse, Cachezähler und Auswertungszahlen. Unbesuchte Werte wurden nicht berechnet; eine neu gehashte ungültige Koordinate im Rest wurde dennoch zurückgewiesen. Abbruchpriorität, Abschluss nach letzter Abfrage und Zurückweisung unvollständiger Forschungsquellen wurden gelesen. Vollständige Suiten, Dokumentation und realer Signaltest wurden vom Hauptlauf geprüft.

## Grenzen

Abbruch erfolgt zwischen vollständigen Abfragen. Vorabprüfung der Pfadsyntax bleibt atomar; noch nicht bearbeitete Werte sind mathematisch ungeprüft. Quellenprüfung innerhalb der Forschung bleibt ebenfalls atomar. Replay startet mit leerem Cache und ist keine Wiederaufnahme ab dem nächsten Schritt. Allgemeine Phasensteuerung, Expansion und M2-Gesamtabschluss bleiben offen.

[Vertrag](../86-grid-partial-contract.md) · [Benutzung](../87-grid-partial-usage.md)
