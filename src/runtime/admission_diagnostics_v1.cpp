#include "bruce/runtime/admission_diagnostics_v1.hpp"

namespace bruce {
namespace {
constexpr std::string_view source_name = "runtime-orchestrator-v1";

std::string_view state_name(AdmissionDiagnosticsStateV1 state) {
    switch (state) {
    case AdmissionDiagnosticsStateV1::QUEUED_NOT_ADMITTED: return "QUEUED_NOT_ADMITTED";
    case AdmissionDiagnosticsStateV1::ADMISSION_RESERVED: return "ADMISSION_RESERVED";
    case AdmissionDiagnosticsStateV1::ACTIVE: return "ACTIVE";
    case AdmissionDiagnosticsStateV1::BLOCKED_HOST_RESERVATION: return "BLOCKED_HOST_RESERVATION";
    case AdmissionDiagnosticsStateV1::CANCELLED_BEFORE_ADMISSION: return "CANCELLED_BEFORE_ADMISSION";
    case AdmissionDiagnosticsStateV1::TERMINAL_AFTER_ADMISSION: return "TERMINAL_AFTER_ADMISSION";
    case AdmissionDiagnosticsStateV1::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string_view reason_name(AdmissionDiagnosticsReasonV1 reason) {
    switch (reason) {
    case AdmissionDiagnosticsReasonV1::NONE: return "NONE";
    case AdmissionDiagnosticsReasonV1::RESERVATION_EXCEEDS_HOST: return "RESERVATION_EXCEEDS_HOST";
    case AdmissionDiagnosticsReasonV1::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

AdmissionDiagnosticsV1 projection(const RuntimeOrchestratorStatusV1& native) {
    AdmissionDiagnosticsV1 out;
    out.enqueue_sequence = native.enqueue_sequence;
    out.submit_sequence = native.submit_sequence;
    out.start_sequence = native.start_sequence;
    out.finish_sequence = native.finish_sequence;
    out.reservation = native.reservation;
    return out;
}
}

AdmissionDiagnosticsV1 admission_diagnostics_v1(const RuntimeOrchestratorStatusV1& native) {
    auto out = projection(native);
    switch (native.state) {
    case RuntimeOrchestratorStateV1::QUEUED:
        if (native.reason == RuntimeOrchestratorReasonV1::NONE) {
            out.state = AdmissionDiagnosticsStateV1::QUEUED_NOT_ADMITTED;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::ADMITTED:
        if (native.reason == RuntimeOrchestratorReasonV1::NONE) {
            out.state = AdmissionDiagnosticsStateV1::ADMISSION_RESERVED;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::STARTING:
    case RuntimeOrchestratorStateV1::RUNNING:
        if (native.reason == RuntimeOrchestratorReasonV1::NONE) {
            out.state = AdmissionDiagnosticsStateV1::ACTIVE;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::DRAINING:
        if (native.reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE) {
            out.state = AdmissionDiagnosticsStateV1::ACTIVE;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::BLOCKED:
        if (native.reason == RuntimeOrchestratorReasonV1::RESERVATION_EXCEEDS_HOST) {
            out.state = AdmissionDiagnosticsStateV1::BLOCKED_HOST_RESERVATION;
            out.reason = AdmissionDiagnosticsReasonV1::RESERVATION_EXCEEDS_HOST;
        }
        return out;
    case RuntimeOrchestratorStateV1::CANCELLED:
        if (native.reason == RuntimeOrchestratorReasonV1::CANCELLED_QUEUED) {
            out.state = AdmissionDiagnosticsStateV1::CANCELLED_BEFORE_ADMISSION;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        } else if (native.reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE) {
            out.state = AdmissionDiagnosticsStateV1::TERMINAL_AFTER_ADMISSION;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::SUCCEEDED:
        if (native.reason == RuntimeOrchestratorReasonV1::NONE) {
            out.state = AdmissionDiagnosticsStateV1::TERMINAL_AFTER_ADMISSION;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    case RuntimeOrchestratorStateV1::FAILED:
        if (native.reason == RuntimeOrchestratorReasonV1::V6_CANCELLED_EXTERNALLY ||
            native.reason == RuntimeOrchestratorReasonV1::V6_FAILED ||
            native.reason == RuntimeOrchestratorReasonV1::START_FAILED) {
            out.state = AdmissionDiagnosticsStateV1::TERMINAL_AFTER_ADMISSION;
            out.reason = AdmissionDiagnosticsReasonV1::NONE;
        }
        return out;
    }
    return out;
}

std::string render_admission_diagnostics_v1(const AdmissionDiagnosticsV1& value) {
    const auto optional_number = [](const std::optional<std::uint64_t>& number) {
        return number ? std::to_string(*number) : "null";
    };
    return "{\"schema\":\"admission-diagnostics-v1\",\"source\":\"" + std::string(source_name) +
        "\",\"state\":\"" + std::string(state_name(value.state)) + "\",\"reason\":\"" +
        std::string(reason_name(value.reason)) + "\",\"enqueue_seq\":" + std::to_string(value.enqueue_sequence) +
        ",\"submit_seq\":" + std::to_string(value.submit_sequence) + ",\"start_seq\":" +
        optional_number(value.start_sequence) + ",\"finish_seq\":" + optional_number(value.finish_sequence) +
        ",\"reservation\":{\"max_parallel_nodes\":" + std::to_string(value.reservation.max_parallel_nodes) +
        ",\"max_total_wall_ms\":" + std::to_string(value.reservation.max_total_wall_ms) +
        ",\"max_started_workers\":" + std::to_string(value.reservation.max_started_workers) +
        ",\"max_total_payload_bytes\":" + std::to_string(value.reservation.max_total_payload_bytes) + "}}";
}

} // namespace bruce
