#pragma once

#include "bruce/artifact/artifact_manifest_v1.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace bruce::artifact_v1 {

inline constexpr std::uint64_t artifact_payload_stream_chunk_bytes_v15 = 65536;

struct ArtifactPayloadStreamV15 {
    std::string stream_id;
    std::string reference;
    std::string sha256;
    std::uint64_t size_bytes{};
    std::uint64_t chunk_bytes{};
    std::uint64_t chunk_count{};
};

enum class ArtifactPayloadStreamErrorCodeV15 {
    invalid_destination,
    invalid_source,
    limit_exceeded,
    deadline_exceeded,
    digest_size_mismatch,
    seal_failure,
    mutation,
};

class ArtifactPayloadStreamErrorV15 final : public Error {
public:
    explicit ArtifactPayloadStreamErrorV15(ArtifactPayloadStreamErrorCodeV15 code);
    ArtifactPayloadStreamErrorCodeV15 code() const noexcept { return code_; }
private:
    ArtifactPayloadStreamErrorCodeV15 code_;
};

// Borrows source_fd at its current offset. The attempt root must already be a
// private owner-only directory. Success adopts stream-<stream_id> atomically;
// failure removes staging and never replaces an existing accepted stream.
ArtifactPayloadStreamV15 write_payload_stream_v15(
    int source_fd,
    const std::filesystem::path& attempt_root,
    std::string_view stream_id,
    const ArtifactIdV1& expected,
    std::uint64_t max_bytes,
    std::chrono::steady_clock::time_point deadline);

// Authenticates the canonical header, seal, exact directory membership and all
// chunk bytes using descriptor-relative, no-follow opens. Chunk filenames are
// chunk-<20-digit zero-padded index>.bin, starting with zero.
void verify_payload_stream_v15(
    const ArtifactPayloadStreamV15& stream,
    const std::filesystem::path& attempt_root);

// Bounded verification for host attempt paths. The two-argument overload
// remains available for callers without an attempt deadline.
void verify_payload_stream_v15(
    const ArtifactPayloadStreamV15& stream,
    const std::filesystem::path& attempt_root,
    std::chrono::steady_clock::time_point deadline);

} // namespace bruce::artifact_v1
