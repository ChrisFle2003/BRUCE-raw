#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
namespace bruce::artifact_v1::detail {
struct PayloadStreamInspectionV1 {
    std::uint64_t count{}, bytes{};
    // Reauthenticate content before the core's final report/log reads, then
    // retain the observed tree for a metadata-only check after all long reads.
    // Neither callback repairs evidence or makes a historical claim.
    std::function<void()> validate_snapshot;
    std::function<void()> validate_metadata;
};
PayloadStreamInspectionV1 inspect_payload_run_streams_v1(
    const std::filesystem::path& run_root,
    const std::vector<std::string>& started_attempt_ids,
    std::uint64_t max_input_bytes,
    std::chrono::steady_clock::time_point deadline);
} // namespace bruce::artifact_v1::detail
