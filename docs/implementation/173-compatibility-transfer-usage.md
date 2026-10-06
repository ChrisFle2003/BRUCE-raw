# Core 0.93: explizite Übergabe an die Z-Auswahl

`CompatibilityTransfer` bindet die vollständige Forschungsquelle, kanonische Regeln, Int64-Auswertungsstelle, Quellbytegrenze und die bestehenden drei Z-Auswahlbudgets. Die CLI stellt `compatibility-plan` und `compatibility-apply` bereit, wahlweise mit Regeldatei oder aktivem Regelspeicher.

Planung und Ausführung verwenden `run_compatibility` einschließlich Quellenreplay, Eignung und Projektion. Ausführung prüft die gebundenen Digests erneut. Ein Plan ist kein Berechtigungstoken; auch ein neu berechneter Plandigest umgeht keine fachliche Prüfung. Native Teilbefunde bleiben Teilbefunde.

Die Tests prüfen bytegleiche Ergebnisse/Bundles zum direkten Pfad, Replay, C++-Planroundtrip, Größenlimits, festgelegte Parameter, knappe Auswahlbudgets, manipulierte Quellen, kanonische Regelgleichheit, inhaltliche Regeländerung einschließlich aktiver Speicher, Überschreibschutz und Ablehnung ungültiger Quellen trotz neu berechneter Planprüfsumme.

Der native Bundlevertrag bleibt unverändert. Planbindung wird im äußeren Transferbericht ausgegeben; für deren Aufbewahrung sind Plan und Bericht zusätzlich zum Bundle erforderlich. Aktive Journalgenerationen werden nicht gebunden: entscheidend ist der Regelinhalt. Es werden keine Regeln verändert.

[CLI-Referenz mit Beispiel, Format und Fehlern](../cli/compatibility-transfer.md).

Nächster Architekturabschnitt: aktuelle Bestandsübersicht konsolidieren und M0–M2 anhand eines begrenzten, konkret geprüften Profils abnehmen. Allgemeine Übergabeketten, gemeinsame universelle Rechenbudgets, Sprache, NanoCell, Cockpit und Phase-DAG bleiben offen.

## Prüfnachweis am 26.09.2026

Lokal und auf Bruce-AI bestanden 117/117 Testgruppen. Der Nutzerpfad `./build/bruce` auf Bruce-AI meldet Core 0.93.0. Installation und TGZ-Paketprüfung bestanden auf beiden Hosts. Die CLI-Referenzprüfung bestand mit 104 ausgeführten Beispielen, 67 dokumentierten Optionen, 366 Diagnoseeinträgen und gültigen Dateilinks. Quellen, Tests und Dokumentation wurden per SCP übertragen. Regelwechsel zur Prüfung des aktiven Speicherpfads fanden ausschließlich in temporären Testspeichern statt.
