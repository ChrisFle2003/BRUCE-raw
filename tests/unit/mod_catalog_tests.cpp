#include "bruce/mod/catalog.hpp"
#include "bruce/mods/language/mod.hpp"
#include "bruce/mods/nanocell/mod.hpp"

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
    const auto& language_manifest = bruce::language_mod_manifest_v1();
    const auto& nanocell_manifest = bruce::nanocell_mod_manifest_v1();
    const auto& catalog = bruce::builtin_mod_catalog_v1();
    const auto manifests = catalog.manifests();

    require(manifests.size() == 2, "the enabled built-in catalog contains Language-Mod and NanoCell-Mod");
    require(manifests[0] == language_manifest, "catalog first entry exactly matches Language-Mod manifest");
    require(manifests[1] == nanocell_manifest, "catalog second entry exactly matches NanoCell-Mod manifest");
    require(catalog.find("language") != nullptr, "Language-Mod is discoverable by ID");
    require(catalog.find("language")->api_version == bruce::mod_api_version_v1,
            "Language-Mod uses the stable V1 API");
    require(catalog.find("nanocell") != nullptr, "NanoCell-Mod is discoverable by ID");
    require(catalog.find("nanocell")->api_version == bruce::mod_api_version_v1,
            "NanoCell-Mod uses the stable V1 API");
    require(catalog.find("unknown") == nullptr, "unknown mod IDs are absent");

    const std::array duplicate{language_manifest, language_manifest};
    require(rejects([&] { static_cast<void>(bruce::ModCatalogV1(duplicate)); }),
            "duplicate manifests are rejected");

    auto earlier = language_manifest;
    earlier.id = "alpha";
    const std::array unsorted{language_manifest, earlier};
    require(rejects([&] { static_cast<void>(bruce::ModCatalogV1(unsorted)); }),
            "catalog manifests must be bytewise sorted by ID");

    return 0;
}
