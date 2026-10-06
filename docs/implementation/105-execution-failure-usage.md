# Core 0.50: Fehlgeschlagene Prüfsitzungen untersuchen

Bruce meldet jetzt bei Fehlern innerhalb einer geprüften Sitzung zusätzlich die betroffene Phase, den bisherigen Verlauf und die ursprüngliche Ursache:

```bash
./build/bruce grid --path invalid --verification replay --output fehler.bruce
```

Dieser bewusst ungültige Auftrag endet mit Exitcode 2. Auf stderr stehen das bisherige `error`-Feld und ein neues `execution`-Objekt:

- `failed_at`: `EXECUTING` – die Berechnung konnte nicht ausgeführt werden.
- `phases`: `CREATED`, `EXECUTING`, `FAILED`.
- `cause`: `InvalidGridPath: expected x,y/x,y`.

Die Datei wird nicht angelegt. Eine bereits vorhandene Datei wird nicht verändert. Fehler in der Serialisierung oder im Replay werden entsprechend der jeweiligen Arbeitsphase zugeordnet. Fehler beim vorherigen Einlesen und späteren Dateischreiben sind weiterhin gewöhnliche CLI-Fehler außerhalb der Sitzung.

Programme müssen die Fehlerphase damit nicht mehr aus einem Text herauslesen. Bibliotheksnutzer können `ExecutionSessionError` fangen und bei Bedarf mit `std::rethrow_if_nested` die ursprüngliche Exception untersuchen.

Erfolgreiche Aufrufe, native Exportdateien und `--verification none` behalten ihr bisheriges Verhalten. Auch ein ordentlich abgebrochener Suchlauf kann eine erfolgreich geprüfte Sitzung liefern: Sein fachlicher Befund bleibt INCOMPLETE. Das ist etwas anderes als ein Fehler des Ablaufs.

## Weitere Roadmap

Die gemeinsame lineare Ausführung besitzt nun strukturierte Erfolgs- und Fehlerausgänge. Phasenübergreifende Kostensteuerung, unterbrechbare Verifikation, allgemeine Phasengraphen und persistente Sitzungsbelege bleiben offen. Der Bericht ist weder mathematischer Beweis noch Wiederaufnahmepunkt.

[Vertrag](104-execution-failure-contract.md) · [Abnahme](acceptance/execution-failure-v1-2026-09-22.md)
