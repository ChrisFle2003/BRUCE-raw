#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
struct NormalizedRouteInputV1;
using TreeAddressV1 = std::vector<std::uint32_t>;

enum class HostClaimScopeV1 {
    unknown, candidate_only, verified_within_supported_scope, verified_exact_result,
    explanation_only, research_only, requires_review, denied
};
struct TreeNodeV1 {
    TreeAddressV1 id;
    std::string key;
    std::string template_id;
    std::vector<std::string> aliases; // Metadata; matching uses only route_pattern.
    HostClaimScopeV1 claim_scope = HostClaimScopeV1::unknown;
    std::string owner_id;
    // Empty for structural nodes; other patterns match a complete token sequence.
    std::vector<std::string> route_pattern;
    bool operator==(const TreeNodeV1&) const = default;
};
struct TreeFragmentV1 {
    TreeAddressV1 mount;
    std::vector<TreeNodeV1> nodes;
};
enum class RouteStatusV1 { matched, unknown, ambiguous };
struct RouteResultV1 {
    RouteStatusV1 status = RouteStatusV1::unknown;
    std::vector<TreeNodeV1> candidates;
};

// The Core sentinel [0] represents the logical parent of top-level addresses.
// Registries can only be obtained from Core or by immutable child mounting.
class TreeRegistryV1 {
public:
    const std::vector<TreeNodeV1>& nodes() const noexcept { return nodes_; }
    const TreeNodeV1* find(const TreeAddressV1& address) const noexcept;
    TreeRegistryV1 mount(TreeFragmentV1 fragment, std::string_view owner_id) const;
    RouteResultV1 route(const NormalizedRouteInputV1& input) const;
    std::string digest() const;
private:
    explicit TreeRegistryV1(std::vector<TreeNodeV1> nodes);
    std::vector<TreeNodeV1> nodes_;
    friend TreeRegistryV1 core_tree_registry_v1();
};
TreeRegistryV1 core_tree_registry_v1();
}
