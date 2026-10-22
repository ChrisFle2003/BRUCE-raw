#include "bruce/mod/catalog.hpp"

#if BRUCE_BUILD_MOD_MCP
#include "bruce/mods/mcp/mod.hpp"
#endif
#if BRUCE_BUILD_MOD_LANGUAGE
#include "bruce/mods/language/mod.hpp"
#endif
#if BRUCE_BUILD_MOD_NANOCELL
#include "bruce/mods/nanocell/mod.hpp"
#endif

namespace bruce {

ModCatalogV1::ModCatalogV1(const std::span<const ModManifestV1> manifests)
    : manifests_(manifests.begin(), manifests.end()) {
    for (std::size_t index = 0; index < manifests_.size(); ++index) {
        validate_mod_manifest(manifests_[index]);
        if (index != 0 && manifests_[index - 1].id >= manifests_[index].id) {
            throw Error("NoncanonicalModCatalog");
        }
    }
}

std::span<const ModManifestV1> ModCatalogV1::manifests() const {
    return manifests_;
}

const ModManifestV1* ModCatalogV1::find(const std::string_view id) const {
    for (const auto& manifest : manifests_) {
        if (manifest.id == id) return &manifest;
    }
    return nullptr;
}

const ModCatalogV1& builtin_mod_catalog_v1() {
    static const std::vector<ModManifestV1> manifests{
#if BRUCE_BUILD_MOD_LANGUAGE
        language_mod_manifest_v1(),
#endif
#if BRUCE_BUILD_MOD_MCP
        mcp_mod_manifest_v1(),
#endif
#if BRUCE_BUILD_MOD_NANOCELL
        nanocell_mod_manifest_v1(),
#endif
    };
    static const ModCatalogV1 catalog(manifests);
    return catalog;
}

std::span<const BuiltinModFeatureV1> builtin_mod_features_v1() {
    static const std::vector<BuiltinModFeatureV1> features{
        {
            .id = "language",
            .version = "language-mod-v1",
            .build_state = BRUCE_BUILD_MOD_LANGUAGE
                ? BuiltinModBuildStateV1::available
                : BuiltinModBuildStateV1::not_built,
            .capabilities = {
                ModCapabilityV1::language_interpretation,
                ModCapabilityV1::core_task_submission,
            },
        },
        {
            .id = "mcp",
            .version = "mcp-mod-v1",
            .build_state = BRUCE_BUILD_MOD_MCP
                ? BuiltinModBuildStateV1::available
                : BuiltinModBuildStateV1::not_built,
            .capabilities = {ModCapabilityV1::presentation},
        },
        {
            .id = "nanocell",
            .version = "nanocell-mod-v1",
            .build_state = BRUCE_BUILD_MOD_NANOCELL
                ? BuiltinModBuildStateV1::available
                : BuiltinModBuildStateV1::not_built,
            .capabilities = {ModCapabilityV1::nanocell_execution},
        },
    };
    return features;
}

} // namespace bruce
