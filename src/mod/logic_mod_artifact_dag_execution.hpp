#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v4.hpp"
#include <functional>

namespace bruce::detail {
// Runs the unchanged V4 wave/budget engine. Host preparation may restore only
// the expected completed predecessor; the engine always verifies it afterwards.
using LogicModPredecessorPreparation = std::function<void(const LogicModPublishedOutputV1&)>;
// Canonical V4 admission primitives.  V6 must use these rather than a
// parallel reconstruction so node order, all-node preflight and target
// validation remain byte/behavior compatible.
std::vector<std::string> canonical_logic_mod_artifact_dag_order_v4(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path&, std::string_view);
void preflight_logic_mod_artifact_dag_node_v4(const LogicModDagNodeV2&);
void validate_logic_mod_artifact_dag_targets_v4(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path&);
std::uint64_t logic_mod_artifact_dag_wave_payload_bytes_v4(const std::vector<const LogicModDagNodeV2*>&,
    const std::vector<LogicModDagNodeRunV2>&);
LogicModDagNodeRunV2 prepare_logic_mod_artifact_dag_node_v4(const LogicModDagNodeV2&,
    const std::filesystem::path&, const std::vector<LogicModDagNodeRunV2>&,
    const LogicModPredecessorPreparation&);
LogicModArtifactDagRunV4 run_logic_mod_artifact_dag_with_predecessor_v4(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store,
    std::string_view spec_digest, LogicModArtifactDagControlV4,
    const LogicModPredecessorPreparation&);
}
