#pragma once

#include "bruce/mod/catalog.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bruce {

enum class ModOriginV2 { builtin, installed };
enum class ModKindV2 { builtin, data, logic };

struct InstalledModProjectionV2 {
    std::string id;
    std::string version;
    ModKindV2 kind = ModKindV2::data;
    std::string lifecycle_state;
    std::string package_digest;
    std::vector<std::string> capabilities;
};

struct UnifiedModEntryV2 {
    std::string id;
    std::string version;
    ModOriginV2 origin = ModOriginV2::builtin;
    ModKindV2 kind = ModKindV2::builtin;
    std::optional<BuiltinModBuildStateV1> build_state;
    std::optional<std::string> lifecycle_state;
    std::optional<std::string> package_digest;
    std::vector<std::string> capabilities;

    bool operator==(const UnifiedModEntryV2&) const = default;
};

struct UnifiedModCatalogV2 {
    std::vector<UnifiedModEntryV2> entries;
};

[[nodiscard]] UnifiedModCatalogV2 make_unified_mod_catalog_v2(
    std::span<const BuiltinModFeatureV1> features,
    std::span<const InstalledModProjectionV2> installed);
[[nodiscard]] std::string render_unified_mod_catalog_v2(const UnifiedModCatalogV2& catalog);

} // namespace bruce
