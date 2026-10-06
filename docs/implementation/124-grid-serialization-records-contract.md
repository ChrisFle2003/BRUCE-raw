# Grid-Inhaltsaufbau mit Abbruchpunkten / Core 0.60

**Fortschreibung Core 0.67:** Die Elternkopie im Fortsetzungs-Payload besitzt jetzt [zusätzliche Kontrollpunkte zwischen 64-KiB-Blöcken](140-resume-copy-blocks-usage.md). Die unten beschriebenen ursprünglichen Grenzen sind insoweit überholt; andere Kopien und Speicherreservierungen bleiben teilweise atomar.

## Vertrag

`pack_grid_batch`, `pack_grid_resume` und `pack_application` akzeptieren nach dem bisherigen Digestcallback einen optionalen `records`-Callback. Bei Grid-Stapeln wird er vor jedem gespeicherten Eingabepfad, jedem Trace-Eintrag und jeder Ergebniszeile abgefragt. Bei Fortsetzungen wird er vor dem Aufbau des Headers einschließlich Elternkopie sowie vor jedem Trace-Eintrag und jeder Ergebniszeile abgefragt.

Die Ergebniszeilen verwenden dieselbe interne Renderfunktion wie bisher; öffentliche Renderer bleiben ohne Callback. Exceptions verlassen den Packer unverändert. Ein Abbruch beim Inhaltsaufbau erreicht weder die äußere Prüfsumme noch den endgültigen Bundlerückgabewert. Ohne Callback oder mit weiterlaufendem Callback bleiben die Bytes unverändert.

Der gemeinsame Packer lehnt einen Records-Callback bei anderen Ergebnistypen mit `SerializationRecordsRequireBatchOrResume` ab. Bisherige direkte C++-Aufrufe bleiben durch Defaultargumente gültig. Die Funktionszeigertypen der drei erweiterten Packer enthalten einen weiteren Parameter; eine binäre ABI-Kompatibilität wird nicht zugesagt.

## Sitzung und CLI

`VerificationControl::serialization_grid_records` ist gegenseitig exklusiv mit `serialization_digest_checkpoints`. Es erfordert einen Callback und einen GridBatchTask oder GridResumeTask. Fehlkonfigurationen werden in EXECUTING abgelehnt. Die bisherigen inneren Replaymodi bleiben separat wählbar.

`--serialization-cancellation grid-record-checkpoints` erfordert `--verification replay`. Der neue Modus aktiviert sowohl die beschriebenen Inhaltspunkte als auch die äußeren Digestpunkte aus Core 0.59. Erfolgsberichte verwenden `application-execution-v9` mit `serialization_cancellation: grid-record-checkpoints`. Frühere Modi behalten ihre bisherigen Berichte.

Ein angeforderter Abbruch beim Inhaltsaufbau erzeugt `VerificationCancelled`, FAILED in SERIALIZING und keinen Rückgabewert der geprüften Sitzung. Der nachgelagerte CLI-Export wird nicht erreicht. Native Berechnung, Cache-Zähler, lokale Stopps und gespeicherte Such-/Abbruchgrenzen bleiben unverändert. Vollständige und unvollständige Bundles sowie Fortsetzungen behalten ihre nativen Versionen.

## Grenzen

Ein einzelner Pfad, eine Trace-Zeile und eine Ergebniszeile werden weiterhin atomar verarbeitet. Das Kopieren eines Eltern-Bundles, Speicherallokationen, feste Header und abschließende Kopien bleiben atomar. Der Elterninhalt wird als bereits vorhandene Zeichenfolge übernommen, nicht mit inneren Recordpunkten neu gepackt. Andere Auftragstypen unterstützen weiter den Digestmodus, aber noch nicht diesen Recordmodus. Keine maximale Signallatenz, kein Speicher- oder phasenübergreifendes Operationsbudget.

[Benutzung](125-grid-serialization-records-usage.md) · [Abnahme](acceptance/grid-serialization-records-v1-2026-09-23.md)
