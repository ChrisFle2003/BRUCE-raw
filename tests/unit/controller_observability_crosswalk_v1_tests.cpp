#include "bruce/observability/controller_observability_crosswalk_v1.hpp"

#include <iostream>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }

void maps_orchestrator_without_turning_status_into_success() {
    bruce::RuntimeOrchestratorStatusV1 status;
    status.run_id = "orv1-safe";
    status.state = bruce::RuntimeOrchestratorStateV1::SUCCEEDED;
    status.reason = bruce::RuntimeOrchestratorReasonV1::NONE;
    const auto fact = bruce::observe_runtime_orchestrator_v1(status);
    check(fact.source == bruce::ControllerObservationSourceV1::RUNTIME_ORCHESTRATOR_V1, "orchestrator source");
    check(fact.phase == bruce::ControllerObservationPhaseV1::TERMINAL, "orchestrator terminal");
    check(fact.terminal == bruce::ControllerObservationTerminalV1::SUCCEEDED, "orchestrator terminal kind");
    check(fact.success == bruce::ControllerObservationFactV1::UNKNOWN, "status invented success");
    check(fact.publication == bruce::ControllerObservationFactV1::UNKNOWN, "status invented publication");
    check(fact.durable_evidence == bruce::ControllerObservationFactV1::UNKNOWN, "status invented evidence");
    check(fact.opaque_id == "redacted-runtime-orchestrator-v1", "opaque source token missing");
}

void native_ids_are_never_reflected_and_v6_cancel_is_exact() {
    const std::string hostile = "/private/path?token=credential:payload://" + std::string(512, 'x');
    bruce::RuntimeOrchestratorStatusV1 runtime;
    runtime.run_id = hostile;
    const auto runtime_fact = bruce::observe_runtime_orchestrator_v1(runtime);
    check(runtime_fact.opaque_id == "redacted-runtime-orchestrator-v1", "runtime id reflected");
    check(bruce::render_controller_observation_v1(runtime_fact).find(hostile) == std::string::npos, "runtime hostile id rendered");

    bruce::LogicModArtifactDagRunV6 v6;
    v6.dag_id = hostile; v6.status = "UNTRUSTED_CANCEL_TEXT";
    const auto v6_fact = bruce::observe_logic_mod_artifact_dag_v6(v6);
    check(v6_fact.opaque_id == "redacted-logic-mod-artifact-dag-v6", "v6 id reflected");
    check(v6_fact.terminal == bruce::ControllerObservationTerminalV1::FAILED, "substring cancel classification accepted");
    check(bruce::render_controller_observation_v1(v6_fact).find("credential") == std::string::npos, "v6 credential-like id rendered");

    bruce::worker_v1::WorkerRunInspectionV1 worker;
    worker.run_id = hostile;
    const auto worker_fact = bruce::observe_v5_worker_run_v1(worker);
    check(worker_fact.opaque_id == "redacted-v5-worker-run-inspection-v1", "v5 id reflected");
    check(bruce::render_controller_observation_v1(worker_fact).find("payload") == std::string::npos, "v5 payload-like id rendered");
}

void unknown_or_historical_states_do_not_claim_liveness_or_cancel() {
    bruce::RuntimeOrchestratorStatusV1 runtime;
    runtime.state = static_cast<bruce::RuntimeOrchestratorStateV1>(255);
    runtime.reason = bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE;
    const auto unknown_runtime = bruce::observe_runtime_orchestrator_v1(runtime);
    check(unknown_runtime.phase == bruce::ControllerObservationPhaseV1::UNKNOWN, "unknown runtime phase inferred");
    check(unknown_runtime.terminal == bruce::ControllerObservationTerminalV1::UNKNOWN, "unknown runtime terminal inferred");
    check(unknown_runtime.cancellation == bruce::ControllerObservationCancellationV1::UNKNOWN, "unknown runtime cancel inferred");

    bruce::worker_v1::WorkerRunInspectionV1 unfinished;
    unfinished.status = "READY";
    const auto unknown_v5 = bruce::observe_v5_worker_run_v1(unfinished);
    check(unknown_v5.phase == bruce::ControllerObservationPhaseV1::UNKNOWN, "unfinished historical log marked active");
    check(unknown_v5.terminal == bruce::ControllerObservationTerminalV1::UNKNOWN, "unfinished historical log marked terminal");
    check(unknown_v5.cancellation == bruce::ControllerObservationCancellationV1::UNKNOWN, "unfinished historical log cancellation inferred");

    bruce::worker_v1::WorkerRunInspectionV1 incomplete;
    incomplete.run_finished = true; incomplete.status = "INCOMPLETE";
    const auto incomplete_v5 = bruce::observe_v5_worker_run_v1(incomplete);
    check(incomplete_v5.phase == bruce::ControllerObservationPhaseV1::UNKNOWN, "incomplete historical log marked terminal");
    check(incomplete_v5.terminal == bruce::ControllerObservationTerminalV1::UNKNOWN, "incomplete historical terminal inferred");
}

