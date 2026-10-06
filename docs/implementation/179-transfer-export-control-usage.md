# Core 0.98: kontrollierter Transferexport

`export_exclusive` erhält einen optionalen Replaycallback. Er prüft vor Dateierzeugung, vor jedem höchstens 64-KiB-Schreibversuch, vor Synchronisation und nach Schließen unmittelbar vor Veröffentlichung. Alle vier Transfer-CLI-Wege reichen ihr bestehendes Konto weiter. Ohne Callback bleibt das bisherige Schreibverhalten erhalten.

Callbackfehler führen durch den bestehenden Aufräumpfad. Kein neuer Zielname entsteht vor erfolgreichem exklusivem Link; vorhandene Ziele werden nicht überschrieben. Nach Veröffentlichung keine weitere Callbackprüfung. Berichte enthalten den Gesamtverbrauch einschließlich Export. Keine neue Option oder Formatversion.

[CLI-Vertrag und Grenzen](../cli/transfer-control.md). Einzelne Systemaufrufe können blockieren; keine maximale Signallatenz. Aufräumfehler und harte Prozessbeendigung können temporäre Reste hinterlassen. Keine zusätzliche Crash-Dauerhaftigkeitszusage und keine Gesamtfreigabe des Blueprints. Regelaktivierung unverändert.

## Verifikation am 26.09.2026

- Der neue Exporttest scheiterte zunächst an der fehlenden Callback-Schnittstelle; nach Umsetzung bestanden vier gezielte Gruppen (Export, beide Transferfamilien und Regelspeicherworkflow).
- Leere Ausgabe, 1 Byte, exakt 65536 Byte, 65537 Byte und mehrere Blöcke geprüft. Abbruch an jedem gemessenen Kontrollpunkt hinterlässt im Test weder Ziel noch temporäre Datei. Kontrollpunkte nach Veröffentlichung werden ausdrücklich ausgeschlossen. Vorhandene Ziele bleiben unverändert; Kollisionsfehler räumen die temporäre Datei auf.
- Die bestehenden Transfergrenztests bestehen weiter. Zwei zusätzliche CLI-Beispiele stoppen mit einem Punkt zu wenig unmittelbar vor Veröffentlichung und prüfen auch fehlende temporäre Reste.
- Lokal (Debug): 119/119 Testgruppen bestanden (124,64 Sekunden). Installation und Paket erfolgreich geprüft, jeweils 26 historische Fixtures.
- 24 Quell-, Test- und Dokumentationsdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Dort frischer Release-Build, 119/119 Testgruppen bestanden (97,10 Sekunden), Installation und Paket erfolgreich. `./build/bruce --version` bestätigt 0.98.0.
- Lokal und remote: 114 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks.

Keine produktiven Regeländerungen. Die Nachweise bestätigen das abgegrenzte Paket, keinen vollständigen Blueprintabschluss und keine maximale Abbruchlatenz.
