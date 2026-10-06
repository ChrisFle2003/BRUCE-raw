> Historischer Dokumentstand, am 26.09.2026 archiviert. Für aktuelle Aussagen gilt [013-human-rule-authority.md](013-human-rule-authority.md). Überholte Einschränkungen und Entwürfe sind hier bewusst unverändert erhalten.

# ADR-013: Regelhoheit ausschließlich beim Menschen

> Fortschreibung Core 0.89: Auf ausdrücklichen Nutzerwunsch gilt die [lokale Bedienkontrolle](../contracts/05-human-rule-approval.md). Die nachfolgenden älteren Anforderungen an unabhängige Prüfstelle, Kontentrennung und Signierung sind als technische Vorgaben abgelöst. Die ausdrückliche Freigabe einer konkreten Änderung bleibt verbindlich. Implementiert ist ein bestätigter Export; eine globale Aktivierungsverwaltung bleibt offen.

## Entscheidung und Status

Vom Nutzer ausdrücklich vorgegeben: Regeln dürfen nur nach menschlicher Interaktion geändert werden. Konkretisierung: Eine Änderung wird erst durch eine ausdrückliche menschliche Freigabe der konkret vorgelegten Änderung wirksam. Allgemeines Weiterarbeiten, erfolgreiche Tests, Erfahrungsstimmen oder ein positiver Score sind keine Freigabe.

**Verbindliche Architekturvorgabe; vollständige technische Durchsetzung noch offen.** Diese ADR implementiert weder Identitätsprüfung noch eine Aktivierungsverwaltung. Die Kernversion bleibt 0.81.

## Geltungsbereich

Die Vorgabe umfasst neue Regeln, Änderungen und Löschungen sowie Aktivierung, Deaktivierung und Rollback. Dazu gehören Filter, Prädikate, Gewichte, Rangfolgeregeln, Zulassungs-/Promotionsregeln und die Auswahl einer anderen aktiven Regelversion. Auch Änderungen der fest eingebauten Regelsemantik im Code benötigen eine konkret geprüfte Freigabe vor ihrem Einsatz. Der Freigabemechanismus selbst darf nicht durch eine Regeländerung umgangen werden.

Ein Kandidat, ein Befund oder eine Erfahrung ist keine aktive Regel. Vorschläge dürfen erstellt, simuliert und geprüft werden. Eine Simulation muss als solche erkennbar bleiben und darf nicht den aktiven Regelzustand verändern. Historischer Replay verwendet den eingebetteten alten Snapshot ausschließlich zur Wiederherstellung; er aktiviert diesen nicht für neue Aufgaben.

## Geplanter Ablauf

1. **Vorschlag:** Alte Regelbasis, vorgeschlagener vollständiger neuer Snapshot, verständlicher Unterschied, Begründung, Zielgeltungsbereich und Prüfnachweise zusammenstellen.
2. **Menschliche Prüfung:** Genau dieses Paket anzeigen. Der Mensch kann zustimmen oder ablehnen. Schweigen, Chataktivität, ein allgemeines „weiter“ oder eine KI-generierte Zustimmung zählen nicht.
3. **Freigabe:** Zustimmung an Vorschlags-ID, vollständigen Inhalt/Digest, alte Basis, Zielgeltungsbereich und Freigabeentscheidung binden. Prüfnachweise allein sind keine Zustimmung.
4. **Aktivierung:** Freigabe und unveränderte Basis erneut prüfen. Nur exakt freigegebenen Inhalt atomar aktivieren. Geänderter Inhalt, veraltete Basis, fehlende oder zurückgezogene Freigabe führen zur Ablehnung.
5. **Protokoll:** Freigabe und Aktivierung getrennt mit menschlichem Akteur, Begründung, alter/neuer Version und Ereignisbezug erhalten. Ablehnungen und fehlgeschlagene Aktivierungen dokumentieren; keine stillen Überschreibungen.

Ein zurückgezogener Vorschlag darf nicht aktiviert werden. Nachträgliches Bearbeiten eines freigegebenen Vorschlags verlangt eine neue Freigabe. Ein Rollback ist ebenfalls ein konkret zu bestätigender Wechsel und darf keinen alten Freigabenachweis beliebig wiederverwenden. Bei widersprüchlichem Zustand bleibt die bisherige aktive Version erhalten.

