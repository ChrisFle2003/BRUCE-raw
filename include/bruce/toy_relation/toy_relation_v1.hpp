#pragma once

#include <cstdint>
#include <string>

namespace bruce {
struct ToyCyclicGroupV1 {
    std::uint32_t modulus, generator, public_q;
};

struct ToyCoefficientStateV1 {
    std::uint32_t a, b, canonical_point;
};

struct ToyGroupProvenanceV1 {
    std::string provenance_id;
    ToyCoefficientStateV1 coefficient_state;
};

struct ToyCollisionRecordV1 {
    std::uint32_t canonical_point;
    std::string existing_provenance_id, new_provenance_id;
};

enum class ToyRelationVerificationStatusV1 { pending, verified };

struct ToyRelationRecordV1 {
    std::uint32_t modulus, coefficient_delta, value_delta, candidate_k;
    ToyRelationVerificationStatusV1 verification_status{ToyRelationVerificationStatusV1::pending};
};

ToyCoefficientStateV1 make_toy_coefficient_state_v1(
    const ToyCyclicGroupV1&, std::uint32_t a, std::uint32_t b);
ToyCollisionRecordV1 detect_toy_relation_collision_v1(
    const ToyGroupProvenanceV1&, const ToyGroupProvenanceV1&);
ToyRelationRecordV1 derive_toy_relation_v1(
    const ToyCyclicGroupV1&, const ToyGroupProvenanceV1&,
    const ToyGroupProvenanceV1&, const ToyCollisionRecordV1&);
ToyRelationRecordV1 verify_toy_relation_v1(
    const ToyCyclicGroupV1&, const ToyGroupProvenanceV1&,
    const ToyGroupProvenanceV1&, const ToyRelationRecordV1&);
} // namespace bruce
