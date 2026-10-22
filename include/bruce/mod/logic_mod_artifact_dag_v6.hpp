#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v5.hpp"

#include <atomic>
#include <memory>

namespace bruce {
// V6 adds only host-local supervision diagnostics.  Neither this report path
// nor its sidecar contents enter identities, canonical records or replay.
struct LogicModArtifactDagControlV6 : LogicModArtifactDagControlV5 {
    std::filesystem::path report_root;
    // Host-library seam for a controller that needs V6 to perform its normal
    // cancellation and drain path.  It is deliberately absent from canonical
    // records, replay and every CLI surface.
    std::shared_ptr<const std::atomic_bool> host_cancel_latch;
};
using LogicModArtifactDagRunV6 = LogicModArtifactDagRunV5;
inline constexpr std::size_t logic_mod_artifact_dag_record_limit_v6 = logic_mod_artifact_dag_record_limit_v5;
LogicModArtifactDagRunV6 run_logic_mod_artifact_dag_v6(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store,
    std::string_view spec_digest, LogicModArtifactDagControlV6,
    const artifact_v4::HttpsTransportV4& transport = artifact_v4::https_transport_v4);
std::string logic_mod_artifact_dag_record_v6(const LogicModArtifactDagRunV6&);
LogicModArtifactDagRunV6 read_logic_mod_artifact_dag_record_v6(std::string_view);
void verify_logic_mod_artifact_dag_replay_v6(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path&, std::string_view, const LogicModArtifactDagRunV6&);
}
