# Abnahme interner Grid-Replaypunkte / Core 0.53 – 23.09.2026

## Lieferung

Optionaler werfender ReplayCheckpoint für GridBatch V1/V2 und rekursive GridResume-V1-Prüfung. CLI-Modus `grid-query-boundaries` koppelt ihn an die bestehende Abbruchanforderung. Native Suchgrenzen werden nicht verändert; unterbrochene Prüfungen liefern keinen Teilbefund und keinen Export. Erfolgsbericht V4; bestehende Modi und native Formate bleiben erhalten.

## Tests

Der neue CLI-Test scheiterte zunächst am unbekannten Modus. 17 gezielte Grid-/Session-/Adaptertestgruppen bestehen. Die neue Unit unterbricht jeden erreichbaren Callbackpunkt in vollständigen, budgetbegrenzten und nativ abgebrochenen Stapeln sowie drei Fortsetzungsgenerationen. Sie prüft identische Bundlebytes bei normal zurückkehrenden Callbacks, unveränderte Originalexception bei Unterbrechung und Ablehnung manipulierter Payloads trotz neu berechnetem Digest.

Ein Sitzungsfall bricht beim vierten Prüfcallback innerhalb des Replay ab; der native Callback wird dabei nicht erneut abgefragt. Nicht unterstützte Replaytypen und fehlende Prüfcallbacks werden abgelehnt. Die CLI-Integration prüft Stapel/Fortsetzung, V4, Bytebudgetkombination, identische Exporte und Ablehnung des Modus für Einzel-Grid.

Beide Gesamtsuiten bestehen: Debug 88/88 Testgruppen in 64,24 Sekunden, UBSan 88/88 in 116,70 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Die vollständige Demo, 19 Strategie-/Planervergleiche gegen Core 0.52 mit bytegleichen Exporten, 26 historische Replay-Fixtures und die lokalen Links in 185 Markdown-Dateien bestehen ebenfalls.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan` sowie `python3 scripts/demo.py`. Nach Ergänzung des Review-Testfalls wurden die drei betroffenen Testgruppen mit `-R 'grid_replay_control|execution_session'` in beiden Builds erneut erfolgreich geprüft. Am Produktcode gab es danach keine Änderung.

## Unabhängiges Review

Keine konkreten Fehler. Zusätzlich zu vier fokussierten Testgruppen prüfte der Reviewer 80 Vergleiche bisheriger nativer/V1–V3-Ausgaben über 15 Stapelkombinationen und fünf Fortsetzungsgenerationen; alle blieben bytegleich. Auch die nativen V4-Bundles waren in allen 20 Fällen identisch.

Der angeregte Testfall wurde ergänzt: Fortsetzungen mit eigener nativer Cancelled-Grenze vor bzw. nach einer zusätzlichen Abfrage und darauf aufbauende weitere Fortsetzungen werden ebenfalls an jedem erreichbaren Replay-Checkpoint unterbrochen. Die Prüfungen bestehen in Debug und UBSan.

## Grenzen

Tests der internen Unterbrechung verwenden deterministische Callbacks; der vorhandene echte SIGTERM-CLI-Test prüft die Signalverdrahtung an Phasengrenzen. Kein Nachweis garantierter Signallatenz. Hashing, einzelne Berechnungen und Payloadaufbau bleiben atomar; kein vollständiger Meilensteinabschluss.

[Vertrag](../110-grid-replay-checkpoints-contract.md) · [Benutzung](../111-grid-replay-checkpoints-usage.md)