## Vertrauensgrenze

`manual`, `approved: true`, ein Name, ein Zeitstempel oder ein Digest beweisen für sich genommen keine menschliche Zustimmung. Die automatische Komponente darf keinen gültigen Freigabenachweis selbst erzeugen können. Vor einer technischen Aktivierungsfunktion muss festgelegt und geprüft werden, woher die menschliche Identität stammt und welcher getrennte Kanal beziehungsweise welche Berechtigung die Zustimmung schützt.

Ein Prozess mit Schreibzugriff auf sämtliche Regel-, Freigabe- und Programmdateien lässt sich durch ein zusätzliches JSON-Feld nicht wirksam einschränken. Die konkrete Trennung von Betriebssystemrechten, interaktiver Freigabe und gegebenenfalls Signaturschlüssel ist eine offene Implementierungsentscheidung. Bis diese Grenze steht, darf keine allgemeine automatische Regelaktivierung angeboten werden.

## Erfahrung und Ranking

Der derzeitige Experience Store speichert geprüfte Stimmen für die Suchreihenfolge und explizite Korrekturen; er aktiviert keine neuen Regeln. Die vorhandene Auswertungsfunktion darf diese Daten nur innerhalb der unveränderten, menschlich festgelegten Semantik verwenden. Erfahrungen dürfen weder neue Prädikate erzeugen noch feste Regelgewichte, Zulassungsbedingungen oder Freigabegrenzen ändern. Ein Vorschlag für eine bessere Regel geht erneut durch die menschliche Prüfung.

## Bestand Core 0.81

- `CompatibilitySnapshot` akzeptiert nur `manual` als Herkunft und bietet keinen Mutator für seine eingelesenen Regeln.
- Die CLI liest den angegebenen Regelsnapshot pro Auftrag; eine geschützte zentrale Verwaltung einer aktiven Version existiert nicht.
- Forschungs- und Experience-Ausgaben enthalten gesperrte Lern-/Publikationsgates; keine automatische Regelpromotion ist implementiert.
- Extern geänderte Regeldateien sind durch diese Eigenschaften nicht als menschlich freigegeben nachgewiesen. Core 0.81 erfüllt noch nicht den vollständigen technischen Freigabevertrag.

## Abnahmekriterien der späteren Implementierung

- Kein aktiver Regelwechsel durch Vorschlag, Simulation, Experience, Tests oder bloßen Import.
- Fehlende, gefälschte, zurückgezogene und für andere Inhalte geltende Freigaben werden abgewiesen.
- Inhalt und Basis werden unmittelbar vor Aktivierung geprüft; konkurrierende Änderungen dürfen keine falsche Freigabe verwenden.
- Neue Regeln, Gewichtsanpassung, Deaktivierung und Rollback benötigen jeweils konkrete Zustimmung.
- Abbruch oder Fehler lässt die alte aktive Version und historische Evidenz erhalten.
- Historischer Replay verändert keine aktive Version.
- Erfolgreiche Freigabe aktiviert exakt den geprüften Inhalt und erzeugt den vollständigen Auditbezug.
- Das System weist nach, dass der automatische Ausführer keine gültige menschliche Freigabe selbst ausstellen kann.

## Versionen und Migration

Bestehende Bundles bleiben unverändert replaybar. Ein künftiges Freigabeformat erhält einen eigenen Versionsvertrag. Alte `manual`-Snapshots werden nicht nachträglich als freigegeben markiert; sie bleiben historische Eingaben oder benötigen eine konkrete initiale Freigabe für einen geschützten neuen Aktivierungspfad. Es werden keine menschlichen Identitäten oder Zustimmungen erfunden.

## Technische Konkretisierung nach Core 0.88

Der [Freigabekanal-Entwurf V1](../contracts/05-human-rule-approval.md) beschreibt getrennte Rollen, Inhalt-/Basis-/Zielbindung, Challengeverwaltung und transaktionale Aktivierung. Auf Bruce-AI wurde das gemeinsame Entwicklerkonto als fehlende Schutztrennung bestätigt. Kein Schlüssel, Konto oder aktiver Regelzustand wurde geändert. Die konkrete Prüfstelle und technische Abnahme bleiben offen.
