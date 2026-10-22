#include "bruce/observability/controller_observability_crosswalk_v1.hpp"

namespace bruce {
namespace {
template <class T> std::string numeric(T value) { return std::to_string(static_cast<unsigned>(value)); }

std::string redacted_id(ControllerObservationSourceV1 source) {
    switch (source) {
    case ControllerObservationSourceV1::RUNTIME_ORCHESTRATOR_V1: return "redacted-runtime-orchestrator-v1";
    case ControllerObservationSourceV1::LOGIC_MOD_ARTIFACT_DAG_V6: return "redacted-logic-mod-artifact-dag-v6";
    case ControllerObservationSourceV1::V5_WORKER_RUN_INSPECTION_V1: return "redacted-v5-worker-run-inspection-v1";
    }
    return "redacted-controller-observation-v1";
}

std::string json_string(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out += hex[ch >> 4]; out += hex[ch & 0x0f];
            } else out += static_cast<char>(ch);
        }
    }
    return out;
}

ControllerObservationPhaseV1 phase(RuntimeOrchestratorStateV1 state) {
    switch (state) {
    case RuntimeOrchestratorStateV1::QUEUED: return ControllerObservationPhaseV1::QUEUED;
    case RuntimeOrchestratorStateV1::ADMITTED:
    case RuntimeOrchestratorStateV1::STARTING:
    case RuntimeOrchestratorStateV1::RUNNING:
    case RuntimeOrchestratorStateV1::DRAINING: return ControllerObservationPhaseV1::ACTIVE;
    case RuntimeOrchestratorStateV1::SUCCEEDED:
    case RuntimeOrchestratorStateV1::FAILED:
    case RuntimeOrchestratorStateV1::BLOCKED:
    case RuntimeOrchestratorStateV1::CANCELLED: return ControllerObservationPhaseV1::TERMINAL;
    }
    return ControllerObservationPhaseV1::UNKNOWN;
}

ControllerObservationTerminalV1 terminal(RuntimeOrchestratorStateV1 state) {
    switch (state) {
    case RuntimeOrchestratorStateV1::SUCCEEDED: return ControllerObservationTerminalV1::SUCCEEDED;
    case RuntimeOrchestratorStateV1::FAILED:
    case RuntimeOrchestratorStateV1::BLOCKED: return ControllerObservationTerminalV1::FAILED;
    case RuntimeOrchestratorStateV1::CANCELLED: return ControllerObservationTerminalV1::CANCELLED;
    case RuntimeOrchestratorStateV1::QUEUED:
    case RuntimeOrchestratorStateV1::ADMITTED:
    case RuntimeOrchestratorStateV1::STARTING:
    case RuntimeOrchestratorStateV1::RUNNING:
    case RuntimeOrchestratorStateV1::DRAINING: return ControllerObservationTerminalV1::NOT_TERMINAL;
    default: return ControllerObservationTerminalV1::UNKNOWN;
    }
}
}

ControllerObservationV1 observe_runtime_orchestrator_v1(const RuntimeOrchestratorStatusV1& status) {
    ControllerObservationV1 out;
    out.source = ControllerObservationSourceV1::RUNTIME_ORCHESTRATOR_V1;
    out.opaque_id = redacted_id(out.source);
    out.phase = phase(status.state);
    out.terminal = terminal(status.state);
    // A value outside the documented state enum has no liveness or control
    // meaning.  In particular, do not turn a co-located reason value into an
    // inferred cancellation fact.
    if (out.phase == ControllerObservationPhaseV1::UNKNOWN ||
        out.terminal == ControllerObservationTerminalV1::UNKNOWN)
        return out;
    if (status.reason == RuntimeOrchestratorReasonV1::CANCELLED_QUEUED ||
        status.reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE)
        out.cancellation = ControllerObservationCancellationV1::ORCHESTRATOR;
    else if (status.reason == RuntimeOrchestratorReasonV1::V6_CANCELLED_EXTERNALLY)
        out.cancellation = ControllerObservationCancellationV1::EXTERNAL;
    else if (out.terminal == ControllerObservationTerminalV1::CANCELLED)
        out.cancellation = ControllerObservationCancellationV1::UNKNOWN;
    else out.cancellation = ControllerObservationCancellationV1::NONE;
    // v6_accepted is canonical completion metadata when available.  The
    // orchestrator state remains insufficient to claim success/publication.
    if (status.v6_accepted && *status.v6_accepted) out.success = ControllerObservationFactV1::YES;
    else if (status.v6_accepted) out.success = ControllerObservationFactV1::NO;
    return out;
}

