# Abnahme GridSpec und Grid-Cache / Core 0.37 – 22.09.2026

## Lieferung

Versionierte GridSpec für das vorhandene lokale UInt64-Grid, vollständige kanonische Adress-/Konfigurationsschlüssel, run-lokaler FIFO-Cache mit höchstens 64 Einträgen und logischem Bytebudget. Der neue Stapelauftrag exportiert Eingaben, Cacheentscheidungen und Einzelergebnisse als `BRUCE_GRID_BATCH_BUNDLE_V1`. Replay startet mit leerem Cache und prüft die gesamte Payload. Einzel-Grid-V1 bleibt kompatibel.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**71 von 71 Testgruppen bestanden**, sowohl Debug (61,06 s) als auch UndefinedBehaviorSanitizer (110,48 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; lokale Dokumentationslinks in 126 Markdown-Dateien geprüft.

Die fünf ursprünglichen neuen Integrationstests scheiterten vor Implementierung erwartungsgemäß an `UnknownCommand: grid-batch`. Nach Umsetzung bestehen sieben Integrationstestmethoden und die neue direkte C++-Suite.

- Alle 64 lokalen Adressen in drei Modi, jeweils zweimal abgefragt: Werte unabhängig mit Python-Potenz/Bitlänge geprüft, jeweils 64 Berechnungen und 64 Treffer.
- Kanonische Adressaliase teilen einen Eintrag; wertgleiche unterschiedliche Adressen, Tiefe und Pfadreihenfolge bleiben getrennt.
- Direkte API prüft unterschiedliche Modi, Moduli, Bitgrenzen und Sentinel-Politiken im selben Cache. Ein ungültiger Auftrag verwendet keinen unpassenden vorhandenen Eintrag.
- Nullbudget, zu kleines Budget, exakt ausreichendes Budget, Verdrängung sowie 64-Eintragsgrenze. Ein Treffer verändert FIFO-Reihenfolge nicht.
- 1024 Wiederholungen: eine Berechnung und 1023 Treffer. 1025 Abfragen werden abgelehnt. Pfadtiefe 64 mit Wiederholung geprüft.
- Lokale Sentinel-Stopps werden nicht gespeichert und verhindern keine Nachbarabfragen. Neue Stapel starten wieder ohne Cacheinhalt.
- Ungültige Eingaben erzeugen keinen Export; bestehende Exportdateien bleiben unverändert.
- Neu gehashte Änderungen an Versionen, Bytebudget, Cacheentscheidungen, Wert, Sentinel-Politik und Anfragezahl werden abgewiesen. Jede gekürzte und neu gehashte Payload eines Referenzbundles wird verworfen, ebenso zusätzliche Inhalte.
- **25 historische Fixtures** liefern unveränderte Ergebnisse, einschließlich eines vor der Änderung tatsächlich mit Core 0.36 erzeugten Grid-V1-Bundles.
- Sechs Eintragskosten bei Tiefe 1 und 64 in allen drei Modi unabhängig gegen rekonstruierte kanonische Schlüssel und mit der Core-0.36-Binärdatei erzeugte Ergebnisbundles abgeglichen. Digests und Byteabrechnung stimmen überein.
- Gesamtdemo: fünf Abfragen mit drei Adressen benötigen mit Cache drei statt fünf Potenzberechnungen, bei identischen Einzelergebnissen. Export und Replay stimmen überein; zwei lokale Stopps beeinträchtigen Nachbaranfragen nicht.

## Unabhängiges Review

Keine blockierenden oder korrektheitsrelevanten Befunde. Ein unabhängiges FIFO-/Zählermodell bestätigte **2400 Cache-Abfragen** über drei Modi, vier Budgets, Adressaliase und Sentinel-Bypass. Die einzelnen Eintragskosten wurden dafür aus Einzelabfragen übernommen, nicht unabhängig nachimplementiert.

**80 Einzel-Grid-Fälle** gegenüber der gesicherten Core-0.36-Binärdatei: Rückgabecode, stdout und stderr identisch. Acht zusätzliche neu gehashte Manipulationen an Metadaten, Statistiken, Trace und Format wurden abgewiesen. Vollständige Suiten und Demo wurden vom Hauptlauf geprüft; Prozess-RAM und Laufzeitgewinn waren kein Gegenstand dieses Reviews.

## Grenzen

Das Cachebudget zählt kanonische Schlüssel- und Ergebnisbundlebytes. Es ist keine Prozess-RAM-Grenze und enthält weder Allocator-Overhead noch die komplette Stapelausgabe. Eingabe-/Anzahlgrenzen beschränken den übrigen Stapel. Ein vermiedener Potenzaufruf ist kein gemessener allgemeiner Geschwindigkeitsgewinn.

Forschungseinbindung, allgemeinere Grid-Formen, Expansion, BigInt im Grid/Z und umfassende Aststeuerung bleiben offen. Die vorhandenen mathematischen Caches werden nicht ersetzt. Keine pauschale M2- oder Gesamtprojektabnahme.

[Vertrag](../78-grid-cache-contract.md) · [Benutzung](../79-grid-cache-usage.md)
