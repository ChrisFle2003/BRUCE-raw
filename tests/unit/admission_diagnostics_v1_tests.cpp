#include "bruce/runtime/admission_diagnostics_v1.hpp"

#include <iostream>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }

bruce::RuntimeOrchestratorStatusV1 status(bruce::RuntimeOrchestratorStateV1 state,
    bruce::RuntimeOrchestratorReasonV1 reason) {
    bruce::RuntimeOrchestratorStatusV1 value;
    value.run_id = "/private/path?credential=secret&payload=" + std::string(4096, 'x');
    value.enqueue_sequence = 11;
    value.submit_sequence = 12;
    value.start_sequence = 13;
    value.finish_sequence = 14;
    value.reservation = {2, 1000, 4, 512};
    value.state = state;
    value.reason = reason;
    value.v6_status = "DAG_HOST_CANCELLED";
    value.dag_id = "dag-private";
    value.sidecar_reference = "/private/sidecar";
    return value;
}

void maps_only_closed_native_pairs() {
    const auto queued = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::QUEUED,
        bruce::RuntimeOrchestratorReasonV1::NONE));
    check(queued.source == bruce::AdmissionDiagnosticsSourceV1::RUNTIME_ORCHESTRATOR_V1, "source");
    check(queued.state == bruce::AdmissionDiagnosticsStateV1::QUEUED_NOT_ADMITTED, "queued mapping");
    check(queued.reason == bruce::AdmissionDiagnosticsReasonV1::NONE, "queued reason");
    check(queued.enqueue_sequence == 11 && queued.start_sequence == 13 && queued.finish_sequence == 14, "sequences lost");
    check(queued.reservation.max_total_payload_bytes == 512, "reservation lost");

    const auto admitted = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::ADMITTED,
        bruce::RuntimeOrchestratorReasonV1::NONE));
    check(admitted.state == bruce::AdmissionDiagnosticsStateV1::ADMISSION_RESERVED, "admitted mapping");
    const auto active = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::RUNNING,
        bruce::RuntimeOrchestratorReasonV1::NONE));
    check(active.state == bruce::AdmissionDiagnosticsStateV1::ACTIVE, "active mapping");
    const auto blocked = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::BLOCKED,
        bruce::RuntimeOrchestratorReasonV1::RESERVATION_EXCEEDS_HOST));
    check(blocked.state == bruce::AdmissionDiagnosticsStateV1::BLOCKED_HOST_RESERVATION, "blocked mapping");
    check(blocked.reason == bruce::AdmissionDiagnosticsReasonV1::RESERVATION_EXCEEDS_HOST, "blocked reason");
    const auto cancelled = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::CANCELLED,
        bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED));
    check(cancelled.state == bruce::AdmissionDiagnosticsStateV1::CANCELLED_BEFORE_ADMISSION, "queued cancellation mapping");
    const auto terminal = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::FAILED,
        bruce::RuntimeOrchestratorReasonV1::V6_FAILED));
    check(terminal.state == bruce::AdmissionDiagnosticsStateV1::TERMINAL_AFTER_ADMISSION, "terminal mapping");
}

void unknown_or_inconsistent_pairs_stay_unknown() {
    const auto invalid_reason = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::QUEUED,
        bruce::RuntimeOrchestratorReasonV1::V6_FAILED));
    check(invalid_reason.state == bruce::AdmissionDiagnosticsStateV1::UNKNOWN, "inconsistent pair inferred");
    check(invalid_reason.reason == bruce::AdmissionDiagnosticsReasonV1::UNKNOWN, "inconsistent reason inferred");
    const auto invalid_enum = bruce::admission_diagnostics_v1(status(static_cast<bruce::RuntimeOrchestratorStateV1>(255),
        static_cast<bruce::RuntimeOrchestratorReasonV1>(255)));
    check(invalid_enum.state == bruce::AdmissionDiagnosticsStateV1::UNKNOWN, "invalid state inferred");
    check(invalid_enum.reason == bruce::AdmissionDiagnosticsReasonV1::UNKNOWN, "invalid enum reason inferred");
}

void renderer_is_source_derived_and_never_leaks_native_metadata() {
    auto diagnostic = bruce::admission_diagnostics_v1(status(bruce::RuntimeOrchestratorStateV1::RUNNING,
        bruce::RuntimeOrchestratorReasonV1::NONE));
    diagnostic.source = static_cast<bruce::AdmissionDiagnosticsSourceV1>(99);
    const auto rendered = bruce::render_admission_diagnostics_v1(diagnostic);
    check(rendered.find("/private") == std::string::npos, "path leaked");
    check(rendered.find("credential") == std::string::npos, "credential leaked");
    check(rendered.find("payload=") == std::string::npos, "payload leaked");
    check(rendered.find("DAG_HOST_CANCELLED") == std::string::npos, "v6 field leaked");
    check(rendered.size() < 512, "unbounded rendering");
    check(rendered.find("runtime-orchestrator-v1") != std::string::npos, "source is not fixed");
}
}

int main() {
    try {
        maps_only_closed_native_pairs();
        unknown_or_inconsistent_pairs_stay_unknown();
        renderer_is_source_derived_and_never_leaks_native_metadata();
        std::cout << "admission diagnostics v1 tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
