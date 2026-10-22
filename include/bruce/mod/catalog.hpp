#pragma once

#include "bruce/mod/mod_api.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace bruce {

enum class BuiltinModBuildStateV1 { not_built, available };

struct BuiltinModFeatureV1 {
    std::string id;
    std::string version;
    BuiltinModBuildStateV1 build_state = BuiltinModBuildStateV1::not_built;
    std::vector<ModCapabilityV1> capabilities;
};

class ModCatalogV1 {
public:
    explicit ModCatalogV1(std::span<const ModManifestV1> manifests);

    [[nodiscard]] std::span<const ModManifestV1> manifests() const;
    [[nodiscard]] const ModManifestV1* find(std::string_view id) const;

private:
    std::vector<ModManifestV1> manifests_;
};

[[nodiscard]] const ModCatalogV1& builtin_mod_catalog_v1();
[[nodiscard]] std::span<const BuiltinModFeatureV1> builtin_mod_features_v1();

} // namespace bruce
