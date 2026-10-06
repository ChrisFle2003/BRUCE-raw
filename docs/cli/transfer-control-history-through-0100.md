# Gemeinsame Übergabesteuerung – Core 0.100.0

[Handbuch](README.md) · [Erfahrungsübergabe](experience-transfer.md) · [Z-Auswahlübergabe](compatibility-transfer.md)

## Geltungsbereich und Aufruf

Die **Planung und Ausführung** beider Übergabetypen nimmt dieselben zusätzlichen Optionen an:

```text
bruce experience apply-add PLAN --from-run RUN [--store STORE] --output NEW
    [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce compatibility-apply PLAN --from-run RUN (--rules RULES | --rule-store STORE)
    [--output RUN] [--max-transfer-checkpoints N]
    [--transfer-cancellation none|checkpoints]
```

Die Klammern in der zweiten Syntax bezeichnen Alternativen und werden nicht eingegeben. `--max-transfer-checkpoints` ist eine dezimale UInt64-Zahl, einschließlich 0. Ohne Angabe gibt es kein Transfer-Prüfpunktlimit. `--transfer-cancellation` ist standardmäßig `none`; `checkpoints` aktiviert die Prüfung von SIGINT und SIGTERM an den Kontrollpunkten. Andere Werte sind Fehler.

Seit Core 0.95 nehmen auch `experience plan-add` und `compatibility-plan` beide Optionen an. Ihre bisherigen Pflichtoptionen bleiben erforderlich. Jeder CLI-Aufruf hat ein eigenes Konto; das Planungsbudget wird nicht im Plan gespeichert oder auf die spätere Ausführung übertragen. Die festgehaltenen Plangrenzen und fachlichen Auswahlbudgets werden durch die neuen Optionen weder geändert noch überschrieben. Es ist eine zusätzliche Steuerung, kein neues Planformat. Auch die Regelverwaltung ist nicht Teil dieses Controllers.

## Zählmodell und Abbruch

Das Modell heißt `transfer-checkpoints-v1`. Eine Kontrollstelle zählt einen Punkt, keine mathematische Operation. Es gibt keine Phasentabelle wie beim allgemeinen Sitzungscontroller. Beide Übergabewege verwenden denselben Controller über ihre Ausführung hinweg.

Bei jedem Punkt gilt diese Reihenfolge:

1. Ist ein aktivierter Abbruch angefordert, Fehler `TransferCancelled`.
2. Ist das optionale Limit bereits ausgeschöpft, Fehler `TransferCheckpointBudgetExceeded: checkpoints=… limit=…`.
3. Andernfalls Zähler erhöhen und fortfahren.

Limit 0 stoppt am ersten Punkt. Wenn Abbruch und Budget gleichzeitig greifen, hat der Abbruch Vorrang. Der Zähler läuft nicht über; der theoretische Überlauf meldet `TransferCheckpointCounterOverflow`. `UnknownTransferCancellation` meldet einen ungültigen Modus.

Ein solcher Fehler ergibt Exit 2, Fehler-JSON auf stderr und keinen fachlichen Teilbericht. Ein **natives Auswahlbudget** kann dagegen wie bisher einen regulären `INCOMPLETE`-Befund erzeugen. Transferlimit und mathematisches Auswahlbudget sind verschiedene Verträge.

## Erfasste Kontrollstellen

| Abschnitt | Experience-Import | Z-Auswahl |
| --- | --- | --- |
| Start und Digestbindung | Eintritt sowie blockweise Quellen-/Basisdigests | Eintritt sowie blockweise Quellen-/Regeldigests |
| Erneute Quellenprüfung | Kontrollpunkte beim Lesen der vorhandenen Erfahrung, im Quellenreplay des neuen Eintrags und beim Prüfen des neuen Snapshots | Bestehende Quellenreplay-/Projektionskontrollpunkte |
| Fachliche Verarbeitung | Vor/nach Snapshotaufbau und abschließender Berichterstellung | Kontrollpunkt an jeder begonnenen Kandidatenauswertung |
| Exportvorbereitung | Snapshot bereits im Speicher; Kontrollpunkt nach Berichtsaufbau | Bei Bundleexport zusätzlich äußere Digestkontrollpunkte; Kontrollpunkt nach Berichtsaufbau |

Alle diese Punkte teilen das optionale Limit. Auch die mehrfach notwendige Prüfung eingebetteter Quellen zählt erneut. Die benötigte Zahl hängt von Inhalt, Pfad, Ausgabeoptionen und Implementierung ab. Ein früher gemessener Wert ist keine versionsübergreifende Zusage. Insbesondere kann `compatibility-apply --output` mehr Punkte benötigen als derselbe Aufruf ohne Export.

