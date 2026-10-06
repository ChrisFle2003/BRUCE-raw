#pragma once

#include "bruce/mod/mod_api.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace bruce {

class ModCatalogV1 {
public:
    explicit ModCatalogV1(std::span<const ModManifestV1> manifests);

    [[nodiscard]] std::span<const ModManifestV1> manifests() const;
    [[nodiscard]] const ModManifestV1* find(std::string_view id) const;

private:
    std::vector<ModManifestV1> manifests_;
};

[[nodiscard]] const ModCatalogV1& builtin_mod_catalog_v1();

} // namespace bruce
