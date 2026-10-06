# Core 0.91: aktive numerische Regeln und Entscheidungsjournal

`rules-stage`, `rules-activate` und `rules-status` verwalten einen expliziten lokalen Speicher. Zustimmung bindet Ziel, Ausgangsgeneration, Journalhash und beide Regelfassungen. Auch Initialisierung und Rückwechsel benötigen eine konkrete Terminalbestätigung.

Die Anwendungsschicht `RuleHistory` prüft Historien und Basisbindung; der CLI-Dateiadapter übernimmt Terminal, Schreibsperre und atomaren Ersatz. Alle bestätigten Fassungen und lokale Entscheidungszeiten bleiben gemeinsam in der Speicherdatei erhalten. Ablehnungen werden nicht protokolliert. Unterbrechungen vor oder nach dem atomaren Ersatz können nicht zu getrennt aktualisierten aktiven Regeln und Journal führen.

`compatibility --rule-store` liest die aktive Fassung einmal und übernimmt sie in den bestehenden Auftrags-/Bundlevertrag. Replay bleibt unabhängig von späteren Änderungen. Die Ausgabe bindet den Regeldigest; Journalpfad und Generation werden nicht in das mathematische Bundle aufgenommen.

Die Tests prüfen Initialisierung, explizite Ablehnung, fehlendes Terminal, konkurrierende Dialoge, Schreibsperre, Rückwechsel und veraltete Zustimmung, beschädigte Dateien, Grenzwerte, Aufgabenübernahme und unverändertes Replay nach einem späteren Wechsel. Es werden nur temporäre Testregeln aktiviert.

Vollständige CLI-/Format-/Fehlerreferenz: [Aktive Regeln](../cli/active-rules.md).

## Abgrenzung

Der lokale Vertrauensrahmen bleibt: Programme desselben Benutzers können Dateien direkt ändern und Bestätigung nachbilden. Die Verwaltung umfasst numerische Kompatibilitätssnapshots. Direkte Regeldateiaufrufe bleiben verfügbar, es gibt keinen globalen Standardspeicher oder vollständigen Schutz sämtlicher Regelarten. Historienmigration, Rotation, dauerhafte Erfassung von Ablehnungen, Journalprovenienz im Laufbundle und allgemeine Ergebnisübergaben bleiben offen. Kein vollständiger Architekturabschluss.

## Prüfnachweis am 25.09.2026

Lokal bestanden 115/115 Testgruppen; nach der abschließenden Hilfetext-Ergänzung bestanden die fünf gezielten Regel-/Versionstests erneut. Auf Bruce-AI bestanden 115/115 Testgruppen mit dem Nutzerpfad `./build/bruce` als Core 0.91.0. Installation und TGZ-Paketprüfung bestanden lokal und remote. Die CLI-Referenzprüfung bestand auf beiden Hosts mit 99 Beispielen, 65 dokumentierten Optionen, 350 Diagnoseeinträgen und gültigen Dateilinks. Die Änderungen wurden per SCP übertragen. Die Prüfungen aktivierten ausschließlich temporäre Testregeln.
