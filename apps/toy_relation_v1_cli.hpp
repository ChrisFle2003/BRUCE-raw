#pragma once

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/toy_relation/toy_relation_v1.hpp"

#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace bruce {
namespace toy_relation_v1_cli_detail {

struct Demo {
    ToyCyclicGroupV1 group;
    std::array<ToyGroupProvenanceV1, 2> provenances;
};

inline Demo demo(bool noninvertible) {
    const ToyCyclicGroupV1 group = noninvertible ?
        ToyCyclicGroupV1{15, 1, 4} : ToyCyclicGroupV1{101, 1, 17};
    return {group, {{{"left", make_toy_coefficient_state_v1(
                        group, noninvertible ? 0 : 2, noninvertible ? 5 : 3)},
                    {"right", make_toy_coefficient_state_v1(
                        group, noninvertible ? 5 : 36, noninvertible ? 0 : 1)}}}};
}

inline std::string render_provenance(const ToyGroupProvenanceV1& provenance) {
    const auto& state = provenance.coefficient_state;
    return "{\"provenance_id\": " + json_string(provenance.provenance_id) +
        ", \"coefficient_state\": {\"a\": " + std::to_string(state.a) +
        ", \"b\": " + std::to_string(state.b) +
        ", \"canonical_point\": " + std::to_string(state.canonical_point) + "}}";
}

inline std::string render(std::string_view scenario, const Demo& input,
                          const ToyCollisionRecordV1& collision,
                          const std::optional<ToyRelationRecordV1>& relation) {
    const auto& group = input.group;
    const bool ordered = input.provenances[0].provenance_id < input.provenances[1].provenance_id;
    const auto& left = input.provenances[ordered ? 0 : 1];
    const auto& right = input.provenances[ordered ? 1 : 0];
    std::string report = "{\n  \"contract\": " + json_string("toy-relation-v1-cli-v1") +
        ",\n  \"scenario\": " + json_string(scenario) +
        ",\n  \"group\": {\"modulus\": " + std::to_string(group.modulus) +
        ", \"generator\": " + std::to_string(group.generator) +
        ", \"public_q\": " + std::to_string(group.public_q) + "},\n  \"provenances\": [\n    " +
        render_provenance(left) + ",\n    " + render_provenance(right) +
        "\n  ],\n  \"collision\": {\"canonical_point\": " + std::to_string(collision.canonical_point) +
        ", \"existing_provenance_id\": " + json_string(collision.existing_provenance_id) +
        ", \"new_provenance_id\": " + json_string(collision.new_provenance_id) + '}';
    if (relation) {
        const auto status = relation->verification_status == ToyRelationVerificationStatusV1::verified ?
            "VERIFIED" : "PENDING";
        report += ",\n  \"relation\": {\"modulus\": " + std::to_string(relation->modulus) +
            ", \"coefficient_delta\": " + std::to_string(relation->coefficient_delta) +
            ", \"value_delta\": " + std::to_string(relation->value_delta) +
            ", \"candidate_k\": " + std::to_string(relation->candidate_k) +
            ", \"verification_status\": " + json_string(status) +
            "},\n  \"status\": " + json_string(status);
    } else {
        report += ",\n  \"status\": " + json_string("NO_UNIQUE_RELATION") +
            ",\n  \"reason\": " + json_string("NonInvertibleToyRelationCoefficient");
    }
    return report + "\n}";
}

} // namespace toy_relation_v1_cli_detail

inline int toy_relation_v1_cli(int argc, char** argv) {
    if (argc != 4 || std::string_view(argv[2]) != "demo" ||
        (std::string_view(argv[3]) != "collision" && std::string_view(argv[3]) != "noninvertible"))
        throw Error("Usage: bruce toy-relation-v1 demo collision|noninvertible");
    const std::string_view scenario = argv[3];
    const auto input = toy_relation_v1_cli_detail::demo(scenario == "noninvertible");
    const auto& left = input.provenances[0];
    const auto& right = input.provenances[1];
    const auto collision = detect_toy_relation_collision_v1(left, right);
    std::optional<ToyRelationRecordV1> relation;
    try {
        relation = derive_toy_relation_v1(input.group, left, right, collision);
    } catch (const Error& error) {
        if (scenario != "noninvertible" ||
            std::string_view(error.what()) != "NonInvertibleToyRelationCoefficient")
            throw;
    }
    if (relation)
        relation = verify_toy_relation_v1(input.group, left, right, *relation);
    std::cout << toy_relation_v1_cli_detail::render(scenario, input, collision, relation) << '\n';
    return 0;
}

} // namespace bruce
