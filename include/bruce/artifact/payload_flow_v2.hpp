#pragma once

#include "bruce/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

inline constexpr std::string_view payload_flow_v2_contract = "bruce-payload-flow-v2";
inline constexpr std::size_t payload_flow_v2_max_nodes = 4096;
inline constexpr std::size_t payload_flow_v2_max_attempts = 4096;
inline constexpr std::size_t payload_flow_v2_max_inputs = 15;
inline constexpr std::size_t payload_flow_v2_max_dependencies = 4096;

// Metadata for one node in the already validated V5 DAG. No request bytes or
// filesystem information is retained in this model.
struct PayloadFlowNodeV2 {
    std::string node_id;
    std::string callback_id;
    std::string request_digest;
    std::vector<std::string> dependencies;
    bool operator==(const PayloadFlowNodeV2&) const = default;
};

// One immutable producer-to-consumer input binding. object_index is currently
// fixed to zero by the V2 contract.
struct PayloadFlowInputBindingV2 {
    std::uint64_t input_index{};
    std::string producer_node_id;
    std::string producer_attempt_id;
    std::string manifest_id;
    std::uint64_t object_index{};
    std::string sha256;
    std::uint64_t size_bytes{};
    bool operator==(const PayloadFlowInputBindingV2&) const = default;
};

struct PayloadFlowAttemptV2 {
    std::string consumer_node_id;
    std::string consumer_attempt_id;
    std::vector<PayloadFlowInputBindingV2> inputs;
    bool operator==(const PayloadFlowAttemptV2&) const = default;
};

struct PayloadFlowV2 {
    std::string run_id;
    std::string spec_digest;
    std::string report_digest;
    bool stream_v15{};
    std::vector<PayloadFlowNodeV2> nodes;
    std::vector<PayloadFlowAttemptV2> attempts;
    bool operator==(const PayloadFlowV2&) const = default;
};

// Compatibility aliases make the sidecar's role explicit at call sites while
// keeping one value model and one validator.
using PayloadFlowSidecarV2 = PayloadFlowV2;
using PayloadFlowSnapshotV2 = PayloadFlowV2;

void validate_payload_flow_v2(const PayloadFlowV2& flow);
[[nodiscard]] std::string encode_payload_flow_v2(const PayloadFlowV2& flow);
[[nodiscard]] PayloadFlowV2 decode_payload_flow_v2(std::string_view bytes);

} // namespace bruce::artifact_v1
