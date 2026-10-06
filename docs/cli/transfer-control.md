# Gemeinsame Übergabesteuerung – Core 0.102.0

[Handbuch](README.md) · [Steuerungsübersicht](control-overview.md) · [Erfahrung](experience-transfer.md) · [Z-Auswahl](compatibility-transfer.md) · [Historische Fortschreibungen](transfer-control-history-through-0100.md)

## Befehle und Optionen

Die folgenden sieben Befehle unterstützen zusätzlich zu ihren jeweiligen Pflichtoptionen denselben Controller:

- `experience add`
- `experience revoke`
- `experience replace`
- `experience plan-add`
- `experience apply-add`
- `compatibility-plan`
- `compatibility-apply`

| Option | Standard | Bedeutung |
| --- | --- | --- |
| `--max-transfer-checkpoints N` | kein Limit | Dezimale UInt64-Zahl einschließlich 0; zusätzliche Prüfpunktgrenze |
| `--transfer-cancellation none\|checkpoints` | `none` | Bei `checkpoints` SIGINT/SIGTERM an Kontrollstellen prüfen |

Planung und Ausführung haben getrennte Konten. Das Konto wird nicht im Plan gespeichert. Die im Plan festgehaltenen Byte-/Auswahlgrenzen bleiben unverändert. Die Regelverwaltung verwendet diesen CLI-Controller nicht.

```bash
./build/bruce experience plan-add --from-run source.bruce --output import.plan --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce experience apply-add import.plan --from-run source.bruce --output neu.experience --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce experience add --from-run source.bruce --output direkt-neu.experience --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce experience revoke --store direkt-neu.experience --id RECORD_ID --reason withdraw --output widerrufen.experience --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce experience replace --store direkt-neu.experience --id RECORD_ID --reason corrected-holdout --from-run source2.bruce --output ersetzt.experience --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce compatibility-plan --from-run source.bruce --rules rules.txt --at 3 --output selection.plan --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce compatibility-apply selection.plan --from-run source.bruce --rules rules.txt --output selection.bruce --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
```

Diese Beispiele setzen vorhandene passende Quellen voraus; Zielpfade müssen neu sein. Für Z-Übergaben kann `--rule-store STORE` die direkte `--rules`-Datei ersetzen. Für Erfahrung kann `--store PREVIOUS` eine bestehende Basis angeben. Details stehen in den jeweiligen Befehlsverträgen.

## Zählung und Fehler

Modell `transfer-checkpoints-v1`: An jedem Punkt zuerst Abbruchprädikat prüfen, dann das Limit vor Erhöhung prüfen, dann den Zähler erhöhen. Limit 0 stoppt am ersten Punkt. Ein bereits ausgeschöpftes Limit wird nicht überschritten. Der theoretische UInt64-Überlauf wird abgefangen.

| Diagnose | Bedeutung |
| --- | --- |
| `TransferCancelled` | Aktiviertes Signalprädikat verlangt Abbruch |
| `TransferCheckpointBudgetExceeded: checkpoints=… limit=…` | Kein weiterer Punkt zugelassen |
| `TransferCheckpointCounterOverflow` | Zähler kann nicht erhöht werden |
| `UnknownTransferCancellation` | Ungültiger Modus |

Fehler ergeben Exit 2 mit Fehler-JSON auf stderr, ohne fachlichen Teilbericht. Native Z-Check-/Tracebudgets werden dagegen vor Beginn einer weiteren Regelprüfung geprüft und können regulär `INCOMPLETE` liefern. Erst eine noch zulässige Regelprüfung ruft den Transfercallback auf. Kandidatenbudgets und native Kandidatenabbrüche behalten ihren Vertrag.

Der benötigte Verbrauch hängt von Inhalt, Pfad, Ausgabe und Core-Version ab. Mehrfach erforderliche Audits zählen erneut. `compatibility-apply --output` braucht zusätzliche Exportpunkte. Ein Messwert ist weder zwischen Plan und Ausführung noch zwischen Versionen übertragbar.

## Erfasster Bereich

| Abschnitt | Angebundene Kontrollstellen |
| --- | --- |
| Direkte Dateiimporte | Vor Öffnen, nach jedem nichtleeren 8-KiB-Block vor Bytegrenze/Anfügen, nach Leseschleife vor Fehlerprüfung; Quellen, Basis, Pläne und direkte Regeln |
| Aktiver Regelspeicher | Vor Pfadauflösung/Laden, Dateilesen, Historieneinstieg, Digests, Hashkette, Historieneinträge und kanonischer Wiederaufbau |
| Eingebettete Regeln | Stage-/Vorschlagseinstiege, Vorschlagsdigests, Snapshot-Lesen und kanonische Ausgabe pro Regelzeile, Prüfung der bisherigen Basis |
| Erfahrung | Quellen-/Basisbindung, vorhandene Quellenreplays, Snapshotaufbau und erneute Snapshotprüfung |
| Z-Auswahl | Quellen-/Regelbindung, native Quellenreplay-/Projektionspunkte, Snapshotverarbeitung, Kandidatengrenzen und jede begonnene Regelprüfung, Snapshotdigest |
| Export | Vor Erzeugen temporärer Datei, vor jedem bis zu 64-KiB-Schreibversuch, vor `fsync`, nach erfolgreichem Schließen vor exklusivem `link` |

