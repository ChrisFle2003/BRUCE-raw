#pragma once

#include "bruce/mod/logic_mod_worker_v1.hpp"

namespace bruce {
// Host-only selection. Paths never enter the canonical chain record or worker
// request. The second step cannot supply its own payload reference.
struct LogicModChainStepV1 {
    const LogicModRegistryV1& registry;
    std::string mod_id, version, template_id, input;
    LogicModOutputV1 output;
    LogicModBudgetsV1 budgets{};
};
struct LogicModArtifactChainRunV1 {
    bool accepted = false;
    std::string status, chain_id;
    LogicModRunV1 first, second;
    LogicModPublishedOutputV1 first_output, second_output;
};
inline constexpr std::size_t logic_mod_artifact_chain_record_limit_v1 = 524288;

// Both steps are preflighted before creating the Store or starting A. Only a
// fully accepted chain is serializable; failed runs cannot become records.
LogicModArtifactChainRunV1 run_logic_mod_artifact_chain_v1(
    const LogicModChainStepV1& first, const LogicModChainStepV1& second,
    const std::filesystem::path& store);
std::string logic_mod_artifact_chain_record_v1(const LogicModArtifactChainRunV1&);
LogicModArtifactChainRunV1 read_logic_mod_artifact_chain_record_v1(std::string_view);
// Read-only verification; never starts a worker. Manifest IDs come from the
// record, so replay callers may leave step.output empty. All failures use the
// controlled CHAIN_REPLAY_MISMATCH diagnostic.
void verify_logic_mod_artifact_chain_replay_v1(
    const LogicModChainStepV1& first, const LogicModChainStepV1& second,
    const std::filesystem::path& store, const LogicModArtifactChainRunV1&);
}
