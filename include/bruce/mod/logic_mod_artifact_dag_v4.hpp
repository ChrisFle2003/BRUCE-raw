#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v3.hpp"
#include <optional>

namespace bruce {
// Local host budgets; absent from DAG identities, specs, records and replay.
struct LogicModArtifactDagControlV4 {
    std::uint32_t max_parallel_nodes = 1;
    std::optional<std::uint64_t> max_total_wall_ms;
    std::optional<std::uint64_t> max_total_payload_bytes;
    std::uint32_t max_started_workers = 16;
    std::optional<std::uint64_t> cancel_after_ms;
};
using LogicModArtifactDagRunV4 = LogicModArtifactDagRunV3;
inline constexpr std::size_t logic_mod_artifact_dag_record_limit_v4 = logic_mod_artifact_dag_record_limit_v3;
// Registry references must outlive the complete call and all joined workers.
LogicModArtifactDagRunV4 run_logic_mod_artifact_dag_v4(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store,
    std::string_view spec_digest, LogicModArtifactDagControlV4 control = {});
std::string logic_mod_artifact_dag_record_v4(const LogicModArtifactDagRunV4&);
LogicModArtifactDagRunV4 read_logic_mod_artifact_dag_record_v4(std::string_view);
void verify_logic_mod_artifact_dag_replay_v4(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path& store, std::string_view spec_digest, const LogicModArtifactDagRunV4&);
}
