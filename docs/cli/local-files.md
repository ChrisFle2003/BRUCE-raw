# Lokale Dateien und Veröffentlichung – Core 0.102.0

[Handbuch](README.md) · [Steuerungsübersicht](control-overview.md) · [Architekturentscheidung](../adr/014-local-file-adapter.md)

Die sieben Übergabebefehle verwenden jetzt denselben Dateiadapter wie der lokale Regelspeicher. Direktes `experience add`, `experience revoke` und `experience replace` akzeptieren zusätzlich die bestehenden Transferoptionen; ohne diese Optionen bleiben ihre bisherigen Befehle, Dateiformate und Prüfpunktverhalten erhalten.

| Vorgang | Vertrag |
| --- | --- |
| Eingabe lesen | Bestehende Bytegrenze, bei Transfersteuerung dieselben 8-KiB-Kontrollpunkte |
| Neue Datei exportieren | Privates temporäres Ziel im selben Verzeichnis; exklusiver `link`, kein Überschreiben; dieselben 64-KiB-Schreibpunkte |
| Aktive Regeln lesen | Kanonische Zielbindung, begrenztes Lesen und Historienprüfung; bei Übergaben mit bestehendem Konto |
| Aktive Regeln wechseln | Konkrete menschliche Bestätigung in der CLI, Sperre, erneutes Laden und Prüfen unter Sperre, gemeinsamer Ersatz von Zustand und Journal |

## Fehler und Aufräumen

Bei erkanntem Callbackabbruch vor Veröffentlichung wird die temporäre Datei aufgeräumt. Ein bereits vorhandenes Exportziel bleibt unverändert. Ein Schreibfehler vor Veröffentlichung erzeugt keine neue Zieldatei; beim bestätigten Regelspeicherwechsel vor `rename` bleibt der bisherige Speicher bestehen. Zielverzeichnisse müssen existieren.

Die Testfälle erzwingen echte kurze Schreibvorgänge und anschließendes Schreibversagen über eine Dateigrößengrenze in einem isolierten Kindprozess. Sie prüfen fehlenden Export, Erhalt des bisherigen Regelspeichers und entfernte temporäre Reste. Die Begrenzung betrifft ausschließlich den Testprozess.

Nicht jeder Fehler bedeutet, dass nichts veröffentlicht wurde: Bei einem neuen Export (auch `nanocell run --output`) wird die temporäre Datei zuerst synchronisiert, dann mit `link` exklusiv unter dem Zielnamen veröffentlicht und der temporäre Name entfernt. Schlägt erst danach die Verzeichnissynchronisation fehl, meldet die CLI `ExportCommitDurabilityUnknown` mit Exit 2. Der Zielname wurde dann bereits verlinkt und kann die vollständigen Bytes enthalten; seine dauerhafte Speicherung nach Absturz ist ungewiss. Vor Wiederholung Ziel und Inhalt prüfen, keine Abwesenheit aus Exit 2 folgern. Ein erneuter Versuch mit demselben Namen kann wegen des bestehenden Ziels scheitern; ein anderer Name kann ein Duplikat erzeugen.

Getrennt davon tritt `RuleStoreCommitDurabilityUnknown` nach erfolgreichem `rename` auf, wenn die Verzeichnissynchronisation des aktiven Regelspeichers scheitert. Der neue Zustand kann dann bereits sichtbar sein. Ein hart beendeter Prozess oder ein Aufräumfehler kann temporäre Reste hinterlassen. Die `.lock`-Datei des Regelspeichers bleibt als wiederverwendbarer Sperrpfad erhalten.

## Menschliche Zustimmung und Grenzen

Der Dateiadapter enthält keinen Terminaldialog. Die CLI fordert ihn weiterhin vor dem Aufruf an. Die übergebene Bestätigung bindet Inhalt, Basis und Ziel, authentifiziert aber keinen Menschen gegenüber Programmen desselben Benutzers. Der [lokale Freigabevertrag](../contracts/05-human-rule-approval.md) bleibt unverändert.

Keine maximale Abbruchlatenz, keine neuen Zeit-/RAM-Limits und kein neuer generischer Überschreibbefehl. Die Unterschiede zwischen Session- und Transfersteuerung bleiben im [Bestandsvertrag](../contracts/06-control-and-publication.md) ausgewiesen.
