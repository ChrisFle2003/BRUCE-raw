#include "bruce/mod/unified_catalog_v2.hpp"

#include "bruce/core/json.hpp"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace bruce {
namespace {

[[noreturn]] void invalid() { throw Error("InvalidUnifiedModCatalogV2"); }

[[nodiscard]] bool valid_token(const std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    return std::ranges::all_of(value, [](const unsigned char ch) {
        return std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_';
    });
}

[[nodiscard]] bool valid_digest(const std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](const char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

[[nodiscard]] std::string capability_name(const ModCapabilityV1 capability) {
    switch (capability) {
    case ModCapabilityV1::language_interpretation: return "language.interpret";
    case ModCapabilityV1::core_task_submission: return "core.task.submit";
    case ModCapabilityV1::presentation: return "presentation";
    case ModCapabilityV1::phase_execution: return "phase.execute";
    case ModCapabilityV1::nanocell_execution: return "nanocell.execute";
    }
    invalid();
}

void canonicalize_capabilities(std::vector<std::string>& capabilities) {
    for (const auto& capability : capabilities) if (!valid_token(capability)) invalid();
    std::ranges::sort(capabilities);
    if (std::ranges::adjacent_find(capabilities) != capabilities.end()) invalid();
}

[[nodiscard]] bool valid_lifecycle(const std::string_view state) {
    return state == "installed_inactive" || state == "active" ||
        state == "disabled" || state == "revoked";
}

[[nodiscard]] auto key(const UnifiedModEntryV2& entry) {
    return std::tie(entry.id, entry.origin, entry.kind, entry.version, entry.package_digest);
}

void validate_entry(const UnifiedModEntryV2& entry) {
    if (!valid_token(entry.id) || !valid_token(entry.version) || entry.capabilities.size() > 16) invalid();
    auto capabilities = entry.capabilities;
    canonicalize_capabilities(capabilities);
    if (capabilities != entry.capabilities) invalid();
    if (entry.origin == ModOriginV2::builtin) {
        if (entry.kind != ModKindV2::builtin || !entry.build_state.has_value() ||
            entry.lifecycle_state.has_value() || entry.package_digest.has_value()) invalid();
        return;
    }
    if (entry.kind == ModKindV2::builtin || entry.build_state.has_value() ||
        !entry.lifecycle_state.has_value() || !entry.package_digest.has_value() ||
        !valid_lifecycle(*entry.lifecycle_state) || !valid_digest(*entry.package_digest)) invalid();
}

[[nodiscard]] std::string_view origin_name(const ModOriginV2 origin) {
    switch (origin) {
    case ModOriginV2::builtin: return "builtin";
    case ModOriginV2::installed: return "installed";
    }
    invalid();
}

[[nodiscard]] std::string_view kind_name(const ModKindV2 kind) {
    switch (kind) {
    case ModKindV2::builtin: return "builtin";
    case ModKindV2::data: return "data";
    case ModKindV2::logic: return "logic";
    }
    invalid();
}

[[nodiscard]] std::string_view build_state_name(const BuiltinModBuildStateV1 state) {
    switch (state) {
    case BuiltinModBuildStateV1::not_built: return "not_built";
    case BuiltinModBuildStateV1::available: return "available";
    }
    invalid();
}

} // namespace

UnifiedModCatalogV2 make_unified_mod_catalog_v2(
    const std::span<const BuiltinModFeatureV1> features,
    const std::span<const InstalledModProjectionV2> installed) {
    UnifiedModCatalogV2 result;
    result.entries.reserve(features.size() + installed.size());
    for (const auto& feature : features) {
        std::vector<std::string> capabilities;
        capabilities.reserve(feature.capabilities.size());
        for (const auto capability : feature.capabilities) capabilities.push_back(capability_name(capability));
        canonicalize_capabilities(capabilities);
        result.entries.push_back({
            .id = feature.id,
            .version = feature.version,
            .origin = ModOriginV2::builtin,
            .kind = ModKindV2::builtin,
            .build_state = feature.build_state,
            .lifecycle_state = std::nullopt,
            .package_digest = std::nullopt,
            .capabilities = std::move(capabilities),
        });
    }
    for (const auto& projection : installed) {
        auto capabilities = projection.capabilities;
        canonicalize_capabilities(capabilities);
        result.entries.push_back({
            .id = projection.id,
            .version = projection.version,
            .origin = ModOriginV2::installed,
            .kind = projection.kind,
            .build_state = std::nullopt,
            .lifecycle_state = projection.lifecycle_state,
            .package_digest = projection.package_digest,
            .capabilities = std::move(capabilities),
        });
    }
    std::ranges::sort(result.entries, {}, key);
    for (std::size_t index = 0; index < result.entries.size(); ++index) {
        validate_entry(result.entries[index]);
        if (index != 0 && key(result.entries[index - 1]) == key(result.entries[index])) invalid();
    }
    return result;
}

std::string render_unified_mod_catalog_v2(const UnifiedModCatalogV2& catalog) {
    std::string result = "{\"contract\":\"bruce-mod-catalog-v2\",\"entries\":[";
    for (std::size_t index = 0; index < catalog.entries.size(); ++index) {
        const auto& entry = catalog.entries[index];
        validate_entry(entry);
        if (index != 0) {
            if (!(key(catalog.entries[index - 1]) < key(entry))) invalid();
            result += ',';
        }
        result += "{\"id\":" + json_string(entry.id) + ",\"version\":" + json_string(entry.version) +
            ",\"origin\":" + json_string(origin_name(entry.origin)) + ",\"kind\":" + json_string(kind_name(entry.kind)) +
            ",\"build_state\":";
        result += entry.build_state ? json_string(build_state_name(*entry.build_state)) : "null";
        result += ",\"lifecycle_state\":";
        result += entry.lifecycle_state ? json_string(*entry.lifecycle_state) : "null";
        result += ",\"package_digest\":";
        result += entry.package_digest ? json_string(*entry.package_digest) : "null";
        result += ",\"capabilities\":[";
        for (std::size_t capability = 0; capability < entry.capabilities.size(); ++capability) {
            if (capability != 0) result += ',';
            result += json_string(entry.capabilities[capability]);
        }
        result += "]}";
    }
    return result + "]}";
}

} // namespace bruce
