# Core 0.102: gemeinsamer lokaler Dateiadapter

Die tatsächliche Dateiarbeit liegt jetzt in [LocalFiles](../../include/bruce/io/local_files.hpp) mit [Implementierung](../../src/io/local_files.cpp). Der Evidenzbaustein behält SHA-256 und kompatible delegierende Lese-/Exportfunktionen. Die fünf Transferbefehle verwenden den Adapter direkt; seit Core 0.102 zählt dazu kontrolliertes direktes `experience add`.

Regelspeicherdateien, Sperren und Commit wurden aus dem CLI-Header verlagert. Die CLI behält menschlichen Dialog und Vorprüfung. Der Adapter erhält die bestätigte kanonische Zielbindung und übernimmt Sperre, erneute Prüfung, gemeinsamen Dateiersatz sowie abschließende Historienprüfung. Keine öffentliche generische Überschreibfunktion und keine neue Regelaktivierung ohne den bestehenden CLI-Dialog.

[ADR-014](../adr/014-local-file-adapter.md) · [Dateivertrag](../cli/local-files.md).

## Abnahmeumfang

Neue Adaptertests prüfen begrenztes Lesen, unverändertes bestehendes Ziel, Callbackabbruch, fehlendes Zielverzeichnis, falsche Bestätigung, erfolgreiche temporäre Testaktivierung und veraltete Basis. Ein isolierter Kindprozess mit Dateigrößengrenze und ignoriertem SIGXFSZ erzwingt echte Schreibfehler für Export und Regelspeicherwechsel; nach dem Fehler werden Zielerhalt und fehlende temporäre Reste geprüft.

Bestehende Export-/Lesekontrolltests und Terminal-/Sperrtests laufen unverändert weiter. Der durchgängige Prüfer dient zusätzlich zum Vergleich fachlicher Artefakte und Prüfpunktverbräuche mit Core 0.101. Es werden ausschließlich temporäre Regeln in Tests aktiviert.

## Grenzen

Keine Änderung mathematischer Strategien oder Controllersemantik. Regelaktivierung bleibt ohne neue Abbruchoption. Fehler nach erfolgreichem Dateiersatz können bereits sichtbaren Zustand betreffen; speziell `RuleStoreCommitDurabilityUnknown` ist keine Rollbackzusage. Aufräumfehler, Prozessabsturz und blockierende Systemaufrufe bleiben bestehende Grenzen. Kein vollständiger Blueprintabschluss.

## Verifikation am 26.09.2026

- Der neue Adaptertest scheiterte zunächst am noch fehlenden Adapterheader. Nach Umsetzung bestanden acht gezielte Testgruppen einschließlich Schreibfehlern, Export-/Lesekontrollstellen, Regeldialog/Sperre und durchgängigem Ablauf.
- Lokal (Debug): 121/121 CTest-Gruppen bestanden (125,97 Sekunden). Installation und Paket erfolgreich, jeweils 26 historische Fixtures.
- Vergleich mit Core 0.101: sieben fachliche Artefakte bytegleich; alle 28 Antworten außerhalb der Versionsabfrage gleich, einschließlich der gemessenen Prüfpunktkonten. Der Vergleich gilt für den benannten 29-Aufruf-Abnahmeablauf.
- 33 Quell-, Test-, Dokumentations- und Nachweisdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen; neue Adapterunterordner im Projekt angelegt. Dort frischer Release-Build und Version 0.102.0 bestätigt.
- Bruce-AI: 121/121 CTest-Gruppen bestanden (97,04 Sekunden), Installation und Paketprüfung erfolgreich.
- Lokal und remote: 118 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks. Zusätzlich 57 Verweise der betroffenen Architektur-/Vertragsdokumente geprüft.
- Durchgängige Abnahme lokal und remote: jeweils 29 Aufrufe erfolgreich. Zwischen beiden Rechnern sieben fachliche Artefakte und sämtliche 29 JSON-Antworten identisch; Binärdigests dürfen wegen Debug-/Release-Build abweichen.

Berichte: [lokaler Lauf](acceptance/control-flow-core0102-local.json), [Remote-Lauf](acceptance/control-flow-core0102-remote.json). Produktionsregeln wurden nicht geändert; Aktivierungstests betreffen ausschließlich temporäre Testdateien. Diese Nachweise sind keine Gesamtfreigabe des Blueprints.
