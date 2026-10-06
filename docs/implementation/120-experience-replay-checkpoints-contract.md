# Erfahrungs-Quellaudits mit Replayprüfpunkten / Core 0.58

## Vertrag

`read_experience(snapshot, checkpoint={})` nimmt einen optionalen werfenden `ReplayCheckpoint` entgegen. Er wird nach der Snapshot-Digestprüfung, vor jedem Ereignis, an Strategie- und Planungsgrenzen der Quellauswertung sowie vor der abschließenden kanonischen Snapshotprüfung abgefragt. Eingebettete exakte und Grid-Forschungsquellen erhalten denselben Callback durch ihre vorhandenen Replaypfade. Auch ersetzte oder widerrufene Nachweise werden weiterhin geprüft.

Eine geschützte Callbackhülle verhindert, dass die Quellenprüfung eine Callbackexception als fachlichen Fehler umdeutet. Insbesondere bleibt ein vom Callback geworfener `ReplayOperationLimit` unverändert; eine tatsächliche Überschreitung des Quellenlimits erzeugt weiterhin `ExperienceSourceOperationLimit`. Mathematische Fehlerbehandlungen dürfen Callbackexceptions ebenfalls nicht verschlucken.

`exact_research` und der interne Portfolioeinstieg erhalten einen optionalen separaten `experience_checkpoint`. Er gilt ausschließlich für den Erfahrungsimport und ersetzt nicht den nativen booleschen Suchabbruchcallback. Der exakte Replaydecoder reicht seinen Replaycallback an diesen Parameter weiter. Damit sind die Audits automatisch in `research-checkpoints` und in geschachtelte `compatibility-checkpoints` eingebunden, einschließlich Grid-Forschung.

## Kompatibilität und Fehler

Ohne Callback bleiben Stimmen, Korrekturketten, Quellenzulassung, Auditkosten, native Abbruchgrenzen und Bundlebytes unverändert. Der native CLI-Erstlauf übergibt weiterhin keinen inneren Erfahrungs-Replaycallback. Ein gespeicherter Suchabbruch überspringt den ohnehin vorgeschalteten Erfahrungsimport nicht.

Kein neues CLI-Flag, Bundleformat oder Sitzungsvertrag: bestehende Forschungsberichte bleiben V5, Z-Kompatibilitätsberichte V7. `VerificationCancelled` führt innerhalb der zusätzlichen Replayprüfung zu FAILED in REPLAYING und verhindert die Rückgabe der Sitzung und deren Export. Eine erfolgreich geprüfte Erfahrungsquelle beeinflusst nur die Strategiereihenfolge, nicht den mathematischen Wahrheitsstatus.

## Grenzen

Quellenbytes, Hashing, Kopien, Snapshotserialisierung und einzelne Rechenoperationen bleiben atomar. Die Prüfung der Hypothesen einer einzelnen Quellstrategie bleibt zwischen den Strategieprüfpunkten atomar. Keine garantierte Signallatenz, kein neues gemeinsames Operationsbudget. Die mutierenden Experience-Befehle und `experience inspect` besitzen weiterhin ihre bisherigen CLI-Abbruchmöglichkeiten; der neue Callback ist dort nicht als Option angebunden.

[Benutzung](121-experience-replay-checkpoints-usage.md) · [Abnahme](acceptance/experience-replay-checkpoints-v1-2026-09-23.md)
