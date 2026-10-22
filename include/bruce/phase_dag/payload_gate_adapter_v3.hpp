#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct PayloadGateMetadataV1 {
    std::string metadata_version{"payload-gate-output-v1"};
    std::string decision;
    std::uint64_t input_count{};
};

struct PayloadGatePayloadV1 {
    std::string payload_version{"payload-gate-payload-v1"};
    std::string decision;
    std::uint64_t input_count{};
    // Digest order follows lexicographic producer node ID order.
    std::vector<std::string> input_payload_digests;
};

std::string pack_payload_gate_request_v1();
void unpack_payload_gate_request_v1(std::string_view);
std::string pack_payload_gate_metadata_v1(const PayloadGateMetadataV1&);
PayloadGateMetadataV1 unpack_payload_gate_metadata_v1(std::string_view);
std::string pack_payload_gate_payload_v1(const PayloadGatePayloadV1&);
PayloadGatePayloadV1 unpack_payload_gate_payload_v1(std::string_view);

} // namespace bruce
