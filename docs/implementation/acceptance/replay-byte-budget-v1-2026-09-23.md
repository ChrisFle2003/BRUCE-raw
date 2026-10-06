# Abnahme Replay-Eingangsbudget / Core 0.51 – 23.09.2026

## Lieferung

Optionales Bytebudget vor dem Replay für alle acht typisierten Auftragstypen. Die CLI bietet `--max-replay-bytes` zusammen mit `--verification replay`; erfolgreiche begrenzte Sitzungen melden Limit und tatsächliche Bundlegröße unter `application-execution-v2`. Bestehende Berichte ohne Limit und native Bundles bleiben erhalten. Budgetüberschreitung beendet die Sitzung vor dem Replay ohne Export.

## Prüfungen

Der neue Integrationstest scheiterte zunächst an der unbekannten Option. Er prüft die exakte Bytegrenze, ein Byte darunter, Null, Ausführung ohne Export, bestehende Zieldateien, unveränderte native Bundles, INCOMPLETE-Erhalt sowie ungültige Optionskombinationen und Zahlen.

Die Adapter-Unit prüft für alle acht Auftragstypen ein exakt passendes Budget und die Ablehnung mit Nullbudget. Die Session-Unit prüft den terminalen Verlauf bei fehlender Replayzulassung und Änderungen am Budget des Aufrufers während eines Callbacks. Dieser letzte Test deckte eine Referenzweitergabe auf; die Übernahme als Wert beseitigte die Änderung während der laufenden Sitzung.

Beide Gesamtsuiten bestehen: Debug 87/87 Testgruppen in 65,04 Sekunden, UBSan 87/87 in 116,07 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Die vollständige Demo, 19 Strategie-/Planervergleiche gegen Core 0.50 mit bytegleichen Exporten, 26 historische Replay-Fixtures und die Prüfung der lokalen Links in 179 Markdown-Dateien sind ebenfalls erfolgreich.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan` sowie `python3 scripts/demo.py`.

## Unabhängiges Review

Keine konkreten Fehler oder blockierenden Testlücken. Vier fokussierte Testgruppen bestanden. Alle acht CLI-Laufarten wurden zusätzlich unabhängig geprüft: Default/None/Replay ohne Budget blieben byteidentisch zur Baseline, Nullbudget scheiterte in REPLAYING und das UInt64-Maximalbudget lieferte den V2-Bericht. Budgetübernahme, Exportreihenfolge und Erhalt der verschachtelten Originalexception wurden geprüft.

## Grenzen

Die neue Fehlerprüfung liegt in REPLAYING vor Aufruf des eigentlichen Replay. Sie ersetzt keine gezielte Fehlerinjektion innerhalb der nativen Replayalgorithmen. Kein Rechenzeit-/Speicher-/Gesamtkostenbudget, keine unterbrechbare Verifikation, kein vollständiger Meilensteinabschluss.

[Vertrag](../106-replay-byte-budget-contract.md) · [Benutzung](../107-replay-byte-budget-usage.md)
