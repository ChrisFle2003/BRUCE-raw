# ADR-012: Optionale geprüfte Ausführungssitzung

## Status

Implementiert mit Core 0.49 im freigegebenen Architektur-Konsolidierungsschwerpunkt.

## Entscheidung

Der erste gemeinsame Phasencontroller ist eine feste synchrone Zustandsmaschine: Ausführung, Serialisierung, Replayprüfung, Freigabe. Er verwendet die acht vorhandenen typisierten Adapter und verändert ihre Algorithmen, Budgets und nativen Exportformate nicht. Fehler beenden die Sitzung, bevor die CLI einen Export beginnt.

Die Funktion `execute_checked_application` liefert nach erfolgreichem Replay eine `ExecutionSession` mit Ergebnis, Bundle und Phasenfolge. `--verification replay` aktiviert sie in der CLI. Die Zustandsmaschine erzwingt Übergänge; native Teilbefunde bleiben auch nach erfolgreicher Reproduzierbarkeitsprüfung Teilbefunde.

Core 0.50 ergänzt [typisierte Sitzungsfehler](../implementation/104-execution-failure-contract.md): Fehlerphase, terminaler Verlauf und verschachtelte Originalexception bleiben erhalten. Die CLI meldet diese Daten strukturiert, ohne Fehler vor oder nach der Sitzung als Arbeitsphasenfehler auszugeben. Erfolgsvertrag und native Formate bleiben unverändert.

## Abgrenzung

Der neue Controller ist kein allgemeiner Scheduler oder Gategraph. Er hat kein gemeinsames Kostenbudget und führt Replay nicht unter dem veränderlichen äußeren Abbruchsignal aus. Seine Anzeige protokolliert den aktuellen Aufruf; das exportierte native Bundle bleibt unabhängig davon unverändert. Persistente Phasenbelege und unterbrechbare Verifikation benötigen eigene spätere Verträge.

[Vertrag](../implementation/102-execution-phases-contract.md) · [Benutzung](../implementation/103-execution-phases-usage.md)

Core 0.51 erweitert die Zulassung zur Replayphase um ein [optionales Bytebudget](../implementation/106-replay-byte-budget-contract.md), das beim Sitzungsstart als Wert übernommen wird. Die Grenze wird nach Serialisierung und vor Replay geprüft; sie ist kein Gesamtkostenbudget.

Core 0.52 ergänzt einen [separaten Prüfcallback](../implementation/108-verification-cancellation-contract.md) an drei Phasengrenzen. Die explizite CLI-Option bewahrt das bisherige Verhalten beim Export nativ abgebrochener Teilbefunde. Keine atomare Prüfoperation erhält dadurch interne Unterbrechungspunkte.

Core 0.53 reicht einen [werfenden Prüfcallback](../implementation/110-grid-replay-checkpoints-contract.md) durch Grid-Stapelreplay und Fortsetzungsketten. Die Trennung vom nativen booleschen Abbruchcallback verhindert, dass eine gestoppte Evidenzprüfung einen neuen Teilbefund erzeugt. Nicht unterstützte Replaytypen werden bei angeforderten inneren Prüfpunkten abgelehnt.

Core 0.54 erweitert den [werfenden Replaycallback auf Formelforschung](../implementation/112-research-replay-checkpoints-contract.md). Ein immer-false-Observer verwendet vorhandene Suchcheckpoints; eine interne Exception-Hülle verhindert die Verwechslung von Callbackfehlern mit mathematischen Zahlenlimits. Gespeicherte native Abbruchgrenzen bleiben unverändert.

Core 0.55 verbindet die [Grid- und Forschungsprüfpunkte im zusammengesetzten Replay](../implementation/114-grid-research-replay-contract.md). Quellenprüfung und Samplebindung reichen denselben Callback weiter; bestehende Zulassungs- und Budgetprüfungen bleiben an ihren bisherigen Stellen.

Core 0.56 bindet [Bruchvergleichsreplays](../implementation/116-comparison-replay-checkpoints-contract.md) über denselben geschützten Observer an. Ein Prüfungsabbruch darf nicht von den nativen NumericLimit-/Normalisierungsbehandlungen als Teilbefund ausgegeben werden.

Core 0.57 reicht den [Replaycallback durch Z-Kompatibilität](../implementation/118-compatibility-replay-checkpoints-contract.md), Forschungsursprünge und Hypothesenprojektion. Ein immer-false-Kandidatenobserver erhält native Abbruchgrenzen; der Erstlauf verwendet weiterhin seine bisherigen Regeln.

Core 0.58 erweitert den [Replaycallback auf Erfahrungs-Quellaudits](../implementation/120-experience-replay-checkpoints-contract.md). Ein separater Importcallback verhindert Änderungen an nativen Suchabbruchgrenzen; eine geschützte Hülle erhält Callbackexceptions gegenüber der Quellenfehlerübersetzung.

Core 0.59 bindet die [äußere Bundle-Prüfsumme](../implementation/122-serialization-digest-checkpoints-contract.md) optional in die Abbruchprüfung von SERIALIZING ein. Der eigene Digestkontext wird bei Callbackexceptions freigegeben; Hash und native Bytes bleiben identisch. Payloadaufbau und innere Digests bleiben zunächst atomar.

Core 0.60 ergänzt [Recordpunkte für Grid-Evidenz](../implementation/124-grid-serialization-records-contract.md). Der zusätzliche Callback bleibt vom Digestcallback getrennt; ein unpassender Ergebnistyp wird abgelehnt. Öffentliche Renderer und native Berechnungsgrenzen bleiben erhalten.

Core 0.61 ergänzt den [separaten Erfahrungsimport-Abbruch im Erstlauf](../implementation/126-execution-experience-checkpoints-contract.md). EXECUTING darf bei gestopptem Quellaudit keinen mathematischen Teilbefund liefern. Der native Suchcallback bleibt unabhängig, ebenso die nachgelagerten Prüfcallbacks.

Core 0.62 ergänzt [Gridquellen- und Projektionspunkte im Erstlauf](../implementation/128-execution-grid-source-checkpoints-contract.md). Die Quellenzulassung bleibt vorgeschaltet; Grid- und Erfahrungsimportcallbacks werden nicht in die native Suche oder spätere Verifikation weitergereicht.

Core 0.63 erweitert die [Z-Quellenvorbereitung im Erstlauf](../implementation/130-execution-compatibility-source-contract.md) um einen separaten Importcallback. Ein Quellauditabbruch erreicht die Kandidatenbewertung nicht und erzeugt keinen neuen Auswahlteilbefund.

Core 0.64 trennt [Import- und Fortsetzungspunkte](../implementation/132-execution-resume-import-contract.md) im Grid-Resume-Pfad. Im nativen Erstlauf erreicht der Importcallback nur Vorgängernachweis und Cache-Präfixaufbau; beim bestehenden Replay bleibt der Callback für beide Bereiche aktiv.
