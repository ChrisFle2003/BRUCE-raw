# Core 0.77: steuerbare Zulassungsprüfung

Die C++-Funktionen `evaluate_application_gates(bundle, checkpoint)` und `audit_experience_source(source, checkpoint)` akzeptieren nun einen optionalen `ReplayCheckpoint`. Ohne Callback bleibt das Verhalten erhalten.

Die Gateprüfung reicht denselben Beobachter durch den allgemeinen Replay und den bestehenden Erfahrungsquellenaudit. Dadurch kann der Aufrufer ein gemeinsames Prüfpunktkonto führen oder durch eine Exception abbrechen. Das ist kein neuer mathematischer Operationszähler.

Callback-Exceptions werden an den vorhandenen Schutzgrenzen gekapselt und unverändert zum Aufrufer zurückgegeben. Insbesondere darf eine vom Callback geworfene `Error` nicht als reguläre Ablehnung der Erfahrungsquelle im Bericht erscheinen. Bei Abbruch wird kein Teilbericht zurückgegeben. Echte Quellenfehler bleiben dagegen reguläre Ablehnungsgründe.

Die Prüfpunkte entsprechen der vorhandenen Replay-/Auditunterstützung. Reine Grid-Bundles unterstützen Replaycallbacks weiterhin nicht und melden bei gesetztem Callback `ReplayCheckpointsUnsupported`. Blockierende Restarbeit und maximale Signallatenz sind nicht garantiert. Ohne Callback bleibt ihre Inspektion möglich.

Die CLI `gate-run` erhält keine neuen Optionen. `application-gates-v1`, Bundlebytes und die fachlichen Zulassungsregeln bleiben erhalten. `render_application_gates` bleibt eine reine Darstellung und prüft nichts.

Die Tests vergleichen beobachtete mit unbeobachteten Ergebnissen, prüfen den ersten und letzten Prüfpunkt in Audit und Gesamtprüfung und kontrollieren die Weitergabe von `Error` und `NumericLimit`. Der letzte Gateprüfpunkt liegt im Quellenaudit und sichert damit besonders die Grenze zwischen Abbruch und fachlicher Ablehnung ab.

Gemeinsame CLI-Steuerung, weitere Payloadunterstützung, allgemeine Gatezustandsmaschinen und automatische Weiterleitung bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und auf Bruce-AI bestanden jeweils 108/108 Testgruppen. Remote wurden Release-Build, Installation und TGZ-Paket für 0.77.0 erfolgreich geprüft. Die CLI-Prüfung bestand lokal und remote mit 89 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
