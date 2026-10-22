#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace bruce::artifact_v1 {

struct ExecuteHandoffInspectionV1 {
    std::string status;
    std::string run_status;
    std::string run_id;
    std::uint64_t node_count{};
    std::uint64_t attempt_count{};
    std::uint64_t binding_count{};
    std::uint64_t verified_input_bytes{};
};

[[nodiscard]] ExecuteHandoffInspectionV1 inspect_execute_handoff_v1(
    const std::filesystem::path& bundle);

} // namespace bruce::artifact_v1
