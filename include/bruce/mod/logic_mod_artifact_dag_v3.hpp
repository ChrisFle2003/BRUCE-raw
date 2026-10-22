#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v2.hpp"
#include <cstdint>

namespace bruce {
// Local host scheduling control; never part of identity, records, or replay.
struct LogicModArtifactDagControlV3 {
    std::uint32_t max_parallel_nodes = 1;
};
using LogicModArtifactDagRunV3 = LogicModArtifactDagRunV2;
inline constexpr std::size_t logic_mod_artifact_dag_record_limit_v3 = logic_mod_artifact_dag_record_limit_v2;
// Registry references must outlive the complete call and its joined waves.
LogicModArtifactDagRunV3 run_logic_mod_artifact_dag_v3(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store,
    std::string_view spec_digest, LogicModArtifactDagControlV3 control = {});
std::string logic_mod_artifact_dag_record_v3(const LogicModArtifactDagRunV3&);
LogicModArtifactDagRunV3 read_logic_mod_artifact_dag_record_v3(std::string_view);
// Read-only V2 verification, with no worker execution or scheduling control.
void verify_logic_mod_artifact_dag_replay_v3(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path& store, std::string_view spec_digest, const LogicModArtifactDagRunV3&);
}
