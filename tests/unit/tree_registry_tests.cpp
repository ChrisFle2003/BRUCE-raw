#include "bruce/tree/registry.hpp"
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
}
int main() {
    try {
        canonical_core_and_child_mount();
        equivalent_insertion_order_has_one_digest();
        invalid_mounts_preserve_original_registry();
        foreign_nodes_and_nested_mounts();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
