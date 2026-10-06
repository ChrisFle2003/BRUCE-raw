# Steuerung und Veröffentlichung – Bestandsvertrag Core 0.102

Dieser Vertrag konsolidiert implementiertes Verhalten. Seit Core 0.102 liegt Dateiarbeit im gemeinsamen lokalen Adapter; die bestehenden Kontrollstellen bleiben erhalten. Er führt keinen neuen Controller und keine neuen CLI-Optionen ein. [Bedienung und Abnahme](../cli/control-overview.md), [Blueprint](../architecture/03-blueprint-consolidation.md).

## Gemeinsame Bedeutung

1. Ein Prüfpunktbudget zählt zugelassene Kontrollstellen, keine universellen mathematischen Operationen, Laufzeit oder Prozessspeicher. Fachliche Suchbudgets und Bytegrenzen behalten ihre eigenen Einheiten.
2. Jedes CLI-Kommando besitzt sein eigenes Konto. Pläne speichern fachliche Grenzen, aber weder verbrauchte Transferpunkte noch ein auf spätere Aufrufe übertragbares Konto.
3. Exit 0 bezeichnet erfolgreiche Verarbeitung. `INCOMPLETE`, Mehrdeutigkeit und Stichprobenbestätigung sind keine allgemeine mathematische Gewissheit. Native Budgeterschöpfung darf ein replaybares Teilergebnis liefern.
4. Ein ausgelöster Steuerungsfehler beendet den betreffenden Aufruf mit Exit 2. Es entsteht kein neues fachliches Teilbericht-JSON auf stdout. Fehlerdiagnosen unterscheiden sich nach Einstieg, siehe unten.
5. Exklusive Exporte überschreiben kein Ziel. Kontrollierter Transferexport prüft bis vor Veröffentlichung; danach gibt es keine weitere Transferprüfung. Ein erkannter Callbackabbruch vor Veröffentlichung räumt die temporäre Datei auf, soweit das Dateisystem dies zulässt.
6. Digests binden Inhalt, bestätigen keine menschliche Identität. Ein Gatebericht oder Transferplan aktiviert keine Regeln. Der [menschliche Freigabevertrag](05-human-rule-approval.md) bleibt maßgeblich.

## Tatsächliche Steuerungswege

| Weg | Konto und Voraussetzung | Kontrollierter Bereich | Bewusst außerhalb |
| --- | --- | --- | --- |
| Acht `ApplicationTask`-Varianten über `execute_cli` (Forschung, Gridvarianten, Vergleich, direkte Kompatibilität) | `--verification replay`, optional `--max-session-checkpoints`; Modell `session-checkpoints-v1` mit EXECUTING/SERIALIZING/REPLAYING | Angebundene Ausführungs-, Pack- und Replaypunkte; zusätzliche Signaloptionen pfadabhängig | Vorgelagertes CLI-Dateilesen, endgültiger Dateiexport, atomare Facharbeit; kein allgemeiner Task-DAG |
| `gate-run` | `--max-session-checkpoints`, optional `--cancellation checkpoints`; alle Punkte in REPLAYING | Replay und Gates einschließlich Quellenaudits | Dateieinlesen; kein Export; Gate ist keine Zielzulassung |
| `compatibility-plan`, `compatibility-apply`, `experience plan-add`, `experience apply-add`, `experience add`, `experience revoke`, `experience replace` mit Transferoption | `--max-transfer-checkpoints`, `--transfer-cancellation none|checkpoints`; Modell `transfer-checkpoints-v1` | Direkte Dateiimporte, Regelspeicher, Probelauf/Ausführung, angebundene Fachprüfungen, Exportvorbereitung | Einzelne Systemaufrufe, Textkopien/-decodierungen, nicht instrumentierte Fachabschnitte; keine Phasenaufteilung |
| `experience revoke/replace/inspect` sowie direktes `experience add` ohne Transferoption | Bisherige fachliche Größen-/Auditgrenzen | Native Prüfungen ohne eigenes CLI-Prüfpunktkonto | Kein Transfer-/Sitzungscontroller für diese Verwaltungsaufrufe |
| Regelverwaltung, `replay`, `inspect-run`, `assess-run`, einfache Rechen-/Adressbefehle | Bisherige lokale, Format- und Fachgrenzen | Jeweiliger nativer Vertrag | Kein neuer übergreifender CLI-Steuerungsvertrag |

Quellen: [CLI](../../apps/bruce_cli.cpp), [Session](../../src/application/execution_session.cpp), [Sessionzähler](../../src/application/session_budget.cpp), [Transfercontroller](../../include/bruce/application/transfer_control.hpp), [Dateiadapter](../../src/io/local_files.cpp).

## Kompatibilitätsmatrix der Steuerungswege

