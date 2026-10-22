#pragma once

#include "bruce/signed_affine_dag/signed_affine_dag_v1.hpp"
#include "bruce/toy_relation/toy_relation_v1.hpp"

#include <cstdint>
#include <string>

namespace bruce {

struct SignedAffineRelationRequestV1 {
    SignedAffineDagSpecV1 spec;
    std::uint32_t modulus;
    std::string left_node_id;
    std::string right_node_id;
};

struct SignedAffineDerivedPathV1 {
    std::string node_id;
    std::string source_node_id;
    std::int64_t source_value;
    std::int64_t multiplier;
    std::int64_t offset;
    std::int64_t output_value;
    ToyGroupProvenanceV1 toy_provenance;
};

struct SignedAffineRelationResultV1 {
    ToyCyclicGroupV1 group;
    SignedAffineDerivedPathV1 left;
    SignedAffineDerivedPathV1 right;
    ToyCollisionRecordV1 collision;
    ToyRelationRecordV1 relation;
};

SignedAffineRelationResultV1 derive_signed_affine_relation_v1(
    const SignedAffineRelationRequestV1& request);

} // namespace bruce
