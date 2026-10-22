#pragma once

#include "bruce/runtime/runtime_orchestrator_v1.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace bruce {

// This is a read-only, admission-specific projection of one already accepted
// RuntimeOrchestratorStatusV1.  It never owns a plan or any V6 result detail.
enum class AdmissionDiagnosticsSourceV1 { RUNTIME_ORCHESTRATOR_V1 };
enum class AdmissionDiagnosticsStateV1 {
    QUEUED_NOT_ADMITTED,
    ADMISSION_RESERVED,
    ACTIVE,
    BLOCKED_HOST_RESERVATION,
    CANCELLED_BEFORE_ADMISSION,
    TERMINAL_AFTER_ADMISSION,
    UNKNOWN,
};
enum class AdmissionDiagnosticsReasonV1 { NONE, RESERVATION_EXCEEDS_HOST, UNKNOWN };

struct AdmissionDiagnosticsV1 {
    AdmissionDiagnosticsSourceV1 source{AdmissionDiagnosticsSourceV1::RUNTIME_ORCHESTRATOR_V1};
    AdmissionDiagnosticsStateV1 state{AdmissionDiagnosticsStateV1::UNKNOWN};
    AdmissionDiagnosticsReasonV1 reason{AdmissionDiagnosticsReasonV1::UNKNOWN};
    std::uint64_t enqueue_sequence{};
    std::uint64_t submit_sequence{};
    std::optional<std::uint64_t> start_sequence;
    std::optional<std::uint64_t> finish_sequence;
    RuntimeOrchestratorReservationV1 reservation;
};

// Closed mapper: unknown enum values or any state/reason pair that cannot be
// produced by the native controller project to UNKNOWN and infer no wait
// reason. It performs no I/O and has no side effects.
[[nodiscard]] AdmissionDiagnosticsV1 admission_diagnostics_v1(const RuntimeOrchestratorStatusV1&);

// Bounded public JSON. The source is fixed and derived here; neither native
// IDs nor V6 fields can enter the output, even via a caller-made model.
[[nodiscard]] std::string render_admission_diagnostics_v1(const AdmissionDiagnosticsV1&);

} // namespace bruce
