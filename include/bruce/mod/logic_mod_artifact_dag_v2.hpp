#pragma once

#include "bruce/mod/logic_mod_worker_v1.hpp"
#include <vector>

namespace bruce {
// Host-only selections. Registry references must outlive the complete call.
struct LogicModDagNodeV2 {
    std::string id;
    const LogicModRegistryV1& registry;
    std::string mod_id, version, template_id, input;
    std::vector<std::string> predecessors;
    std::optional<LogicModOutputV1> input_artifact;
    LogicModOutputV1 output_artifact;
    LogicModBudgetsV1 budgets{};
};
struct LogicModDagNodeRunV2 {
    std::string id;
    std::vector<std::string> predecessors;
    LogicModRunV1 run;
    LogicModPublishedOutputV1 result_artifact;
    std::optional<LogicModPublishedOutputV1> internal_input_artifact;
};
struct LogicModArtifactDagRunV2 {
    bool accepted = false;
    std::string status, dag_id, spec_digest;
    std::vector<std::string> node_order;
    std::vector<LogicModDagNodeRunV2> nodes;
};
inline constexpr std::size_t logic_mod_artifact_dag_record_limit_v2 = 8 * 1024 * 1024;
LogicModArtifactDagRunV2 run_logic_mod_artifact_dag_v2(
    const std::vector<LogicModDagNodeV2>&, const std::filesystem::path& store, std::string_view spec_digest);
std::string logic_mod_artifact_dag_record_v2(const LogicModArtifactDagRunV2&);
LogicModArtifactDagRunV2 read_logic_mod_artifact_dag_record_v2(std::string_view);
// Read-only; revalidates current package authorization and all fresh artifacts.
// Every rejection is DAG_REPLAY_MISMATCH, with no worker execution.
void verify_logic_mod_artifact_dag_replay_v2(const std::vector<LogicModDagNodeV2>&,
    const std::filesystem::path& store, std::string_view spec_digest, const LogicModArtifactDagRunV2&);
}
