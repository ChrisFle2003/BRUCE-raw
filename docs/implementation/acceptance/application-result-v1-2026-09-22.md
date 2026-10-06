# Abnahme Ergebnis-/Replay-Vertrag / Core 0.48 – 22.09.2026

## Lieferung

Gemeinsamer typisierter Replay für acht Laufarten. Der bisherige Replay-Befehl verwendet ihn mit unveränderter Darstellung. `inspect-run` prüft den vollständigen Inhalt und zeigt anschließend Auftragstyp, Ausführungsstand, nativen Status und Stopgrund. Keine neue Bundlehülle und keine automatische mathematische Aufwertung.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**85 von 85 Testgruppen bestanden**, sowohl Debug (64,51 s) als auch UndefinedBehaviorSanitizer (117,01 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 161 Markdown-Dateien geprüft. Drei gezielte Adapter-/Katalog-/Inspektionstestgruppen bestehen ebenfalls.

Die ersten beiden Integrationstests scheiterten vor Umsetzung am unbekannten Befehl. Die Integration umfasst jetzt drei Tests: Grid-Teilbefund gegenüber lokalem Sentinelstopp, abgewiesene Manipulation, erhaltene fachliche Vergleichsstatus sowie alle historischen Fixtures. Der bestehende Adapter-Unit prüft nun für alle acht Laufarten den typisierten Replay, Erhalt der Variante, identische Darstellung und passende Inspektionsart.

## Unabhängiges Review

Keine konkreten Fehler. Adapter-Unit und die ersten beiden Integrationstests bestanden unabhängig. Alle 26 historischen Fixtures lieferten identische Replay-Ausgaben, Exitcodes und Fehlerausgaben gegenüber Core 0.47. Ein Vergleich mit erschöpftem Budget blieb unvollständig. Eine neu gehashte Statusmanipulation wurde vor der Inspektionsausgabe abgewiesen. Die Trennung von lokalem Gridstopp, Ausführungsabschluss und mathematischer Aussage wurde geprüft.

## Grenzen

Die Inspektion ist ein vollständiger Replay, keine schnelle Metadatenabfrage. Kein neues Inspektionsbudget, keine allgemeine Phasen-Zustandsmaschine oder Änderung bestehender Gateverträge. Mathematische Beweisangaben und Gültigkeitsbereiche bleiben im vollständigen nativen Ergebnis. Experience-Snapshots behalten ihre eigene Inspektion. Kein vollständiger M0–M2-Abschluss.

[Vertrag](../100-application-result-contract.md) · [Benutzung](../101-application-result-usage.md)
