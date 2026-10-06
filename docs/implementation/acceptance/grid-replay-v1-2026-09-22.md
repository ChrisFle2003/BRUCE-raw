# Abnahme Grid-Einzelauftrag / Core 0.36 – 22.09.2026

## Lieferung

Die bisherige Grid-CLI delegiert an `GridTask` und `GridRun`. Ein erfolgreicher Einzelauftrag oder Sentinel-Stopp lässt sich als `BRUCE_GRID_BUNDLE_V1` exportieren. Replay rekonstruiert sämtliche Parameter, rechnet das letzte lokale Koordinatenpaar erneut und vergleicht die vollständige Payload einschließlich vier Trace-Ereignissen und Ergebnis. Die bestehenden Modi und gültigen JSON-Ausgaben bleiben erhalten.

## Ausgeführte Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**69 von 69 CTest-Gruppen bestanden**, sowohl in Debug (61,74 s) als auch mit UndefinedBehaviorSanitizer (112,75 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich; lokale Dokumentationslinks in 123 Markdown-Dateien geprüft.

Die vier neuen CLI-Testmethoden scheiterten vor der Implementierung erwartungsgemäß an `UnknownOption: --output` und bestehen nach der Umsetzung. Eine weitere direkte C++-Suite prüft die Anwendungs-API und den Codec.

- Alle 64 lokalen Koordinatenpaare in allen drei Modi: 192 Fälle mit unabhängiger Python-Potenz-/Bitlängenberechnung, unverändertem CLI-Ergebnis und identischem Replay.
- Pfadtiefen 1, 12 und 64; gleiche Werte an unterschiedlichen Adressen bleiben unterscheidbar. Die ursprüngliche Schreibweise einschließlich führender Nullen bleibt erhalten.
- Sentinel-Stopp am letzten `(0,0)` mit null Auswertungen; ein früheres `(0,0)` stoppt ein späteres Blatt nicht.
- Exakte Bitgrenzen, Modulus 1 und maximaler UInt64-Modulus, fehlender/ungültiger Modulus, ungültige Modi/Politiken, zu tiefe oder ungültige Pfade sowie Bytegrenzen.
- Ungültige Aufträge erzeugen keinen Export; vorhandene Dateien bleiben beim erneuten Export unverändert.
- Alle Verkürzungen eines Referenzbundles und seiner neu gehashten Payload werden abgewiesen. Geänderte Metadaten, Trace, Ergebnisse und zusätzliche Inhalte werden auch nach erneuter Digestberechnung abgelehnt.
- Alle **24 historischen Bundle-Fixtures** liefern weiterhin das gespeicherte Ergebnis.
- Die Gesamtdemo exportiert und replayt alle drei Modi bei Tiefe 12 sowie einen Sentinel-Stopp.

## Unabhängiges Review

Das lesende Review fand keine blockierenden Probleme. Bei **120 Vergleichen** mit der vor Änderung gesicherten Core-0.35-Binärdatei waren Rückgabecode, Standardausgabe und Fehlerausgabe identisch. Zusätzlich wurden sieben neu gehashte Metadaten-/Trace-/Formatmanipulationen abgewiesen. Der Hinweis auf die zunächst fehlende `--output`-Angabe in der CLI-Hilfe wurde vor dem abschließenden Build behoben.

## Grenzen

Die Lieferung wertet ein einzelnes lokales Blatt aus; `64^depth` ist eine theoretische Raumangabe. Sie führt keine Expansion, neue Suchfamilie oder allgemeine Aststeuerung ein. Vollständige GridSpec-/Cacheidentität, Forschungseinbindung und BigInt im Grid/Z bleiben offen. Der Digest bestätigt keine externe Urheberschaft. Replay ist eine reproduzierte Berechnung, kein allgemeiner mathematischer Beweis. Kein vollständiger M2- oder Gesamtprojektabschluss.

[Vertrag](../76-grid-replay-contract.md) · [Benutzung](../77-grid-replay-usage.md)
