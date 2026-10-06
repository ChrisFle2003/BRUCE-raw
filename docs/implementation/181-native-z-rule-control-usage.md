# Core 0.100: native Z-Regelsteuerung

`run_compatibility` besitzt einen optionalen separaten Regelcallback. `evaluate_compatibility` ruft ihn vor jeder zulässigen Regelprüfung nach Prüfung der nativen Check-/Tracebudgets auf. Regelsnapshot, kanonische Darstellung und abschließender Digest sind ebenfalls angebunden. Beide Transferwege reichen ihren geschützten Callback für Quellen- und Regelpfad weiter.

Quellencheckpoint und Regelcallback bleiben getrennte API-Parameter. Bestehende direkte Aufrufe und Replay erhalten standardmäßig keinen Regelcallback. Ohne Callback bleiben native Formate und Ergebnisse unverändert. Fachliche Budgeterschöpfung bleibt `INCOMPLETE`; Callbackfehler brechen den Transfer ab. Freigabe und Regelaktivierung unverändert.

[CLI-Vertrag](../cli/transfer-control.md). Einzelprädikate, Kandidatenvorbereitung und Auswahlzusammenführung bleiben atomare Abschnitte. Keine maximale Signallatenz, keine universelle Rechenkostensteuerung und kein 1.0-/Blueprintabschluss.

## Verifikation am 26.09.2026

- Neue native Callbacktests scheiterten vor Umsetzung an der fehlenden Schnittstelle. Danach vier gezielte Gruppen erfolgreich (Z-Verträge, Transfer, Regelspeicher und Replaysteuerung).
- Zwei Kandidaten mit jeweils 128 Regeln: 256 tatsächlich begonnene Regelprüfungen und Abbruch an jeder einzelnen Stelle nachgewiesen. Native Gesamtverarbeitung zusätzlich am Anfang, innerhalb und am Ende unterbrochen; Fehler unverändert weitergereicht.
- Kontrollierte native Bundles bytegleich zum bisherigen Pfad. Fachliches Checkbudget 1 erzeugt weiterhin `INCOMPLETE` mit `OperationBudgetExceeded` und genau einem Regelcallback.
- Lokal (Debug): 119/119 Testgruppen bestanden (125,23 Sekunden). Installation und Paket erfolgreich, jeweils 26 historische Fixtures.
- 26 Quell-, Test- und Dokumentationsdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Frischer Release-Build auf Bruce-AI; 119/119 Testgruppen bestanden (97,06 Sekunden), Installation und Paket erfolgreich. `./build/bruce --version` bestätigt 0.100.0.
- Lokal und remote: 118 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks. Neue Beispiele prüfen fachliche Budgeterschöpfung bei aktivem Transferkonto.

Keine produktiven Regeländerungen. Die Nachweise gelten für dieses Paket, nicht für einen vollständigen Blueprintabschluss oder eine garantierte Abbruchlatenz.
