# Abnahme Ausführungsphasen / Core 0.49 – 22.09.2026

## Lieferung

Optionale synchrone Sitzung mit erzwungener Phasenfolge CREATED → EXECUTING → SERIALIZING → REPLAYING → READY. Arbeitsphasen können terminal fehlschlagen. Acht CLI-Laufarten unterstützen `--verification replay`; Freigabe und Export erfolgen erst nach typisiertem Replay und Inhaltsvergleich. Native Formate bleiben unverändert, Teilbefunde ausdrücklich unvollständig.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

Beide Gesamtsuiten bestehen: Debug 87/87 Testgruppen in 63,66 Sekunden, UBSan 87/87 in 115,36 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Auch die vollständige Demo und die fünf gezielten Session-/Adapter-/Inspektionstestgruppen bestehen. Die 26 historischen Replay-Fixtures liefern weiterhin ihr gespeichertes Ergebnis; die lokalen Links aller 165 Markdown-Dateien wurden geprüft.

Die neuen Integrationstests scheiterten vor Umsetzung an der unbekannten Option. Sie prüfen vollständige und unvollständige Stapel, exakte Phasenfolge, lokales Sentinelende, bytegleiche native Exporte, identisches Replay, ungültige Modi und Fehler ohne Export. `none` entspricht dem bisherigen Aufruf.

Die neue Zustands-Unit prüft legale und unerlaubte Übergänge, terminale Fehler in allen Arbeitsphasen sowie Abbruchweitergabe ohne erneute Abfrage während des Replay. Die Adapter-Unit führt alle acht Auftragstypen zusätzlich als geprüfte Sitzung aus und vergleicht die Bundlebytes mit direkter nativer Ausführung.

19 gültige Strategie-/Planerfälle gegen Core 0.48 lieferten unveränderte Ausgaben, Exporte und historischen Replay.

## Unabhängiges Review

Keine konkreten Fehler. Session-/Adapter-Units und beide Integrationstests bestanden unabhängig. Alle acht CLI-Routen wurden gegen Core 0.48 geprüft: `none` unverändert, geprüfte Ausführung mit identischem nativem Ergebnis und bytegleichen Exporten. Teilbefunde und lokaler Gridstopp waren enthalten. Callbackverhalten, Exportreihenfolge und Fehler ohne Export wurden geprüft.

## Grenzen

READY bestätigt den Prüfablauf, nicht mathematische Allgemeingültigkeit oder Vollständigkeit. Der Bericht ist kein persistierter Phasennachweis. Zusätzliche Replayarbeit ist nicht durch ein gemeinsames Sitzungskostenbudget begrenzt; Replay/Serialisierung und Quellenprüfung bleiben atomar. Kein allgemeiner Phasengraph, keine Hintergrundjobs und keine persistente Fortsetzung allgemeiner Forschung. Kein vollständiger M0–M2-Abschluss.

[Vertrag](../102-execution-phases-contract.md) · [Benutzung](../103-execution-phases-usage.md) · [ADR](../../adr/012-checked-execution-phases.md)
