#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <filesystem>

namespace bruce {

enum class CommonRuntimeHostV2AcceptanceDisposition {
    ACCEPT,
    CANCEL,
    PROTOCOL_ERROR,
};

struct CommonRuntimeHostV2AcceptanceDecision {
    CommonRuntimeHostV2AcceptanceDisposition disposition{
        CommonRuntimeHostV2AcceptanceDisposition::PROTOCOL_ERROR};
    worker_v1::WorkerResult result;
    int host_exit_code{1};
    bool append_cancel_requested{};
    bool append_artifact_accepted{};
};

// Decide whether one canonical READY result may cross the host acceptance
// boundary. A successful NOT_REQUESTED probe is the linearization point: a
// cancellation marker published after this function returns is late.
[[nodiscard]] CommonRuntimeHostV2AcceptanceDecision
decide_common_runtime_host_v2_acceptance(
    const std::filesystem::path& run_root, worker_v1::WorkerResult result);

} // namespace bruce
