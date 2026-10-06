# Core 0.76: strukturierter Zulassungsbericht

`evaluate_application_gates(bundle)` liefert nach erfolgreichem Replay einen `ApplicationGateReport`. Er enthält eigene Strings für `kind` und Ablehnungsgründe, `replay_checked`, `applicable`, die Entscheidungen `selection` und `experience_source` sowie die weiterhin gesperrten Befugnisse `learn_promotion`, `publish`, `general_proof`.

`ApplicationGateDecision` besteht aus `allowed` und `reason`. Die fachlichen Prüfungen bleiben dieselben wie in [Core 0.75](155-run-gates-usage.md). Die Gründe sind weiterhin stabile Textcodes der bestehenden Prüfer; dieses Paket führt keinen gemeinsamen Enum für sämtliche Importfehler ein.

`render_application_gates(report)` formatiert ausschließlich die übergebenen Felder. Es prüft weder Herkunft noch Berechtigungen. Ein manuell erstellter oder veränderter Bericht ist daher kein Zulassungsnachweis. Der Standardbericht ist nicht replaygeprüft und lässt nichts zu. Verbraucher müssen den Bericht aus dem Prüfer beziehen und die vorhandenen Import-/Projektionsgrenzen weiterhin beachten.

`inspect_application_gates` verbindet Prüfung und Ausgabe. Die CLI `gate-run` behält `application-gates-v1`, ihre Feldreihenfolge und Semantik. Es gibt keine neuen CLI-Optionen und keine Änderung an gespeicherten Bundles.

Die Adaptertests prüfen alle acht Anwendungstypen, Anwendbarkeit, gesperrte Befugnisse, Ausgabeparität und Manipulationsablehnung. Die bestehenden CLI-Tests prüfen weiterhin Auswahl und Erfahrungsquelle gegen den tatsächlichen Import.

Damit können C++-Verbraucher auf die Zulassungsbewertung zugreifen, ohne JSON zu parsen. Automatische Weiterleitung, allgemeine Gatezustandsmaschinen und zusätzliche Geltungsbereiche bleiben offen. Optimierung bleibt nachrangig.

## Abnahme am 25.09.2026

Lokal und auf Bruce-AI bestanden jeweils 108/108 Testgruppen. Remote wurden Release-Build, Installation und TGZ-Paket für 0.76.0 geprüft. Die CLI-Prüfung bestand lokal und remote mit 89 Beispielaufrufen und gültigen internen Links. Quellen und Dokumentation wurden per SCP übertragen.
