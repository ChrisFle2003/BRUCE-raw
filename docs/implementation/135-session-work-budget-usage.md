# Ein Budget für die gesamte Prüfsitzung / Core 0.65

**Fortschreibung Core 0.67:** Die Elternkopie im Fortsetzungs-Payload besitzt jetzt [zusätzliche Kontrollpunkte zwischen 64-KiB-Blöcken](140-resume-copy-blocks-usage.md). Die unten beschriebenen ursprünglichen Grenzen sind insoweit überholt; andere Kopien und Speicherreservierungen bleiben teilweise atomar.

Bruce kann vorhandene Arbeitsprüfpunkte jetzt über Ausführung, Importe, Ergebnisaufbereitung und erneute Prüfung hinweg gemeinsam begrenzen. Es genügt eine zusätzliche Option; einzelne Import- oder Replaymodi müssen dafür nicht aktiviert werden.

```bash
./build/bruce grid-batch --paths '2,6;4,3' \
  --verification replay --max-session-checkpoints 1000
```

Der V14-Bericht enthält unter `execution.session_budget`:

- `model: session-checkpoints-v1`: gezählt werden zugelassene Prüfpunkte, keine Rechenoperationen.
- `max_checkpoints`: das vorgegebene Limit.
- `checkpoints`: gesamter Verbrauch, ohne Rücksetzung beim Phasenwechsel.
- `by_phase`: Anteile von `EXECUTING`, `SERIALIZING` und `REPLAYING`.

Das gilt für alle acht Auftragstypen, auch für Erfahrungsquellen, Grid-Forschung, Z-Forschungsimporte und mehrstufige Grid-Fortsetzungen. Einzelne Grid-Auswertungen und deren Einzelreplay sind intern weiterhin atomar.

## Grenze und Fehler

Ein Budget von 0 verhindert die Ausführung. Genau ausreichend viele Punkte sind zulässig; der nächste Punkt löst `SessionCheckpointBudgetExceeded` aus. Der Fehlerbericht `application-execution-failure-v2` enthält die betroffene Phase und den bis dahin zugelassenen Verbrauch. Es gibt kein freigegebenes Ergebnis und keinen Export; eine bereits vorhandene Zieldatei bleibt erhalten.

```bash
./build/bruce grid-batch --paths '2,6;4,3' \
  --verification replay --max-session-checkpoints 0
```

Vorhandene lokale Suchgrenzen gelten zusätzlich. Erzeugt ein regulärer nativer Abbruch einen Teilbefund, kann dieser wie bisher exportiert werden, sofern das gemeinsame Budget noch für Serialisierung und Prüfung reicht. Signalcallbacks behalten ihre bisherigen Aufrufpunkte; das Budget aktiviert keine zusätzlichen Signalabfragen.

## C++

```cpp
bruce::GridBatchTask task;
task.paths={"2,6","4,3"};
bruce::SessionBudget budget;
budget.max_checkpoints=1000;
auto session=bruce::execute_checked_application(task,{},{},{},budget);
```

`session.session_usage` enthält den Verbrauch; bei Fehlern liefert `ExecutionSessionError::session_usage()` den Stand. Das Budget wird bei Sitzungsbeginn kopiert. Der zusätzliche `ExecutionControl::work_checkpoint` ist ein werfender Arbeitsbeobachter für native Suchpunkte und passende Importe; er ist vom booleschen Abbruchcallback getrennt und erzeugt keinen nativen Teilbefund.

## Verträglichkeit und Grenzen

Ohne Option bleiben die bisherigen Berichte und Callbackaufrufe bestehen. Mit ausreichendem Budget bleiben mathematischer Befund und Bundlebytes gleich; Sitzungsbudgets gehören nicht zum mathematischen Replayformat.

Die Zahlen gelten für dieses Kontrollpunktmodell und den konkreten Ausführungspfad. Sie sind keine CPU-Zeit, Speichergrenze oder allgemeine mathematische Operationszahl. Innere Hashes, große Kopien, viele Payloadaufbauten, einzelne Berechnungen und Berichtsrendering bleiben atomar. Das garantiert keine maximale Signallatenz. Allgemeine Fortsetzungen und vollständige M0–M2-Abnahmen bleiben weitere Arbeit.

[Vertrag](134-session-work-budget-contract.md) · [Architekturstand](75-current-architecture-and-milestones.md) · [Abnahmematrix](136-m0-m2-acceptance-matrix.md)
