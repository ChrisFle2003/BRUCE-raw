#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v4.hpp"
#include "bruce/artifact/artifact_store_v4.hpp"

namespace bruce {
// Host-only restore policy. Never part of specs, identities, records or replay.
struct LogicModArtifactDagControlV5 : LogicModArtifactDagControlV4 {
    std::optional<std::string> remote_restore;
};
using LogicModArtifactDagRunV5 = LogicModArtifactDagRunV4;
inline constexpr std::size_t logic_mod_artifact_dag_record_limit_v5 = logic_mod_artifact_dag_record_limit_v4;
LogicModArtifactDagRunV5 run_logic_mod_artifact_dag_v5(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store,
    std::string_view spec_digest, LogicModArtifactDagControlV5 control = {},
    const artifact_v4::HttpsTransportV4& transport = artifact_v4::https_transport_v4);
std::string logic_mod_artifact_dag_record_v5(const LogicModArtifactDagRunV5&);
LogicModArtifactDagRunV5 read_logic_mod_artifact_dag_record_v5(std::string_view);
void verify_logic_mod_artifact_dag_replay_v5(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path& store, std::string_view spec_digest, const LogicModArtifactDagRunV5&);
}
