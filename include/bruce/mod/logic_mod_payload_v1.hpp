#pragma once

#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include <cstdint>
#include <filesystem>

namespace bruce {
inline constexpr std::uint64_t logic_mod_payload_limit_v1 = 1024 * 1024;

// Host-only, move-only ownership of a verified immutable snapshot. The borrowed
// FD is read-only, sealed and CLOEXEC; the runner explicitly maps it to FD 6.
// The authorization identity is a request-time snapshot, never perpetual grant.
class LogicModPayloadV1 {
public:
    ~LogicModPayloadV1();
    LogicModPayloadV1(const LogicModPayloadV1&) = delete;
    LogicModPayloadV1& operator=(const LogicModPayloadV1&) = delete;
    LogicModPayloadV1(LogicModPayloadV1&&) noexcept;
    LogicModPayloadV1& operator=(LogicModPayloadV1&&) noexcept;
    int fd() const noexcept { return fd_; }
    const std::string& manifest_id() const noexcept { return manifest_id_; }
    std::uint64_t object_index() const noexcept { return object_index_; }
    const std::string& sha256() const noexcept { return sha256_; }
    std::uint64_t size_bytes() const noexcept { return size_bytes_; }
    std::string_view codec() const noexcept { return "artifact-bytes-v1"; }
    const std::string& authorization_identity() const noexcept { return authorization_identity_; }
    std::string canonical_identity() const;
private:
    LogicModPayloadV1() = default;
    int fd_ = -1;
    std::string manifest_id_, sha256_, authorization_identity_;
    std::uint64_t object_index_ = 0, size_bytes_ = 0;
    friend LogicModPayloadV1 resolve_logic_mod_payload_v1(const LogicModBindingV1&,
        const std::filesystem::path&, std::string_view, std::uint64_t);
};

// Requires a registry-issued artifact.input.v1 binding. Opens only an existing
// local store and fully validates the V1.2 manifest and object before returning.
// No spawn, store mutation, path/URL interpretation of origins, or worker I/O.
LogicModPayloadV1 resolve_logic_mod_payload_v1(const LogicModBindingV1& input_binding,
    const std::filesystem::path& store, std::string_view manifest_id,
    std::uint64_t object_index);
}
