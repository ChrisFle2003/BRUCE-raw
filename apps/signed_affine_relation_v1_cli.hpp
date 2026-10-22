#pragma once

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/signed_affine_relation/signed_affine_relation_v1.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace bruce {
namespace signed_affine_relation_v1_cli_detail {

inline SignedAffineDagSpecV1 demo_spec() {
    return {{{"z-double", "source", "double-v1", std::nullopt, 1},
             {"source", std::nullopt, "", 2, 1},
             {"a-add", "source", "add-two-v1", std::nullopt, 1}}};
}

inline std::string render_path(const SignedAffineDerivedPathV1& path) {
    const auto& provenance = path.toy_provenance;
    const auto& state = provenance.coefficient_state;
    return "{\"node_id\": " + json_string(path.node_id) +
        ", \"source_node_id\": " + json_string(path.source_node_id) +
        ", \"source_value\": " + std::to_string(path.source_value) +
        ", \"multiplier\": " + std::to_string(path.multiplier) +
        ", \"offset\": " + std::to_string(path.offset) +
        ", \"output_value\": " + std::to_string(path.output_value) +
        ", \"toy_provenance\": {\"provenance_id\": " + json_string(provenance.provenance_id) +
        ", \"coefficient_state\": {\"a\": " + std::to_string(state.a) +
        ", \"b\": " + std::to_string(state.b) +
        ", \"canonical_point\": " + std::to_string(state.canonical_point) + "}}}";
}

inline std::string render(const SignedAffineRelationResultV1& result) {
    const auto& group = result.group;
    const auto& collision = result.collision;
    const auto& relation = result.relation;
    const auto status = relation.verification_status == ToyRelationVerificationStatusV1::verified ?
        "VERIFIED" : "PENDING";
    return "{\n  \"contract\": " + json_string("signed-affine-relation-v1-cli-v1") +
        ",\n  \"scenario\": " + json_string("merge") +
        ",\n  \"source\": {\"node_id\": " + json_string(result.left.source_node_id) +
        ", \"value\": " + std::to_string(result.left.source_value) +
        "},\n  \"group\": {\"modulus\": " + std::to_string(group.modulus) +
        ", \"generator\": " + std::to_string(group.generator) +
        ", \"public_q\": " + std::to_string(group.public_q) +
        "},\n  \"left\": " + render_path(result.left) +
        ",\n  \"right\": " + render_path(result.right) +
        ",\n  \"collision\": {\"canonical_point\": " + std::to_string(collision.canonical_point) +
        ", \"existing_provenance_id\": " + json_string(collision.existing_provenance_id) +
        ", \"new_provenance_id\": " + json_string(collision.new_provenance_id) +
        "},\n  \"relation\": {\"modulus\": " + std::to_string(relation.modulus) +
        ", \"coefficient_delta\": " + std::to_string(relation.coefficient_delta) +
        ", \"value_delta\": " + std::to_string(relation.value_delta) +
        ", \"candidate_k\": " + std::to_string(relation.candidate_k) +
        ", \"verification_status\": " + json_string(status) +
        "},\n  \"status\": " + json_string(status) + "\n}";
}

} // namespace signed_affine_relation_v1_cli_detail

inline int signed_affine_relation_v1_cli(int argc, char** argv) {
    if (argc != 4 || std::string_view(argv[2]) != "demo" ||
        std::string_view(argv[3]) != "merge")
        throw Error("Usage: bruce signed-affine-relation-v1 demo merge");
    const auto result = derive_signed_affine_relation_v1(
        {signed_affine_relation_v1_cli_detail::demo_spec(), 101, "a-add", "z-double"});
    const auto report = signed_affine_relation_v1_cli_detail::render(result);
    std::cout << report << '\n';
    return 0;
}

} // namespace bruce