Seit Core 0.98 liegt der letzte Punkt unmittelbar vor der exklusiven Veröffentlichung der fertig geschriebenen temporären Datei. Ein dort oder zuvor erkannter Transferabbruch veröffentlicht keine neue Ausgabedatei. Es gibt nach dem Export keine weitere Transferprüfung, die einen bereits erfolgreichen Export nachträglich als abgebrochen meldet.

## Ausgabe bei aktivierter Steuerung

Ohne beide Optionen bleibt die bisherige Ausgabe bytegleich. Sobald mindestens eine Option ausdrücklich angegeben ist, auch `--transfer-cancellation none`, wird der bisherige Transferbericht umschlossen:

```json
{
  "contract": "controlled-transfer-v1",
  "result": {"contract": "experience-transfer-result-v1"},
  "usage": {
    "model": "transfer-checkpoints-v1",
    "max_checkpoints": 1000000,
    "checkpoints": 123
  }
}
```

Das Beispiel ist schematisch: `result` enthält den vollständigen bisherigen Transferbericht, und `123` ist kein zugesicherter Verbrauch. Für die Z-Auswahl heißt dessen Vertrag `compatibility-transfer-result-v1`. Ohne gesetztes Limit ist `usage.max_checkpoints` JSON `null`. Die exportierten Snapshot-/Bundlebytes bleiben gegenüber dem unkontrollierten Pfad unverändert.

Beispiel mit vorhandenen Eingaben und neuem Ausgabeziel:

```bash
./build/bruce experience apply-add import.plan --from-run source.bruce --output neu.experience --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
```

Für das eigentliche fachliche Ergebnis ist bei gesteuerter Ausführung die zusätzliche `result`-Ebene zu berücksichtigen. Die nativen Dateien behalten ihre bisherigen Formate und Replayeigenschaften.

## Bewusste Grenzen

Seit Core 0.96 besitzen die direkt gelesenen Transferdateien Kontrollpunkte vor dem Öffnen, nach jedem gelesenen Block und nach Ende der Leseschleife. Öffnen und ein einzelner Leseaufruf selbst bleiben atomar beziehungsweise können im Betriebssystem blockieren. Plandecodierung und Teile des Parsens, Kopierens, Packens und Renderns bleiben außerhalb innerer Kontrollpunkte. Vor der Signalinstallation kann ein Prozesssignal weiterhin die normale Betriebssystemwirkung haben. Die Controllerprüfung besitzt keine garantierte maximale Signallatenz und ist kein Prozess-RAM-/Zeitlimit.

Seit Core 0.98 besitzt der Transferexport Kontrollpunkte während der Vorbereitung. Nach dem letzten Punkt vor der Veröffentlichung eintreffende Signale müssen keinen Transferabbruch mehr auslösen. Einzelne Schreib-, Synchronisations-, Schließ- und Veröffentlichungsaufrufe bleiben ohne innere Kontrollpunkte. Normale Dateisystemfehler und Prozessbeendigung behalten ihre bisherigen Verträge.

Die beiden Planungsvorläufe verwenden seit Core 0.95 ebenfalls diese Steuerung. Ein gemeinsames Modell für sämtliche Verwaltungs-, Planungs- und Ausführungswege sowie universelle mathematische Rechenkosten ist damit nicht implementiert.

## Tests

Beide CLI-Tests vergleichen ausreichend budgetierte Dateien und Berichte mit dem bisherigen Pfad. Sie prüfen Limit 0, genau ausreichendes Limit und einen Punkt zu wenig einschließlich fehlender Ausgabe. Für SIGTERM synchronisiert eine FIFO das Einlesen einer temporären Quelle mit dem bereits installierten Handler; anschließend wird der Abbruch am Transferpunkt nachgewiesen. Das misst keine Signallatenz innerhalb einer langen mathematischen Einzeloperation. C++-Tests lösen zusätzlich deterministische Abbrüche am Anfang, innerhalb und am Ende der tatsächlichen Kontrollpunktfolge aus und prüfen die Priorität gegenüber dem Budget.

## Planung seit Core 0.95

Die Planung zählt Eintritt, Quellen-/Basisdigests und den vollständigen nativen Probelauf mit dessen vorhandenen Kontrollpunkten. Nach Erstellung der Planbytes und des Berichts liegt ein Kontrollpunkt; seit Core 0.98 folgen weitere Punkte im Export. Die kleinen Planprüfsummen und Berichterstellung bleiben atomare Abschnitte. Bei Abbruch wird kein Plan exportiert. Regeln und Experience-Basis werden nicht verändert.

