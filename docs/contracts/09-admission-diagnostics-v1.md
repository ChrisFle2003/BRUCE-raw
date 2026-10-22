# Admission Diagnostics V1

**IMPLEMENTIERT UND BELEGT:** Admission Diagnostics V1 ist eine additive,
rein lesende Projektion eines bereits akzeptierten
`RuntimeOrchestratorStatusV1`. Sie erklärt ausschließlich die volatile
lokale Zulassungslage eines Runtime-Orchestrator-V1-Laufs. Sie ist kein
V6-Ergebnis, kein Record, kein Persistenz- oder Recoveryformat und keine
globale Ressourcenmessung.

## Öffentliche JSONL-Abfrage

Der bestehende foreground-Host akzeptiert zusätzlich genau diese kanonische
Requestform:

```json
{"action":"admission-status","run_id":"orv1-1"}
```

Die Aktion liest ausschließlich den vorhandenen In-Memory-Status über
`RuntimeOrchestratorV1::status`. Sie nimmt keine Pfade, Specs, Stores,
Records, Reports, Paket- oder Truststore-Daten an und öffnet keine Datei.
Eine unbekannte, sonst gültige `run_id` liefert geschlossen `RUN_NOT_FOUND`.
Eine ungültige Kennung oder zusätzliche Felder liefern `INVALID_COMMAND`.

Die erfolgreiche Antwort enthält `admission_status` mit festem Schema
`admission-diagnostics-v1`, fester Quelle `runtime-orchestrator-v1`,
Zulassungszustand, Zulassungsgrund, vorhandenen Enqueue-/Submit-/Start-/
Finish-Sequenzen und der deklarierten Reservation. Sie enthält keine native
`run_id`, keine DAG-ID, keine V6-Statusdaten, keine Sidecar-Referenz, keine
Pfade, URLs, PIDs, Payloads, Frames, Signaturen oder Credentials.

## Geschlossene Abbildung

| Native, konsistente Kombination | Öffentlicher Zustand | Grund |
|---|---|---|
| `QUEUED` / `NONE` | `QUEUED_NOT_ADMITTED` | `NONE` |
| `ADMITTED` / `NONE` | `ADMISSION_RESERVED` | `NONE` |
| `STARTING` oder `RUNNING` / `NONE`; `DRAINING` / `CANCELLED_ACTIVE` | `ACTIVE` | `NONE` |
| `BLOCKED` / `RESERVATION_EXCEEDS_HOST` | `BLOCKED_HOST_RESERVATION` | `RESERVATION_EXCEEDS_HOST` |
| `CANCELLED` / `CANCELLED_QUEUED` | `CANCELLED_BEFORE_ADMISSION` | `NONE` |
| erfolgreich, fehlgeschlagen oder nach aktiver Zulassung abgebrochen | `TERMINAL_AFTER_ADMISSION` | `NONE` |

Jede unbekannte Enum-Ausprägung und jede inkonsistente State-/Reason-Kombination
ergibt geschlossen `UNKNOWN`/`UNKNOWN`. Die Schicht leitet keinen Wartgrund,
keine Ausführungsursache und keinen Erfolg aus unvollständiger Evidenz ab.

## Grenzen und Belege

Die Projektion startet keine Worker, sendet keinen Cancel, reserviert oder
gibt keine Ressourcen frei, publiziert keinen Record und rekonstruiert nach
Hostende keine Queue. Eine `BLOCKED_HOST_RESERVATION`-Diagnose bezieht sich
nur auf die deklarierte Reservation gegenüber den expliziten Hostlimits; sie
ist keine CPU-, RAM-, Prozess-, I/O- oder Netzwerkbudgetaussage.

Belege: [Mapper und Renderer](../../src/runtime/admission_diagnostics_v1.cpp),
[öffentliche Modelle](../../include/bruce/runtime/admission_diagnostics_v1.hpp),
[Unit-Verträge](../../tests/unit/admission_diagnostics_v1_tests.cpp) und
[JSONL-Workflow](../../tests/integration/test_runtime_orchestrator_v1_cli.py).
