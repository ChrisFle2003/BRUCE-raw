#pragma once

#include "bruce/artifact/artifact_store_v3.hpp"

namespace bruce::artifact_v4 {

inline constexpr std::size_t mirror_manifest_limit_v4 = 1024 * 1024;
inline constexpr std::uint64_t mirror_total_object_limit_v4 = bruce::max_file_bytes;
struct MirrorResultV4 {
    std::string manifest_id;
    std::string manifest_sha256;
    std::uint64_t manifest_size_bytes{};
    std::uint64_t object_count{};
    std::uint64_t total_object_bytes{};
    bool operator==(const MirrorResultV4&) const = default;
};
struct HttpsRequestV4 {
    artifact_v3::HttpsRequestV3 endpoint;
    std::string method;
    std::string_view upload_bytes;
};
using HttpsTransportV4 = std::function<void(const HttpsRequestV4&,
    const artifact_v3::HttpsHeadersV3&, const artifact_v3::HttpsBodyV3&)>;
void https_transport_v4(const HttpsRequestV4& request,
    const artifact_v3::HttpsHeadersV3& headers, const artifact_v3::HttpsBodyV3& body);

// Host-only, bounded HTTPS operations. All local bytes are verified before
// upload; the manifest envelope is uploaded last. Restore requires an absent
// local manifest and verifies every remote byte before any local publication.
// Existing entries are never replaced. Every failure is PROTOCOL_ERROR.
MirrorResultV4 mirror_to_remote_v4(std::string_view manifest_id,
    const std::filesystem::path& store, std::string_view remote,
    const HttpsTransportV4& transport = https_transport_v4);
MirrorResultV4 restore_from_remote_v4(std::string_view manifest_id,
    const std::filesystem::path& store, std::string_view remote,
    const HttpsTransportV4& transport = https_transport_v4);

} // namespace bruce::artifact_v4