ControllerObservationV1 observe_logic_mod_artifact_dag_v6(
    const LogicModArtifactDagRunV6& result,
    std::optional<std::vector<mod::SupervisionEventV6>> validated_sidecar) {
    ControllerObservationV1 out;
    out.source = ControllerObservationSourceV1::LOGIC_MOD_ARTIFACT_DAG_V6;
    out.opaque_id = redacted_id(out.source);
    out.phase = ControllerObservationPhaseV1::TERMINAL;
    out.terminal = result.accepted ? ControllerObservationTerminalV1::SUCCEEDED :
        (result.status == "DAG_HOST_CANCELLED" ? ControllerObservationTerminalV1::CANCELLED : ControllerObservationTerminalV1::FAILED);
    out.success = result.accepted ? ControllerObservationFactV1::YES : ControllerObservationFactV1::NO;
    out.cancellation = out.terminal == ControllerObservationTerminalV1::CANCELLED ?
        ControllerObservationCancellationV1::V6_HOST : ControllerObservationCancellationV1::NONE;
    // Presence only proves that a caller supplied a validated diagnostic trace;
    // it neither changes accepted nor exposes its contents.
    if (validated_sidecar && !validated_sidecar->empty()) out.diagnostic = ControllerObservationFactV1::YES;
    return out;
}

ControllerObservationV1 observe_v5_worker_run_v1(const worker_v1::WorkerRunInspectionV1& inspection) {
    ControllerObservationV1 out;
    out.source = ControllerObservationSourceV1::V5_WORKER_RUN_INSPECTION_V1;
    out.opaque_id = redacted_id(out.source);
    // This inspector is historical and read-only.  An unfinished (including
    // explicitly INCOMPLETE) log is not a current liveness observation.
    if (!inspection.run_finished || inspection.status == "INCOMPLETE") return out;
    out.phase = inspection.run_finished ? ControllerObservationPhaseV1::TERMINAL : ControllerObservationPhaseV1::ACTIVE;
    if (inspection.status == "CANCELLED") {
        out.terminal = ControllerObservationTerminalV1::CANCELLED;
        out.cancellation = ControllerObservationCancellationV1::V5_OWNER;
    } else if (inspection.status == "FAILED") out.terminal = ControllerObservationTerminalV1::FAILED;
    else out.terminal = ControllerObservationTerminalV1::COMPLETED_UNVERIFIED;
    if (out.cancellation == ControllerObservationCancellationV1::UNKNOWN)
        out.cancellation = ControllerObservationCancellationV1::NONE;
    return out;
}

std::string render_controller_observation_v1(const ControllerObservationV1& value) {
    return "{\"schema\":\"controller-observability-crosswalk-v1\",\"source\":" + numeric(value.source) +
        ",\"id\":\"" + json_string(redacted_id(value.source)) + "\",\"phase\":" + numeric(value.phase) +
        ",\"terminal\":" + numeric(value.terminal) + ",\"cancellation\":" + numeric(value.cancellation) +
        ",\"success\":" + numeric(value.success) + ",\"publication\":" + numeric(value.publication) +
        ",\"durable_evidence\":" + numeric(value.durable_evidence) + ",\"diagnostic\":" + numeric(value.diagnostic) + "}";
}
} // namespace bruce
