# Host Lifecycle Contract Matrix V1

**IMPLEMENTIERT UND BELEGT:** Diese Matrix ist eine additive, lesende
Gegenüberstellung bereits vorhandener lokaler Controller. Sie führt keine
gemeinsame Runtime, keine gemeinsame Abbruch- oder Veröffentlichungssemantik
und keinen Recovery-Pfad ein. Ihre Aussagen sind jeweils mit Quelle und
ausführbarem Nachweis verknüpft.

## Geltungsbereich und Nicht-Zuordnung

Die Matrix vergleicht genau diese drei Implementierungen:

1. den `CommonRuntimeHostV2Batch` für die ausgewählten V5-Worker-Routen,
2. Logic-Mod Artifact DAG V6,
3. Runtime Orchestrator V1 als Controller für vorbereitete V6-Läufe.

Keiner dieser Einträge ist eine Familie von
`CommonRuntimeCompatibilityV1`. Dieses Format beschreibt ausschließlich die
vier älteren CLI-Steuerungsfamilien Session, Gate, Transfer und Management;
es ist kein allgemeines Host-Lifecycle-Protokoll. Die vorhandenen Hosts und
der Orchestrator behalten ihre nativen Verträge.

| Matrix-ID | Tatsächlicher Owner | Eingebundene Route | Primäre Quellen |
| --- | --- | --- | --- |
| `v5-batch-host-v2` | `CommonRuntimeHostV2Batch` | ausgewählte lokale `dag-scheduler-v5 execute`- und Payload-Routen | [`common_runtime_host_v2_batch.hpp`](../../include/bruce/application/common_runtime_host_v2_batch.hpp), [`dag_scheduler_v5_cli.hpp`](../../apps/dag_scheduler_v5_cli.hpp) |
| `logic-mod-artifact-dag-v6` | Logic-Mod-DAG-V6-Host | `logic-mod-artifact-dag-v6 run` | [`logic_mod_artifact_dag_v6.hpp`](../../include/bruce/mod/logic_mod_artifact_dag_v6.hpp), [`logic_mod_artifact_dag_v6.cpp`](../../src/mod/logic_mod_artifact_dag_v6.cpp) |
| `runtime-orchestrator-v1` | `RuntimeOrchestratorV1` | `runtime-orchestrator-v1 serve` | [`runtime_orchestrator_v1.hpp`](../../include/bruce/runtime/runtime_orchestrator_v1.hpp), [`runtime_orchestrator_v1.cpp`](../../src/runtime/runtime_orchestrator_v1.cpp) |

## Lebenszyklus-Crosswalk

| Aspekt | `v5-batch-host-v2` | `logic-mod-artifact-dag-v6` | `runtime-orchestrator-v1` |
| --- | --- | --- | --- |
| **Quellmarkierung** | Batch-Host-Run-ID und private append-only Worker-JSONL; der Scheduler behält DAG-Gating und Receipt-Reihenfolge. | V2-DAG-ID/Spec-Digest und die private V6-Supervision-Spur; die Sidecar-Daten gehören nicht zu DAG-ID, Record oder Replay. | opaque `orv1-*`-Run-ID, FIFO- und Ereignissequenzen sowie eine interne V6-Planinstanz; keine Planpfade im Status. |
| **Abbruch-Owner** | V5-Scheduler beobachtet den owner-only Abortmarker und fordert an seinen aktiven Batch-Handles Abbruch an. | Der V6-Host kontrolliert seine eigenen Attempts; er beobachtet eigene Limits/Cancellation und optional die private Host-Latch. | Der Orchestrator setzt ausschließlich die private V6-Latch. Er startet keinen Worker, sendet kein Signal und liest keine Frames. |
| **Abbruch-Semantik** | Nach `CANCEL_REQUESTED` keine neuen Starts/Retry; ein spätes `READY` wird nicht akzeptiert. Worker-Supervision führt Grace, Eskalation und Reaping aus. | Beobachtet V6 das Latch an seiner Admission- oder Pollinggrenze, drainiert es nativ zu `DAG_HOST_CANCELLED`; ein bereits akzeptierter Erfolg kann die spätere Latch-Race gewinnen. V6 verwaltet Sidecar, Prozessgruppe und Reaping. | Queue-Cancel ruft V6 nicht auf. Active-Cancel wird `DRAINING`; die Reservation bleibt bis zum Join des V6-Aufrufs gehalten. Ein externer V6-Cancel wird als `V6_CANCELLED_EXTERNALLY` und nicht als Controller-Cancel klassifiziert. |
| **Terminal / Veröffentlichung** | Der Batch-Host schließt den RunLog erst nach Worker-/Watchdog-Reaping. Scheduler-/Payload-Schicht entscheidet über Receipt-, Store- und Report-Veröffentlichung. | Nur ein normal abgeschlossener, hostakzeptierter DAG kann den kanonischen Erfolgsrecord bereitstellen; Cancel/Budget/Fehler liefern keinen akzeptierten Erfolg. Der V6-Planadapter publiziert diesen bestehenden Record nur bei `accepted`. | Der Controllerkern hat keine eigene V6-Veröffentlichungspolitik oder ein neues Format: Der vorbereitete Planadapter kann den bestehenden akzeptierten V6-Record mit seinem vorhandenen Publisher schreiben. Der Orchestrator veröffentlicht außerdem seinen in-memory Terminalstatus nach Thread-Join und gibt dann genau einmal Reservationen frei. |
| **Persistenz und Evidenz** | Private JSONL und bestehende V5-Bundle-/Report-/optional Store-Evidenz; der Inspector bleibt read-only. | V6-Sidecar unter `REPORT.run/control/` und der bestehende V2-formatierte Record; Replay bleibt lokal und ohne Orchestrator. | Keine Queue-Datei, kein Journal, keine Status- oder Job-Rekonstruktion. JSONL-CLI-Antworten sind Prozesskommunikation, keine Persistenz. |
| **Status / Redaktion** | Öffentliche V5-Reports bleiben routenspezifisch; der Host projiziert nur validierte Resultatmetadaten. | Öffentlicher CLI-Report/Record folgt dem V2–V6-Vertrag; private Supervision bleibt Diagnostik. | Status zeigt nur opaque IDs, Sequenzen, Zustand, Grund, Reservation und begrenzte V6-Abschlussmetadaten. Keine Pfade, URLs, PID/PGID, Credentials, Payloads, Frames, Signaturen oder Rohtelemetrie. |
| **Ressourcenbehauptung** | `--workers` begrenzt parallele Worker; RLIMIT_AS und Peak gelten pro beobachtetem Worker-PID, nicht aggregiert pro Prozessgruppe. | V4/V6-DAG-Grenzen sind je DAG: parallele Knoten, Wall, gestartete Worker und Payload; keine cgroup- oder hostweite Budgetzusage. | Fünf Hostlimits sind atomare Zulassungsreservationen, keine CPU-, RAM-, Prozess-, I/O-, Netzwerk- oder cgroup-Erzwingung. |
| **Fehlerhülle** | V5-Routen behalten ihre vorhandenen CLI-/Report- und Worker-Fehlerverträge. | V6 behält V2–V6 Resultatstatus/Record/Replay; die Latch wird nicht serialisiert. | Strikte foreground JSONL-Antworten mit `runtime-orchestrator-v1`; Eingabefehler sind redigierte Fehlerobjekte. Es existiert keine neue V5- oder V6-Fehlerhülle. |