Die Wege sind absichtlich nicht auf eine universelle Runtime normalisiert.
[ADR-015](../adr/015-control-contract-compatibility.md) hält diese Entscheidung
fest; `control_contract_matrix` prüft die unten sichtbaren Grenzen mit echten
temporären CLI-Eingaben. Die Matrix beschreibt vorhandene Verträge, keine
Zusagen für neue Verwaltungswege.

| Familie | Version und Einheit | Signal-/Budgetreihenfolge | Öffentliche Fehlerhülle | Veröffentlichung und Testnachweis |
| --- | --- | --- | --- | --- |
| Sitzung | `session-checkpoints-v1`; zusätzlich getrennte Serialisierungsbytegrenze | pfadabhängig; an Replay-/Serialisierungspunkten kann der Zähler vor der Signalprüfung verbraucht werden und ein erschöpftes Budget zuerst melden | `application-execution-failure-v1` ohne aktives Budget, `-v2` bei Sitzungsprüfpunkten, `-v3` bei Serialisierungsbytes | Vorbereitendes Lesen und endgültiger Export liegen außerhalb; `control_contract_matrix`, `control_contract_matrix_unit`, `session_work_budget_workflows`, `execution_session_states` |
| Gate | lokales Gateinspektionskonto mit `--max-session-checkpoints` | Signal vor Zählerverbrauch | einfaches `{"error":"…"}` | Dateilesen liegt außerhalb; kein Dateiexport; `control_contract_matrix`, `result_assessment_workflows` |
| Transfer | `transfer-checkpoints-v1` | Signal, Budgetprüfung, Zählererhöhung | einfaches `{"error":"…"}` | Kontrollierter Export endet vor Veröffentlichung; Tempdateien werden bei erkanntem Abbruch bestmöglich entfernt; `control_contract_matrix`, `compatibility_transfer_workflows`, `experience_transfer_workflows`, `compatibility_contracts`, `experience_contracts` |

Die Kontrollstellen messen keine universellen mathematischen Operationen,
Prozesszeit oder Prozessspeicher. Nicht instrumentierte atomare Facharbeit
bleibt außerhalb dieser Zahlen. Ein beobachteter Verbrauch aus einem Aufruf
ist keine Übertragung oder Versionserwartung für einen anderen Aufruf.

## Abbruch, Budget und Fehler bleiben unterscheidbar

| Fall | Aktuelles Verhalten |
| --- | --- |
| Transferpunkt | Zuerst Signalprädikat (`TransferCancelled`), danach Budgetprüfung (`TransferCheckpointBudgetExceeded`), danach Zählererhöhung. Fehler-JSON auf stderr. |
| Session-Replay-/Serialisierungspunkt mit Signalprüfung | Zuerst Zählerverbrauch, dann Signalprüfung. Bei gleichzeitiger Erschöpfung kann das Budget zuerst melden. Andere Ausführungspfade besitzen eigene Abbruchsemantik. Kein globaler Vorrang wird behauptet. |
| `gate-run`-Punkt | Signal vor Zählerverbrauch; `GateInspectionCancelled` oder Sessionbudgetfehler im einfachen Fehler-JSON. |
| Native Z-Regelprüfung | Native Check-/Tracegrenzen vor Regelcallback; bei Erschöpfung regulärer `INCOMPLETE`-Befund. Ein danach zulässiger Regelcallback kann als Transferfehler abbrechen. |
| Sessionfehler | `execution`-Fehlerhülle mit Phase, Ursache und gegebenenfalls Verbrauch; kein fachliches `result`. Ein nachfolgender CLI-Exportfehler liegt außerhalb der Session und verwendet das einfache Fehler-JSON. |
| Transfer-/sonstiger CLI-Fehler | `{"error":"…"}`; kein verlässlicher strukturierter Teilverbrauch bei Fehlern. |

Diese Unterschiede sind Bestand, keine versehentlich vereinheitlichte Zusage. Eine spätere gemeinsame Runtime muss Priorität, Fehlerhülle und Dateigrenzen ausdrücklich migrieren und gegen bestehende Aufrufe prüfen.

## Nachweis und verbleibende Arbeit

[Reproduzierbare Abnahme](../implementation/182-control-consolidation-usage.md) verbindet einen unabhängigen Forschungsbefund mit zwei separaten Zielen. Z-Auswahl ist keine Erfahrungsquelle. Erfahrung übernimmt die ursprüngliche geeignete Quelle und führt deren Audit erneut aus.

Die Abnahme belegt Quellen-/Zielbindung, Replay, Budgetgrenzen und Veröffentlichung für dieses konkrete Profil. Spezielle Signal-, Regelhistorien- und Blocktests bleiben Teil der vollständigen Suite. Keine garantierte Signallatenz, keine universellen Zeit-/RAM-Grenzen, keine automatische Regelpromotion, keine Gesamtfreigabe M0–M2 oder des Blueprints.
