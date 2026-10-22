#include "bruce/mod/unified_catalog_v2.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] bool rejects(const auto& action) {
    try {
        action();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

} // namespace

int main() {
    const std::array features{
        bruce::BuiltinModFeatureV1{
            .id = "language",
            .version = "language-mod-v1",
            .build_state = bruce::BuiltinModBuildStateV1::not_built,
            .capabilities = {
                bruce::ModCapabilityV1::language_interpretation,
                bruce::ModCapabilityV1::core_task_submission,
            },
        },
    };
    const std::array installed{
        bruce::InstalledModProjectionV2{
            .id = "addon",
            .version = "v2",
            .kind = bruce::ModKindV2::logic,
            .lifecycle_state = "active",
            .package_digest = std::string(64, 'a'),
            .capabilities = {"tree.extend", "dataset.read"},
        },
    };

    const auto catalog = bruce::make_unified_mod_catalog_v2(features, installed);
    require(catalog.entries.size() == 2, "built-in and installed entries are projected");
    require(catalog.entries[0].id == "addon", "entries sort by ID");
    require(catalog.entries[0].origin == bruce::ModOriginV2::installed,
            "installed origin is explicit");
    require(!catalog.entries[0].build_state.has_value(), "installed entries have null build state");
    require(catalog.entries[0].capabilities == std::vector<std::string>({"dataset.read", "tree.extend"}),
            "installed capabilities are canonicalized");
    require(catalog.entries[1].origin == bruce::ModOriginV2::builtin,
            "built-in origin is explicit");
    require(catalog.entries[1].kind == bruce::ModKindV2::builtin,
            "built-in kind is explicit");
    require(catalog.entries[1].build_state == bruce::BuiltinModBuildStateV1::not_built,
            "not-built state is retained");
    require(!catalog.entries[1].lifecycle_state.has_value() &&
            !catalog.entries[1].package_digest.has_value(),
            "built-ins have no lifecycle state or package digest");

    const std::string expected =
        "{\"contract\":\"bruce-mod-catalog-v2\",\"entries\":["
        "{\"id\":\"addon\",\"version\":\"v2\",\"origin\":\"installed\",\"kind\":\"logic\","
        "\"build_state\":null,\"lifecycle_state\":\"active\",\"package_digest\":\"" +
        std::string(64, 'a') +
        "\",\"capabilities\":[\"dataset.read\",\"tree.extend\"]},"
        "{\"id\":\"language\",\"version\":\"language-mod-v1\",\"origin\":\"builtin\",\"kind\":\"builtin\","
        "\"build_state\":\"not_built\",\"lifecycle_state\":null,\"package_digest\":null,"
        "\"capabilities\":[\"core.task.submit\",\"language.interpret\"]}]}";
    require(bruce::render_unified_mod_catalog_v2(catalog) == expected,
            "catalog rendering is canonical and semantic");
    require(bruce::render_unified_mod_catalog_v2(catalog) ==
            bruce::render_unified_mod_catalog_v2(catalog), "rendering is deterministic");

    auto duplicate_capability = installed;
    duplicate_capability[0].capabilities = {"dataset.read", "dataset.read"};
    require(rejects([&] { (void)bruce::make_unified_mod_catalog_v2(features, duplicate_capability); }),
            "duplicate capabilities are rejected");

    auto invalid_kind = installed;
    invalid_kind[0].kind = bruce::ModKindV2::builtin;
    require(rejects([&] { (void)bruce::make_unified_mod_catalog_v2(features, invalid_kind); }),
            "installed entries cannot masquerade as built-ins");

    const std::array duplicate_identity{installed[0], installed[0]};
    require(rejects([&] { (void)bruce::make_unified_mod_catalog_v2(features, duplicate_identity); }),
            "duplicate installed identities are rejected");

    return 0;
}
