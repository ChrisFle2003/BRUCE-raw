#pragma once

#include "bruce/artifact/artifact_store_v1.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v2 {

struct ProviderV2 {
    std::string id;
    std::filesystem::path root;
    bool operator==(const ProviderV2&) const = default;
};

struct ProviderConfigV2 {
    std::vector<ProviderV2> providers;
    bool operator==(const ProviderConfigV2&) const = default;
};

// Canonical bytes: {"contract":"bruce-artifact-store-providers-v2",
// "providers":[{"id":"mirror-a","root":"/absolute/root"}]} followed by LF,
// with no whitespace and providers sorted by unique ASCII ID (1..128 bytes).
// Configuration reads do not open mirrors or the destination store.
// Every public failure throws bruce::Error("PROTOCOL_ERROR").
ProviderConfigV2 read_provider_config_v2(const std::filesystem::path& path);

// Only the digest-derived objects/xx/<sha256>.blob is read from the named
// mirror. Payload is streamed in fixed chunks into private staging; V1 is
// opened only after exact digest/size verification. No payload is returned.
artifact_v1::ArtifactIdV1 fetch_to_store_v2(
    const ProviderConfigV2& config,
    std::string_view provider_id,
    const artifact_v1::ArtifactIdV1& expected,
    const std::filesystem::path& store);

} // namespace bruce::artifact_v2
