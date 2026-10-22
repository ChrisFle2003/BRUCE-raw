#include "bruce/signed_affine_dag/signed_affine_dag_v1.hpp"
#include "bruce/core/types.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace {
using Node = bruce::SignedAffineExecutionNodeV1;
using Spec = bruce::SignedAffineDagSpecV1;
constexpr auto low = std::numeric_limits<std::int64_t>::min();
constexpr auto high = std::numeric_limits<std::int64_t>::max();

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Node source(std::string id, std::int64_t value, std::int64_t weight = 1) {
    return {std::move(id), std::nullopt, "", value, weight};
}

Node derived(std::string id, std::string predecessor, std::string op,
             std::int64_t weight = 1) {
    return {std::move(id), std::move(predecessor), std::move(op), std::nullopt, weight};
}

Spec merge_spec(std::int64_t second_weight = 1) {
    return {{derived("z-double", "source", "double-v1", second_weight),
             source("source", 2), derived("a-add", "source", "add-two-v1")}};
}

template<class Action>
std::string rejected(Action action, std::string_view code) {
    try {
        action();
    } catch (const bruce::Error& error) {
        const std::string message = error.what();
        require(message == code || message.starts_with(std::string(code) + ":"),
                "wrong controlled error code");
        return message;
    }
    throw std::runtime_error("invalid operation was accepted");
}

void invalid(const Spec& spec, std::string_view code) {
    rejected([&] { bruce::validate_signed_affine_dag_v1(spec); }, code);
    auto output = bruce::execute_signed_affine_dag_v1({{source("sentinel", 99)}});
    rejected([&] { output = bruce::execute_signed_affine_dag_v1(spec); }, code);
    require(output.states.size() == 1 && output.states.front().value == 99,
            "failure published partial output");
}

const bruce::SignedAffineSemanticStateV1& state(
    const bruce::SignedAffineDagResultV1& result, std::int64_t value) {
    const auto found = std::find_if(result.states.begin(), result.states.end(),
        [&](const auto& candidate) { return candidate.value == value; });
    require(found != result.states.end(), "expected semantic state missing");
    return *found;
}

// Compare all public fields in a stable text projection, not object padding.
std::string projection(const bruce::SignedAffineDagResultV1& result) {
    std::ostringstream text;
    for (const auto& item : result.states) {
        text << item.value << ':' << item.weight_balance << '\n';
        for (const auto& path : item.paths) {
            text << path.node_id << '|' << path.predecessor_node_id << '|'
                 << path.operator_id << '|' << path.input_value << '|'
                 << path.output_value << '|' << path.path_weight << '\n';
        }
    }
    return text.str();
}

void merge_and_complete_provenance() {
    const auto result = bruce::execute_signed_affine_dag_v1(merge_spec());
    require(result.states.size() == 2, "equal results duplicated a semantic state");
    require(result.states[0].value == 2 && result.states[1].value == 4,
            "states not ordered by value");
    const auto& merged = state(result, 4);
    require(merged.paths.size() == 2, "merge retains both paths");
    require(merged.weight_balance == 2, "merge weight balance");
    require(projection(result) ==
        "2:1\nsource|||2|2|1\n"
        "4:2\na-add|source|add-two-v1|2|4|1\nz-double|source|double-v1|2|4|1\n",
        "provenance contract or exact node ordering changed");
}

void cancellation_retains_provenance() {
    const auto result = bruce::execute_signed_affine_dag_v1(merge_spec(-1));
    require(state(result, 4).paths.size() == 2, "zero balance erased provenance");
    require(state(result, 4).weight_balance == 0, "cancellation is not exact");
    require(projection(result) ==
        "2:1\nsource|||2|2|1\n"
        "4:0\na-add|source|add-two-v1|2|4|1\nz-double|source|double-v1|2|4|-1\n",
        "cancellation path records changed");
}

