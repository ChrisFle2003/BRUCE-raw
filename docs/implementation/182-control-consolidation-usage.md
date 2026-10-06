# Core 0.101: Konsolidierung und durchgängige Abnahme

Dieses Paket ändert keine mathematischen Algorithmen, bestehenden Controller oder Regelaktivierung. Es ergänzt einen ausführbaren Abnahmeprüfer und konsolidiert die bisher verstreuten Steuerungsverträge.

- [Bestandsvertrag mit Abdeckungsmatrix, Fehlern und Prioritäten](../contracts/06-control-and-publication.md)
- [CLI-Einstieg mit Ablauf und Prüfanleitung](../cli/control-overview.md)
- [Prüfer](../../scripts/verify_control_flow.py), als `control_flow_acceptance` in CTest registriert.

## Umfang

Ein unabhängiger Portfolio-Forschungsbefund mit Holdout wird in einer Session geprüft, erneut replayt und auf Gateeignung geprüft. Derselbe Befund dient separat als Quelle für Z-Auswahl und Erfahrungsimport. Ein Z-Auswahlbundle wird ausdrücklich als Erfahrungsquelle abgelehnt. Dieser Befund korrigiert die zuvor vorgeschlagene lineare Kette: eine solche direkte Importkante ist nicht implementiert.

29 CLI-Aufrufe prüfen die beiden Zweige samt Quellen-/Plan-/Snapshotdigests, exakten Transferbudgets, Abbruch ohne Ziel oder temporären Rest, geänderten Quellen/Regeln/Basen, fachlichem Teilbefund und notwendigem Terminaldialog für Regelaktivierung. Nur temporäre Testdateien werden verwendet. Der Prüfer gibt Erfolg erst nach allen Assertions aus und kann einen neuen maschinenlesbaren Bericht schreiben.

## Bestehende Unterschiede

Transfer- und Sessioncontroller bleiben getrennt. Ihre Zählung, Phasen, Signalprioritäten und Fehlerhüllen sind nicht durchgängig gleich. Die Matrix dokumentiert diese Unterschiede explizit; sie behauptet keine neue gemeinsame Runtime. Verwaltungsbefehle und Teile der Dateipersistenz bleiben ohne gemeinsames Konto.

Die bestehenden Core-0.93-Abnahmen bleiben historische Nachweise. Dieses Profil ist keine Gesamtfreigabe des Blueprints. Optimierungen, neue mathematische Familien und neue Regelaktivierungen sind nicht Bestandteil dieses Pakets.

## Abnahme am 26.09.2026

- Neuer durchgängiger Prüfer: lokal und auf Bruce-AI jeweils 29/29 CLI-Aufrufe mit allen zusätzlichen Prüfbedingungen erfolgreich.
- Lokal (Debug): 120/120 CTest-Gruppen bestanden (124,51 Sekunden). Installation und Paketprüfung erfolgreich, jeweils 26 historische Fixtures.
- 25 Quell-/Dokumentations-/Nachweisdateien ausdrücklich per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Dort frischer Release-Build; `./build/bruce --version` bestätigt 0.101.0.
- Bruce-AI: 120/120 CTest-Gruppen bestanden (97,41 Sekunden), Installation und Paket erfolgreich.
- Lokal und remote: 118 Handbuchbeispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge, gültige CLI-Links. Zusätzlich 56 lokale Verweise der fünf neuen/konsolidierten Kerndokumente geprüft.
- Sieben fachliche Artefakte sind anhand SHA-256 zwischen lokalem und Remote-Lauf identisch: Forschungsquelle, Z-Plan/-Bundle, Erfahrungsplan/-Snapshot sowie eingeschränkter Z-Plan und dessen Teilbefundbundle.

Maschinenlesbare Nachweise: [lokaler Lauf](acceptance/control-flow-core0101-local.json), [Remote-Lauf](acceptance/control-flow-core0101-remote.json). Die Binärdigests dürfen zwischen Debug- und Release-Build abweichen. Der temporäre Regelvorschlag enthält eine konkrete Pfadbindung; dessen Digest ist daher nicht Teil des ortsunabhängigen Artefaktvergleichs.

**Ergebnis:** Das beschriebene verzweigte Steuerungsprofil ist abgenommen. Historische Core-0.93-Nachweise werden nicht überschrieben. Keine produktiven Regeländerungen, kein neuer Regelaktivierungspfad und keine Gesamtfreigabe des Blueprints.
