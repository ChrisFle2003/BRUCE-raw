# Abnahme Forschungsreplay-Checkpoints / Core 0.54 – 23.09.2026

## Lieferung

Optionaler werfender Replaycallback für Int64- und exakte Forschung. Der Observer liefert niemals einen nativen Abbruchwert und erhält gespeicherte Suchgrenzen. Geschützte Weitergabe bewahrt ursprüngliche Callbackexceptions auch gegenüber numerischen Fehlerbehandlungen. CLI-Modus `research-checkpoints`, Erfolgsbericht V5; bisherige Modi und Bundleformate bleiben erhalten.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung am unbekannten Modus. Sechs fokussierte Session-/Adapter-/Replaytestgruppen bestehen. Die neue Matrix prüft 19 gültige Strategie-/Planerkombinationen, jeweils ohne nativen Abbruchcallback sowie mit einer nativen Abbruchanforderung ab dem ersten Kandidaten. Sie prüft identische Bundlebytes, Callbackexceptions vom Typ NumericLimit am Anfang, in der Mitte und am Ende sowie Ablehnung manipulierter Payloads mit neu berechnetem Digest.

Die Testfixtures wurden vor dem erfolgreichen Lauf korrigiert: Der gültige feste Portfolio-Planer heißt `portfolio`; `portfolio-expression` benötigt eine erlaubte Gleichheitseinstellung statt `syntax`. Am Produktcode lag für diese beiden Fehler keine Änderung an.

Ein Sitzungsfall unterbricht innerhalb der Int64-Suche des Replay, ohne den äußeren nativen Callback erneut zu fragen. Fehlende und widersprüchliche Callbackmodi werden abgelehnt. Zusätzliche Tests prüfen Operationslimit vor Auftragszulassung, ablehnende Auftragszulassung und historische Forschungsfixtures mit gesetztem Observer.

Beide Gesamtsuiten bestehen: Debug 89/89 Testgruppen in 64,72 Sekunden, UBSan 89/89 in 116,66 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Nach Ergänzung der historischen Forschungsfixtures und Zulassungsprüfungen wurde die betroffene Testgruppe in beiden Builds zusätzlich erfolgreich geprüft.

Die vollständige Demo, 19 Strategie-/Planervergleiche gegen Core 0.53 mit bytegleichen Exporten, 26 historische Replay-Fixtures und die lokalen Links in 188 Markdown-Dateien bestehen. Von den historischen Fixtures wurden außerdem alle 23 Forschungsbundles mit gesetztem Replaycallback identisch reproduziert.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan`, die zusätzliche Prüfung mit `-R research_replay_control` sowie `python3 scripts/demo.py`.

## Unabhängiges Review

Keine Produktfehler. Vier fokussierte Tests bestanden nach Fixturekorrektur. Der Reviewer prüfte zusätzlich 28 Vergleiche früherer Ausgaben über sieben Forschungsrouten; native/V1–V3-Ausgaben blieben bytegleich und V5 erzeugte identische native Bundles. Leerer Observer, unveränderte Suchgrenzen, Exceptionweitergabe sowie Operationslimit/Zulassung wurden geprüft.

## Grenzen

Deterministische Callbacks prüfen die internen Abbruchpfade. Einzeloperationen und geschachtelte Erfahrungs-Quellaudits bleiben atomar; kein allgemeines Latenz- oder Kostenversprechen. Kein vollständiger Meilensteinabschluss.

[Vertrag](../112-research-replay-checkpoints-contract.md) · [Benutzung](../113-research-replay-checkpoints-usage.md)
