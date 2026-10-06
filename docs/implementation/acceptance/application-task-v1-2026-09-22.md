# Abnahme typisierter Auftragseinstieg / Core 0.47 – 22.09.2026

## Lieferung

`ApplicationTask` und `ApplicationRun` verbinden acht bestehende Laufarten. Gemeinsame Ausführung, Darstellung und Exportserialisierung delegieren an vorhandene Fachfunktionen. Alle acht CLI-Pfade nutzen den Einstieg. `task-contracts` zeigt Umfang und Grenzen. Keine Änderung mathematischer Algorithmen oder Wireformate.

## Prüfungen

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
cmake --build build-ubsan -j4
ctest --test-dir build-ubsan --output-on-failure -j4
python3 scripts/demo.py
```

**84 von 84 Testgruppen bestanden**, sowohl Debug (64,66 s) als auch UndefinedBehaviorSanitizer (116,50 s; `-fsanitize=undefined -fno-sanitize-recover=all`). Keine Sanitizer-Meldung. Gesamtdemo erfolgreich. Alle 26 historischen Bundle-Fixtures liefern unveränderte Ergebnisse. Lokale Links in 158 Markdown-Dateien geprüft. Die beiden neuen Adapter-/Katalogtestgruppen bestehen ebenfalls.

Der Katalogtest scheiterte vor Umsetzung am unbekannten Befehl. Der C++-Test vergleicht alle acht Adapter direkt mit den Fachfunktionen: identische JSON- und Bundlebytes. Er prüft lokale Abbruchweitergabe für Forschung, Stapel, Fortsetzung, Grid-Forschung, Vergleich und Z; atomare Einzel-Grid-Auswertung ohne Callbackaufruf; abgelehnte ungültige Pfade sowie identisches Replay eines Teilbefundes.

19 gültige Strategie-/Planerfälle gegen Core 0.46 liefern identisches JSON, bytegleiche Exporte und identischen historischen Replay.

## Unabhängiges Review

Keine konkreten Fehler. Der Application-Task-Unit bestand unabhängig. 13 weitere Baseline-Fälle über alle acht Auftragstypen, einschließlich begrenzter Läufe, lieferten identische Ausgaben, Exitcodes und Bundles. Synchrone Lebensdauer, Erhaltung fehlender Callbacks, lokale Checkpoints und Delegation der Codecs wurden geprüft. Der anschließend ergänzte Z-Abbruchtest wird durch den Hauptlauf abgedeckt.

## Architekturabnahme und Grenzen

Ein gemeinsamer typisierter C++-Auftragseinstieg ist für acht vorhandene Laufarten implementiert und von der CLI genutzt. Fachliche Validatoren, Budgeteinheiten, Ergebnisaussagen und Replaydecoder bleiben erhalten. Der Einstieg selbst hat keinen Dateizugriff. Kein dynamisches Plugin-System, generisches JSON-Auftragsformat, Hintergrundjob-System oder allgemeiner Phasen-/Gatevertrag. Hilfsbefehle und Experience-Verwaltung bleiben außerhalb des Laufvertrags. Keine pauschale M0–M2-Gesamtabnahme.

[Architekturentscheidung](../../adr/011-typed-application-entry.md) · [Benutzung](../99-application-task-usage.md) · [Plan](../98-application-task-plan.md)
