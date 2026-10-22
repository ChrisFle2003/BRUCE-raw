#pragma once

#include "bruce/artifact/artifact_manifest_v1.hpp"

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

inline constexpr std::size_t artifact_object_limit_v1 = bruce::max_file_bytes;
inline constexpr std::size_t artifact_store_max_object_bytes_v1 = artifact_object_limit_v1;

struct ArtifactMetadataV1 {
    ArtifactIdV1 id;
    std::string media_type;
    std::string origin;
    bool operator==(const ArtifactMetadataV1&) const = default;
};

struct ArtifactResolvedObjectV1 {
    ArtifactIdV1 id;
    std::string origin;
    bool operator==(const ArtifactResolvedObjectV1&) const = default;
};

struct ArtifactResolvedManifestV1 {
    std::string manifest_id;
    std::string run_id;
    std::string node_id;
    std::string attempt_id;
    std::string manifest_status;
    std::string result_code;
    std::vector<ArtifactResolvedObjectV1> objects;
    bool operator==(const ArtifactResolvedManifestV1&) const = default;
};

// Metadata returned by the host-only bounded object copier.  The copier never
// returns payload bytes and never interprets manifest origin as a path.
struct ArtifactObjectCopyUsageV13 {
    ArtifactIdV1 object;
    std::uint64_t bytes_written{};
    std::uint64_t chunks{};
    bool operator==(const ArtifactObjectCopyUsageV13&) const = default;
};

bool is_valid_manifest_id_v1(std::string_view value);

class ArtifactStoreV1 {
public:
    static constexpr std::size_t object_limit = artifact_object_limit_v1;

    static ArtifactStoreV1 open(const std::filesystem::path& root);
    static ArtifactStoreV1 open_existing(const std::filesystem::path& root);
    ArtifactStoreV1() = default;
    ~ArtifactStoreV1();
    ArtifactStoreV1(const ArtifactStoreV1&) = delete;
    ArtifactStoreV1& operator=(const ArtifactStoreV1&) = delete;
    ArtifactStoreV1(ArtifactStoreV1&& other) noexcept;
    ArtifactStoreV1& operator=(ArtifactStoreV1&& other) noexcept;

    ArtifactIdV1 put_bytes(std::string_view bytes);
    ArtifactIdV1 put_file(const std::filesystem::path& source,
                          std::string_view expected_digest = {},
                          std::optional<std::uint64_t> expected_size = {});
    bool contains(const ArtifactIdV1& id) const;
    ArtifactMetadataV1 inspect(const ArtifactIdV1& id) const;
    void verify(const ArtifactIdV1& id) const;
    // Digest-addressed read operations discover the bounded size from the
    // descriptor before verifying the object. They are the safe public
    // boundary for callers such as the metadata-only CLI, which intentionally
    // receives no size hint from the user.
    ArtifactMetadataV1 inspect(std::string_view sha256) const;
    void verify(std::string_view sha256) const;

    // Copy one verified content-addressed object into a caller-created,
    // Bruce-owned regular destination.  The destination is opened by the
    // caller and is never replaced or truncated by this API.  The copy is
    // descriptor-relative to the Store root, uses fixed chunks, and checks
    // the optional monotonic deadline before every read/write operation.
    ArtifactObjectCopyUsageV13 copy_object_to_fd(
        const ArtifactIdV1& id,
        int destination_fd,
        std::uint64_t max_bytes,
        std::chrono::steady_clock::time_point deadline) const;

    // Preflight only the selected entry's safety; absence is allowed. Uses a
    // pinned descriptor without following links, opening devices or reading
    // bytes. Publication still validates its freshly opened conflict entry.
    void validate_manifest_target(std::string_view manifest_id) const;
    // Existence is independent of readability or target type. Uses the pinned
    // Store descriptor and never follows the selected entry; only ENOENT is
    // absence. This permits restore gates to reject unreadable existing data.
    bool manifest_entry_exists(std::string_view manifest_id) const;
    std::string publish_manifest(std::string_view manifest_id,
                                 const ArtifactManifestV1& manifest);
    ArtifactManifestV1 read_manifest(std::string_view manifest_id) const;
    ArtifactResolvedManifestV1 resolve_manifest(std::string_view manifest_id) const;

private:
    explicit ArtifactStoreV1(int root_fd, std::filesystem::path root);
    int root_fd_{-1};
    std::filesystem::path root_;
};

} // namespace bruce::artifact_v1