Eine erfolgreich gelesene reguläre Datei mit N Byte benötigt im Leser `2 + ceil(N / 8192)` Punkte. Ein erfolgreicher Export ohne kurze Schreibvorgänge oder Wiederholungen benötigt `3 + ceil(N / 65536)` Exportpunkte. Weitere fachliche und CLI-Punkte kommen hinzu. Kurze Schreibvorgänge und EINTR-Wiederholungen können den Verbrauch erhöhen.

Die Quellen- und Regelcallbacks im nativen Z-Ausführer sind getrennte API-Parameter; der Transfer bindet beide an dasselbe Konto. Bestehende direkte CLI-Befehle und Replay erhalten dadurch keine neuen Optionen. Die Abdeckungsmatrix im [Bestandsvertrag](../contracts/06-control-and-publication.md) trennt diese Wege.

## Ausgabe und Veröffentlichung

Ohne beide Steuerungsoptionen bleibt die bisherige unmittelbare JSON-Ausgabe erhalten. Sobald mindestens eine ausdrücklich angegeben ist, auch `--transfer-cancellation none`, umschließt die CLI den bisherigen vollständigen Bericht:

```json
{"contract":"controlled-transfer-v1","result":{},"usage":{"model":"transfer-checkpoints-v1","max_checkpoints":1000000,"checkpoints":123}}
```

Das Beispiel ist schematisch; `result` enthält den jeweiligen bisherigen Plan-/Ausführungsbericht. Ohne Limit ist `max_checkpoints` null. Für Plan-/Apply-Transfers bleiben die vier inneren Verträge `experience-add-transfer-v1`, `experience-transfer-result-v1`, `compatibility-transfer-v1` und `compatibility-transfer-result-v1` maßgeblich; direkte `experience add`-, `revoke`- und `replace`-Aufrufe enthalten stattdessen ihren nativen Experience-Bericht. Native Plan-/Snapshot-/Bundleformate bleiben unverändert.

Die temporäre Exportdatei liegt privat im Zielverzeichnis. Vor dem letzten Kontrollpunkt erkannter Abbruch veröffentlicht kein Ziel und führt durch den Aufräumpfad. Vorhandene Ziele werden nicht überschrieben. Nach erfolgreichem `link` gibt es keine weitere Callbackprüfung. Der Erfolgsbericht enthält den Verbrauch einschließlich Export. Ein erst nach dem letzten Punkt eintreffendes Signal muss keinen Abbruch mehr auslösen. Für kontrolliertes direktes `experience add` werden Quelle und optionale Basis über denselben Adapter gelesen, der fachliche Snapshotaufbau mit demselben Callback ausgeführt und ausschließlich über diese exklusive Veröffentlichung ausgegeben. Kontrolliertes `experience revoke` liest seine optionale Basis über diesen Adapter; kontrolliertes `experience replace` liest optionale Basis und Ersatzquellenbundle darüber. Beide führen die bestehende Experience-Operation mit demselben Callback aus und veröffentlichen ausschließlich über den exklusiven Export.

## Grenzen und Nachweise

Einzelne Systemaufrufe können blockieren. Quoted-Textdecodierung, Kopien, einzelne Zeilen/Prädikate, äußere Stage-Rekonstruktion, Kandidatenvorbereitung, Auswahlzusammenführung und Teile der Darstellung bleiben atomare Arbeit. Vor Signalinstallation gelten normale Betriebssystemwirkungen. Prozessabsturz, SIGKILL oder Aufräumfehler können temporäre Reste hinterlassen. Keine neue Crash-Dauerhaftigkeitszusage, maximale Signallatenz oder universelle Zeit-/RAM-Grenze.

Die Regelaktivierung besitzt ihren unveränderten eigenen Commit- und Bestätigungsweg. Ein Transfer ist keine Zustimmung zu Regeländerungen.

Die Suite prüft exakte Budgets und einen Punkt zu wenig, echte Signale an synchronisierten Dateigrenzen, Abbrüche innerhalb der Regel-/Historienverarbeitung und an jeder Exportkontrollstelle. Der [durchgängige Abnahmeprüfer](control-overview.md) verbindet Forschung, Quellenbindung, zwei Übergaben und erneute Ergebnisprüfung. Diese Nachweise sind keine Gesamtfreigabe des Blueprints.
