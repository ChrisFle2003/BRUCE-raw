# Gemeinsamer logischer Rahmen für Grid-Import und Forschung – Core 0.40

## Begrenzter Umfang

`--max-total-operations` ergänzt den bestehenden Grid-Forschungsablauf in X- und Y-Richtung um einen optionalen gemeinsamen Arbeitsrahmen. Ohne diese Option bleiben Verhalten und V1-/V2-Exporte unverändert. Die Option ist ausschließlich zusammen mit `--grid-train` zulässig.

Die neue Politik heißt `grid-source-bytes-plus-research-v1`. Sie verbindet eine **atomare Importabrechnung** mit dem vorhandenen logischen Operationsbudget der Forschung. Sie ist kein Scheduler, keine Zeitmessung, keine Zählung sämtlicher CPU-Instruktionen und keine Prozessspeichergrenze.

## Abrechnung und Zulassung

Sei `S` die Summe der vollständigen serialisierten Bytes beider Grid-Quellexporte. Ein fehlender Holdout trägt null Bytes bei. Für jede dieser Bytes wird ein logischer Importschritt berechnet. Auch Header, Digest, Trace und gespeichertes Ergebnis gehören zur geprüften Quelle und damit zu `S`.

Vor dem Quellenreplay gelten:

1. Die vorhandenen Quellgrenzen von höchstens 2 MiB je Quelle bleiben aktiv.
2. Gesamtbudget `T` und ursprünglich gewünschtes Forschungsbudget `R` müssen jeweils 1–10000000 betragen.
3. Es muss `T > S` gelten. Die gesamte Quellenprüfung und mindestens eine Forschungseinheit müssen zugelassen sein. Andernfalls: `InsufficientGridImportBudget`, kein Forschungsauftrag und kein Export.
4. Die Forschung erhält `min(R, T-S)` als effektives Operationsbudget. Das bisherige `--max-operations` wird dadurch niemals erhöht.

Nach erfolgreicher Quellenprüfung läuft die bestehende Forschung mit diesem Restbudget. Die Gesamtnutzung ist `S + research.operations`. Sie kann kleiner als `T` bleiben, etwa wenn die nächste unteilbare Forschungsoperation nicht mehr passt oder das lokale Forschungsbudget zuerst endet.

Die Byteabrechnung ist absichtlich an den tatsächlich geprüften Export gebunden. Andere Cacheparameter oder Darstellungen können dessen Größe und damit die Importkosten ändern, selbst bei gleichen mathematischen Samples. Der Bytekostensatz behauptet keine gemessene Gleichwertigkeit mit einer arithmetischen CPU-Operation.

Einlesen, äußere Metadaten-/Digestprüfung, Exportdarstellung und die spätere Z-Auswahl werden nicht zusätzlich als einzelne Operationen dieses Rahmens gemessen. Die eigentliche Grid-Quellenprüfung bleibt ein begrenzter, nicht einzeln unterbrechbarer Abschnitt. Für sie gibt es keine exportierbaren halbfertigen Quellenprüfungen. Quellenfehler bleiben Fehler ohne Export. Das ist noch kein allgemeines phasenübergreifendes Abbruch-/Fortsetzungsmodell.

## Forschungsabbrüche und Darstellung

Kandidaten-, Trace-, Ergebnis-, Zahlen- und Abbruchgrenzen bleiben unverändert aktiv. Erschöpft sich während der Forschung ihr effektives Budget, bleibt der bestehende `INCOMPLETE`-Befund samt Abbruchposition, bereits vorhandenen Befunden und Quellen exportierbar. Callback-Abbrüche behalten ihre bisherigen Regeln. Ein unvollständiger Forschungsbefund darf wie bisher nicht durch Z gerankt werden.

Die äußere JSON-Ausgabe ergänzt `work_budget`:

| Feld | Bedeutung |
| --- | --- |
| `policy` | Versionierte Abrechnungsregel |
| `limit` | Gewünschter Gesamtrahmen `T` |
| `source_operations` | Quellenbytes `S` |
| `requested_research_limit` | Ursprüngliches Forschungsbudget `R` |
| `research_limit` | Effektives Budget `min(R,T-S)` |
| `used` | Quellenkosten plus tatsächlich verbrauchte Forschungsoperationen |

Der innere Forschungsbefund zeigt weiterhin nur seine eigenen `operations_used`. Die Anwendungs-API nimmt das optionale Gesamtbudget als letzten Parameter von `research_from_grid` entgegen.

## V3-Export und Replay

`BRUCE_GRID_RESEARCH_BUNDLE_V3` mit Engine `bruce-grid-research-v3` enthält Politik, Gesamtbudget, ursprüngliches Forschungsbudget und den vollständigen bisherigen V1-/V2-Forschungsexport mit dem effektiven Budget. Das äußere Ergebnis enthält die gemeinsame Abrechnung. Ein V3-Bundle darf kein weiteres V3-Bundle einbetten.

Replay prüft die Importzulassung vor dem Grid-Quellenreplay. Das effektive Forschungsbudget wird dem vorhandenen exakten Replay als Obergrenze übergeben; eine höhere im inneren Auftrag deklarierte Operationsgrenze wird vor seiner Ausführung abgewiesen. Danach müssen deklarierte effektive Grenze, Quellenbytes, tatsächliche Nutzung und vollständige kanonische Payload übereinstimmen. Eine niedrigere, aber ebenfalls widersprüchliche innere Grenze wird beim abschließenden Vergleich abgewiesen.

Quellen, Projektion, geordnete Samplebindung und X-/Y-Geltungsbereich bleiben vollständig erhalten. Forschungs- und Z-Auswahl-Replay funktionieren ohne ursprüngliche Dateien. Digests sind weiterhin keine Urhebersignatur. Z behält das gesamte V3-Bundle als Herkunft; ihre eigene Berechnung ist nicht Teil des bereits abgeschlossenen Forschungsbudgets.

## Weitere Roadmap

Ein gemeinsamer logischer Rahmen ist jetzt für diesen konkreten Import-/Forschungsablauf vorhanden. Ein allgemeiner Aufgaben-/Strategie-/Gate-Vertrag, inkrementelle Quellenprüfung, Wiederaufnahme, dynamische Grid-Expansion, Experience-Import der Grid-Forschungswrapper und gemessene Laufzeit-/Speicherbudgets bleiben offen. Kein vollständiger M0-/M2-Abschluss.

[Benutzung](85-grid-budget-usage.md) · [Abnahme](acceptance/grid-budget-v1-2026-09-22.md)
