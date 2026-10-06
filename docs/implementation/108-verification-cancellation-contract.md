# Abbruch an Prüfgrenzen – Vertrag V1

Core 0.52 ergänzt eine optionale Abbruchprüfung für die gemeinsame Prüfsitzung. Das Ziel ist, auf eine Abbruchanforderung zwischen atomaren Arbeitsabschnitten zu reagieren und einen Export nach erkanntem Abbruch zu verhindern. Native Suchabbruchregeln und Replayalgorithmen bleiben erhalten.

## Bibliotheksvertrag

Der vierte Parameter von `execute_checked_application` ist eine als Wert übernommene `VerificationControl` mit optionalem `cancelled`-Callback. Er ist unabhängig von `ExecutionControl`, dessen Callback weiterhin ausschließlich an native Suchcheckpoints weitergereicht wird.

Ein vorhandener Prüfcallback wird an genau drei Stellen aufgerufen, sofern keine vorherige Ablehnung erfolgt:

1. Nach Eintritt in SERIALIZING, vor `pack_application`.
2. Nach Eintritt in REPLAYING, vor Bytebudgetprüfung und `replay_application`.
3. Nach erfolgreichem Replay und Ergebnisvergleich, vor READY.

Ein wahres Ergebnis löst `VerificationCancelled` aus. Die bestehende `ExecutionSessionError` enthält die betroffene Arbeitsphase und den terminalen Verlauf bis FAILED. Die Sitzung liefert weder ein Ergebnis noch ein Bundle an ihren Aufrufer zurück; die CLI beginnt keinen Export. Bereits bestehende Zieldateien bleiben erhalten. Eine Exception aus dem Prüfcallback wird wie andere Arbeitsphasenfehler einschließlich ihrer ursprünglichen Ursache erhalten.

Ein erkannter Abbruch am zweiten Prüfpunkt hat Vorrang vor einer dort ebenfalls möglichen Bytebudgetüberschreitung. Fehler aus einer atomaren Operation, beispielsweise ein fehlgeschlagener Replayvergleich, werden vor einem erst danach prüfbaren Abbruch gemeldet. Der Callback läuft nicht innerhalb des Replay und verändert keine gespeicherte native Abbruchposition.

## CLI und Berichte

`--verification-cancellation phase-boundaries` schaltet die Prüfgrenzen ein und setzt `--verification replay` voraus. SIGINT und SIGTERM setzen eine Abbruchanforderung; der Signalhandler selbst führt keine Fehlerbehandlung oder Ausgabe aus. Die Option gilt für alle acht Laufarten, einschließlich des atomaren Einzel-Grid.

Ohne Option oder mit `--verification-cancellation none` bleibt das bisherige Verhalten erhalten. `none` ist auch ohne Replay zulässig. Unbekannte Werte werden zurückgewiesen.

Erfolgreiche Sitzungen mit Prüfcallback verwenden `application-execution-v3` und ergänzen `execution.verification_cancellation: "phase-boundaries"`. Alle bisherigen Erfolgsfelder bleiben erhalten, ebenso ein gegebenenfalls gesetztes Replay-Bytebudget. Ohne Prüfcallback bleiben V1 bzw. V2 unverändert. Fehler verwenden weiterhin `application-execution-failure-v1`, Exitcode 2 und hier die Ursache `VerificationCancelled`.

FAILED beschreibt in diesem Fall die nicht freigegebene Prüfsitzung, keine mathematische Widerlegung. Ein nativer INCOMPLETE-Befund ist davon unabhängig: Wenn nur die native Suche abgebrochen wird und der Prüfcallback false liefert, kann dieser Teilbefund vollständig geprüft werden. In der CLI teilen beide Kontrollen die Signalanforderung; mit aktivierten Prüfgrenzen führt ein Suchabbruch durch Signal deshalb anschließend zum Abbruch der Sitzung statt zum Export eines geprüften Teilbefunds.

## Grenzen

Serialisierung, Replay und Ergebnisvergleich bleiben innerhalb ihrer Ausführung atomar. Eine währenddessen gesetzte Anforderung wird erst am nächsten Prüfpunkt erkannt. Es gibt keine zugesicherte maximale Abbruchlatenz. Signale nach dem letzten Prüfpunkt, Ausgabeformatierung und Dateischreiben liegen außerhalb dieses Vertrags; dies ist keine abbrechbare Exporttransaktion. Eingabedateien werden vor der Sitzung eingelesen.

Der Bericht beschreibt die aktuelle Sitzung. Die Einstellung wird nicht im nativen Bundle persistiert. Keine Wiederaufnahme einer abgebrochenen Prüfung, kein allgemeiner Phasengraph und kein gemeinsames Operationsbudget. Die Lieferung schließt keinen gesamten Meilenstein ab.

[Benutzung](109-verification-cancellation-usage.md) · [Abnahme](acceptance/verification-cancellation-v1-2026-09-23.md)
