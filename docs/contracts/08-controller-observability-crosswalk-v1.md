# Controller Observability Crosswalk V1

**IMPLEMENTED AND VERIFIED:** This is an additive, in-process and read-only
projection of three existing controller result families. Its current evidence
is [`controller_observability_crosswalk_v1.hpp`](../../include/bruce/observability/controller_observability_crosswalk_v1.hpp),
[`controller_observability_crosswalk_v1.cpp`](../../src/observability/controller_observability_crosswalk_v1.cpp),
and the `controller_observability_crosswalk_v1_contracts` CTest target.

## Sources and limits

| Source family | Native input | What the crosswalk may say | What remains unknown |
| --- | --- | --- | --- |
| Runtime Orchestrator V1 | `RuntimeOrchestratorStatusV1` | queue/active/terminal phase and explicit controller/external cancellation reason | a status alone does not prove success, publication, or durable evidence |
| Logic-Mod Artifact DAG V6 | canonical `LogicModArtifactDagRunV6`, optionally already validated V6 supervision events | canonical acceptance and success result; diagnostic presence | publication and durable persistence are not inferred from an in-memory result |
| V5 worker inspection | `WorkerRunInspectionV1` | observed completion/cancellation shape | worker status does not prove success, publication, or durable evidence |

An out-of-range Runtime Orchestrator state maps phase, terminal and
cancellation to `UNKNOWN`. `WorkerRunInspectionV1` is historical inspection,
not a liveness source: `run_finished == false` and `INCOMPLETE` map phase,
terminal and cancellation to `UNKNOWN` rather than `ACTIVE` or
`NOT_TERMINAL`.

The optional V6 sidecar parameter accepts only events a caller has already
obtained through `validate_logic_mod_supervision_log_v6`. It is diagnostic
only: it cannot alter the canonical V6 terminal result, acceptance,
publication, or cancellation authority.

## Explicit non-features

The adapters do not open paths, validate sidecars, start workers, issue
cancellation, write records, publish artifacts, reconstruct queues, or create
a common host lifecycle. Rendering is bounded and redacted: it contains source
family, a fixed source-family token instead of every native identifier, enum
facts and no paths, URLs, PIDs, payloads, frames, signatures, credentials, raw
telemetry or sidecar contents. V6 recognizes cancellation only for its exact
documented canonical `DAG_HOST_CANCELLED` status; arbitrary status text is not
substring-classified. The renderer independently derives that token from the
source family and never serializes `ControllerObservationV1::opaque_id`, even
when an external caller manually constructs the value object.

## Verification

`controller_observability_crosswalk_v1_contracts` covers cross-family mapping,
missing evidence, V6-sidecar non-authority, and redacted rendering. It is a
focused projection test; it does not replace native V5, V6, or Orchestrator
contracts.

## CLI-Evidenzbrücke V1

**IMPLEMENTIERT UND BELEGT:** `controller-observability-v1 v5 BUNDLE` und
`controller-observability-v1 v6 RECORD` materialisieren nur die bereits
bestehenden V5- bzw. V6-Evidenztypen und übergeben sie anschließend an die
unveränderte Crosswalk-Projektion. Der V5-Pfad nutzt den bestehenden sicheren
`BUNDLE.run`-Inspector. Der V6-Pfad akzeptiert nur einen privaten, regulären,
einfach verlinkten, dem effektiven Benutzer gehörenden und begrenzten,
descriptor-relativ ohne Symlink-Folge gelesenen kanonischen Record. Fehler
werden geschlossen als `INVALID_EVIDENCE` ausgegeben; sie offenbaren weder
Pfad noch OS-Diagnostik. Keine Runtime-Orchestrator-Offline-Evidenz und keine
V6-Diagnostik-Sidecars sind eine CLI-Quelle.