## Zulässige Beziehungen

`runtime-orchestrator-v1` darf einen vorbereiteten V6-Plan auf einem
controller-eigenen Thread aufrufen und eine private Latch weiterreichen. V6
bleibt dabei Owner von Attempt, FD-6-Handoff, Sidecar, Remote-Restore und
Worker-Drain. Der vorbereitete V6-Planadapter behält den bestehenden
Record-Publisher und schreibt nur einen akzeptierten V6-Record. V5 Batch Host und V6 teilen weder
Queue-Objekte noch Retry- oder Abbruchobjekte.

```text
V5 scheduler ──owns──> CommonRuntimeHostV2Batch ──owns──> Worker V1 attempts

RuntimeOrchestratorV1 ──latch + join──> Logic-Mod DAG V6 ──owns──> V6 attempts
```

Dies ist eine Architekturkarte, keine gemeinsame Ausführungs-Pipeline. Es
gibt insbesondere **keine** gemeinsame Runtime, **keine** gemeinsame
Cancellation-API, **keine** gemeinsame Publication-API und **kein** gemeinsames
Recovery-/Resume-Protokoll.

## Ausführbare, fokussierte Nachweise

| Behauptung | Nachweis |
| --- | --- |
| V5 Batch-Owner, Cancel und Abschluss | `common_runtime_host_v2_batch_contracts`; [`test_common_runtime_host_v2_stage3b.py`](../../tests/integration/test_common_runtime_host_v2_stage3b.py), [`test_common_runtime_host_v2_stage3c.py`](../../tests/integration/test_common_runtime_host_v2_stage3c.py), [`test_common_runtime_host_v2_stage3d.py`](../../tests/integration/test_common_runtime_host_v2_stage3d.py) |
| V6 Latch, Drain und unveränderte V2–V6-Records | `logic_mod_artifact_dag_v[2-6]_contracts`; [`test_logic_mod_artifact_dag_v6_cli.py`](../../tests/integration/test_logic_mod_artifact_dag_v6_cli.py) |
| Orchestrator FIFO, Join, einmalige Reservation-Freigabe, Redaktion und EOF-Drain | `runtime_orchestrator_v1_contracts`; [`test_runtime_orchestrator_v1_cli.py`](../../tests/integration/test_runtime_orchestrator_v1_cli.py) |
| Diese Matrix bindet nur die genannten nativen Anker und verbietet die falsche Compatibility-Zuordnung | `host_lifecycle_contract_matrix_v1` über [`verify_host_lifecycle_contract_matrix_v1.py`](../../scripts/verify_host_lifecycle_contract_matrix_v1.py) |

Der letzte Test ist absichtlich rein lesend: Er prüft, dass die Matrix ihre
Quellanker und ihre ausdrücklichen Nicht-Garantien behält. Er ersetzt die
oben genannten Produkt-/Workflowtests nicht.

## Offene Grenzen

GPU-/Netzwerk-Ausführung, freie Code-Plugins, externer Payload-Store als
gemeinsame Runtime-Schicht, globale Recovery und eine übergreifende
Orchestrierung bleiben **DOKUMENTIERT / KONZEPT**. Diese Matrix ist kein
Migrationsvertrag für sie.
