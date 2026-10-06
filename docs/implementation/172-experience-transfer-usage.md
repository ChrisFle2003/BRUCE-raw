# Core 0.92: explizite Übergabe an den Erfahrungsimport

`ExperienceTransfer` in der Anwendungsschicht bindet Forschungsbundle, bisherige Snapshotbytes, feste Aktion `experience-add` und eigene Bytegrenzen. Die CLI stellt `experience plan-add` und `experience apply-add` bereit.

Planung führt den bestehenden Import einmal vollständig im Speicher aus. Die Ausführung liest die gebundenen Eingaben erneut, prüft deren Digests und führt den Import mitsamt Quellenreplay und Zielprüfung erneut aus. Es werden keine früheren Gateberichte als Berechtigungen übernommen. Beide Schritte verwenden den bestehenden Experience-Anwendungseinstieg; alte Snapshots bleiben unverändert und neue Ausgaben werden exklusiv angelegt.

Der Ergebnisbericht verbindet Plan-, Quell-, Basis- und Ergebnisdigest mit dem nativen Erfahrungsbericht. Die Snapshotbytes sind gegenüber dem direkten Import unverändert. Tests prüfen beide Anwendungseinstiege, leere und vorhandene Zielbasis, manipulierte Quelle und Plan, fehlende/abweichende Basis, Größenlimits, Duplikate, Überschreibschutz und erneute fachliche Ablehnung trotz neu berechneter Planprüfsumme.

Vollständige Bedienung und Formate: [CLI-Referenz](../cli/experience-transfer.md).

Die neue Grenze zählt Eingabebytes, keine universellen Rechenoperationen. Ein Plan ist keine Freigabe, kein menschlicher Bestätigungsnachweis und keine Regelaktivierung. Allgemeine Übergaben, weitere Zielaktionen, gemeinsame Operationssteuerung und M0–M2-Gesamtabnahme bleiben offen.

## Prüfnachweis am 25.09.2026

Lokal und auf Bruce-AI bestanden 116/116 Testgruppen. Der dortige Nutzerpfad `./build/bruce` meldet Core 0.92.0. Installation und TGZ-Paketprüfung bestanden auf beiden Hosts. Die CLI-Referenzprüfung bestand mit 101 ausgeführten Beispielen, 67 dokumentierten Optionen, 358 Diagnoseeinträgen und gültigen Dateilinks. Quellen, Tests und Dokumentation wurden per SCP übertragen. Alle neuen Importprüfungen verwenden temporäre Testdaten; aktive Regeln wurden nicht verändert.
