#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace bruce::artifact_v1 {

struct PayloadFlowInspectionV2 {
    std::string status;
    std::string run_status;
    std::string run_id;
    std::uint64_t node_count{};
    std::uint64_t attempt_count{};
    std::uint64_t binding_count{};
    std::uint64_t verified_input_bytes{};
    bool stream_v15{};
    std::string input_completeness;
};

// Read-only, local consistency validation of a flow-v2 sidecar and its
// associated V5 payload run. All failures are Error("PROTOCOL_ERROR").
[[nodiscard]] PayloadFlowInspectionV2 inspect_payload_flow_v2(
    const std::filesystem::path& bundle);

} // namespace bruce::artifact_v1
