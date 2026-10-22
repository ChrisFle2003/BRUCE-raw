#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
namespace bruce::artifact_v1 {
struct PayloadRunInspectOptionsV1 { bool verify_streams_v15{false}; };
struct PayloadRunInspectionV1 {
    bool run_finished{};
    std::string run_status, source_contract, run_id;
    std::uint64_t last_seq{}, node_count{}, attempt_count{}, accepted_artifact_count{};
    std::uint64_t verified_stream_count{}, verified_stream_bytes{};
    bool streams_requested{};
};
// Read-only consistency check. All failures are bruce::Error("PROTOCOL_ERROR").
[[nodiscard]] PayloadRunInspectionV1 inspect_payload_run_v1(
    const std::filesystem::path& bundle, const PayloadRunInspectOptionsV1& options = {});
}
