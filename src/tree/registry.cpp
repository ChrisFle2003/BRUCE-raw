#include "bruce/tree/registry.hpp"
#include "bruce/tree/normalizer.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace bruce {
namespace {
void field(std::string& bytes, std::string_view value) {
    bytes += std::to_string(value.size()) + ":";
    bytes += value;
}
void address(std::string& bytes, const TreeAddressV1& id) {
    field(bytes, std::to_string(id.size()));
    for (const auto segment : id) field(bytes, std::to_string(segment));
}
bool digits(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; });
}
bool integer_token(std::string_view value) {
    if (!value.empty() && value.front() == '-') value.remove_prefix(1);
    return digits(value);
}
bool rational_token(std::string_view value) {
    const auto slash = value.find('/');
    if (slash == std::string_view::npos) return integer_token(value);
    const auto denominator = value.substr(slash + 1);
    return integer_token(value.substr(0, slash)) && digits(denominator) &&
        std::any_of(denominator.begin(), denominator.end(), [](char c) { return c != '0'; });
}
}
TreeRegistryV1::TreeRegistryV1(std::vector<TreeNodeV1> nodes) : nodes_(std::move(nodes)) {
    std::sort(nodes_.begin(), nodes_.end(), [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
}
const TreeNodeV1* TreeRegistryV1::find(const TreeAddressV1& id) const noexcept {
    const auto found = std::lower_bound(nodes_.begin(), nodes_.end(), id,
        [](const auto& node, const auto& value) { return node.id < value; });
    return found != nodes_.end() && found->id == id ? &*found : nullptr;
}
TreeRegistryV1 TreeRegistryV1::mount(TreeFragmentV1 fragment, std::string_view owner_id) const {
    if (owner_id.empty() || owner_id == "core") throw Error("InvalidTreeOwner");
    if (!find(fragment.mount)) throw Error("TreeMountNotFound");
    if (fragment.nodes.empty()) throw Error("EmptyTreeFragment");
    const bool root_mount = fragment.mount == TreeAddressV1{0};
    auto combined = nodes_;
    std::set<TreeAddressV1> added;
    for (auto& node : fragment.nodes) {
        if (node.id.empty() || std::find(node.id.begin(), node.id.end(), 0) != node.id.end())
            throw Error("InvalidTreeAddress");
        if (!root_mount && (node.id.size() <= fragment.mount.size() ||
            !std::equal(fragment.mount.begin(), fragment.mount.end(), node.id.begin())))
            throw Error("TreeNodeOutsideMount");
        if (find(node.id) || !added.insert(node.id).second) throw Error("DuplicateTreeAddress");
        if (node.key.empty()) throw Error("InvalidTreeKey");
        if (!node.route_pattern.empty() && node.template_id.empty()) throw Error("MissingRouteTemplate");
        for (const auto& atom : node.route_pattern) {
            if (atom == "<rational>" || atom == "<integer>") continue;
            const auto normalized = normalize_route_text_v1(atom);
            if (normalized.tokens.size() != 1 || normalized.normalized_text != atom ||
                atom.front() == '<' || atom.back() == '>') throw Error("InvalidRoutePattern");
        }
        if (!node.owner_id.empty() && node.owner_id != owner_id) throw Error("InvalidTreeOwner");
        node.owner_id = owner_id;
        combined.push_back(std::move(node));
    }
    TreeRegistryV1 candidate(std::move(combined));
    std::set<std::pair<TreeAddressV1, std::string>> sibling_keys;
    for (const auto& node : candidate.nodes_) {
        if (node.id == TreeAddressV1{0}) continue;
        auto parent = node.id;
        parent.pop_back();
        if (parent.empty()) parent = {0};
        if (!candidate.find(parent)) throw Error("TreeParentNotFound");
        if (!sibling_keys.emplace(parent, node.key).second) throw Error("DuplicateTreeSiblingKey");
    }
    return candidate;
}
RouteResultV1 TreeRegistryV1::route(const NormalizedRouteInputV1& input) const {
    if (input.normalization_version != token_normalization_version_v1) throw Error("UnsupportedRouteNormalization");
    const auto canonical = normalize_route_text_v1(input.normalized_text);
    if (canonical.normalized_text != input.normalized_text || canonical.tokens != input.tokens)
        throw Error("InvalidNormalizedRouteInput");
    RouteResultV1 result;
    for (const auto& node : nodes_) {
        if (node.route_pattern.empty() || node.template_id.empty() || node.route_pattern.size() != input.tokens.size())
            continue;
        bool matches = true;
        for (std::size_t i = 0; i < node.route_pattern.size(); ++i) {
            const auto& atom = node.route_pattern[i];
            const auto& token = input.tokens[i];
            if (!(atom == "<rational>" ? rational_token(token) :
                  atom == "<integer>" ? integer_token(token) : atom == token)) {
                matches = false;
                break;
            }
        }
        if (matches) result.candidates.push_back(node);
    }
    if (result.candidates.size() == 1) result.status = RouteStatusV1::matched;
    else if (result.candidates.size() > 1) result.status = RouteStatusV1::ambiguous;
    return result;
}
std::string TreeRegistryV1::digest() const {
    std::string bytes = "BRUCE_TREE_REGISTRY_V1\n";
    field(bytes, std::to_string(nodes_.size()));
    for (const auto& node : nodes_) {
        address(bytes, node.id);
        field(bytes, node.key);
        field(bytes, node.template_id);
        field(bytes, std::to_string(node.aliases.size()));
        for (const auto& alias : node.aliases) field(bytes, alias);
        field(bytes, std::to_string(static_cast<unsigned>(node.claim_scope)));
        field(bytes, node.owner_id);
        field(bytes, std::to_string(node.route_pattern.size()));
        for (const auto& atom : node.route_pattern) field(bytes, atom);
    }
    return sha256(bytes);
}
TreeRegistryV1 core_tree_registry_v1() {
    return TreeRegistryV1({
        {{0}, "root", "", {}, HostClaimScopeV1::unknown, "core", {}},
        {{1}, "math", "", {}, HostClaimScopeV1::unknown, "core", {}},
        {{1,1}, "rational", "", {}, HostClaimScopeV1::unknown, "core", {}},
        {{1,1,1}, "add", "math.rational.add.v1", {}, HostClaimScopeV1::unknown, "core", {"<rational>", "plus", "<rational>"}},
        {{1,1,2}, "subtract", "math.rational.subtract.v1", {}, HostClaimScopeV1::unknown, "core", {"<rational>", "minus", "<rational>"}},
        {{1,1,3}, "multiply", "math.rational.multiply.v1", {}, HostClaimScopeV1::unknown, "core", {"<rational>", "mal", "<rational>"}},
        {{1,1,4}, "divide", "math.rational.divide.v1", {}, HostClaimScopeV1::unknown, "core", {"<rational>", "durch", "<rational>"}}
    });
}
}
