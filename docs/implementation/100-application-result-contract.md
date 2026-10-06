# Gemeinsamer Ergebnis- und Replay-Vertrag – Core 0.48

## Entscheidung und Umsetzung

Der gemeinsame Auftragseinstieg aus Core 0.47 erhält einen typisierten Replay-Einstieg. `replay_application` liefert dieselbe `ApplicationRun`-Variante wie die Ausführung. Die bisherige bekannte Headerzuordnung und alle Fachdecoder bleiben unverändert. `replay_output` nutzt nun diesen Einstieg und anschließend den gemeinsamen Renderer.

`inspect_application` führt zuerst den vollständigen Replay durch und erzeugt erst nach dessen Erfolg eine Zusammenfassung. Die CLI stellt sie als `bruce inspect-run DATEI` bereit. Es handelt sich nicht um das bloße Lesen behaupteter JSON-Felder.

## Ergebnisfelder

| Feld | Bedeutung |
| --- | --- |
| `contract` | `application-result-v1` |
| `kind` | Einer der acht typisierten Laufarten aus `application-task-v1` |
| `replay_checked` | Ausgabe nur nach erfolgreich abgeschlossenem Replay; dann `true` |
| `lifecycle` | `INCOMPLETE` bei nativem Teilbefund, sonst `COMPLETED` |
| `status` | Unveränderter fachlicher Ergebnisstatus |
| `stop_reason` | Unveränderter fachlicher Stopgrund |
| `scope` | Hinweis, dass die Zusammenfassung nur den Ausführungsstand beschreibt |

Ein Grid-Sentinel beendet die lokale Auswertung: Lifecycle `COMPLETED`, fachlich `STOPPED / GridSentinel`. Er bedeutet keinen Abbruch sämtlicher anderer Abfragen. Grid-Fortsetzungen übernehmen den Stand ihres enthaltenen Stapels, Grid-Forschung den Stand ihrer Forschung. Z-Auswahl übernimmt ihren eigenen Ergebnisstatus.

`COMPLETED` bedeutet ausschließlich, dass der native Auftrag keinen `INCOMPLETE`-Teilbefund meldet. Kandidaten, Widerlegungen, Mehrdeutigkeit und unentschiedene fachliche Vergleiche bleiben unterscheidbar. Mathematische Gültigkeitsbereiche und Beweisangaben werden weder vereinheitlicht noch aufgewertet; dafür ist weiterhin das vollständige native Ergebnis zuständig.

## Kompatibilität und Grenzen

Die Zusammenfassung ist eine neue abgeleitete Anzeige, kein neues Run-Bundleformat. Bereits bestehende JSON-Ausgaben und Exporte bleiben unverändert. Unbekannte Versionen, Manipulationen und Replayabweichungen bleiben Fehler ohne Zusammenfassung. Der neue Einstieg unterstützt nur die acht vorhandenen Laufarten; Experience-Snapshots werden weiterhin mit `experience inspect` geprüft.

Die Inspektion kann so viel Arbeit benötigen wie ein vollständiger Replay und ist keine schnelle Metadatenabfrage. Sie wird nicht durch ein neues Inspektionsbudget begrenzt. Die bisherigen Größen- und Fachgrenzen gelten weiterhin. Es gibt keine neue Phasen-Zustandsmaschine, Hintergrundausführung, automatische Gatefreigabe oder Fortsetzung der allgemeinen Formelforschung.

[Benutzung](101-application-result-usage.md) · [Abnahme](acceptance/application-result-v1-2026-09-22.md)
