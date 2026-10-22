#pragma once

#include <filesystem>
#include <string_view>

namespace bruce::worker_v1 {

inline constexpr std::string_view kMarker = "BRUCE_WORKER_CANCEL_V1\n";

enum class AbortRequestResult {
    REQUESTED,
    ALREADY_REQUESTED,
};

enum class AbortProbeResult {
    NOT_REQUESTED,
    REQUESTED,
    INVALID,
};

// Create the owner-only cancellation marker for one private V5 run root.
// Invalid, unavailable or unsafe control trees raise bruce::Error("PROTOCOL_ERROR").
AbortRequestResult request_abort(const std::filesystem::path& run_root);

// Inspect the marker without modifying the run tree. Any unavailable or
// malformed state is INVALID; it is never treated as NOT_REQUESTED.
AbortProbeResult probe_abort(const std::filesystem::path& run_root);

} // namespace bruce::worker_v1
