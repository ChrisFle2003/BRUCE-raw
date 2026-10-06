# Core 0.88: Experience-Inspektion über den Anwendungseinstieg

`ExperienceInspectTask { snapshot }` ergänzt die bestehenden drei Erfahrungsaufträge. `execute_experience` prüft den übergebenen Snapshot mit dem vorhandenen Storeleser einschließlich der eingebetteten Quellaudits. Das Ergebnis enthält unveränderte Snapshotbytes und den bisherigen Bericht.

`experience inspect SNAPSHOT` verwendet jetzt denselben Anwendungseinstieg wie Hinzufügen, Widerrufen und Ersetzen. Die CLI liest weiterhin die Datei mit der bestehenden 1-MiB-Grenze und gibt nur den Bericht aus. Inspektion führt keinen Export aus. Dateiformate, Reportschema, Optionen und Fehlerbehandlung bleiben erhalten.

Die neue Variante wird an die vorhandene ExperienceTask-Variantenliste angehängt; die acht allgemeinen ApplicationTask-Laufarten bleiben unverändert. Externe C++-Besucher von ExperienceTask müssen die zusätzliche Variante behandeln. Es wird kein allgemeiner neuer Sitzungs- oder Bundlevertrag eingeführt.

Der Auftrag besitzt seine Eingabebytes; das Ergebnis ist kein Freigabenachweis und keine Regelaktivierung. Die unveränderte Quellenprüfung wird nicht durch vorab erstellte Reports ersetzt. Die menschliche Regelhoheit nach ADR-013 bleibt verbindlich.

Tests vergleichen Inspektionsbytes und Bericht mit dem bisherigen direkten Storeaufruf und prüfen die Ablehnung manipulierter Snapshots. Bestehende Experience-CLI-, Quellen-, Korrektur- und Replaytests bleiben maßgeblich.

Damit laufen alle vier Experience-CLI-Aktionen über die Anwendungsschicht. Geschützte menschliche Freigaben, allgemeine Ergebnisübergaben und vollständige Bibliotheksentkopplung bleiben offen.

## Prüfnachweis am 25.09.2026

Lokal und remote bestanden 111/111 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI wurde als 0.88.0 gebaut und geprüft. Installation und TGZ-Paketprüfung bestanden; die CLI-Prüfung bestand lokal und remote mit 92 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
