# Geprüfte Ausführungssitzung – Vertrag V1

Core 0.49 ergänzt für die acht typisierten Laufarten eine optionale synchrone Ausführungssitzung. Ihr Zweck ist eine gemeinsame kontrollierte Reihenfolge zwischen Berechnung, Serialisierung und Replayprüfung. Die Fachalgorithmen und ihre Gatebedingungen bleiben erhalten.

## Phasen und Übergänge

`CREATED → EXECUTING → SERIALIZING → REPLAYING → READY`

Aus jeder der drei Arbeitsphasen ist außerdem `FAILED` zulässig. READY und FAILED sind terminal. Übersprungene Phasen und Übergänge aus terminalen Zuständen werden zurückgewiesen.

1. EXECUTING delegiert über `execute_application` an den vorhandenen Fachalgorithmus.
2. SERIALIZING erzeugt dessen unverändertes natives Bundle.
3. REPLAYING ruft den gemeinsamen typisierten Replay auf. Ergebnisvariante und vollständige native Darstellung müssen mit dem zuerst erzeugten Ergebnis übereinstimmen.
4. READY gibt das geprüfte Ergebnis und genau dieses Bundle an den Aufrufer zurück. Erst danach kann die CLI exportieren.

Ein Fehler führt zu einer Exception mit der fehlgeschlagenen Arbeitsphase. Es entsteht kein Session-Ergebnis und die CLI startet keinen Export. Fehler beim anschließenden Schreiben der Datei bleiben Exportfehler außerhalb der Sitzung.

Seit Core 0.50 sind Arbeitsphasenfehler außerdem [typisiert und strukturiert auswertbar](104-execution-failure-contract.md); Originalursache und terminaler Verlauf bleiben erhalten.

## Ergebnis und Aussagegrenze

`--verification replay` schaltet die Sitzung ein. Die CLI-Ausgabe enthält `execution` mit Vertrag `application-execution-v1`, Phasenfolge, `replay_verified: true` und `result_complete`, dazu unter `result` die unveränderte native Ausgabe. `result_complete` ist bei nativem INCOMPLETE false, ansonsten true. Ein lokaler Grid-Sentinel kann daher eine fertige Sitzung mit fachlichem STOPPED ergeben.

READY bedeutet, dass die Reproduzierbarkeitsprüfung bestanden wurde. Es ist kein Beweis mathematischer Allgemeingültigkeit, keine Freigabe für Regelpromotion und keine Aufhebung eines Teilbefunds. Ein unvollständiger Suchlauf kann reproduzierbar sein und READY erreichen, bleibt aber unter `result` ausdrücklich INCOMPLETE.

Die Phasenanzeige ist ein Bericht des aktuellen Aufrufs, kein zusätzlich signiertes oder persistiertes Evidenzformat. `--output` speichert ausschließlich das bestehende native Bundle. Ein späterer Replay liefert dessen native Ausgabe; die damalige Sessionanzeige wird nicht darin behauptet.

## Abbruch, Kosten und Kompatibilität

Das Benutzersignal wird nur während der ersten Ausführung an native Checkpoints weitergegeben. Der Replay verwendet die gespeicherte Abbruchposition und fragt das äußere Signal nicht erneut ab. Quellenprüfung, Cacheaufbau, Serialisierung und Replay bleiben atomar im Sinne dieses Controllers. Eine während der Prüfphase gesetzte Abbruchanforderung beendet diese Phase nicht vorzeitig.

Der zusätzliche Replay verursacht zusätzliche Arbeit und Speicherbelegung. Es gibt noch kein gemeinsames Sitzungskostenbudget; native Suchbudgets gelten unverändert. Der Modus serialisiert auch ohne `--output`, weil dies zur Replayprüfung notwendig ist. Daher können Bundlegrößenlimits hier einen Auftrag ablehnen, dessen reine ungespeicherte Ausführung möglich wäre.

Ohne Option oder mit `--verification none` bleibt der bisherige Ablauf erhalten. Die Option gilt nur für die acht Laufarten, nicht für Hilfsbefehle oder Experience-Verwaltung. Keine Hintergrundjobs, keine Persistenz der Sitzung, keine beliebigen Phasengraphen und keine Wiederaufnahme allgemeiner Formelforschung.

[Benutzung](103-execution-phases-usage.md) · [Abnahme](acceptance/execution-phases-v1-2026-09-22.md)
