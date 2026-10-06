# Core 0.47 – Gemeinsamer typisierter Auftragseinstieg

## Freigegebener Konsolidierungsschritt

Die bestehenden Laufarten erhalten einen gemeinsamen C++-Einstieg, den die CLI tatsächlich verwendet. Fachliche Parameter bleiben in ihren bestehenden Typen. Keine neue Mathematik, keine Änderung der Wireformate und keine allgemeine Phasensteuerung.

## Umsetzung und Abnahme

1. `application_task.hpp/.cpp`: `ApplicationTask` als geschlossene Variante aus Int64-Forschung, exakter Forschung, Grid, Grid-Stapel, Grid-Fortsetzung, Grid-Forschung, Bruchvergleich und Kompatibilität. Kleine Requesttypen halten die bisherigen zusammengesetzten Gridargumente.
2. `ApplicationRun` als entsprechende Ergebnisvariante, gemeinsame Ausführung, Darstellung und Exportserialisierung. Jeder Adapter delegiert genau einmal an den vorhandenen Executor und Codec. Kein Dateizugriff im gemeinsamen Executor.
3. Abbruchsignal ohne vermeintlich universellen Operationszähler; lokale Checkpoints und gespeicherte Abbruchpositionen bleiben den Fachmodulen überlassen. Einzel-Grid bleibt atomar.
4. Alle acht CLI-Laufpfade nutzen den gemeinsamen Einstieg; Koordinaten-/Taschenrechnerfunktionen und Experience-Snapshotänderungen bleiben eigene Hilfs-/Verwaltungsbefehle. `task-contracts` beschreibt die integrierten Laufarten.
5. Tests vor Umsetzung: typisierte Ausführung gegen direkte Executoraufrufe, identische Ergebnis-/Bundlebytes, Callbackweitergabe und ungültige Eingaben. Anschließend Vergleich gegen Core 0.46, vollständige Debug-/UBSan-Suiten, Demo und Dokumentation.

## Grenzen

Dies ist ein kompilierter geschlossener Vertrag, kein dynamisches Plugin-System und kein generisches JSON-Aufgabenformat. Budgets bleiben fachlich getrennt; eine gemeinsame Variante vereinheitlicht weder deren Einheiten noch die Gate-Zustandssteuerung. Bestehende Replaydecoder werden nicht durch ein neues Format ersetzt. M0–M2 bleiben nur in den konkret geprüften Teilbereichen konsolidiert.
