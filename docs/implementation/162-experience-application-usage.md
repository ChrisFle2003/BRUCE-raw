# Core 0.82: Anwendungseinstieg für Erfahrungsaktionen

## Schnittstelle

`experience_task.hpp` enthält `ExperienceAddTask { previous, source }`, `ExperienceRevokeTask { previous, target, reason }` und `ExperienceReplaceTask { previous, target, reason, source }`. `ExperienceTask` ist ihre Variante. Alle Strings besitzen ihre Inhalte.

`execute_experience(task)` liefert `ExperienceResult { snapshot, report }`. Die Anwendungsschicht verwendet die vorhandenen Storefunktionen und prüft/rendert den neuen Snapshot wie bisher. Der Einstieg liest und schreibt keine Dateien und akzeptiert keine vorab ausgestellte Gatefreigabe als Ersatz für die Quellenprüfung.

Die CLI-Befehle `experience add`, `revoke` und `replace` erzeugen nun diesen Auftrag. Dateilesen und exklusiver Export bleiben in der CLI. Optionen, Snapshotformate und Berichtschema bleiben erhalten; vorhandene Zielpfade werden weiterhin nicht überschrieben.

## Regelhoheit und Grenzen

Dies sind explizite Änderungen an Erfahrungssnapshots, keine aktiven Regeländerungen. Der Adapter besitzt weder Regelargumente noch eine Aktivierungsfunktion. Die vorhandene Routing-Semantik bleibt unverändert. [ADR-013](../adr/013-human-rule-authority.md) gilt weiterhin: Jede aktive Regeländerung erfordert konkrete menschliche Freigabe. Der Adapter implementiert und behauptet keinen geschützten menschlichen Freigabekanal.

Die acht allgemeinen `ApplicationTask`-Varianten bleiben unverändert. Der neue begrenzte Adapter hat noch keinen eigenen Bundle-/Sitzungsvertrag und keine zusätzlichen Steuerungsoptionen. Unterliegende Experience-Funktionen bleiben als bestehende API verfügbar; dieser Schritt erzwingt keine Zugriffstrennung gegen beliebige C++-Aufrufer.

## Abnahme

Adaptertests vergleichen Hinzufügen, Widerruf und Ersetzung mit den bisherigen nativen Snapshotbytes und Berichten. Sie vergleichen Ablehnungen für doppelte Quellen, unbekannte IDs, ungültige Korrekturgründe und manipulierte Quellen. Der ursprüngliche Snapshot bleibt unverändert. Bestehende CLI-Workflows prüfen Quellenzulassung, Widerruf, Ersetzung und exklusiven Export.

Damit ist das begrenzte Paket A des Blueprint umgesetzt. Offen bleiben die Abhängigkeitsbereinigung zwischen Experience und Portfolio, typisierte Ergebnisübergaben und die geschützte Regel-Freigabe-/Aktivierungsverwaltung. Keine automatische Erfahrungsaktion und keine allgemeine Lernpromotion wird eingeführt.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 108/108 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.82.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
