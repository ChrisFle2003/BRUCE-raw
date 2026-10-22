#pragma once
#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include <cstddef>
#include <string>
#include <string_view>
namespace bruce {
inline constexpr std::size_t mcp_max_inspect_bytes_v1 = 1024U * 1024U;
inline constexpr std::size_t mcp_max_inspect_bytes_v3 = 5U * 1024U * 1024U;
enum class McpInspectionStatusV1 { valid, rejected };
struct McpInspectionResultV1 {
    McpInspectionStatusV1 status;
    std::string code;
    std::string json;
};
[[nodiscard]] std::string mcp_inspection_status_name(McpInspectionStatusV1 status);
[[nodiscard]] McpInspectionResultV1 inspect_phase_dag_v2_bundle(std::string_view bytes);
[[nodiscard]] McpInspectionResultV1 inspect_phase_dag_v3_bundle(std::string_view bytes);
// Structural V4 inspect only: no replay and no callback execution.
[[nodiscard]] McpInspectionResultV1 inspect_dag_scheduler_v4_bundle(std::string_view bytes);
namespace mcp_inspector_detail {
// Internal catalog-injection seam for verifying the no-execution boundary.
[[nodiscard]] McpInspectionResultV1 inspect_dag_scheduler_v4_bundle(
    std::string_view bytes, DagSchedulerCallbackRegistryV2 registry);
}
}
