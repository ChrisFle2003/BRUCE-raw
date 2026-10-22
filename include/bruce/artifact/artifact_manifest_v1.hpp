#pragma once

#include "bruce/core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

struct ArtifactIdV1 {
    std::string sha256;
    std::uint64_t size_bytes{};
    bool operator==(const ArtifactIdV1&) const = default;
};

struct ArtifactManifestObjectV1 {
    ArtifactIdV1 id;
    std::string media_type;
    std::string origin;
    bool operator==(const ArtifactManifestObjectV1&) const = default;
};

struct ArtifactManifestV1 {
    std::string run_id;
    std::string node_id;
    std::string attempt_id;
    std::string status;
    std::string result_code;
    std::vector<ArtifactManifestObjectV1> objects;
    bool operator==(const ArtifactManifestV1&) const = default;
};

inline constexpr std::size_t artifact_manifest_token_limit_v1 = 128;
inline constexpr std::size_t artifact_manifest_object_limit_v1 = 1024;
inline constexpr std::string_view artifact_manifest_contract_v1 = "BRUCE_ARTIFACT_MANIFEST_V1";

std::string encode_manifest_v1(const ArtifactManifestV1& manifest);
ArtifactManifestV1 decode_manifest_v1(std::string_view bytes);

} // namespace bruce::artifact_v1
