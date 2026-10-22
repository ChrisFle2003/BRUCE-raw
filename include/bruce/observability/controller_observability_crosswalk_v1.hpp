#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"
#include "bruce/mod/logic_mod_supervision_log_v6.hpp"
#include "bruce/runtime/runtime_orchestrator_v1.hpp"
#include "bruce/worker/worker_run_inspector_v1.hpp"

#include <optional>
#include <string>
#include <vector>

namespace bruce {

// A bounded, read-only comparison vocabulary.  UNKNOWN is deliberate: this
// layer never turns a controller status or diagnostic into an authority claim.
enum class ControllerObservationSourceV1 {
    RUNTIME_ORCHESTRATOR_V1,
    LOGIC_MOD_ARTIFACT_DAG_V6,
    V5_WORKER_RUN_INSPECTION_V1,
};
enum class ControllerObservationPhaseV1 { QUEUED, ACTIVE, TERMINAL, UNKNOWN };
enum class ControllerObservationTerminalV1 {
    NOT_TERMINAL, SUCCEEDED, FAILED, CANCELLED, COMPLETED_UNVERIFIED, UNKNOWN,
};
enum class ControllerObservationCancellationV1 {
    NONE, ORCHESTRATOR, V6_HOST, V5_OWNER, EXTERNAL, UNKNOWN,
};
enum class ControllerObservationFactV1 { YES, NO, UNKNOWN };

struct ControllerObservationV1 {
    ControllerObservationSourceV1 source{};
    // Adapter-produced values contain a fixed source-family token, never a
    // copy, hash or truncation of a native identifier.  The public renderer
    // independently derives its token from source and never serializes this
    // field, so manually constructed values cannot bypass redaction either.
    std::string opaque_id;
    ControllerObservationPhaseV1 phase{ControllerObservationPhaseV1::UNKNOWN};
    ControllerObservationTerminalV1 terminal{ControllerObservationTerminalV1::UNKNOWN};
    ControllerObservationCancellationV1 cancellation{ControllerObservationCancellationV1::UNKNOWN};
    // These claims intentionally have independent provenance.  In particular,
    // a sidecar is diagnostics only and a status string is not success proof.
    ControllerObservationFactV1 success{ControllerObservationFactV1::UNKNOWN};
    ControllerObservationFactV1 publication{ControllerObservationFactV1::UNKNOWN};
    ControllerObservationFactV1 durable_evidence{ControllerObservationFactV1::UNKNOWN};
    ControllerObservationFactV1 diagnostic{ControllerObservationFactV1::UNKNOWN};
};

// Inputs are already-materialized native projections.  These adapters do not
// open files, invoke workers, mutate cancellation state, publish records or
// reconstruct queues.  Optional V6 sidecar events must have been produced by
// validate_logic_mod_supervision_log_v6 by the caller; even then they remain
// diagnostics and cannot change canonical V6 authority or success.
[[nodiscard]] ControllerObservationV1 observe_runtime_orchestrator_v1(
    const RuntimeOrchestratorStatusV1& status);
[[nodiscard]] ControllerObservationV1 observe_logic_mod_artifact_dag_v6(
    const LogicModArtifactDagRunV6& result,
    std::optional<std::vector<mod::SupervisionEventV6>> validated_sidecar = std::nullopt);
[[nodiscard]] ControllerObservationV1 observe_v5_worker_run_v1(
    const worker_v1::WorkerRunInspectionV1& inspection);

// Bounded redacted diagnostic rendering: only source family, fixed token and
// enum facts. It intentionally has no path, URL, process, payload, frame,
// signature, credential or raw-sidecar field.
[[nodiscard]] std::string render_controller_observation_v1(const ControllerObservationV1&);

} // namespace bruce
