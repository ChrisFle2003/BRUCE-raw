#include "bruce/signed_affine_dag/signed_affine_dag_v1.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace bruce {
namespace {

constexpr std::array<AffineOperatorV1, 3> operators{{
    {"add-two-v1", 1, 2}, {"double-v1", 2, 0}, {"subtract-one-v1", 1, -1}}};

const AffineOperatorV1& find_operator(std::string_view id) {
    for (const auto& op : operators) {
        if (op.id == id) return op;
    }
    throw Error("InvalidSignedAffineDagOperator: " + std::string(id));
}

[[noreturn]] void overflow(std::string_view node_id) {
    throw Error("SignedAffineDagArithmeticOverflow: " + std::string(node_id));
}

std::int64_t checked_add(std::int64_t left, std::int64_t right, std::string_view node_id) {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) ||
        (right < 0 && left < minimum - right)) overflow(node_id);
    return left + right;
}

std::int64_t checked_multiply(std::int64_t left, std::int64_t right,
                              std::string_view node_id) {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (left > 0) {
        if ((right > 0 && left > maximum / right) ||
            (right < 0 && right < minimum / left)) overflow(node_id);
    } else if (left < 0) {
        if ((right > 0 && left < minimum / right) ||
            (right < 0 && left < maximum / right)) overflow(node_id);
    }
    return left * right;
}

struct ExecutionPlan {
    std::vector<std::size_t> order;
    std::vector<std::optional<std::size_t>> predecessors;
};

ExecutionPlan validate_and_plan(const SignedAffineDagSpecV1& spec) {
    std::map<std::string_view, std::size_t> ids;
    for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
        const auto& node = spec.nodes[index];
        if (node.node_id.empty() || !ids.emplace(node.node_id, index).second)
            throw Error("InvalidSignedAffineDagNodeId: " + node.node_id);
    }

    std::size_t sources = 0;
    for (const auto& node : spec.nodes) {
        if (node.path_weight != -1 && node.path_weight != 1)
            throw Error("InvalidSignedAffineDagPathWeight: " + node.node_id);
        if (!node.predecessor_node_id) {
            if (!node.operator_id.empty() || !node.source_value)
                throw Error("InvalidSignedAffineDagSource: " + node.node_id);
            ++sources;
        } else {
            if (node.source_value)
                throw Error("InvalidSignedAffineDagSource: " + node.node_id);
            find_operator(node.operator_id);
        }
    }
    if (sources == 0) throw Error("InvalidSignedAffineDagSource: no source");

    ExecutionPlan plan;
    plan.predecessors.resize(spec.nodes.size());
    std::vector<std::vector<std::size_t>> children(spec.nodes.size());
    std::vector<std::size_t> indegrees(spec.nodes.size(), 0);
    std::set<std::pair<std::string_view, std::size_t>> ready;
    for (std::size_t index = 0; index < spec.nodes.size(); ++index) {
        const auto& node = spec.nodes[index];
        if (!node.predecessor_node_id) {
            ready.emplace(node.node_id, index);
            continue;
        }
        const auto predecessor = ids.find(*node.predecessor_node_id);
        if (predecessor == ids.end())
            throw Error("UnknownSignedAffineDagPredecessor: " + node.node_id);
        plan.predecessors[index] = predecessor->second;
        children[predecessor->second].push_back(index);
        indegrees[index] = 1;
    }

    plan.order.reserve(spec.nodes.size());
    while (!ready.empty()) {
        const auto index = ready.begin()->second;
        ready.erase(ready.begin());
        plan.order.push_back(index);
        for (const auto child : children[index]) {
            if (--indegrees[child] == 0) ready.emplace(spec.nodes[child].node_id, child);
        }
    }
    if (plan.order.size() != spec.nodes.size()) throw Error("SignedAffineDagCycle");
    return plan;
}

} // namespace

void validate_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec) {
    validate_and_plan(spec);
}

SignedAffineDagResultV1 execute_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec) {
    const auto plan = validate_and_plan(spec);
    std::vector<std::int64_t> values(spec.nodes.size());
    std::map<std::int64_t, SignedAffineSemanticStateV1> states;
    for (const auto index : plan.order) {
        const auto& node = spec.nodes[index];
        const auto predecessor = plan.predecessors[index];
        const auto input = predecessor ? values[*predecessor] : *node.source_value;
        auto output = input;
        if (predecessor) {
            const auto& op = find_operator(node.operator_id);
            output = checked_add(checked_multiply(op.a, input, node.node_id), op.b, node.node_id);
        }
        values[index] = output;
        auto& state = states[output];
        state.value = output;
        state.weight_balance = checked_add(state.weight_balance, node.path_weight, node.node_id);
        state.paths.push_back({node.node_id, node.predecessor_node_id.value_or(""),
                               node.operator_id, input, output, node.path_weight});
    }

    SignedAffineDagResultV1 result;
    result.states.reserve(states.size());
    for (auto& [value, state] : states) {
        std::sort(state.paths.begin(), state.paths.end(), [](const auto& left, const auto& right) {
            return left.node_id < right.node_id;
        });
        result.states.push_back(std::move(state));
    }
    return result;
}

} // namespace bruce
