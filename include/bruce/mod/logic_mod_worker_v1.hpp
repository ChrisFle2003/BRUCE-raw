#pragma once
#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/mod/logic_mod_result_artifact_v1.hpp"
#include <cstdint>
#include <filesystem>

namespace bruce {
struct LogicModBudgetsV1 {
    std::uint32_t wall_ms = 250;
    std::uint32_t memory_bytes = 64 * 1024 * 1024;
    std::uint32_t input_bytes = 4096;
    std::uint32_t output_bytes = 16384;
};
struct LogicModRunV1 {
    bool accepted = false;
    std::string status, result, validator = "rational-power-v1", claim_scope = "candidate_only";
    std::string binding_identity, normalized_input, budget_decision;
    std::string request_digest, response_digest, response_frame;
    bool input_present = false;
    std::string input_manifest_id, input_sha256, input_codec;
    std::uint64_t input_object_index = 0, input_size_bytes = 0;
    bool output_present = false;
    std::string output_manifest_id, output_sha256, output_codec;
    std::uint64_t output_object_index = 0, output_size_bytes = 0;
};
// Host-only local selection; paths never enter worker frames or run records.
struct LogicModInputV1 {
    std::filesystem::path store;
    std::string manifest_id;
    std::uint64_t object_index = 0;
};
// Linux static ELF only. Fresh registry authorization is issued inside every
// call; only sealed copies of the freshly bound worker bytes are executable.
// This V1 host validator supports bounded rational powers, candidate_only.
LogicModRunV1 run_logic_mod_worker_v1(const LogicModRegistryV1&, std::string_view mod_id,
    std::string_view version, std::string_view template_id, std::string_view capability,
    std::string_view input, LogicModBudgetsV1,
    const std::optional<LogicModInputV1>& payload_input = std::nullopt,
    const std::optional<LogicModOutputV1>& output = std::nullopt);
// Executes an already freshly authorized immutable binding. This is the
// durable-installed adapter; it never reopens a worker pathname.
LogicModRunV1 run_logic_mod_worker_v1(const LogicModBindingV1&, std::string_view input,
                                      LogicModBudgetsV1);
std::string logic_mod_run_record_v1(const LogicModRunV1&);
LogicModRunV1 read_logic_mod_run_record_v1(std::string_view);
// Revalidates an accepted transcript against current authorization, exact
// request/budgets and host mathematics. Does not spawn or trust worker claims.
void verify_logic_mod_replay_v1(const LogicModRegistryV1&, std::string_view mod_id,
    std::string_view version, std::string_view template_id, std::string_view input,
    LogicModBudgetsV1, const LogicModRunV1&,
    const std::optional<LogicModInputV1>& payload_input = std::nullopt,
    const std::optional<LogicModOutputV1>& output = std::nullopt);
}
