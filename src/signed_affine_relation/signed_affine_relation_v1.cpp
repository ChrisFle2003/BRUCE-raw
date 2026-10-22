#include "bruce/signed_affine_relation/signed_affine_relation_v1.hpp"

#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace bruce {
namespace {

[[noreturn]] void invalid_trace(std::string_view node_id) {
    throw Error("InvalidSignedAffineRelationTrace: " + std::string(node_id));
}
[[noreturn]] void overflow(std::string_view node_id) {
    throw Error("SignedAffineRelationCoefficientOverflow: " + std::string(node_id));
}
std::int64_t checked_add(std::int64_t left, std::int64_t right, std::string_view node_id) {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) ||
        (right < 0 && left < minimum - right)) overflow(node_id);
    return left + right;
}
std::int64_t checked_multiply(std::int64_t left, std::int64_t right, std::string_view node_id) {
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
std::uint32_t normalize(std::int64_t value, std::uint32_t modulus) {
    const auto remainder = value % static_cast<std::int64_t>(modulus);
    return static_cast<std::uint32_t>(remainder < 0 ? remainder + modulus : remainder);
}
struct Form {
    std::int64_t multiplier, offset;
};

} // namespace

SignedAffineRelationResultV1 derive_signed_affine_relation_v1(
    const SignedAffineRelationRequestV1& request) {
    // This single execution supplies every path and precedes bridge validation.
    const auto trace = execute_signed_affine_dag_v1(request.spec);
    std::map<std::string_view, const SignedAffineExecutionNodeV1*> nodes;
    for (const auto& node : request.spec.nodes) nodes.emplace(node.node_id, &node);
    std::map<std::string_view, const SignedAffinePathRecordV1*> records;
    const SignedAffinePathRecordV1* source = nullptr;
    std::size_t source_count = 0;
    for (const auto& state : trace.states) {
        for (const auto& path : state.paths) {
            const auto node = nodes.find(path.node_id);
            if (path.node_id.empty() || !records.emplace(path.node_id, &path).second ||
                path.output_value != state.value || node == nodes.end()) invalid_trace(path.node_id);
            const auto& expected = *node->second;
            if (path.predecessor_node_id != expected.predecessor_node_id.value_or("") ||
                path.operator_id != expected.operator_id || path.path_weight != expected.path_weight)
                invalid_trace(path.node_id);
            if (path.predecessor_node_id.empty()) {
                ++source_count;
                source = &path;
                if (!expected.source_value || path.input_value != *expected.source_value ||
                    !path.operator_id.empty() || path.output_value != path.input_value)
                    invalid_trace(path.node_id);
            }
        }
    }
    if (records.size() != nodes.size()) invalid_trace("incomplete trace");
    if (source_count != 1) throw Error("InvalidSignedAffineRelationSourceCount");
    if (request.left_node_id.empty() || request.right_node_id.empty() ||
        request.left_node_id == request.right_node_id)
        throw Error("InvalidSignedAffineRelationSelection");
    for (const auto& id : {request.left_node_id, request.right_node_id}) {
        if (!records.contains(id)) throw Error("UnknownSignedAffineRelationNode: " + id);
    }

    // Semantic states are ordered by value, so follow predecessors before unwinding.
    std::map<std::string_view, Form> forms;
    std::set<std::string_view> resolving;
    const auto reconstruct = [&](std::string_view id) -> Form {
        if (const auto cached = forms.find(id); cached != forms.end()) return cached->second;
        std::vector<const SignedAffinePathRecordV1*> pending;
        auto predecessor = id;
        while (!forms.contains(predecessor)) {
            const auto found = records.find(predecessor);
            if (found == records.end() || !resolving.insert(predecessor).second)
                invalid_trace(predecessor);
            const auto& path = *found->second;
            pending.push_back(&path);
            if (path.predecessor_node_id.empty()) break;
            predecessor = path.predecessor_node_id;
        }
        while (!pending.empty()) {
            const auto& path = *pending.back();
            const std::string_view current_id = path.node_id;
            Form form{1, 0};
            if (!path.predecessor_node_id.empty()) {
                form = forms.at(path.predecessor_node_id);
                if (path.input_value != records.at(path.predecessor_node_id)->output_value)
                    invalid_trace(current_id);
                if (path.operator_id == "add-two-v1") {
                    form.offset = checked_add(form.offset, 2, current_id);
                } else if (path.operator_id == "double-v1") {
                    form.multiplier = checked_multiply(form.multiplier, 2, current_id);
                    form.offset = checked_multiply(form.offset, 2, current_id);
                } else if (path.operator_id == "subtract-one-v1") {
                    form.offset = checked_add(form.offset, -1, current_id);
                } else {
                    invalid_trace(current_id);
                }
            } else if (path.node_id != source->node_id) {
                invalid_trace(current_id);
            }
            const auto value = checked_add(
                checked_multiply(form.multiplier, source->output_value, current_id),
                form.offset, current_id);
            if (value != path.output_value) invalid_trace(current_id);
            resolving.erase(current_id);
            forms.emplace(current_id, form);
            pending.pop_back();
        }
        return forms.at(id);
    };
    for (const auto& [id, path] : records) {
        (void)path;
        reconstruct(id);
    }
    const auto& left_record = *records.at(request.left_node_id);
    const auto& right_record = *records.at(request.right_node_id);
    if (left_record.output_value != right_record.output_value)
        throw Error("NoSignedAffineRelationCollision");

    ToyCyclicGroupV1 group{request.modulus, 1, 0};
    // Delegate modulus validation before any signed remainder (including n=0).
    make_toy_coefficient_state_v1(group, 0, 0);
    group.public_q = normalize(source->output_value, group.modulus);
    const auto derive_path = [&](const SignedAffinePathRecordV1& path) {
        const auto form = forms.at(path.node_id);
        return SignedAffineDerivedPathV1{path.node_id, source->node_id, source->output_value,
            form.multiplier, form.offset, path.output_value,
            {path.node_id, make_toy_coefficient_state_v1(group, normalize(form.offset, group.modulus),
                                                       normalize(form.multiplier, group.modulus))}};
    };
    auto left = derive_path(left_record), right = derive_path(right_record);
    const auto collision = [&] {
        try { return detect_toy_relation_collision_v1(left.toy_provenance, right.toy_provenance); }
        catch (const Error& error) {
            if (std::string_view(error.what()) == "NoToyRelationCollision")
                throw Error("NoSignedAffineRelationCollision");
            throw;
        }
    }();
    const auto relation = derive_toy_relation_v1(group, left.toy_provenance, right.toy_provenance, collision);
    const auto verified = verify_toy_relation_v1(group, left.toy_provenance, right.toy_provenance, relation);
    return {group, std::move(left), std::move(right), collision, verified};
}

} // namespace bruce