void deterministic_permutations_and_repeated_runs() {
    auto spec = merge_spec();
    const auto expected = projection(bruce::execute_signed_affine_dag_v1(spec));
    std::sort(spec.nodes.begin(), spec.nodes.end(),
              [](const auto& a, const auto& b) { return a.node_id < b.node_id; });
    do {
        for (int repeat = 0; repeat != 4; ++repeat)
            require(projection(bruce::execute_signed_affine_dag_v1(spec)) == expected,
                    "input order or repeated run changed output bytes");
    } while (std::next_permutation(spec.nodes.begin(), spec.nodes.end(),
             [](const auto& a, const auto& b) { return a.node_id < b.node_id; }));
}

void all_operators_chains_and_explicit_weights() {
    const Spec spec{{derived("3-subtract", "2-double", "subtract-one-v1", -1),
                     source("0-source", -3, -1),
                     derived("2-double", "1-add", "double-v1"),
                     derived("1-add", "0-source", "add-two-v1")}};
    const auto result = bruce::execute_signed_affine_dag_v1(spec);
    require(projection(result) ==
        "-3:-2\n0-source|||-3|-3|-1\n3-subtract|2-double|subtract-one-v1|-2|-3|-1\n"
        "-2:1\n2-double|1-add|double-v1|-1|-2|1\n"
        "-1:1\n1-add|0-source|add-two-v1|-3|-1|1\n",
        "fixed operator calculation, chained input, or explicit path weight changed");
}

void sources_merge_and_sort() {
    const auto result = bruce::execute_signed_affine_dag_v1(
        {{source("z", 7, -1), source("low", -9), source("a", 7)}});
    require(projection(result) == "-9:1\nlow|||-9|-9|1\n7:0\na|||7|7|1\nz|||7|7|-1\n",
            "source semantic merge or provenance changed");
}

void empty_and_no_source() {
    invalid({}, "InvalidSignedAffineDagSource");
    invalid({{derived("self", "self", "double-v1")}}, "InvalidSignedAffineDagSource");
}

void empty_and_duplicate_ids() {
    invalid({{source("", 1)}}, "InvalidSignedAffineDagNodeId");
    invalid({{source("same", 1), source("same", 2)}}, "InvalidSignedAffineDagNodeId");
}

void unknown_predecessors() {
    invalid({{source("source", 1), derived("node", "missing", "double-v1")}},
            "UnknownSignedAffineDagPredecessor");
    invalid({{source("source", 1), derived("node", "", "double-v1")}},
            "UnknownSignedAffineDagPredecessor");
}

void unknown_operators() {
    for (const auto* op : {"triple-v1", "", "double-v2", "2*x"})
        invalid({{source("source", 1), derived("node", "source", op)}},
                "InvalidSignedAffineDagOperator");
}

void invalid_path_weights() {
    for (const auto weight : {std::int64_t{0}, std::int64_t{2}, std::int64_t{-2}, low, high}) {
        invalid({{source("source", 1, weight)}}, "InvalidSignedAffineDagPathWeight");
        invalid({{source("source", 1), derived("node", "source", "double-v1", weight)}},
                "InvalidSignedAffineDagPathWeight");
    }
}

void invalid_source_fields() {
    auto node = source("source", 1);
    node.source_value.reset();
    invalid({{node}}, "InvalidSignedAffineDagSource");
    node = source("source", 1);
    node.operator_id = "double-v1";
    invalid({{node}}, "InvalidSignedAffineDagSource");
    node = derived("node", "source", "double-v1");
    node.source_value = 2;
    invalid({{source("source", 1), node}}, "InvalidSignedAffineDagSource");
}

void cycles() {
    invalid({{source("source", 1), derived("self", "self", "double-v1")}},
            "SignedAffineDagCycle");
    invalid({{derived("b", "a", "add-two-v1"), source("source", 1),
              derived("a", "b", "double-v1")}}, "SignedAffineDagCycle");
}

void validation_precedes_every_calculation() {
    auto spec = Spec{{source("a-source", high), derived("b-overflow", "a-source", "double-v1"),
                      derived("z-invalid", "absent", "add-two-v1")}};
    invalid(spec, "UnknownSignedAffineDagPredecessor");
    spec.nodes.back() = derived("z-invalid", "z-invalid", "add-two-v1");
    invalid(spec, "SignedAffineDagCycle");
}

