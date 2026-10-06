# Core 0.99: eingebettete Regelprüfung im Transferkonto

`RuleStage::parse`, `RuleProposal` und `check_current` sowie `CompatibilitySnapshot` und dessen kanonische Darstellung nehmen optionale Replaycallbacks entgegen. Die Historienprüfung reicht ihren Callback durch diese Leseschritte. Direkte Regeln in der Transferplanung/-ausführung werden ebenfalls mit Callback kanonisiert.

Kontrollpunkte liegen an Einstiegen, äußeren Prüfsummen und einzelnen Regelzeilen. Kein Callback wird im Objekt gespeichert. Ohne Callback bleiben Formate und fachliche Ergebnisse gleich. Freigabe, Aktivierung und bestehende Verwaltungsbefehle bleiben unverändert.

[CLI-Vertrag und Grenzen](../cli/transfer-control.md). Quoted-Textdecodierung, einzelne Zeilen, Kopien und äußere Stage-Rekonstruktion bleiben atomar. Die native erneute Snapshotverarbeitung im Z-Lauf wird hier nicht erweitert. Keine maximale Signallatenz, keine allgemeine Rechenkostensteuerung und kein vollständiger Blueprintabschluss.

## Verifikation am 26.09.2026

- Neuer Vorschlagstest scheiterte zunächst an der fehlenden Callback-Schnittstelle; danach fünf gezielte Gruppen erfolgreich (Vorschläge, Historie, Regelspeicher, Transfer und Z-Verträge).
- Vorschlag mit 128 Regeln: identische Bytes und Regeln; Abbrüche am Anfang, innerhalb und am Ende weitergereicht. Basisprüfung und Ablehnung beschädigter Prüfsummen bleiben wirksam.
- Regelparser und kanonische Ausgabe besitzen jeweils 129 Punkte bei 128 Regeln. Jeder einzelne Punkt wurde mit einem ausgelösten Callbackfehler geprüft.
- Lokal (Debug): 119/119 Testgruppen bestanden (125,17 Sekunden). Installation und Paket jeweils mit 26 historischen Fixtures erfolgreich geprüft.
- 28 Quell-, Test- und Dokumentationsdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Frischer Release-Build auf Bruce-AI; dort 119/119 Testgruppen bestanden (96,77 Sekunden), Installation und Paket erfolgreich. `./build/bruce --version` bestätigt 0.99.0.
- Lokal und remote: 116 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks. Neue Beispiele führen Planung und Ausführung mit 128 direkten Regeln und Transferkonto aus.

Keine produktiven Regeländerungen. Die Tests bestätigen diesen Umfang, keine maximale Abbruchlatenz und keinen vollständigen Blueprintabschluss.
