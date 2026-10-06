# Core 0.95: gemeinsame Steuerung der Transferplanung

`ExperienceTransfer::plan` und `CompatibilityTransfer::plan` nehmen einen optionalen Replaycallback entgegen. Derselbe Callback wird durch Digests und den vollständigen nativen Probelauf geleitet. Beide CLI-Planbefehle verwenden den vorhandenen `TransferControl` mit `--max-transfer-checkpoints` und `--transfer-cancellation`.

Vor dem exklusiven Planexport liegt ein abschließender Kontrollpunkt. Bei erkanntem Abbruch entsteht keine Plandatei. Mit ausreichend Budget bleiben Planbytes und fachlicher Bericht identisch. Die optionale Ausgabehülle zeigt den Verbrauch. Das Konto ist auf einen Aufruf begrenzt und wird nicht im Plan persistiert. Aktive Regeln werden nicht geändert.

[Genauer CLI-Vertrag](../cli/transfer-control.md). Dateilesen, kleine Planserialisierung und Export besitzen weiterhin keine inneren Kontrollpunkte. Keine garantierte Signallatenz, keine universellen Zeit-/RAM-/Operationsbudgets und keine Gesamtfreigabe des Blueprints.

## Verifikation am 26.09.2026

- Vor Implementierung scheiterten beide neuen Planungstests erwartungsgemäß mit `UnknownOption: --max-transfer-checkpoints`.
- Danach: 4/4 gezielte CLI-/API-Testgruppen bestanden, einschließlich real zugestelltem SIGTERM, Kontrollpunkten innerhalb der Planung, Abbruchpriorität und exakten Budgetgrenzen.
- Lokal (Debug): 117/117 Testgruppen bestanden (124,74 Sekunden). Installation und Paket jeweils mit 26 historischen Fixtures erfolgreich geprüft.
- 29 Quell-, Test- und Dokumentationsdateien ausdrücklich per SCP nach Bruce-AI übertragen; dort neu als Release gebaut. `./build/bruce --version` bestätigt 0.95.0.
- Bruce-AI: 117/117 Testgruppen bestanden (97,84 Sekunden), Installations- und Paketprüfung erfolgreich.
- Lokal und remote: 108 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks. Erfolgreiche kontrollierte Planungen erzeugen bytegleiche Pläne; bei erkanntem Budget-/Signalabbruch wird kein neuer Plan exportiert.

Keine produktiven Regeländerungen. Die Nachweise bestätigen dieses abgegrenzte Architekturpaket, keinen vollständigen Blueprintabschluss.