void affine_overflow_has_no_partial_result() {
    for (const auto& [value, op] : std::vector<std::pair<std::int64_t, std::string>>{
             {high, "double-v1"}, {low, "double-v1"}, {high, "add-two-v1"},
             {high - 1, "add-two-v1"}, {low, "subtract-one-v1"}}) {
        const Spec spec{{source("source", value), derived("overflow", "source", op)}};
        bruce::validate_signed_affine_dag_v1(spec);
        auto output = bruce::execute_signed_affine_dag_v1({{source("sentinel", 99)}});
        rejected([&] { output = bruce::execute_signed_affine_dag_v1(spec); },
                 "SignedAffineDagArithmeticOverflow");
        require(output.states.size() == 1 && output.states.front().value == 99,
                "arithmetic failure published partial output");
    }
}

void affine_boundaries_remain_valid() {
    for (const auto& [value, op, expected] :
         std::vector<std::tuple<std::int64_t, std::string, std::int64_t>>{
             {high / 2, "double-v1", high - 1}, {low / 2, "double-v1", low},
             {high - 2, "add-two-v1", high}, {low, "add-two-v1", low + 2},
             {low + 1, "subtract-one-v1", low}, {high, "subtract-one-v1", high - 1}}) {
        const auto result = bruce::execute_signed_affine_dag_v1(
            {{source("source", value), derived("node", "source", op)}});
        const auto& paths = state(result, expected).paths;
        require(paths.size() == 1 && paths.front().input_value == value &&
                paths.front().output_value == expected, "valid affine bound was rejected");
    }
}

void lexicographic_kahn_order() {
    // Both children become ready at once; errors expose the first executed ID.
    const Spec spec{{derived("z-overflow", "source", "double-v1"), source("source", high),
                     derived("a-overflow", "source", "add-two-v1")}};
    const auto message = rejected([&] { bruce::execute_signed_affine_dag_v1(spec); },
                                 "SignedAffineDagArithmeticOverflow");
    require(message.find("a-overflow") != std::string::npos,
            "simultaneously ready nodes were not executed lexicographically");
    // Re-select after m-source: a-overflow precedes still-ready z-source.
    // Level batching would instead make 0-overflow ready and execute it first.
    const Spec dynamic{{source("m-source", high), source("z-source", high),
                        derived("a-overflow", "m-source", "double-v1"),
                        derived("0-overflow", "z-source", "add-two-v1")}};
    const auto dynamic_message = rejected([&] { bruce::execute_signed_affine_dag_v1(dynamic); },
                                         "SignedAffineDagArithmeticOverflow");
    require(dynamic_message.find("a-overflow") != std::string::npos,
            ("Kahn traversal did not re-order the ready set: " + dynamic_message).c_str());
}

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"merge_and_complete_provenance", merge_and_complete_provenance},
        {"cancellation_retains_provenance", cancellation_retains_provenance},
        {"deterministic_permutations_and_repeated_runs", deterministic_permutations_and_repeated_runs},
        {"all_operators_chains_and_explicit_weights", all_operators_chains_and_explicit_weights},
        {"sources_merge_and_sort", sources_merge_and_sort},
        {"empty_and_no_source", empty_and_no_source},
        {"empty_and_duplicate_ids", empty_and_duplicate_ids},
        {"unknown_predecessors", unknown_predecessors},
        {"unknown_operators", unknown_operators},
        {"invalid_path_weights", invalid_path_weights},
        {"invalid_source_fields", invalid_source_fields},
        {"cycles", cycles},
        {"validation_precedes_every_calculation", validation_precedes_every_calculation},
        {"affine_overflow_has_no_partial_result", affine_overflow_has_no_partial_result},
        {"affine_boundaries_remain_valid", affine_boundaries_remain_valid},
        {"lexicographic_kahn_order", lexicographic_kahn_order}};
    std::size_t passed = 0;
    for (const auto& [name, action] : cases) {
        try { action(); ++passed; }
        catch (const std::exception& error) {
            std::cerr << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << passed << " signed affine DAG V1 contracts passed\n";
}