void renderer_never_serializes_caller_constructed_opaque_id() {
    bruce::ControllerObservationV1 supplied;
    supplied.source = bruce::ControllerObservationSourceV1::LOGIC_MOD_ARTIFACT_DAG_V6;
    supplied.opaque_id = "/private/path?credential=token&payload=" + std::string(1024 * 1024, 'x');
    const auto rendered = bruce::render_controller_observation_v1(supplied);
    check(rendered.find("private") == std::string::npos, "direct renderer path leak");
    check(rendered.find("credential") == std::string::npos, "direct renderer credential leak");
    check(rendered.find("payload") == std::string::npos, "direct renderer payload leak");
    check(rendered.find("redacted-logic-mod-artifact-dag-v6") != std::string::npos, "direct renderer source token missing");
    check(rendered.size() < 512, "direct renderer retained huge id");
}

void maps_orchestrator_cancel_origin_without_leaking_details() {
    bruce::RuntimeOrchestratorStatusV1 status;
    status.run_id = "orv1-cancel";
    status.state = bruce::RuntimeOrchestratorStateV1::CANCELLED;
    status.reason = bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE;
    status.sidecar_reference = "/private/report.run/control/supervision-v6.jsonl";
    const auto fact = bruce::observe_runtime_orchestrator_v1(status);
    check(fact.cancellation == bruce::ControllerObservationCancellationV1::ORCHESTRATOR, "cancel origin");
    check(fact.diagnostic == bruce::ControllerObservationFactV1::UNKNOWN, "sidecar reference exposed as diagnostic");
    const auto rendered = bruce::render_controller_observation_v1(fact);
    check(rendered.find("private") == std::string::npos && rendered.find("sidecar") == std::string::npos, "redaction failure");
}

void canonical_v6_is_distinct_from_sidecar_diagnostic() {
    bruce::LogicModArtifactDagRunV6 run;
    run.accepted = true;
    run.status = "DAG_READY";
    run.dag_id = "dag-safe";
    bruce::mod::SupervisionEventV6 event;
    event.seq = 1; event.event = "ATTEMPT_FINISHED"; event.status = "FAILED";
    const auto fact = bruce::observe_logic_mod_artifact_dag_v6(run, std::vector<bruce::mod::SupervisionEventV6>{event});
    check(fact.source == bruce::ControllerObservationSourceV1::LOGIC_MOD_ARTIFACT_DAG_V6, "v6 source");
    check(fact.terminal == bruce::ControllerObservationTerminalV1::SUCCEEDED, "canonical accepted not mapped");
    check(fact.success == bruce::ControllerObservationFactV1::YES, "canonical accepted missing");
    check(fact.publication == bruce::ControllerObservationFactV1::UNKNOWN, "in-memory canonical result invented publication");
    check(fact.diagnostic == bruce::ControllerObservationFactV1::YES, "validated sidecar diagnostic absent");
    check(fact.cancellation == bruce::ControllerObservationCancellationV1::NONE, "sidecar overrode canonical cancellation");
}

void missing_or_incomplete_evidence_stays_unknown() {
    bruce::LogicModArtifactDagRunV6 run;
    run.accepted = false; run.status = "DAG_HOST_CANCELLED"; run.dag_id = "dag-cancel";
    const auto v6 = bruce::observe_logic_mod_artifact_dag_v6(run);
    check(v6.diagnostic == bruce::ControllerObservationFactV1::UNKNOWN, "missing sidecar inferred");
    check(v6.durable_evidence == bruce::ControllerObservationFactV1::UNKNOWN, "memory result inferred durable");

    bruce::worker_v1::WorkerRunInspectionV1 worker;
    worker.run_id = "worker-safe"; worker.status = "READY"; worker.run_finished = true;
    const auto v5 = bruce::observe_v5_worker_run_v1(worker);
    check(v5.source == bruce::ControllerObservationSourceV1::V5_WORKER_RUN_INSPECTION_V1, "v5 source");
    check(v5.terminal == bruce::ControllerObservationTerminalV1::COMPLETED_UNVERIFIED, "v5 terminal");
    check(v5.success == bruce::ControllerObservationFactV1::UNKNOWN, "worker status invented success");
    check(v5.publication == bruce::ControllerObservationFactV1::UNKNOWN, "worker status invented publication");
    check(v5.durable_evidence == bruce::ControllerObservationFactV1::UNKNOWN, "worker inspection inferred durable");
}
}

int main() {
    try {
        maps_orchestrator_without_turning_status_into_success();
        maps_orchestrator_cancel_origin_without_leaking_details();
        canonical_v6_is_distinct_from_sidecar_diagnostic();
        missing_or_incomplete_evidence_stays_unknown();
        native_ids_are_never_reflected_and_v6_cancel_is_exact();
        unknown_or_historical_states_do_not_claim_liveness_or_cancel();
        renderer_never_serializes_caller_constructed_opaque_id();
        std::cout << "controller observability crosswalk v1 tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
