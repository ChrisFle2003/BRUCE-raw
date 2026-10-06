# Abnahme zusammengesetzter Grid-Forschungsreplay / Core 0.55 – 23.09.2026

## Lieferung

Der vorhandene Forschungs-Prüfcallback reicht nun durch GridResearch V1–V4 einschließlich Quellstapeln, Fortsetzungsketten, Projektion, mathematischem Replay und Samplebindung. CLI-Modus und V5-Bericht werden für diesen zusätzlichen Auftragstyp unterstützt. Native Formate, Abbruchpositionen und Budgetregeln bleiben erhalten.

## Prüfungen

Der neue CLI-Test scheiterte vor Umsetzung an der bisherigen Typablehnung. Alle 19 gezielten Grid-/Session-/Replay-/Adaptergruppen bestehen. Die neue Unit unterbricht jeden erreichbaren Callbackpunkt in acht Fällen: X-/Y-Projektion, V3-Budget einschließlich Teilbefund, nativer Forschungsabbruch, fortgesetzte Quelle, V4-Erfahrungsbudget und historische V3-Erfahrungsplanung ohne Experience-Bytebelastung.

Normal zurückkehrende Callbacks müssen identische Bundlebytes liefern. Weitere Prüfungen sichern Samplebindung, Forschungszulassung vor einem manipulierten Quellreplay, Quellenbudget vor Quellenprüfung und das mathematische Operationslimit. Ein Sitzungsfall stoppt innerhalb des Quellenreplay in REPLAYING ohne Freigabe. Die CLI-Integration prüft V5, Gesamtbudget, Replay-Bytebudget und identischen nativen Export.

Beide Gesamtsuiten bestehen: Debug 90/90 Testgruppen in 65,11 Sekunden, UBSan 90/90 in 117,25 Sekunden. UBSan lief mit `-fsanitize=undefined -fno-sanitize-recover=all` ohne Diagnosen. Die vollständige Demo, 19 Strategie-/Planervergleiche gegen Core 0.54 mit bytegleichen Exporten, 26 historische Replay-Fixtures und die lokalen Links in 191 Markdown-Dateien bestehen ebenfalls.

Ausgeführt wurden `cmake --build build -j4`, `ctest --test-dir build --output-on-failure -j4`, die entsprechenden Befehle für `build-ubsan` sowie `python3 scripts/demo.py`.

## Unabhängiges Review

Keine konkreten Fehler. Fünf fokussierte Testgruppen bestanden unabhängig. Der Reviewer prüfte 48 zusätzliche Baseline-Vergleiche über X/Y, Stapel-/Fortsetzungsquellen sowie fehlende, kleine und große Gesamtbudgets: native/V1–V3-Ausgaben blieben bytegleich. Zwölf entsprechende V5-Exporte waren identisch zu den nativen Baseline-Bundles. Callbackweitergabe und Zulassungsreihenfolge wurden geprüft.

## Grenzen

Interne Abbruchpunkte werden mit deterministischen Callbacks geprüft. Keine Zusage maximaler Signallatenz, keine neuen inneren Punkte in Erfahrungs-Quellaudits, Einzeloperationen oder Serialisierung. Kein vollständiger Meilensteinabschluss.

[Vertrag](../114-grid-research-replay-contract.md) · [Benutzung](../115-grid-research-replay-usage.md)
