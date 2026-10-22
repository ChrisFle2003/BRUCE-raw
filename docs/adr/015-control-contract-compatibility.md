# ADR-015: Kompatible Steuerungsverträge erhalten und prüfen

## Status

Akzeptiert für den lokalen Core-0.102-Arbeitsstand. Diese Entscheidung
versioniert und prüft vorhandenes Verhalten; sie führt keine neue gemeinsame
Runtime ein.

## Kontext

BRUCE besitzt getrennte Sitzungs-, Gate-, Transfer- und read-only
Management-Steuerungen. Sie zählen
verschiedene Einheiten, besitzen unterschiedliche Kontrollstellen und liefern
bei einem Fehler nicht dieselbe JSON-Hülle. Die Unterschiede sind im
[Steuerungs- und Veröffentlichungsvertrag](../contracts/06-control-and-publication.md)
dokumentiert. Eine Vereinheitlichung ohne einen expliziten Migrationsvertrag
könnte verlässliche Fehlerprioritäten, Exportgrenzen oder gespeicherte
Automationen verändern.

`experience revoke` und `experience replace` besitzen mit
`--max-transfer-checkpoints` beziehungsweise `--transfer-cancellation` einen
opt-in Transferweg. Ohne diese Optionen bleiben sie direkte native
Verwaltungsaufrufe mit bisherigem Bericht und unveränderten Snapshotbytes.
`experience inspect` hat keinen Transfer- oder Sitzungscontroller; mit den
optionalen Management-Parametern nutzt es den eigenen read-only
Management-Checkpoint-Controller.
Die vorhandene opt-in-Erweiterung rechtfertigt nicht, weitere Verwaltungswege
nachträglich als Session- oder Transferweg zu behandeln.

## Entscheidung

Die vier vorhandenen Controllerfamilien werden als voneinander getrennte,
öffentliche Verträge erhalten:

| Familie | Vertrag | Fehlerhülle |
| --- | --- | --- |
| Sitzung | `session-checkpoints-v1` | `application-execution-failure-v1`, `-v2` oder `-v3` entsprechend aktivem Sitzungs-/Serialisierungsbudget |
| Gate | Gateinspektion mit eigenem Sitzungsprüfpunktkonto | einfaches `{"error":"…"}` |
| Transfer | `transfer-checkpoints-v1` | einfaches `{"error":"…"}` |
| Management | `management-checkpoints-v1` bei read-only Inspektion | einfaches `{"error":"…"}` |

Die Vertragsmatrix in `06-control-and-publication.md` bestimmt für jede
Familie Budgeteinheit, Kontrollstellen, Signal-/Budgetreihenfolge,
Fehlerhülle, Veröffentlichungsgrenze und ausführbare Nachweise.
`control_contract_matrix` ergänzt die fokussierten bestehenden CTest-Fälle.
Es prüft echte CLI-Aufrufe, die unterschiedliche Fehlerhüllen bei einem
Nullbudget, fehlende Ausgabe nach einem Abbruch und einen replaybaren nativen
`INCOMPLETE`-Befund zeigen.

## Unveränderte Semantik

- Optionen, Exit-Codes, Fehlerprioritäten, Bundleformate, Ergebnisfelder und
  fachliche Mathematik bleiben unverändert.
- Sitzungsphasen behalten ihre vorhandene pfadabhängige Reihenfolge. Bei
  Replay-/Serialisierungspunkten können Zählerverbrauch und Signalprüfung
  anders geordnet sein als bei Gate- oder Transferpunkten.
- Gateinspektion prüft ein Signal vor dem Zählerverbrauch. Transferpunkte
  prüfen Signal, Budget und danach den Zähler in dieser Reihenfolge.
- Dateilesen, finale Exporte, atomare Fachoperationen, Prozesszeit und
  Prozessspeicher werden nicht zu einer gemeinsamen globalen Einheit erklärt.
- Ein reguläres fachliches `INCOMPLETE` bleibt ein erfolgreicher,
  gegebenenfalls replaybarer Befund; es wird nicht als Steuerungsfehler
  umgedeutet.

## Konsequenzen

Neue Steuerungsarbeit muss zuerst eine eigene Versions- und
Kompatibilitätsentscheidung treffen. Der bestehende opt-in Transferweg für
`experience revoke/replace` darf weder die Aufrufe ohne Transferoptionen
verändern noch automatisch auf weitere Verwaltungswege übertragen werden.

Der Vorteil ist eine überprüfbare Migrationsgrundlage. Der Preis ist, dass
die vier Fehlerpfade vorerst sichtbar unterschiedlich bleiben. Das ist
bewusst: Lesbare Kompatibilität ist sicherer als eine stillschweigende
Normalisierung.

## Nachweise

- CTest `control_contract_matrix` für die öffentlichen
  Nullbudget-/Ausgabegrenzen und den nativen Teilbefund.
- CTest `session_work_budget_workflows` und `execution_session_states` für
  Sitzungsbudget und die deterministische Budget-vor-Signal-Priorität an
  Serialisierungs- und Replaypunkten.
- CTest `result_assessment_workflows` für Gatebudget und dessen
  Signal-/Budgetkollision.
- CTest `compatibility_transfer_workflows` und
  `experience_transfer_workflows` für Transferbudget und Export sowie
  `compatibility_contracts` und `experience_contracts` für die
  Signal-vor-Budget-Priorität.
- CTest `experience_replace_workflows` für direkte und kontrollierte
  Ersetzung, identische Snapshotbytes, Budgetabbruch ohne Veröffentlichung
  und Schutz eines bestehenden Ziels.
- [Nutzungseintrag](../implementation/184-control-contract-matrix-usage.md)
  hält den lokal beobachteten Prüfstand fest.
