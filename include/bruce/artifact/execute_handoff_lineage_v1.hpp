#pragma once

#include "bruce/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

inline constexpr std::string_view execute_handoff_lineage_v1_contract =
    "bruce-execute-handoff-v1";
inline constexpr std::size_t execute_handoff_lineage_v1_max_nodes = 64;
inline constexpr std::size_t execute_handoff_lineage_v1_max_attempts = 256;
inline constexpr std::size_t execute_handoff_lineage_v1_max_dependencies = 15;
inline constexpr std::size_t execute_handoff_lineage_v1_max_inputs = 15;
inline constexpr std::size_t execute_handoff_lineage_v1_max_token_bytes = 128;
inline constexpr std::size_t execute_handoff_lineage_v1_max_reference_bytes = 512;

struct ExecuteHandoffNodeV1 {
    std::string node_id;
    std::string callback_id;
    std::string request_digest;
    std::vector<std::string> dependencies;
    bool operator==(const ExecuteHandoffNodeV1&) const = default;
};

struct ExecuteHandoffInputBindingV1 {
    std::uint64_t input_index{};
    std::string producer_node_id;
    std::string producer_attempt_id;
    std::string reference;
    std::string sha256;
    std::uint64_t size_bytes{};
    bool operator==(const ExecuteHandoffInputBindingV1&) const = default;
};

struct ExecuteHandoffAttemptV1 {
    std::string consumer_node_id;
    std::string consumer_attempt_id;
    std::vector<ExecuteHandoffInputBindingV1> inputs;
    bool operator==(const ExecuteHandoffAttemptV1&) const = default;
};

struct ExecuteHandoffLineageV1 {
    std::string run_id;
    std::string spec_digest;
    std::string report_digest;
    std::vector<ExecuteHandoffNodeV1> nodes;
    std::vector<ExecuteHandoffAttemptV1> attempts;
    bool operator==(const ExecuteHandoffLineageV1&) const = default;
};

void validate_execute_handoff_lineage_v1(const ExecuteHandoffLineageV1& lineage);
[[nodiscard]] std::string encode_execute_handoff_lineage_v1(
    const ExecuteHandoffLineageV1& lineage);
[[nodiscard]] ExecuteHandoffLineageV1 decode_execute_handoff_lineage_v1(
    std::string_view bytes);

} // namespace bruce::artifact_v1