```bash
./build/bruce experience plan-add --from-run source.bruce --output import.plan --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
./build/bruce compatibility-plan --from-run source.bruce --rules rules.txt --at 3 --output selection.plan --max-transfer-checkpoints 1000000 --transfer-cancellation checkpoints
```

Bei expliziter Steuerungsoption enthält `controlled-transfer-v1.result` den bisherigen Planbericht (`experience-add-transfer-v1` oder `compatibility-transfer-v1`). Ohne Optionen bleiben Bericht und Planbytes unverändert. Ein gemessener Ausführungsverbrauch ist kein Planungsbudget und umgekehrt. Tests prüfen auch für beide Planer exakte Budgets, einen Punkt zu wenig, Limit 0 und zugestelltes SIGTERM; direkte API-Tests stoppen zusätzlich innerhalb des Probelaufs.

## Dateiimport seit Core 0.96

Das gleiche Konto umfasst bei beiden Plan- und Ausführungsbefehlen jetzt das Lesen von Quellen, Experience-Basisdateien, Übergabeplänen und direkten `--rules`-Dateien. Seit Core 0.97 reicht auch `--rule-store` das Konto an den Dateileser und die Historienprüfung weiter. Andere CLI-Befehle verwenden weiterhin ihren bisherigen Dateilesepfad.

`read_bounded` prüft vor dem Öffnen, nach jedem nichtleeren gelesenen Block (höchstens 8192 Byte) vor Größenprüfung und Anfügen sowie nach der Leseschleife vor der Fehlerprüfung. Eine erfolgreich gelesene reguläre Datei mit N Byte verbraucht hier `2 + ceil(N / 8192)` Punkte, auch die leere Datei also zwei. Weitere fachliche und CLI-Kontrollpunkte kommen hinzu. Ein Rückruf kann damit vor einem konkurrierenden Datei-/Bytefehler abbrechen. Ohne Callback bleiben die bisherigen Bytes und Dateifehler erhalten.

Mit Transferlimit 0 wird die erste kontrollierte Datei nicht geöffnet; bei Z-Übergaben liegt zusätzlich ein CLI-Punkt vor dem Laden der Regeln. Verbräuche aus Core 0.95 reichen deshalb nicht unverändert für Core 0.96. Es gibt keine neue Option und keine neue Version der Plan- oder Bundleformate.

Ein SIGTERM während eines blockierenden Dateiaufrufs wird erst am nächsten erreichbaren Kontrollpunkt verarbeitet. Die Tests weisen Abbruchpunkte und Fehlerweitergabe nach, keine feste Zeitgrenze für Geräte, FIFOs oder Netzwerkdateisysteme. Der kontrollierte Export ist im folgenden Abschnitt beschrieben.

## Aktiver Regelspeicher seit Core 0.97

Bei `compatibility-plan` und `compatibility-apply` verwendet `--rule-store` dasselbe Konto: vor Pfadauflösung und Laden, beim blockweisen Dateilesen, beim Einstieg in die Historienprüfung, beim Start-/Außendigest, vor jedem Entscheidungsdatensatz, beim Fortschreiben der Hashkette sowie beim kanonischen Wiederaufbau einschließlich äußerer Prüfsumme. Kein Callback wird gespeichert oder über den Aufruf hinaus verwendet.

Die Kontrolle liest ausschließlich. Sie bestätigt keine Regeländerung und verändert weder Generation noch Journal oder aktive Regeln. Die späteren Quellen- und Auswahlprüfungen bleiben verpflichtend. `rules-stage`, `rules-activate`, `rules-status` und der direkte Befehl `compatibility` bekommen hierdurch keine neuen Optionen; ihre bisherigen Aufrufe verwenden keinen Callback.

Pfadauflösung, einzelne Dateioperationen und Kopien bleiben atomare Abschnitte. Seit Core 0.99 besitzen eingebettete Regelvorschläge eigene Kontrollpunkte; quoted-Textdecodierung und einzelne Zeilen bleiben atomar. Die Historie wird beim Wiederaufbau zusätzlich pro Datensatz geprüft. Deshalb sind frühere Verbrauchszahlen kein verlässliches Budget für Core 0.97. Eine maximale Signallatenz ist weiterhin nicht zugesichert. Seit Core 0.98 ist die Exportvorbereitung ebenfalls kontrolliert.

## Kontrollierter Export seit Core 0.98

