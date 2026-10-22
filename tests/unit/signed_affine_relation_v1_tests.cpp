#include "bruce/signed_affine_relation/signed_affine_relation_v1.hpp"
#include "bruce/core/types.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace bruce;
using Node = SignedAffineExecutionNodeV1;
using Request = SignedAffineRelationRequestV1;
using Result = SignedAffineRelationResultV1;
using Status = ToyRelationVerificationStatusV1;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Node source(std::string id, std::int64_t value) {
    return {std::move(id), std::nullopt, "", value, 1};
}
Node step(std::string id, std::string parent, std::string op) {
    return {std::move(id), std::move(parent), std::move(op), std::nullopt, 1};
}
Request merge() {
    return {{{step("z-double", "source", "double-v1"), source("source", 2),
              step("a-add", "source", "add-two-v1")}}, 101, "a-add", "z-double"};
}
std::string projection(const Result& result) {
    std::ostringstream out;
    out << result.group.modulus << '|' << result.group.generator << '|' << result.group.public_q;
    for (const auto* path : {&result.left, &result.right}) {
        const auto& state = path->toy_provenance.coefficient_state;
        out << '|' << path->node_id << '|' << path->source_node_id << '|' << path->source_value
            << '|' << path->multiplier << '|' << path->offset << '|' << path->output_value
            << '|' << path->toy_provenance.provenance_id << '|' << state.a << '|' << state.b
            << '|' << state.canonical_point;
    }
    const auto& c = result.collision;
    const auto& r = result.relation;
    out << '|' << c.canonical_point << '|' << c.existing_provenance_id << '|' << c.new_provenance_id
        << '|' << r.modulus << '|' << r.coefficient_delta << '|' << r.value_delta
        << '|' << r.candidate_k << '|' << static_cast<int>(r.verification_status);
    return out.str();
}
std::string projection(const Request& request) {
    std::ostringstream out;
    out << request.modulus << '|' << request.left_node_id << '|' << request.right_node_id;
    for (const auto& node : request.spec.nodes) {
        out << '|' << node.node_id << '|' << node.predecessor_node_id.has_value()
            << '|' << node.predecessor_node_id.value_or("") << '|' << node.operator_id
            << '|' << node.source_value.has_value() << '|' << node.source_value.value_or(0)
            << '|' << node.path_weight;
    }
    return out.str();
}
void rejected(const Request& request, const char* code) {
    // A successful result remains wholly intact when assignment throws.
    auto sentinel = derive_signed_affine_relation_v1(merge());
    const auto before = projection(sentinel), input = projection(request);
    try { sentinel = derive_signed_affine_relation_v1(request); }
    catch (const Error& error) {
        const std::string message = error.what();
        require(message == code || message.starts_with(std::string(code) + ":"),
                "wrong controlled error code");
        require(projection(sentinel) == before, "rejection published a partial result");
        require(projection(request) == input, "rejection mutated the request");
        return;
    }
    throw std::runtime_error("invalid bridge request accepted");
}
void exact_merge_and_independent_verification() {
    const auto request = merge();
    const auto before = projection(request);
    const auto result = derive_signed_affine_relation_v1(request);
    require(projection(result) ==
        "101|1|2|a-add|source|2|1|2|4|a-add|2|1|4|z-double|source|2|2|0|4|z-double|0|2|4|4|a-add|z-double|101|100|99|2|1",
        "merge forms, Toy provenance or canonical relation changed");
    require(projection(request) == before, "success mutated the request");
    require(verify_toy_relation_v1(result.group, result.left.toy_provenance,
        result.right.toy_provenance, result.relation).verification_status == Status::verified,
        "returned relation does not independently verify");
}
void deeper_cross_state_predecessor_chain() {
    const Request request{{{step("z-deep", "d-minus", "subtract-one-v1"),
        step("a-add", "b-add", "add-two-v1"), source("source", 2),
        step("c-double", "b-double", "double-v1"), step("b-add", "source", "add-two-v1"),
        step("b-double", "source", "double-v1"), step("d-minus", "c-double", "subtract-one-v1")}},
        101, "a-add", "z-deep"};
    const auto trace = execute_signed_affine_dag_v1(request.spec);
    require(trace.states[2].value == 6 && trace.states[3].value == 7 && trace.states[4].value == 8,
            "fixture does not force predecessors later in result-state order");
    const auto result = derive_signed_affine_relation_v1(request);
    require(result.left.multiplier == 1 && result.left.offset == 4 && result.left.output_value == 6,
            "deep add path composition");
    require(result.right.multiplier == 4 && result.right.offset == -2 && result.right.output_value == 6 &&
            result.right.source_node_id == "source" && result.right.source_value == 2,
            "recursive predecessor composition");
    require(result.right.toy_provenance.coefficient_state.a == 99 &&
            result.right.toy_provenance.coefficient_state.b == 4 &&
            result.relation.candidate_k == 2 && result.relation.verification_status == Status::verified,
            "deep signed offset mapping");
}
void deep_bounded_value_predecessor_chain() {
    // A leaf sorting before all ancestors forces the full uncached path.
    Request request{{{source("z-source", 2), step("z-double", "z-source", "double-v1")}},
                    101, "a-selected", "z-double"};
    std::string previous = "z-source";
    for (int i = 0; i < 60000; ++i) {
        const auto id = "p-" + std::to_string(i);
        request.spec.nodes.push_back(step(id, previous,
            i % 3 == 0 ? "add-two-v1" : "subtract-one-v1"));
        previous = id;
    }
    request.spec.nodes.push_back(step("a-selected", previous, "add-two-v1"));
    const auto trace = execute_signed_affine_dag_v1(request.spec);
    require(request.spec.nodes.size() == 60003 && trace.states.size() == 3 &&
            trace.states[0].value == 2 && trace.states[1].value == 3 && trace.states[2].value == 4,
            "deep fixture must remain a valid bounded-value DAG");
    const auto result = derive_signed_affine_relation_v1(request);
    require(projection(result) ==
        "101|1|2|a-selected|z-source|2|1|2|4|a-selected|2|1|4|z-double|z-source|2|2|0|4|z-double|0|2|4|4|a-selected|z-double|101|100|99|2|1",
        "deep path forms, Toy provenance or verified relation changed");
    require(verify_toy_relation_v1(result.group, result.left.toy_provenance,
        result.right.toy_provenance, result.relation).verification_status == Status::verified,
        "deep path relation does not independently verify");
}
void reversed_selection_preserves_requested_paths() {
    auto request = merge();
    std::swap(request.left_node_id, request.right_node_id);
    const auto result = derive_signed_affine_relation_v1(request);
    require(result.left.node_id == "z-double" && result.left.multiplier == 2 && result.left.offset == 0 &&
            result.right.node_id == "a-add" && result.right.multiplier == 1 && result.right.offset == 2,
            "requested display provenance was reordered");
    require(result.collision.existing_provenance_id == "a-add" &&
            result.collision.new_provenance_id == "z-double" && result.collision.canonical_point == 4 &&
            result.relation.coefficient_delta == 100 && result.relation.value_delta == 99 &&
            result.relation.candidate_k == 2 && result.relation.verification_status == Status::verified,
            "Toy canonical ordering or signed deltas changed");
}
void negative_source_and_offset_normalize() {
    const Request request{{{source("source", -2), step("minus", "source", "subtract-one-v1"),
        step("a-minus", "minus", "subtract-one-v1"), step("z-double", "source", "double-v1")}},
        101, "a-minus", "z-double"};
    const auto result = derive_signed_affine_relation_v1(request);
    require(projection(result) ==
        "101|1|99|a-minus|source|-2|1|-2|-4|a-minus|99|1|97|z-double|source|-2|2|0|-4|z-double|0|2|97|97|a-minus|z-double|101|100|2|99|1",
        "negative source or offset failed exact normalization");
}
void source_count_failures() {
    auto request = merge();
    request.spec.nodes.push_back(source("second-source", 17));
    rejected(request, "InvalidSignedAffineRelationSourceCount");
    request.spec.nodes.clear();
    rejected(request, "InvalidSignedAffineDagSource");
}
void selection_failures() {
    for (const auto& ids : std::vector<std::pair<std::string, std::string>>{
            {"a-add", "a-add"}, {"", "z-double"}, {"a-add", ""}, {"", ""}}) {
        auto request = merge();
        request.left_node_id = ids.first; request.right_node_id = ids.second;
        rejected(request, "InvalidSignedAffineRelationSelection");
    }
    for (const auto& ids : std::vector<std::pair<std::string, std::string>>{
            {"unknown", "z-double"}, {"a-add", "unknown"}}) {
        auto request = merge();
        request.left_node_id = ids.first; request.right_node_id = ids.second;
        rejected(request, "UnknownSignedAffineRelationNode");
    }
}
void unequal_outputs_even_if_modular_points_match() {
    auto request = merge();
    request.spec.nodes.push_back(step("different", "a-add", "add-two-v1"));
    request.right_node_id = "different";
    rejected(request, "NoSignedAffineRelationCollision");
    // Value 105 shares point 4 mod 101 but is not an integer equality merge.
    request.spec.nodes = {source("source", 2), step("a-add", "source", "add-two-v1")};
    std::string previous = "source";
    for (int i = 0; i < 52; ++i) {
        auto id = "add-" + std::to_string(i);
        request.spec.nodes.push_back(step(id, previous, "add-two-v1")); previous = id;
    }
    request.spec.nodes.push_back(step("different", previous, "subtract-one-v1"));
    rejected(request, "NoSignedAffineRelationCollision");
}
void invalid_modulus_propagates_toy_errors() {
    for (auto modulus : {0u, 1u, 2u, 65536u, std::numeric_limits<std::uint32_t>::max()}) {
        auto request = merge(); request.modulus = modulus;
        rejected(request, "InvalidToyRelationModulus");
    }
}
void noninvertible_has_no_result() {
    const Request request{{{source("source", 2), step("add1", "source", "add-two-v1"),
        step("add2", "add1", "add-two-v1"), step("a-add3", "add2", "add-two-v1"),
        step("double1", "source", "double-v1"), step("z-double2", "double1", "double-v1")}},
        15, "a-add3", "z-double2"};
    rejected(request, "NonInvertibleToyRelationCoefficient");
}
void coefficient_overflow_with_valid_zero_source_trace() {
    auto request = merge(); request.spec.nodes = {source("source", 0)};
    std::string previous = "source";
    for (int i = 1; i <= 63; ++i) {
        auto id = "double-" + std::to_string(i);
        request.spec.nodes.push_back(step(id, previous, "double-v1")); previous = id;
    }
    request.left_node_id = "source"; request.right_node_id = previous;
    require(execute_signed_affine_dag_v1(request.spec).states.front().value == 0,
            "overflow fixture must remain executable by DAG V1");
    rejected(request, "SignedAffineRelationCoefficientOverflow");
}
void back_substitution_overflow_with_valid_trace() {
    const auto value = std::numeric_limits<std::int64_t>::max() / 2 + 1;
    const Request request{{{source("source", value), step("minus", "source", "subtract-one-v1"),
        step("double", "minus", "double-v1")}}, 101, "source", "double"};
    require(execute_signed_affine_dag_v1(request.spec).states.back().value ==
            std::numeric_limits<std::int64_t>::max() - 1,
            "back-substitution fixture must remain executable");
    rejected(request, "SignedAffineRelationCoefficientOverflow");
}
void existing_dag_errors_precede_bridge_validation() {
    auto request = merge(); request.modulus = 0; request.left_node_id.clear();
    request.spec.nodes[0].operator_id = "unknown";
    rejected(request, "InvalidSignedAffineDagOperator");
    request = merge(); request.spec.nodes[0].predecessor_node_id = "missing";
    rejected(request, "UnknownSignedAffineDagPredecessor");
    request = merge(); request.spec.nodes[1].source_value = std::numeric_limits<std::int64_t>::max();
    rejected(request, "SignedAffineDagArithmeticOverflow");
    request = merge(); request.spec.nodes[0].predecessor_node_id = "a-add";
    request.spec.nodes[2].predecessor_node_id = "z-double";
    rejected(request, "SignedAffineDagCycle");
}
void zero_source_and_modulus_boundaries() {
    Request request{{{source("source", 0), step("double", "source", "double-v1")}},
                    3, "source", "double"};
    const auto zero = derive_signed_affine_relation_v1(request);
    require(zero.left.multiplier == 1 && zero.left.offset == 0 &&
            zero.right.multiplier == 2 && zero.right.offset == 0 &&
            zero.relation.candidate_k == 0 && zero.relation.verification_status == Status::verified,
            "source selection, zero public Q or lower modulus limit rejected");
    request = merge(); request.modulus = 65535;
    const auto upper = derive_signed_affine_relation_v1(request);
    require(upper.relation.candidate_k == 2 && upper.relation.verification_status == Status::verified,
            "upper modulus boundary rejected");
}
void repeated_and_permuted_input_is_stable() {
    auto request = merge();
    const auto expected = projection(derive_signed_affine_relation_v1(request));
    for (int i = 0; i < 6; ++i) {
        std::rotate(request.spec.nodes.begin(), request.spec.nodes.begin() + 1, request.spec.nodes.end());
        require(projection(derive_signed_affine_relation_v1(request)) == expected,
                "repeat or spec permutation changed complete output");
    }
}
} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"exact_merge_and_independent_verification", exact_merge_and_independent_verification},
        {"deeper_cross_state_predecessor_chain", deeper_cross_state_predecessor_chain},
        {"deep_bounded_value_predecessor_chain", deep_bounded_value_predecessor_chain},
        {"reversed_selection_preserves_requested_paths", reversed_selection_preserves_requested_paths},
        {"negative_source_and_offset_normalize", negative_source_and_offset_normalize},
        {"source_count_failures", source_count_failures},
        {"selection_failures", selection_failures},
        {"unequal_outputs_even_if_modular_points_match", unequal_outputs_even_if_modular_points_match},
        {"invalid_modulus_propagates_toy_errors", invalid_modulus_propagates_toy_errors},
        {"noninvertible_has_no_result", noninvertible_has_no_result},
        {"coefficient_overflow_with_valid_zero_source_trace", coefficient_overflow_with_valid_zero_source_trace},
        {"back_substitution_overflow_with_valid_trace", back_substitution_overflow_with_valid_trace},
        {"existing_dag_errors_precede_bridge_validation", existing_dag_errors_precede_bridge_validation},
        {"zero_source_and_modulus_boundaries", zero_source_and_modulus_boundaries},
        {"repeated_and_permuted_input_is_stable", repeated_and_permuted_input_is_stable}};
    std::size_t passed = 0;
    for (const auto& [name, action] : cases) {
        try { action(); ++passed; }
        catch (const std::exception& error) { std::cerr << name << ": " << error.what() << '\n'; return 1; }
    }
    std::cout << passed << " signed affine relation V1 contracts passed\n";
}
