#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bruce {

struct EvidenceGateMetadataV1 {
    std::string metadata_version{"evidence-gate-output-v1"};
    std::string decision;
    std::uint64_t input_count{}, complete_count{}, incomplete_count{}, rejected_count{};
};

std::string pack_evidence_gate_request_v1();
void unpack_evidence_gate_request_v1(std::string_view);
std::string pack_evidence_gate_metadata_v1(const EvidenceGateMetadataV1&);
EvidenceGateMetadataV1 unpack_evidence_gate_metadata_v1(std::string_view);

} // namespace bruce