Alle vier Transferbefehle reichen dasselbe Konto an `export_exclusive` weiter. Der Export prüft vor Größenprüfung und Erzeugen der temporären Datei, vor jedem Schreibversuch von höchstens 65536 Byte, vor `fsync` und nach erfolgreichem Schließen unmittelbar vor `link`. Ein erfolgreicher regulärer Export ohne kurze Schreibvorgänge oder Wiederholungen verbraucht `3 + ceil(N / 65536)` zusätzliche Punkte. Kurze Schreibvorgänge und EINTR-Wiederholungen können weitere Punkte benötigen. Ohne Callback bleibt der bisherige Schreibpfad erhalten.

Die temporäre Datei liegt privat im Zielverzeichnis. Callbackfehler vor Veröffentlichung schließen sie bei Bedarf und entfernen sie; die Zieldatei entsteht nicht. Ein vorhandenes Ziel wird nie überschrieben. Nach erfolgreichem `link` erfolgt keine weitere Callbackprüfung. Die Ausgabehülle wird anschließend mit dem gesamten Verbrauch erzeugt. Ein Budget von einem Punkt weniger als dem gemessenen Gesamtverbrauch stoppt damit direkt vor Veröffentlichung.

Aufräumen wird bei regulären Callbackfehlern ausgeführt und getestet. Prozessabsturz, SIGKILL oder Fehler beim Entfernen einer Datei können temporäre Reste hinterlassen. Einzelne Betriebssystemaufrufe können blockieren. Der Export bekommt hierdurch keine neue Zusage zur Crash-Dauerhaftigkeit oder maximalen Signallatenz. Die Regelaktivierung besitzt weiterhin ihren eigenen unveränderten Commitpfad.

## Eingebettete Regelprüfung seit Core 0.99

Das Historienkonto erreicht nun `RuleStage::parse`, den `RuleProposal`-Decoder und die Prüfung der bisherigen Regelbasis. Es zählt den Einstieg in Vorschläge, deren äußere Prüfsummen, den Einstieg in numerische Regelsnapshots sowie jede gelesene und jede kanonisch ausgegebene Regelzeile. Die Kanonisierung direkter Regeln in `CompatibilityTransfer::plan/apply` verwendet ebenfalls diesen Callback.

Callbacks werden nicht gespeichert. Ohne Callback bleiben Bytes und fachliche Prüfergebnisse unverändert. Die Regelverwaltung bekommt keine neuen Optionen; menschliche Freigabe und Aktivierung bleiben unverändert. Callbackfehler werden weitergereicht und nicht als fachliche Ablehnung umgedeutet.

Restarbeit: quoted-Textdecodierung, Kopien, Verarbeitung einer einzelnen Zeile und die kanonische Wiederherstellung des äußeren `RuleStage` bleiben atomar. Seit Core 0.100 ist auch die erneute Snapshotverarbeitung im nativen Z-Lauf angeschlossen. Damit ist keine maximale Abbruchlatenz zugesichert. Der zusätzliche Verbrauch erfordert gegebenenfalls höhere Budgets als Core 0.98.

## Native Z-Regelauswertung seit Core 0.100

Die Transferplanung und -ausführung reichen ihr Konto nun zusätzlich an den nativen Regelpfad weiter: Regelsnapshot lesen, kanonische Darstellung, jede tatsächlich begonnene Regelprüfung eines Kandidaten und abschließende Snapshot-Prüfsumme. Der vorhandene Quellencheckpoint bleibt eine getrennte API-Komponente; der Transfer verwendet für beide dasselbe Konto. Direkte bestehende Aufrufe und Replay erhalten standardmäßig keinen zusätzlichen Regelcallback und behalten ihre bisherigen Kontrollverträge.

Das native Auswahlbudget wird vor Beginn einer weiteren Regelprüfung geprüft. Ist es ausgeschöpft, entsteht weiterhin ein regulärer `INCOMPLETE`-Befund mit `OperationBudgetExceeded` beziehungsweise `TraceBudgetExceeded`. Erst für eine zulässige nächste Prüfung wird der Regelcallback aufgerufen; ein dort ausgelöster Transferabbruch propagiert als Fehler und erzeugt keinen Teilbericht oder Export. Kandidatenabbruch und Kandidatenbudgets bleiben unverändert.

Es wird ein Prüfpunkt gezählt, keine allgemeine mathematische Operation. Einzelne Prädikatauswertung, Kandidatenvorbereitung, Auswahlzusammenführung und Berichtsdarstellung behalten atomare Arbeit. Keine maximale Signallatenz. Verbräuche können gegenüber Core 0.99 steigen; native Bytes und Freigaberegeln ändern sich nicht.
