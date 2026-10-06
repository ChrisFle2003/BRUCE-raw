# Core 0.96: Dateilesen im Transferkonto

`read_bounded` erhält einen optionalen Replaycallback. Kontrollpunkte liegen vor dem Dateiöffnen, nach jedem nichtleeren Leseabschnitt von höchstens 8192 Byte und nach Ende der Leseschleife. Der Callback kann die Verarbeitung vor Größenprüfung und Anfügen stoppen. Ohne Callback bleibt das bestehende Verhalten erhalten.

Beide Transferfamilien reichen das vorhandene Konto beim Planen und Ausführen an direkte Dateiimporte weiter: Quelle, Experience-Basis, Übergabeplan und direkte Regeln. Der Z-Transfer installiert seine Signalsteuerung jetzt vor dem Laden der Regeln. Das innere Laden eines aktiven Regelspeichers bleibt unverändert.

[CLI-Vertrag und Zählung](../cli/transfer-control.md). Keine neue Option oder Formatversion, aber höhere Prüfpunktsummen als in Core 0.95. Bytegrenzen gelten weiter. Betriebssystemaufrufe können blockieren; keine maximale Signallatenz oder universellen Zeit-/RAM-Grenzen. Export bleibt ohne innere Transferkontrollpunkte. Keine produktiven Regeländerungen und kein vollständiger Blueprintabschluss.

## Verifikation am 26.09.2026

- Der neue API-Test scheiterte vor der Implementierung an der fehlenden Callback-Schnittstelle; nach Umsetzung bestanden alle drei gezielten Gruppen (`input_read_control` und beide Transferworkflows).
- Leseprüfung: leere Datei, 1 Byte, exakt 8192 Byte, 8193 Byte und mehrere Blöcke; Abbruch an jedem gemessenen Punkt, unveränderte Bytes und weiterhin wirksame Bytegrenzen. Prüfung vor Öffnen einer fehlenden Datei.
- Alle vier Transferbefehle stoppen mit Budget 0 vor Dateizugriff. Bestehende Tests für exakte Budgets, fehlenden Export bei Abbruch und zugestelltes SIGTERM bestehen weiterhin.
- Lokal (Debug): 118/118 CTest-Gruppen bestanden (124,91 Sekunden). Installation und Paketprüfung erfolgreich, jeweils 26 historische Fixtures.
- 26 Quell-, Test- und Dokumentationsdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Dort frischer Release-Build und 118/118 CTest-Gruppen bestanden (96,95 Sekunden); Installation und Paketprüfung erfolgreich. `./build/bruce --version` bestätigt 0.96.0.
- Lokal und remote: 110 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks.

Dies bestätigt das abgegrenzte Paket. Die Tests messen keine maximale Abbruchlatenz. Produktive Regeln wurden nicht verändert.
