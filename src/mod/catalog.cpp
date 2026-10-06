#include "bruce/mod/catalog.hpp"

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
#if BRUCE_BUILD_MOD_LANGUAGE && BRUCE_BUILD_MOD_NANOCELL
    static const ModManifestV1 manifests[] = {
        language_mod_manifest_v1(),
        nanocell_mod_manifest_v1(),
    };
    static const ModCatalogV1 catalog(manifests);
#elif BRUCE_BUILD_MOD_LANGUAGE
    static const ModManifestV1 manifests[] = {language_mod_manifest_v1()};
    static const ModCatalogV1 catalog(manifests);
#elif BRUCE_BUILD_MOD_NANOCELL
    static const ModManifestV1 manifests[] = {nanocell_mod_manifest_v1()};
    static const ModCatalogV1 catalog(manifests);
#else
    static const ModCatalogV1 catalog(std::span<const ModManifestV1>{});
#endif
    return catalog;
}

} // namespace bruce
