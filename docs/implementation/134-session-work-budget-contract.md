# Gemeinsames Prüfpunktbudget / Core 0.65

**Fortschreibung Core 0.67:** Die Elternkopie im Fortsetzungs-Payload besitzt jetzt [zusätzliche Kontrollpunkte zwischen 64-KiB-Blöcken](140-resume-copy-blocks-usage.md). Die unten beschriebenen ursprünglichen Grenzen sind insoweit überholt; andere Kopien und Speicherreservierungen bleiben teilweise atomar.

## Zweck und Entscheidung

Die freigegebene Architekturkonsolidierung erhält einen ersten phasenübergreifenden Arbeitsrahmen. Bestehende Operationszähler besitzen verschiedene Einheiten. Deshalb wird weder ihre Summe noch eine Callbackanzahl als mathematische Operationszahl ausgegeben. Ein optionales SessionBudget.max_checkpoints zählt ausdrücklich erreichte Prüfpunkte (Modell session-checkpoints-v1).

Ein globales Zeit-/Speicherlimit wäre umfassender, benötigt aber weitere Laufzeitinstrumentierung. Nur Phasengrenzen zu zählen würde lange innere Schleifen nicht begrenzen. Gewählt wird die Anbindung aller bereits vorhandenen inneren Prüfpunkte plus der vier Sitzungsgrenzen.

## Vertrag

- execute_checked_application erhält ein optionales fünftes Argument SessionBudget. Ohne max_checkpoints bleiben Ausführung, Callbackaufrufe, Berichte und Bundlebytes unverändert.
- Mit Budget wird ein gemeinsamer, sitzungslokaler Zähler vor Ausführung, vor Serialisierung, vor Replay und vor READY belastet. Außerdem zählen native Such-/Abfragepunkte, passende Erfahrungs-/Grid-/Z-/Fortsetzungsimporte, äußere Digestblöcke, Grid-Datensätze und unterstützte Replaypunkte. Alle Auftragsvarianten sind zugelassen; Grid-Einzelauswertung und ihr Replay bleiben intern atomar.
- Ein Punkt kostet eine Einheit. Bis einschließlich Limit wird zugelassen. Der nächste Punkt wirft SessionCheckpointBudgetExceeded; der Zähler bleibt am Limit, auch bei UINT64_MAX ohne Überlauf. Limit 0 verhindert die native Ausführung.
- Der gleiche Zähler bleibt über alle drei Arbeitsphasen erhalten. Erfolgs- und Fehlerbericht enthalten Limit, zugelassene Punkte und Phasenanteile. Die Summe der Phasenanteile ist der Gesamtverbrauch.
- Budgetüberschreitung ist ein terminaler Sitzungsfehler der aktuellen Phase, kein mathematischer Teilbefund. Kein READY und kein Export. Lokale Suchgrenzen und reguläre native Abbrüche behalten ihre bisherigen Teilbefunde, sofern das Sitzungsbudget für deren Prüfung reicht.
- Vorhandene explizite Abbruchcallbacks bleiben unabhängig: zusätzliche Budgetpunkte rufen sie nicht zusätzlich auf. An gemeinsamen Punkten wird zunächst das Budget belastet. Unpassende Callbackmodi werden weiterhin abgelehnt.
- Budget und Zähler werden nicht in mathematische Bundles aufgenommen. Ausreichendes Budget liefert identische native Resultate und Bundles. Erfolgsbericht application-execution-v14; Fehlerbericht mit aktivem Budget application-execution-failure-v2, sonst bestehendes v1.
- CLI: --max-session-checkpoints N erfordert --verification replay, gilt für alle Laufbefehle und akzeptiert UInt64 einschließlich 0. Kein zusätzlicher Import-/Replaymodus erforderlich.

## Grenzen und weitere Konsolidierung

Das Modell begrenzt Kontrollpunkte, keine CPU-Zeit, Prozess-RAM oder einheitliche mathematische Operationen. Einlesevorgänge vor der Sitzung, Export nach READY, einzelne Rechenoperationen, innere Hashes, große Kopien und viele Payloadaufbauten bleiben atomar. Das Berichtsrendering ist ebenfalls nicht blockweise instrumentiert. Kein maximaler Signallatenznachweis, keine allgemeine Fortsetzung und kein vollständiger M0–M2-Abschluss.

Nächste Schritte: atomare Restarbeit gezielt instrumentieren; Speicher-/Bytebudgets separat definieren; M0–M2-Abnahmematrix und reproduzierbare Auslieferung schließen.
