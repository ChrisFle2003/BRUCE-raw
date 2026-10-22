#include "bruce/mod/logic_mod_payload_v1.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace bruce {
namespace {
struct Fd {
    int n;
    explicit Fd(int value) : n(value) { if (n < 0) throw Error("LogicModPayloadOpenFailed"); }
    ~Fd() { if (n >= 0) ::close(n); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
void owner_only(int fd, bool directory) {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || status.st_uid != ::geteuid() || (status.st_mode & 0077) != 0 ||
        (directory ? !S_ISDIR(status.st_mode) : (!S_ISREG(status.st_mode) || status.st_nlink != 1)))
        throw Error("LogicModPayloadStorePermissions");
}
// Traverse every component without following symlinks, including store parents.
int open_root(const std::filesystem::path& root) {
    Fd cursor(::open(root.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    for (const auto& component : root) {
        if (component == "/" || component == "." || component.empty()) continue;
        if (component == "..") throw Error("LogicModPayloadStorePathInvalid");
        Fd next(::openat(cursor.n, component.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        std::swap(cursor.n, next.n);
    }
    owner_only(cursor.n, true);
    return std::exchange(cursor.n, -1);
}
std::string read_verified(const std::filesystem::path& path, const artifact_v1::ArtifactIdV1& id) {
    Fd root(open_root(path));
    Fd objects(::openat(root.n, "objects", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    owner_only(objects.n, true);
    Fd shard(::openat(objects.n, id.sha256.substr(0, 2).c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    owner_only(shard.n, true);
    Fd source(::openat(shard.n, (id.sha256 + ".blob").c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    owner_only(source.n, false);
    struct stat status{};
    if (::fstat(source.n, &status) != 0 || status.st_size < 0 || static_cast<std::uint64_t>(status.st_size) != id.size_bytes)
        throw Error("LogicModPayloadObjectCorrupt");
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(id.size_bytes));
    std::array<char, 65536> buffer{};
    while (true) {
        const auto n = ::read(source.n, buffer.data(), buffer.size());
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) throw Error("LogicModPayloadObjectCorrupt");
        if (n == 0) break;
        if (static_cast<std::uint64_t>(n) > id.size_bytes - bytes.size()) throw Error("LogicModPayloadObjectCorrupt");
        bytes.append(buffer.data(), static_cast<std::size_t>(n));
    }
    owner_only(source.n, false);
    if (bytes.size() != id.size_bytes || bruce::sha256(bytes) != id.sha256) throw Error("LogicModPayloadObjectCorrupt");
    return bytes;
}
int seal(std::string_view bytes) {
    Fd writable(static_cast<int>(::syscall(SYS_memfd_create, "bruce-logic-payload", MFD_CLOEXEC | MFD_ALLOW_SEALING)));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = ::write(writable.n, bytes.data() + offset, bytes.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw Error("LogicModPayloadSealFailed");
        offset += static_cast<std::size_t>(n);
    }
    if (::fchmod(writable.n, 0400) != 0 ||
        ::fcntl(writable.n, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) != 0)
        throw Error("LogicModPayloadSealFailed");
    // Reopen this owned sealed descriptor read-only; no store path reaches the worker.
    Fd readonly(::open(("/proc/self/fd/" + std::to_string(writable.n)).c_str(), O_RDONLY | O_CLOEXEC));
    const int result = ::fcntl(readonly.n, F_DUPFD_CLOEXEC, 10);
    if (result < 0) throw Error("LogicModPayloadSealFailed");
    return result;
}
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
}
LogicModPayloadV1::~LogicModPayloadV1() { if (fd_ >= 0) ::close(fd_); }
LogicModPayloadV1::LogicModPayloadV1(LogicModPayloadV1&& other) noexcept { *this = std::move(other); }
LogicModPayloadV1& LogicModPayloadV1::operator=(LogicModPayloadV1&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = std::exchange(other.fd_, -1);
        manifest_id_ = std::move(other.manifest_id_); sha256_ = std::move(other.sha256_);
        authorization_identity_ = std::move(other.authorization_identity_);
        object_index_ = other.object_index_; size_bytes_ = other.size_bytes_;
    }
    return *this;
}
std::string LogicModPayloadV1::canonical_identity() const {
    return "BRUCE_LOGIC_PAYLOAD_V1\n" + field(manifest_id_) + field(std::to_string(object_index_)) +
        field(sha256_) + field(std::to_string(size_bytes_)) + field(codec()) + '\n';
}
LogicModPayloadV1 resolve_logic_mod_payload_v1(const LogicModBindingV1& binding,
    const std::filesystem::path& root, std::string_view manifest_id, std::uint64_t object_index) {
    if (binding.capability() != "artifact.input.v1") throw Error("LogicModPayloadCapabilityDenied");
    auto store = artifact_v1::ArtifactStoreV1::open_existing(root);
    const auto manifest = store.resolve_manifest(manifest_id);
    if (manifest.manifest_status != "READY") throw Error("LogicModPayloadManifestNotReady");
    if (object_index >= manifest.objects.size()) throw Error("LogicModPayloadObjectIndexInvalid");
    const auto& id = manifest.objects[static_cast<std::size_t>(object_index)].id;
    if (id.size_bytes > logic_mod_payload_limit_v1) throw Error("LogicModPayloadLimitExceeded");
    LogicModPayloadV1 result;
    result.manifest_id_ = manifest_id; result.object_index_ = object_index;
    result.sha256_ = id.sha256; result.size_bytes_ = id.size_bytes;
    result.authorization_identity_ = binding.canonical_identity();
    result.fd_ = seal(read_verified(root, id));
    return result;
}
}
