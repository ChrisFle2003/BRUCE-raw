#pragma once

#include "bruce/phase_dag/phase_dag_v2.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace bruce {

struct NanoCellPhaseRequestV1 {
    std::string core_layout_version;
    std::array<std::uint8_t, 8> core_bytes{};
    std::uint8_t active_index{};
    std::string context_id, context_bytes, context_digest, technique_id;
    std::uint64_t max_operations{}, max_trace_entries{};
};

struct NanoCellPhaseMetadataV1 {
    std::string metadata_version{"nanocell-phase-output-v1"};
    std::string nanocell_status, technique_id, technique_version, core_layout_version;
    std::uint8_t active_index{};
    std::string core_digest, context_id, context_digest, technique_digest;
    std::uint64_t operations_used{}, trace_entries{};
};

std::string pack_nanocell_phase_request_v1(const NanoCellPhaseRequestV1&);
NanoCellPhaseRequestV1 unpack_nanocell_phase_request_v1(std::string_view);
std::string pack_nanocell_phase_metadata_v1(const NanoCellPhaseMetadataV1&);
NanoCellPhaseMetadataV1 unpack_nanocell_phase_metadata_v1(std::string_view);
std::span<const PhaseAdapterV2> phase_dag_nanocell_catalog_v2();

} // namespace bruce
