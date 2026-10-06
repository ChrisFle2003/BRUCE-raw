# Core 0.48: Laufbefunde einheitlich nachprüfen

Bruce kann gespeicherte Läufe jetzt über einen gemeinsamen Einstieg vollständig replayen und ihren Ausführungsstand einheitlich anzeigen.

```bash
./build/bruce grid-batch --paths '2,6;4,3' \
  --max-query-steps 1 --output teil.bruce
./build/bruce inspect-run teil.bruce
```

Die Ausgabe nennt `kind: grid-batch`, `lifecycle: INCOMPLETE`, `status: INCOMPLETE` und `stop_reason: QueryBudgetExceeded`. `replay_checked: true` erscheint erst, nachdem der Inhalt erfolgreich erneut berechnet und verglichen wurde.

```bash
./build/bruce grid --path '0,0' --zero-zero stop --output stopp.bruce
./build/bruce inspect-run stopp.bruce
```

Dieser Auftrag meldet `lifecycle: COMPLETED`, aber fachlich `status: STOPPED` und `stop_reason: GridSentinel`. Die einzelne Auswertung ist beendet; aus ihr folgt kein mathematischer Wert oder globaler Abbruch anderer Aufgaben.

Für die vollständigen Ergebnisse und ihre mathematischen Aussagegrenzen bleibt `bruce replay DATEI` verfügbar. Kandidat, bestätigte Samples, Widerlegung, Mehrdeutigkeit und ein bewiesener Bruchvergleich werden nicht zu einem gemeinsamen Wahrheitsstatus zusammengefasst.

## Architekturfortschritt

Ausführung und Replay liefern jetzt denselben typisierten Ergebnisvertrag für acht Laufarten. Künftige Oberflächen müssen dafür keine eigene zweite Formatzuordnung pflegen. Der bisherige Replay-Befehl verwendet den gemeinsamen Weg bereits.

Das ist die gemeinsame Ergebnisgrenze für M0/M1, keine allgemeine Phasensteuerung. Ressourcen-/Phasenbudgets, dynamische Strategieports, eine Gate-Zustandsmaschine und fortsetzbare Forschung bleiben weitere Architekturarbeit. Experience-Snapshots bleiben beim separaten Befehl `experience inspect`.

[Vertrag](100-application-result-contract.md) · [Abnahme](acceptance/application-result-v1-2026-09-22.md)
