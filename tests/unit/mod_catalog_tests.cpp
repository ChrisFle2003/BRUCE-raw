#include "bruce/mod/catalog.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool rejects(const auto& action) {
    try {
        action();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

void require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const auto features = bruce::builtin_mod_features_v1();
    require(features.size() == 3, "all stable built-in IDs are present in every build");
    require(features[0].id == "language" && features[0].version == "language-mod-v1",
            "Language identity is stable");
    require(features[1].id == "mcp" && features[1].version == "mcp-mod-v1",
            "MCP identity is stable");
    require(features[2].id == "nanocell" && features[2].version == "nanocell-mod-v1",
            "NanoCell identity is stable");

    const std::array expected_available{
        BRUCE_BUILD_MOD_LANGUAGE != 0,
        BRUCE_BUILD_MOD_MCP != 0,
        BRUCE_BUILD_MOD_NANOCELL != 0,
    };
    const auto& catalog = bruce::builtin_mod_catalog_v1();
    std::size_t available_count = 0;
    for (std::size_t index = 0; index < features.size(); ++index) {
        const bool available = features[index].build_state == bruce::BuiltinModBuildStateV1::available;
        require(available == expected_available[index], "feature state matches its compile flag");
        const auto* manifest = catalog.find(features[index].id);
        require((manifest != nullptr) == available, "only available features have V1 manifests");
        if (manifest != nullptr) {
            ++available_count;
            require(manifest->version == features[index].version, "feature and manifest versions agree");
            require(manifest->capabilities == features[index].capabilities,
                    "feature and manifest capabilities agree");
        }
    }
    require(catalog.manifests().size() == available_count, "catalog has no hidden manifests");
    require(catalog.find("unknown") == nullptr, "unknown mod IDs are absent");

    bruce::ModManifestV1 duplicate_manifest{
        .id = "duplicate",
        .version = "duplicate-v1",
        .api_version = std::string(bruce::mod_api_version_v1),
        .capabilities = {bruce::ModCapabilityV1::presentation},
    };
    const std::array duplicate{duplicate_manifest, duplicate_manifest};
    require(rejects([&] { static_cast<void>(bruce::ModCatalogV1(duplicate)); }),
            "duplicate manifests are rejected");

    auto earlier = duplicate_manifest;
    earlier.id = "alpha";
    const std::array unsorted{duplicate_manifest, earlier};
    require(rejects([&] { static_cast<void>(bruce::ModCatalogV1(unsorted)); }),
            "catalog manifests must be bytewise sorted by ID");

    return 0;
}
