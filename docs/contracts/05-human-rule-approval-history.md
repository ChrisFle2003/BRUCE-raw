> Historischer Dokumentstand, am 26.09.2026 archiviert. Für aktuelle Aussagen gilt [05-human-rule-approval.md](05-human-rule-approval.md). Überholte Einschränkungen und Entwürfe sind hier bewusst unverändert erhalten.

# Lokale menschliche Regelprüfung

**Fortschreibung Core 0.91:** Zusätzlich zum unten beschriebenen Einzeldatei-Export ist die [aktive lokale Verwaltung](../cli/active-rules.md) implementiert: Terminalbestätigung, generationengebundene Basisprüfung unter Schreibsperre, atomarer Wechsel mit Journal und explizite Nutzung durch `compatibility --rule-store`. Die nachfolgenden offenen Punkte beschreiben den älteren Exportpfad. Der lokale Vertrauensrahmen und die fehlende globale Durchsetzung gelten weiterhin.

Stand: Core 0.89. Die Nutzerentscheidung für ausschließlich lokalen Betrieb ersetzt den früheren Entwurf mit getrennten Konten, Signierschlüsseln und Aktivierungsdienst. Grundlage bleibt [ADR-013](../adr/013-human-rule-authority.md): Nur eine ausdrückliche Entscheidung über die konkrete Änderung gilt als Freigabe.

## Implementierter Umfang

`rules-propose OLD NEW PROPOSAL` prüft beide Dateien mit dem bestehenden Snapshotparser und speichert beide kanonischen Fassungen mit einer SHA-256-Prüfsumme exklusiv in einer neuen Vorschlagsdatei. Identische Fassungen werden abgelehnt. Ein Vorschlag ist keine Freigabe.

`rules-approve PROPOSAL CURRENT OUTPUT` prüft das Paket und vergleicht CURRENT mit der eingebetteten Ausgangsfassung. Der Dialog benötigt Terminaleingabe und Terminalausgabe. Er zeigt die vollständige alte und neue Fassung mit Minus-/Plus-Präfixen sowie den Ausgabepfad. Bestätigt wird durch Eingabe von `CONFIRM ` und dem vollständig angezeigten Digest. Dieser bindet das Paket und den Ausgabepfad. Andere Eingaben und Dateiende lehnen ab. Unmittelbar vor dem exklusiven Export wird CURRENT erneut geprüft.

OUTPUT enthält danach die vorgeschlagene kanonische Regeldatei. Vorhandene Dateien werden nicht überschrieben. Die alte Fassung bleibt in CURRENT und im Vorschlag erhalten. Die Erfolgsausgabe enthält Vorschlags- und Bestätigungsdigest; ein dauerhaftes Entscheidungsjournal wird noch nicht geschrieben.

## Grenzen

Dies ist ein bestätigter Export, keine globale Aktivierung. Bestehende `compatibility --rules`-Aufrufe lesen weiterhin die ausdrücklich angegebene Datei. Sie verlangen keinen Freigabenachweis. Es gibt keinen globalen aktiven Snapshot und keine atomare Transaktion zwischen CURRENT und OUTPUT. Ein paralleler Schreiber kann CURRENT nach der letzten Prüfung verändern.

Ein Terminal ist keine Identitätsprüfung. Programme unter derselben Benutzerkennung können einen Terminaldialog nachbilden oder Dateien direkt ändern. Genau diese lokale Vertrauensgrenze wird akzeptiert; Kontentrennung und Signierung sind keine Voraussetzung dieses Pakets. Die Prüfsumme erkennt Beschädigungen, ist aber keine Signatur. Parsergültigkeit beweist weder fachliche Richtigkeit noch externe Evidenz.

Noch offen: dauerhaftes Entscheidungsjournal, verwaltete aktive Version samt transaktionaler Basisprüfung und deren ausdrückliche Verwendung durch Aufgaben. Ein allgemeines „weiter“, Erfahrungen, Scores oder Tests aktivieren keine Regeln.
