# Core 0.90: Regelvorschläge in der Anwendungsschicht

Die bisher im CLI-Adapter enthaltene Paketprüfung liegt jetzt in `application/rule_proposal`. Die Klasse `RuleProposal` hält geprüfte Vorschlagsbytes und beide kanonischen Regelfassungen. Sie benötigt weder Terminal noch Dateizugriff.

## Schnittstelle

- `RuleProposal::create(previous, proposed)` prüft und kanonisiert beide Snapshottexte und erstellt das bestehende Paketformat.
- `RuleProposal(bytes)` prüft ein eingelesenes Paket einschließlich Größenlimit, Digest, Version und eingebetteter Snapshots.
- `bytes()`, `previous()`, `proposed()` und `digest()` liefern schreibgeschützte Ansichten auf den gehaltenen Inhalt.
- `check_current(current)` vergleicht einen Snapshottext mit der kanonischen Ausgangsfassung und meldet bei Abweichung `RuleBaseChanged`.
- `confirmation(output_path)` berechnet die bisherige Bindung an Paketbytes und den wörtlichen Ausgabepfad. Der Name bezeichnet den Bestätigungsdigest, keine erteilte Zustimmung.

Der Anwendungseinstieg besitzt keine Freigabe-, Aktivierungs-, Journal- oder Exportaktion. Auch ein erfolgreich konstruierter Vorschlag ist kein Berechtigungstoken. Dateilesen, Terminalprüfung, verständliche Anzeige, exakte Eingabeprüfung, erneutes Einlesen der Basis und exklusiver Export bleiben im CLI-Adapter.

## Kompatibilität und Prüfung

Die Befehle `rules-propose` und `rules-approve`, Paketversion, Digestbildung, Dialog und Ausgabefelder bleiben unverändert. Der neue direkte Anwendungstest prüft das exakte Paketformat, kanonisch gleichwertige Basis, abweichende Basis, Pfadbindung, beschädigte und identische Pakete sowie die Paketgrößengrenze. Die vorhandenen CLI-Tests prüfen weiterhin den Terminalablauf und Überschreibschutz.

Dies bereitet eine gemeinsame Verwendung durch spätere Bedienoberflächen vor. Ein Cockpit ist dadurch noch nicht implementiert. Aktive Versionsverwaltung, transaktionale Basisprüfung und dauerhaftes Entscheidungsjournal bleiben offen. Die lokale Vertrauensgrenze aus dem [Freigabevertrag](../contracts/05-human-rule-approval.md) bleibt bestehen.
