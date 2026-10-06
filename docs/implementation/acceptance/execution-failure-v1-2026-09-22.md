# Abnahme strukturierter Sitzungsfehler / Core 0.50 – 22.09.2026

## Lieferung

Typisierte `ExecutionSessionError` mit Arbeitsphase, terminalem Verlauf, Originalmeldung und verschachtelter Originalexception. Die CLI ergänzt bei Fehlern innerhalb einer geprüften Sitzung einen Bericht `application-execution-failure-v1`. Fehlertext und Exitcode bleiben erhalten. Vorbereitungs- und Exportfehler bleiben außerhalb dieses Vertrags; native Ergebnisse und Bundleformate ändern sich nicht.

## Prüfungen

Der neue Integrationstest scheiterte vor der Implementierung am fehlenden `execution`-Objekt. Er prüft nun strukturierte Diagnose, kein Ergebnis und keinen Export bei Sitzungsfehler, Erhalt vorhandener Dateien, Abgrenzung von Parsing-/Exportfehlern sowie JSON mit Anführungszeichen und Zeilenumbruch in der Ursache.

Die Unit prüft eine echte Callback-Exception einschließlich Originaltyp und Lebensdauer nach Verlassen des catch-Blocks. Ein gültiger, lang mit Nullen aufgefüllter Gridpfad erzeugt nach erfolgreicher Berechnung ein zu großes Bundle und prüft den Fehler in SERIALIZING. Die bestehenden Phasentests prüfen weiterhin FAILED aus allen drei Arbeitsphasen. Ein tatsächlicher Replayfehler wird in diesem Paket nicht gezielt injiziert.

Die erste neue Escaping-Assertion erwartete ausschließlich die Kurzschreibweise für Zeilenumbrüche; der bestehende JSON-Encoder verwendet gültiges `\u000a`. Die Prüfung wurde auf den dekodierten Inhalt umgestellt.

Gesamtverifikation bestanden:

- Debug: 87/87 Testgruppen in 62,70 Sekunden.
- UBSan mit `-fsanitize=undefined -fno-sanitize-recover=all`: 87/87 Testgruppen in 115,13 Sekunden, ohne Diagnosen.
- Vollständige Demo einschließlich des neuen Fehlerfalls erfolgreich.
- 19 Strategie-/Planerfälle gegen Core 0.49: identische Ergebnisse, bytegleiche Exporte und identischer historischer Replay.
- 26 historische Bundle-Fixtures: Replay stimmt mit dem gespeicherten Ergebnis überein.
- Lokale Links in 176 Markdown-Dateien geprüft, keine ungültigen Ziele.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan` und `python3 scripts/demo.py`.

## Review

Unabhängiges Review ohne konkrete Funktionsfehler. Zwei Session-Testgruppen und sieben zusätzliche Baseline-Vergleiche bestanden beim Reviewer. Seine Anregung zum Exception-Lebensdauertest wurde umgesetzt; die fehlende gezielte Replay-Fehlerinjektion ist oben abgegrenzt.

## Grenzen

Keine allgemeine Fehlerklassifikation aller CLI-Phasen, keine Wiederaufnahme, kein automatischer Retry, kein persistierter Fehlerbeleg und kein vollständiger M0–M2-Abschluss.

[Vertrag](../104-execution-failure-contract.md) · [Benutzung](../105-execution-failure-usage.md)
