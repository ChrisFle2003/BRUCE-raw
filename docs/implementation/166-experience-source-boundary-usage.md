# Core 0.86: eigener Erfahrungsquellenaudit

`verify_experience_source(bundle, checkpoint)` liegt in `application/experience_source` und liefert nach der bestehenden Quellenprüfung einen `ExperienceRecord`. Der bisherige interne Audit wurde ausgelagert; Format-, Größen-, Arbeits-, Holdout- und Unabhängigkeitsprüfungen bleiben erhalten. Grid-Einbettungen werden weiterhin über denselben internen Pfad geprüft.

Der Store ruft diesen Einstieg beim Lesen und Anwenden von Ereignissen auf. Er akzeptiert keine extern vorgeprüften Records als Ersatz für die Bundleprüfung. Ereignisverwaltung, Deduplizierung, Widerruf, Ersetzung und Serialisierung bleiben im Store. Der bisherige lesende Adapter `audit_experience_source` bleibt kompatibel und delegiert an den neuen Einstieg.

Callbackexceptions werden unverändert weitergereicht. Ein zurückgegebener Record ist weder ein menschlicher Freigabenachweis noch eine Zulassung für einen konkreten Zielspeicher. Der neue Einstieg aktiviert keine Regeln; ADR-013 bleibt unverändert.

Die Storeimplementierung hängt jetzt vom expliziten Quellenaudit-Einstieg ab, statt dessen Forschungs-/Gridlogik selbst zu enthalten. Sie ist dadurch noch keine eigenständig von Application unabhängige Bibliothek. Der bestehende JSON-Helfer bleibt ebenfalls eine Restabhängigkeit.

Tests vergleichen Quellenfingerprint, Trainingsfingerprint, Kontext und Stimmen des direkten Audits mit dem echten Storeimport und prüfen Callbackweitergabe. Die bestehenden Snapshot-, Grid-, Erfahrungs-, Replay- und Abbruchtests sichern die bisherigen Formate und Fehlerpfade.

CLI-Verhalten, Regelentscheidungen, Snapshotbytes und Routingsemantik bleiben erhalten. Eine geschützte menschliche Regel-Freigabeverwaltung bleibt offen.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 110/110 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.86.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
