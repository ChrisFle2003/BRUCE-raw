# Core 0.73: gemeinsamer Ergebnisvertrag

## Zweck

Alle acht Anwendungsergebnisse erhalten eine gemeinsame typisierte Bewertung. Verbraucher müssen dadurch den Zusammenhang zwischen fachlichem Status, Vollständigkeit und Aussagebereich nicht erneut aus acht nativen JSON-Formaten ableiten. Die bisherige Ergebnisinspektion und der Sitzungscontroller verwenden denselben Auswerter.

## API und Vertrauensgrenze

`assess_application(const ApplicationRun&)` liefert `ApplicationAssessment`: `kind`, `status`, `stop_reason`, `complete`, den Enum `ClaimScope` und `proof_established`. Die Funktion klassifiziert einen vorliegenden Speicherbefund. Sie führt keine eigene Herkunfts-/Manipulationsprüfung durch und ist keine Zulassungsprüfung für beliebig konstruierte C++-Objekte.

`assess_application_bundle` rekonstruiert dagegen zuerst das gesamte übergebene Bundle durch den bestehenden Replay und bewertet erst danach. Diese Vertrauensgrenze wird durch die CLI genutzt:

```bash
./build/bruce assess-run lauf.bruce
```

Genau ein Dateiparameter; keine zusätzlichen Optionen. Datei-/Replayfehler erscheinen mit Exit 2. Die maximale Eingabegröße bleibt 32 MiB.

```json
{
  "contract": "application-assessment-v1",
  "kind": "fraction-comparison",
  "replay_checked": true,
  "status": "PROVEN_DIFFERENT",
  "stop_reason": "Completed",
  "result_complete": true,
  "claim_scope": "supplied-polynomial-fractions-over-Q",
  "proof_established": true
}
```

## Aussagebereiche

| Ergebnisarten | `claim_scope` | Beweiskennzeichen |
| --- | --- | --- |
| Int64-, exakte und Grid-Forschung | `declared-family-sample-evidence` | Immer false, auch bei `VERIFIED_ON_SAMPLES` |
| Grid, Grid-Batch, Grid-Resume | `grid-evaluation` | Immer false; keine Promotion zur Formelwahrheit |
| Bruchvergleich | `supplied-polynomial-fractions-over-Q` | Nur bei abgeschlossenem `PROVEN_EQUIVALENT` oder `PROVEN_DIFFERENT` und Stopgrund `Completed` true |
| Kompatibilität | `numeric-rule-selection` | Immer false; numerische Auswahl ist kein mathematischer Beweis |

Das Beweiskennzeichen ist ausschließlich zusammen mit dem Aussagebereich und Status zu interpretieren. `PROVEN_DIFFERENT` bestätigt einen Unterschied, keine Gleichheit. Ein `UNDECIDED`-Vergleich erhält keinen Beweisstatus. `result_complete` übernimmt bewusst den bisherigen Lebenszyklusvertrag: false genau bei `INCOMPLETE`. Ein vollständig abgearbeiteter, aber unentschiedener Vergleich oder ein gestoppter Grid-Zweig kann deshalb vollständig sein.

## Kompatibilität und Abnahme

Native Ergebnisse, Bundlebytes, Replay und der vorhandene `inspect-run`-Vertrag bleiben erhalten. Sitzungs-JSON verwendet weiterhin dieselben Versionen und dieselbe Vollständigkeitssemantik. Das neue Assessment-JSON besitzt einen eigenen Vertrag, keine neue Bundlefamilie.

`application_task_adapters` prüft die Anbindung aller acht Ergebnisvarianten. `result_assessment_workflows` prüft Samplegrenzen, Teilbefunde, gestoppte Zweige, begrenzte Bruchbeweise, Manipulationsablehnung und historische Fixtures. Vor Implementierung scheiterten die neuen CLI-Tests erwartungsgemäß mit `UnknownCommand: assess-run`.

## Ausgeführte Prüfung am 25.09.2026

- Lokaler Debug-Build: 108/108 CTest-Gruppen bestanden.
- Per SCP an Bruce-AI übertragen; dort Release-Build und 108/108 CTest-Gruppen bestanden.
- Remote Installations- und TGZ-Paketprüfung für Version 0.73.0 bestanden, einschließlich historischer Fixture-Wiederherstellung.
- CLI-Dokumentationsprüfung lokal und remote: 87 erfolgreiche Beispielaufrufe, 63 dokumentierte Optionen, 321 Diagnosen, interne Links OK.

Diese Prüfungen belegen den untersuchten Linux-Stand, keine allgemeine Architektur- oder plattformübergreifende Freigabe.

## Weitere Architektur

Dieser Schritt ist die gemeinsame Ergebnisbewertung, noch keine allgemeine Gate-Zustandsmaschine. Zulassungsregeln für Auswahl/Experience, Technik-/Ast-/Run-Geltungsbereiche, externe Evidenz, Strategieerweiterungen und Phase-DAG bleiben eigene Pakete. Es wird keine automatische Lernpromotion oder Veröffentlichungsfreigabe eingeführt. Leistungsoptimierung wird vorerst zurückgestellt.
