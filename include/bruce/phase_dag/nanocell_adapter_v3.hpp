#pragma once

#include "bruce/phase_dag/nanocell_adapter_v2.hpp"
#include "bruce/phase_dag/phase_dag_v3.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bruce {

struct NanoCellPhasePayloadV1 {
    std::string payload_version{"nanocell-phase-payload-v1"};
    std::string nanocell_status;
    std::optional<std::uint8_t> final_state;
};

// Canonical length-prefixed fields: version, status, presence (0/1),
// followed by exactly one raw byte only when final_state is present.
std::string pack_nanocell_phase_payload_v1(const NanoCellPhasePayloadV1&);
NanoCellPhasePayloadV1 unpack_nanocell_phase_payload_v1(std::string_view);
std::span<const PhaseAdapterV3> phase_dag_nanocell_catalog_v3();

} // namespace bruce
