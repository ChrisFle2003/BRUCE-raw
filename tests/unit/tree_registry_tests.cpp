#include "bruce/tree/registry.hpp"
#include "bruce/tree/normalizer.hpp"
#include "bruce/core/types.hpp"
#include <iostream>
#include <utility>

namespace {
void require(bool value, const char* message) {
    if (!value) throw bruce::Error(message);
}
bruce::TreeNodeV1 node(bruce::TreeAddressV1 id, std::string key) {
    bruce::TreeNodeV1 result;
    result.id = std::move(id);
    result.key = std::move(key);
    result.template_id = "math.rational.power.v1";
    return result;
}
void canonical_core_and_child_mount() {
    const auto core = bruce::core_tree_registry_v1();
    require(core.nodes().size() == 7, "Core tree is incomplete");
    for (const auto& address : std::vector<bruce::TreeAddressV1>{{0}, {1}, {1,1}, {1,1,1}, {1,1,2}, {1,1,3}, {1,1,4}}) {
        const auto* found = core.find(address);
        require(found && found->owner_id == "core", "Missing Core-owned address");
    }
    const auto original_digest = core.digest();
    const auto extended = core.mount({{1,1}, {node({1,1,5}, "power")}}, "user.power");
    require(extended.find({1,1,5})->owner_id == "user.power", "Child ownership was not assigned");
    require(!core.find({1,1,5}) && core.digest() == original_digest, "Mount mutated original tree");
    require(extended.digest() != original_digest && extended.digest().size() == 64, "Mount did not bind new tree to digest");
}
void equivalent_insertion_order_has_one_digest() {
    const auto core = bruce::core_tree_registry_v1();
    auto parent = node({1,1,5}, "power");
    auto child = node({1,1,5,1}, "integer");
    const auto first = core.mount({{1,1}, {parent, child}}, "user.power");
    const auto second = core.mount({{1,1}, {child, parent}}, "user.power");
    require(first.digest() == second.digest(), "Insertion order changed canonical digest");
    auto changed = parent;
    changed.template_id = "math.rational.power.v2";
    require(core.mount({{1,1}, {changed, child}}, "user.power").digest() != first.digest(), "Template change absent from digest");
    changed = parent;
    changed.aliases = {"hoch"};
    require(core.mount({{1,1}, {changed, child}}, "user.power").digest() != first.digest(), "Alias change absent from digest");
    changed = parent;
    changed.claim_scope = bruce::HostClaimScopeV1::candidate_only;
    require(core.mount({{1,1}, {changed, child}}, "user.power").digest() != first.digest(), "Claim scope absent from digest");
    require(core.mount({{1,1}, {parent, child}}, "user.other").digest() != first.digest(), "Owner absent from digest");
}
void invalid_mounts_preserve_original_registry() {
    const auto core = bruce::core_tree_registry_v1();
    const auto before = core.digest();
    const std::vector<bruce::TreeFragmentV1> invalid = {
        {{1,1}, {}}, // A successful mount must add nodes and change the digest.
        {{1,1}, {node({1,1,1}, "overwrite")}}, // Protected Core address.
        {{1,1}, {node({}, "empty")}},
        {{1,1}, {node({0}, "sentinel")}},
        {{1,1}, {node({1,1,0}, "zero")}},
        {{1,1}, {node({1,1,5,0}, "nested-zero")}},
        {{1,1}, {node({1,1,5,1}, "orphan")}},
        {{1,2}, {node({1,2,1}, "missing-mount")}},
        {{}, {node({1,1,5}, "empty-mount")}},
        {{1,1}, {node({1,2}, "sibling")}},
        {{1,1}, {node({1,1}, "self")}},
        {{1,1}, {node({1}, "ancestor")}},
        {{1,1}, {node({1,1,5}, "power"), node({1,1,5}, "other")}},
        {{1,1}, {node({1,1,5}, "add")}}, // Existing key under the same parent.
        {{1,1}, {node({1,1,5}, "power"), node({1,1,6}, "power")}},
        {{1,1}, {node({1,1,5}, "")}}
    };
    for (const auto& fragment : invalid) {
        bool rejected = false;
        try { (void)core.mount(fragment, "user.power"); }
        catch (const bruce::Error&) { rejected = true; }
        require(rejected, "Invalid fragment accepted");
        require(core.digest() == before && core.nodes().size() == 7, "Rejected mount mutated original registry");
    }
    for (const auto* owner : {"", "core"}) {
        bool rejected = false;
        try { (void)core.mount({{1,1}, {node({1,1,5}, "power")}}, owner); }
        catch (const bruce::Error&) { rejected = true; }
        require(rejected, "Invalid mod ownership accepted");
    }
    auto forged = node({1,1,5}, "power");
    forged.owner_id = "core";
    bool rejected = false;
    try { (void)core.mount({{1,1}, {forged}}, "user.power"); }
    catch (const bruce::Error&) { rejected = true; }
    require(rejected, "Forged node ownership accepted");
}
void foreign_nodes_and_nested_mounts() {
    const auto original = bruce::core_tree_registry_v1().mount({{1,1}, {node({1,1,5}, "power")}}, "user.power");
    const auto before = original.digest();
    bool rejected = false;
    try { (void)original.mount({{1,1}, {node({1,1,5}, "replacement")}}, "user.other"); }
    catch (const bruce::Error&) { rejected = true; }
    require(rejected && original.digest() == before, "Foreign overwrite accepted or mutated registry");
    const auto nested = original.mount({{1,1,5}, {node({1,1,5,1}, "integer")}}, "user.other");
    require(nested.find({1,1,5,1})->owner_id == "user.other", "Valid foreign child extension rejected");
    const auto root_extension = original.mount({{0}, {node({2}, "science"), node({2,1}, "power")}}, "user.science");
    require(root_extension.find({2,1}) && !original.find({2}), "Logical root extension failed");
}
void controlled_core_routes() {
    const auto core = bruce::core_tree_registry_v1();
    const auto normalized = bruce::normalize_route_text_v1(" \t3/4 PLUS\r\n5/6 \v");
    require(normalized.normalized_text == "3/4 plus 5/6", "ASCII normalization changed controlled input");
    require(normalized.tokens == std::vector<std::string>{"3/4", "plus", "5/6"}, "Wrong route tokens");
    const auto addition = core.route(normalized);
    require(addition.status == bruce::RouteStatusV1::matched && addition.candidates.size() == 1,
        "Addition did not produce one match");
    require(addition.candidates[0].id == bruce::TreeAddressV1{1,1,1} &&
        addition.candidates[0].template_id == "math.rational.add.v1", "Wrong addition candidate");
    for (const auto& [input, id] : std::vector<std::pair<std::string, bruce::TreeAddressV1>>{
        {"-3/4 minus 5", {1,1,2}}, {"3 mal -5/6", {1,1,3}}, {"3/4 durch 5/6", {1,1,4}}}) {
        const auto result = core.route(bruce::normalize_route_text_v1(input));
        require(result.status == bruce::RouteStatusV1::matched && result.candidates.size() == 1 &&
            result.candidates[0].id == id,
            "Canonical Core operation not matched");
    }
    for (const auto& input : {"", "3/4 adds 5/6", "3/4 plsu 5/6", "plus 3/4 5/6",
        "3/4plus5/6", "3/4 plus 5/6 extra", "3/0 plus 1", "3/-4 plus 1", "+3 plus 1",
        "3.0 plus 1", "3//4 plus 1", "--3 plus 1", "3/4 plus 5/6!", "3/4\xc2\xa0plus 5/6"}) {
        const auto result = core.route(bruce::normalize_route_text_v1(input));
        require(result.status == bruce::RouteStatusV1::unknown && result.candidates.empty(),
            "Unsupported input was inferred or accepted");
    }
}
void mod_patterns_and_collisions() {
    auto power = node({1,1,5}, "power");
    power.aliases = {"hoch"};
    power.route_pattern = {"<rational>", "hoch", "<integer>"};
    const auto core = bruce::core_tree_registry_v1();
    const auto first = core.mount({{1,1}, {power}}, "user.power");
    const auto result = first.route(bruce::normalize_route_text_v1("3/4 hoch 2"));
    require(result.status == bruce::RouteStatusV1::matched && result.candidates.size() == 1 &&
        result.candidates[0].id == bruce::TreeAddressV1{1,1,5}, "Mounted power route absent");
    require(first.route(bruce::normalize_route_text_v1("3/4 hoch 2/1")).status == bruce::RouteStatusV1::unknown,
        "Fraction matched integer placeholder");
    require(first.route(bruce::normalize_route_text_v1("3/4 potenz 2")).status == bruce::RouteStatusV1::unknown,
        "Undeclared synonym inferred");
    auto other = power;
    other.id = {1,1,6};
    other.key = "other-power";
    other.template_id = "math.rational.other-power.v1";
    const auto collision = first.mount({{1,1}, {other}}, "user.other");
    const auto ambiguous = collision.route(bruce::normalize_route_text_v1("3/4 hoch 2"));
    require(ambiguous.status == bruce::RouteStatusV1::ambiguous && ambiguous.candidates.size() == 2,
        "Collision chose a template or lost candidates");
    require(ambiguous.candidates[0].id == bruce::TreeAddressV1{1,1,5} &&
        ambiguous.candidates[1].id == bruce::TreeAddressV1{1,1,6} &&
        ambiguous.candidates[0].owner_id == "user.power" && ambiguous.candidates[1].owner_id == "user.other",
        "Ambiguous result omitted mod addresses or owners");
    auto changed = power;
    changed.route_pattern[1] = "potenz";
    require(core.mount({{1,1}, {changed}}, "user.power").digest() != first.digest(),
        "Route grammar change absent from registry digest");
    require(core.route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::unknown,
        "Mounted route leaked into original registry");
}
void invalid_patterns_are_rejected() {
    const auto core = bruce::core_tree_registry_v1();
    for (const auto& pattern : std::vector<std::vector<std::string>>{
        {"<rational>", "HOCH", "<integer>"}, {"<rational>", " hoch", "<integer>"},
        {"<rational>", "", "<integer>"}, {"<rational>", "hoch zwei", "<integer>"},
        {"<decimal>", "hoch", "<integer>"}}) {
        auto power = node({1,1,5}, "power");
        power.route_pattern = pattern;
        bool rejected = false;
        try { (void)core.mount({{1,1}, {power}}, "user.power"); }
        catch (const bruce::Error&) { rejected = true; }
        require(rejected, "Noncanonical or unsupported route pattern accepted");
    }
    auto power = node({1,1,5}, "power");
    power.route_pattern = {"<rational>", "hoch", "<integer>"};
    power.template_id.clear();
    bool rejected = false;
    try { (void)core.mount({{1,1}, {power}}, "user.power"); }
    catch (const bruce::Error&) { rejected = true; }
    require(rejected, "Routed node without template accepted");
}
void normalization_boundaries_and_numeric_overlap() {
    const auto normalized = bruce::normalize_route_text_v1(" Ä A\fB\vC ");
    require(normalized.normalized_text == "Ä a b c", "Normalizer changed non-ASCII bytes or missed whitespace");
    for (const auto& text : std::vector<std::string>{std::string(1, '\xff'), "\xc0\xaf", "\xe2\x82",
        "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x28\xa1"}) {
        bool rejected = false;
        try { (void)bruce::normalize_route_text_v1(text); }
        catch (const bruce::Error&) { rejected = true; }
        require(rejected, "Malformed UTF-8 accepted");
    }
    const auto core = bruce::core_tree_registry_v1();
    auto input = bruce::normalize_route_text_v1("3/4 plus 5/6");
    input.normalization_version = "unrecognized-v2";
    bool rejected = false;
    try { (void)core.route(input); } catch (const bruce::Error&) { rejected = true; }
    require(rejected, "Unsupported normalization version accepted");
    input = bruce::normalize_route_text_v1("3/4 plus 5/6");
    input.tokens[1] = "minus";
    rejected = false;
    try { (void)core.route(input); } catch (const bruce::Error&) { rejected = true; }
    require(rejected, "Forged normalized tokens accepted");
    require(core.route(bruce::normalize_route_text_v1("-0003/0004 plus 9999999999999999999999999999999"))
        .status == bruce::RouteStatusV1::matched, "Literal digit syntax was restricted to machine integers");
    require(core.route(bruce::normalize_route_text_v1("3/000 plus 1")).status == bruce::RouteStatusV1::unknown,
        "Zero denominator with leading zeroes accepted");
    auto integer = node({1,1,5}, "integer-add");
    integer.route_pattern = {"<integer>", "plus", "<integer>"};
    const auto result = core.mount({{1,1}, {integer}}, "user.integer").route(bruce::normalize_route_text_v1("3 plus 5"));
    require(result.status == bruce::RouteStatusV1::ambiguous && result.candidates.size() == 2 &&
        result.candidates[0].id == bruce::TreeAddressV1{1,1,1} && result.candidates[1].id == bruce::TreeAddressV1{1,1,5},
        "Overlapping numeric types selected a preferred route");
}
}
int main() {
    try {
        canonical_core_and_child_mount();
        equivalent_insertion_order_has_one_digest();
        invalid_mounts_preserve_original_registry();
        foreign_nodes_and_nested_mounts();
        controlled_core_routes();
        mod_patterns_and_collisions();
        invalid_patterns_are_rejected();
        normalization_boundaries_and_numeric_overlap();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
