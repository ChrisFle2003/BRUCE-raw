# Abnahme Prüfgrenzen-Abbruch / Core 0.52 – 23.09.2026

## Lieferung

Separater optionaler Prüfcallback mit drei Checkpoints: vor Serialisierung, vor Replayzulassung, vor Freigabe. CLI-Modus `--verification-cancellation phase-boundaries` verbindet diese mit SIGINT/SIGTERM. Ein erkannter Abbruch verhindert READY und Export und erhält Phase und Ursache im bestehenden Fehlervertrag. Erfolgreiche optionale Sitzungen verwenden V3; bisherige V1/V2-Berichte und native Bundles bleiben erhalten.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung an der unbekannten Option. Die fokussierten Session-/Adaptertests bestehen. Alle acht Adapter prüfen drei zusätzliche Callbacks bei unveränderten nativen Bundlebytes. Die Session-Unit prüft Abbruch an jedem der drei Checkpoints, Erhalt eines unabhängig nativ abgebrochenen Teilbefunds und Abbruchvorrang vor dem Replay-Bytebudget.

Die CLI-Integration prüft Modusvalidierung, V3-Bericht und Kombination mit Bytebudget. Ein Linux-Signaltest wartet auf den installierten SIGTERM-Handler, sendet ein echtes Signal an einen Forschungsprozess und prüft Exitcode, strukturierte Abbruchursache, leeres stdout und fehlenden Export. Er verwendet keine produktiven Testhooks. Auf anderen Betriebssystemen wird nur dieser auf /proc beruhende Test übersprungen.

Beide Gesamtsuiten bestehen: Debug 87/87 Testgruppen in 64,17 Sekunden, UBSan 87/87 in 115,54 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Nach Ergänzung der zwei Review-Tests wurden die vier betroffenen Session-/Adaptergruppen in beiden Builds erneut erfolgreich geprüft; am Produktcode gab es danach keine Änderung.

Die vollständige Demo, 19 Strategie-/Planervergleiche gegen Core 0.51 mit bytegleichen Exporten, 26 historische Replay-Fixtures und die lokalen Links in 182 Markdown-Dateien bestehen ebenfalls.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan`, die fokussierte Wiederholung mit `-R 'execution_session|application_task'` sowie `python3 scripts/demo.py`.

## Unabhängiges Review

Keine konkreten Funktionsfehler. Alle acht CLI-Laufarten wurden zusätzlich gegen Core 0.51 geprüft: native Ausgabe, V1/V2 und explizites `none` unverändert; V3 mit und ohne Bytebudget korrekt. Vier fokussierte Testgruppen einschließlich echtem SIGTERM bestanden unabhängig.

Die beiden angeregten Tests wurden ergänzt und bestehen: Eine Exception aus dem Prüfcallback behält Phase und verschachtelten Originaltyp; eine Änderung am ursprünglichen `VerificationControl` während der nativen Ausführung ändert den zuvor kopierten Prüfcallback nicht.

## Grenzen

Keine Unterbrechung innerhalb atomarer Prüfoperationen und keine maximale Abbruchlatenz. Ein Signal nach dem letzten Checkpoint liegt außerhalb der Zusage. Kein persistierter Sitzungsbeleg und kein vollständiger Meilensteinabschluss.

[Vertrag](../108-verification-cancellation-contract.md) · [Benutzung](../109-verification-cancellation-usage.md)
