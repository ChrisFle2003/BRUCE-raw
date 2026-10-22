#pragma once

#include "bruce/artifact/artifact_store_v1.hpp"

#include <functional>

namespace bruce::artifact_v3 {

struct ProviderV3 {
    std::string id;
    std::string origin;
    bool operator==(const ProviderV3&) const = default;
};
struct ProviderConfigV3 {
    std::vector<ProviderV3> providers;
    bool operator==(const ProviderConfigV3&) const = default;
};

// Canonical owner-only regular configuration; every ancestor is opened without
// following symlinks. IDs are sorted/unique; origins admit only canonical HTTPS.
ProviderConfigV3 read_provider_config_v3(const std::filesystem::path& path);

// Internal transport port. Only fetch_to_store_v3 derives these values from an
// allowlisted provider and identity. Transports emit exactly one header event,
// zero or more body chunks, then return only after complete response framing.
// Exceptions, non-200, size/digest mismatch, and expired deadlines fail closed.
struct HttpsRequestV3 {
    std::string host;
    int port{443};
    std::string path;
    std::chrono::steady_clock::time_point connect_deadline;
    std::chrono::steady_clock::time_point deadline;
};
using HttpsHeadersV3 = std::function<void(int, std::optional<std::uint64_t>)>;
using HttpsBodyV3 = std::function<void(std::string_view)>;
using HttpsTransportV3 = std::function<void(const HttpsRequestV3&, const HttpsHeadersV3&, const HttpsBodyV3&)>;

// Linux production adapter: one TLS-verified GET in a supervised child, private
// bounded IPC, no redirects/retries. Parent enforces both absolute deadlines
// and always kills/reaps on failure. No filesystem publication in the child.
void https_get_v3(const HttpsRequestV3& request, const HttpsHeadersV3& headers,
                  const HttpsBodyV3& body);

// Shared host transport primitives. Canonical HTTPS origins have no credentials,
// query, fragment or URL escapes. PUT uses the same TLS/deadline supervision as
// GET and accepts only 200/201/204 responses, with no redirects or retries.
HttpsRequestV3 https_request_v3(std::string_view origin, std::string_view path,
    std::chrono::steady_clock::time_point deadline);
void https_put_v3(const HttpsRequestV3& request, std::string_view bytes,
    const HttpsHeadersV3& headers, const HttpsBodyV3& body);

// Buffers at most expected.size_bytes (<=32 MiB), verifies before opening V1,
// then publishes atomically. All public failures are Error("PROTOCOL_ERROR").
artifact_v1::ArtifactIdV1 fetch_to_store_v3(
    const ProviderConfigV3& config, std::string_view provider_id,
    const artifact_v1::ArtifactIdV1& expected, const std::filesystem::path& store,
    const HttpsTransportV3& transport = https_get_v3);

} // namespace bruce::artifact_v3
