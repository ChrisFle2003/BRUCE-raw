# Core 0.97: Regelspeicher im Transferkonto

`active_rule_snapshot` und der lokale Ladeadapter nehmen einen optionalen Replaycallback entgegen. Er wird beim Dateilesen und beim Aufbau von `RuleHistory` weitergereicht. Kontrollpunkte begleiten äußere Digests, Hashkette, einzelne Historieneinträge und kanonischen Wiederaufbau. Der Callback wird nicht im Objekt gespeichert. Ohne Callback bleiben Format und Verhalten erhalten.

Nur `compatibility-plan` und `compatibility-apply` reichen ihren vorhandenen Transfercallback weiter. Regelverwaltung und direkte Kompatibilitätsausführung bleiben auf ihrem bisherigen Weg. Es gibt keinen neuen Aktivierungspfad: produktive Regeln werden nicht verändert.

[CLI-Vertrag](../cli/transfer-control.md). Einzelne Regelvorschläge, Pfadauflösung, blockierende Dateiaufrufe und Kopien bleiben atomar; keine maximale Signallatenz. Export besitzt weiterhin keine inneren Transferkontrollpunkte. Keine universellen Zeit-/RAM-Limits und keine Gesamtfreigabe des Blueprints.

## Verifikation am 26.09.2026

- Vor Umsetzung scheiterte der neue API-Test an der fehlenden Callback-Schnittstelle. Danach bestanden drei gezielte Gruppen: Historienvertrag, Regelspeicherworkflow und Z-Transferworkflow.
- Eine Historie mit 128 Entscheidungen wurde mit und ohne Callback identisch wiedergegeben. Callbackfehler am Anfang, in der Mitte und am Ende werden unverändert weitergereicht; beschädigte Historien bleiben abgelehnt.
- Beide Z-Transferwege mit `--rule-store`: identische Berichte und Ausgabebytes, exakt ausreichendes Budget, ein Punkt zu wenig und frühe Budgetabbrüche geprüft. Bei Abbruch keine Ausgabe; Regelspeicherbytes unverändert. Bestehende Freigabe-, Sperr- und veraltete-Basis-Tests bestehen weiter.
- Lokal (Debug): 118/118 Testgruppen bestanden (124,92 Sekunden). Installation und Paket erfolgreich, jeweils 26 historische Fixtures.
- 26 Quell-, Test- und Dokumentationsdateien per SCP nach `/home/chris/Desktop/Bruce_raw` übertragen. Frischer Release-Build auf Bruce-AI; dort 118/118 Testgruppen bestanden (97,28 Sekunden), Installation und Paket erfolgreich. `./build/bruce --version` bestätigt 0.97.0.
- Lokal und remote: 112 CLI-Beispiele, 69 dokumentierte Optionen, 370 Diagnoseeinträge und gültige Handbuchlinks.

Die Tests bestätigen dieses Paket, keine Gesamtfreigabe und keine maximale Abbruchlatenz. Regelaktivierungen in Tests betreffen ausschließlich temporäre Testdateien; produktive Regeln wurden nicht geändert.
