#include "bruce/artifact/artifact_store_v1.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <linux/fs.h>
#include <memory>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace bruce::artifact_v1 {
namespace {
class Fd {
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { if (fd_ >= 0) ::close(fd_); fd_ = other.fd_; other.fd_ = -1; }
        return *this;
    }
    int get() const { return fd_; }
    int release() { const int result = fd_; fd_ = -1; return result; }
    explicit operator bool() const { return fd_ >= 0; }
private:
    int fd_;
};

class Digest {
public:
    Digest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1)
            throw Error("ArtifactDigestFailure");
    }
    void update(const void* data, std::size_t size) {
        if (EVP_DigestUpdate(context_.get(), data, size) != 1)
            throw Error("ArtifactDigestFailure");
    }
    std::string finish() {
        std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
        unsigned length = 0;
        if (EVP_DigestFinal_ex(context_.get(), bytes.data(), &length) != 1)
            throw Error("ArtifactDigestFailure");
        static constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(length * 2);
        for (unsigned index = 0; index < length; ++index) {
            result.push_back(hex[bytes[index] >> 4]);
            result.push_back(hex[bytes[index] & 15]);
        }
        return result;
    }
private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
};

std::vector<std::string> components(const std::filesystem::path& path, std::string_view escape_error) {
    if (path.empty()) throw Error("ArtifactSourceMissing");
    std::vector<std::string> result;
    for (const auto& part : path) {
        const auto text = part.string();
        if (text.empty() || text == "." || text == "/") continue;
        if (text == "..") throw Error(std::string(escape_error));
        result.push_back(text);
    }
    if (result.empty()) throw Error(std::string(escape_error));
    return result;
}

void check_owner_only(int fd, std::string_view permission_error, bool regular = false) {
    struct stat status{};
    if (::fstat(fd, &status) != 0) throw Error(std::string(permission_error));
    if ((status.st_mode & 0077) != 0) throw Error(std::string(permission_error));
    if (regular && (!S_ISREG(status.st_mode) || status.st_nlink != 1))
        throw Error(std::string(permission_error));
}

Fd open_directory_tree(const std::filesystem::path& path, bool create, std::string_view symlink_error,
                       std::string_view permission_error) {
    const bool absolute = path.is_absolute();
    Fd current(::open(absolute ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!current) throw Error("ArtifactStoreOpenFailed");
    std::vector<std::string> names = components(path, symlink_error);
    for (const auto& name : names) {
        Fd next(::openat(current.get(), name.c_str(),
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        if (!next && errno == ENOENT && create) {
            if (::mkdirat(current.get(), name.c_str(), 0700) != 0 && errno != EEXIST)
                throw Error("ArtifactStoreCreateFailed");
            next = Fd(::openat(current.get(), name.c_str(),
                               O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        }
        if (!next) {
            if (errno == ELOOP) throw Error(std::string(symlink_error));
            throw Error("ArtifactStoreOpenFailed");
        }
        current = std::move(next);
    }
    check_owner_only(current.get(), permission_error);
    return current;
}

Fd open_child_directory(int parent, std::string_view name, std::string_view error) {
    Fd result(::openat(parent, std::string(name).c_str(),
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!result) {
        if (errno == ELOOP) throw Error(std::string(error));
        throw Error("ArtifactStoreOpenFailed");
    }
    check_owner_only(result.get(), "ArtifactStorePermissions");
    return result;
}

bool ascii_alnum(unsigned char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9');
}

bool digest_text(std::string_view value) {
    if (value.size() != 64) return false;
    for (const unsigned char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

void validate_id(const ArtifactIdV1& id) {
    if (!digest_text(id.sha256) || id.size_bytes > artifact_object_limit_v1)
        throw Error("ArtifactIdInvalid");
}

Fd open_source(const std::filesystem::path& path) {
    const bool absolute = path.is_absolute();
    const auto names = components(path, "ArtifactSourcePathEscape");
    Fd current(::open(absolute ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!current) throw Error("ArtifactSourceMissing");
    for (std::size_t index = 0; index + 1 < names.size(); ++index) {
        Fd next(::openat(current.get(), names[index].c_str(),
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
        if (!next) {
            struct stat component_status{};
            if (::fstatat(current.get(), names[index].c_str(), &component_status,
                          AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(component_status.st_mode))
                throw Error("ArtifactSourceSymlink");
            if (errno == ELOOP) throw Error("ArtifactSourceSymlink");
            throw Error("ArtifactSourceMissing");
        }
        current = std::move(next);
    }
    Fd source(::openat(current.get(), names.back().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (!source) {
        if (errno == ELOOP) throw Error("ArtifactSourceSymlink");
        throw Error("ArtifactSourceMissing");
    }
    struct stat status{};
    if (::fstat(source.get(), &status) != 0) throw Error("ArtifactSourceMissing");
    if (!S_ISREG(status.st_mode)) throw Error("ArtifactSourceNotRegular");
    if ((status.st_mode & 0077) != 0) throw Error("ArtifactSourcePermissions");
    if (status.st_nlink != 1) throw Error("ArtifactSourceHardlink");
    if (static_cast<std::uint64_t>(status.st_size) > artifact_object_limit_v1)
        throw Error("ArtifactObjectTooLarge");
    return source;
}

std::string read_all(int fd, std::string_view too_large_error = "ArtifactObjectTooLarge") {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0)
        throw Error("ArtifactReadFailed");
    if (static_cast<std::uint64_t>(status.st_size) > artifact_object_limit_v1)
        throw Error(std::string(too_large_error));
    if (::lseek(fd, 0, SEEK_SET) < 0) throw Error("ArtifactReadFailed");
    std::string result;
    std::array<char, 65536> buffer{};
    while (true) {
        const auto count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw Error("ArtifactReadFailed");
        if (count == 0) break;
        if (result.size() > artifact_object_limit_v1 - static_cast<std::size_t>(count))
            throw Error(std::string(too_large_error));
        result.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return result;
}

std::string verify_fd(int fd, std::uint64_t expected_size) {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_nlink != 1 ||
        (status.st_mode & 0077) != 0)
        throw Error("ArtifactObjectCorrupt");
    if (status.st_size < 0 || static_cast<std::uint64_t>(status.st_size) != expected_size ||
        expected_size > artifact_object_limit_v1) throw Error("ArtifactObjectCorrupt");
    if (::lseek(fd, 0, SEEK_SET) < 0) throw Error("ArtifactObjectCorrupt");
    Digest digest;
    std::array<char, 65536> buffer{};
    std::uint64_t total = 0;
    while (true) {
        const auto count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw Error("ArtifactObjectCorrupt");
        if (count == 0) break;
        total += static_cast<std::uint64_t>(count);
        if (total > artifact_object_limit_v1) throw Error("ArtifactObjectCorrupt");
        digest.update(buffer.data(), static_cast<std::size_t>(count));
    }
    if (total != expected_size) throw Error("ArtifactObjectCorrupt");
    return digest.finish();
}

std::atomic<std::uint64_t> temp_counter{0};
Fd make_temp(int directory_fd, std::string& name) {
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        name = ".tmp." + std::to_string(static_cast<unsigned long long>(::getpid())) + "." +
               std::to_string(static_cast<unsigned long long>(temp_counter.fetch_add(1)));
        const int fd = ::openat(directory_fd, name.c_str(), O_WRONLY | O_CREAT | O_EXCL |
                                 O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd >= 0) return Fd(fd);
        if (errno != EEXIST) throw Error("ArtifactTempCreateFailed");
    }
    throw Error("ArtifactTempCreateFailed");
}

void write_all(int fd, const char* data, std::size_t size) {
    while (size != 0) {
        const auto count = ::write(fd, data, size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw Error("ArtifactPublishWriteFailed");
        data += count;
        size -= static_cast<std::size_t>(count);
    }
}

void check_copy_deadline(std::chrono::steady_clock::time_point deadline) {
    if (std::chrono::steady_clock::now() >= deadline)
        throw Error("ArtifactCopyDeadlineExceeded");
}

struct PublicationTarget {
    Fd directory;
    std::string name;
};

template<class Reader, class Resolver>
ArtifactIdV1 publish_stream(int temporary_directory_fd, std::string_view expected_digest,
                            std::optional<std::uint64_t> expected_size, Reader&& reader,
                            Resolver&& resolver) {
    Digest digest;
    std::uint64_t size = 0;
    std::string temp_name;
    Fd temp = make_temp(temporary_directory_fd, temp_name);
    try {
        reader(temp.get(), digest, size);
        if (size > artifact_object_limit_v1) throw Error("ArtifactObjectTooLarge");
        ArtifactIdV1 id{digest.finish(), size};
        if (!expected_digest.empty() && expected_digest != id.sha256)
            throw Error("ArtifactExpectedDigestMismatch");
        if (expected_size.has_value() && expected_size.value() != id.size_bytes)
            throw Error("ArtifactExpectedSizeMismatch");
        if (::fsync(temp.get()) != 0) throw Error("ArtifactPublishSyncFailed");
        auto target = resolver(id);
        if (::syscall(SYS_renameat2, temporary_directory_fd, temp_name.c_str(),
                      target.directory.get(), target.name.c_str(), RENAME_NOREPLACE) != 0) {
            if (errno != EEXIST) throw Error("ArtifactPublishFailed");
            ::unlinkat(temporary_directory_fd, temp_name.c_str(), 0);
            // Do not wait for a writer if a conflicting target is a FIFO.
            // verify_fd validates the opened inode before reading any bytes.
            Fd existing(::openat(target.directory.get(), target.name.c_str(),
                                 O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
            if (!existing) throw Error("ArtifactObjectConflict");
            try {
                if (verify_fd(existing.get(), id.size_bytes) == id.sha256) return id;
            } catch (const Error&) {
                // An existing object with a bad inode or digest is a conflict.
            }
            throw Error("ArtifactObjectConflict");
        }
        if (::fsync(target.directory.get()) != 0) throw Error("ArtifactPublishSyncFailed");
        return id;
    } catch (...) {
        ::unlinkat(temporary_directory_fd, temp_name.c_str(), 0);
        throw;
    }
}
Fd open_object_parent(int root_fd, std::string_view digest) {
    Fd objects = open_child_directory(root_fd, "objects", "ArtifactStoreSymlink");
    Fd prefix(::openat(objects.get(), std::string(digest.substr(0, 2)).c_str(),
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!prefix) {
        if (errno == ENOENT) throw Error("ArtifactObjectMissing");
        throw Error("ArtifactObjectCorrupt");
    }
    check_owner_only(prefix.get(), "ArtifactStorePermissions");
    return prefix;
}

Fd ensure_object_parent(int root_fd, std::string_view digest) {
    Fd objects = open_child_directory(root_fd, "objects", "ArtifactStoreSymlink");
    const std::string prefix_name(digest.substr(0, 2));
    if (::mkdirat(objects.get(), prefix_name.c_str(), 0700) != 0 && errno != EEXIST)
        throw Error("ArtifactStoreCreateFailed");
    Fd prefix(::openat(objects.get(), prefix_name.c_str(),
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!prefix) {
        if (errno == ELOOP) throw Error("ArtifactStoreSymlink");
        throw Error("ArtifactStoreOpenFailed");
    }
    check_owner_only(prefix.get(), "ArtifactStorePermissions");
    return prefix;
}

Fd open_object(int root_fd, std::string_view digest) {
    Fd prefix = open_object_parent(root_fd, digest);
    Fd object(::openat(prefix.get(), (std::string(digest) + ".blob").c_str(),
                       O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!object) {
        if (errno == ENOENT) throw Error("ArtifactObjectMissing");
        throw Error("ArtifactObjectCorrupt");
    }
    return object;
}


} // namespace

bool is_valid_manifest_id_v1(std::string_view value) {
    if (value.empty() || value.size() > artifact_manifest_token_limit_v1 ||
        !ascii_alnum(static_cast<unsigned char>(value.front()))) return false;
    for (const unsigned char c : value) {
        if (!(ascii_alnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

ArtifactStoreV1::ArtifactStoreV1(int root_fd, std::filesystem::path root)
    : root_fd_(root_fd), root_(std::move(root)) {}

ArtifactStoreV1::~ArtifactStoreV1() { if (root_fd_ >= 0) ::close(root_fd_); }

ArtifactStoreV1::ArtifactStoreV1(ArtifactStoreV1&& other) noexcept
    : root_fd_(other.root_fd_), root_(std::move(other.root_)) { other.root_fd_ = -1; }

ArtifactStoreV1& ArtifactStoreV1::operator=(ArtifactStoreV1&& other) noexcept {
    if (this != &other) {
        if (root_fd_ >= 0) ::close(root_fd_);
        root_fd_ = other.root_fd_;
        root_ = std::move(other.root_);
        other.root_fd_ = -1;
    }
    return *this;
}

ArtifactStoreV1 ArtifactStoreV1::open(const std::filesystem::path& root) {
    Fd root_fd = open_directory_tree(root, true, "ArtifactStoreSymlink", "ArtifactStorePermissions");
    Fd lock(::openat(root_fd.get(), "store.lock", O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (!lock) throw Error("ArtifactStoreCreateFailed");
    check_owner_only(lock.get(), "ArtifactStorePermissions", true);
    Fd objects;
    Fd manifests;
    try {
        objects = open_child_directory(root_fd.get(), "objects", "ArtifactStoreSymlink");
    } catch (const Error&) {
        if (::mkdirat(root_fd.get(), "objects", 0700) != 0 && errno != EEXIST)
            throw Error("ArtifactStoreCreateFailed");
        objects = open_child_directory(root_fd.get(), "objects", "ArtifactStoreSymlink");
    }
    try {
        manifests = open_child_directory(root_fd.get(), "manifests", "ArtifactStoreSymlink");
    } catch (const Error&) {
        if (::mkdirat(root_fd.get(), "manifests", 0700) != 0 && errno != EEXIST)
            throw Error("ArtifactStoreCreateFailed");
        manifests = open_child_directory(root_fd.get(), "manifests", "ArtifactStoreSymlink");
    }
    return ArtifactStoreV1(root_fd.release(), root);
}

ArtifactStoreV1 ArtifactStoreV1::open_existing(const std::filesystem::path& root) {
    Fd root_fd = open_directory_tree(root, false, "ArtifactStoreSymlink",
                                     "ArtifactStorePermissions");
    Fd lock(::openat(root_fd.get(), "store.lock", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!lock) {
        if (errno == ELOOP) throw Error("ArtifactStoreSymlink");
        throw Error("ArtifactStoreOpenFailed");
    }
    check_owner_only(lock.get(), "ArtifactStorePermissions", true);
    Fd objects = open_child_directory(root_fd.get(), "objects", "ArtifactStoreSymlink");
    Fd manifests = open_child_directory(root_fd.get(), "manifests", "ArtifactStoreSymlink");
    return ArtifactStoreV1(root_fd.release(), root);
}

ArtifactIdV1 ArtifactStoreV1::put_bytes(std::string_view bytes) {
    if (bytes.size() > artifact_object_limit_v1) throw Error("ArtifactObjectTooLarge");
    Fd objects = open_child_directory(root_fd_, "objects", "ArtifactStoreSymlink");
    return publish_stream(objects.get(), {}, bytes.size(),
        [&](int fd, Digest& state, std::uint64_t& size) {
            constexpr std::size_t block = 65536;
            for (std::size_t offset = 0; offset < bytes.size();) {
                const auto count = std::min(block, bytes.size() - offset);
                write_all(fd, bytes.data() + offset, count);
                state.update(bytes.data() + offset, count);
                offset += count;
                size += count;
            }
        },
        [&](const ArtifactIdV1& id) {
            return PublicationTarget{ensure_object_parent(root_fd_, id.sha256), id.sha256 + ".blob"};
        });
}

ArtifactIdV1 ArtifactStoreV1::put_file(const std::filesystem::path& source,
                                       std::string_view expected_digest,
                                       std::optional<std::uint64_t> expected_size) {
    if (!expected_digest.empty() && !digest_text(expected_digest))
        throw Error("ArtifactExpectedDigestInvalid");
    if (expected_size.has_value() && expected_size.value() > artifact_object_limit_v1)
        throw Error("ArtifactExpectedSizeInvalid");
    Fd source_fd = open_source(source);
    Fd objects = open_child_directory(root_fd_, "objects", "ArtifactStoreSymlink");
    return publish_stream(objects.get(), expected_digest, expected_size,
        [&](int fd, Digest& state, std::uint64_t& size) {
            std::array<char, 65536> buffer{};
            while (true) {
                const auto count = ::read(source_fd.get(), buffer.data(), buffer.size());
                if (count < 0 && errno == EINTR) continue;
                if (count < 0) throw Error("ArtifactSourceReadFailed");
                if (count == 0) break;
                if (size > artifact_object_limit_v1 - static_cast<std::size_t>(count))
                    throw Error("ArtifactObjectTooLarge");
                write_all(fd, buffer.data(), static_cast<std::size_t>(count));
                state.update(buffer.data(), static_cast<std::size_t>(count));
                size += static_cast<std::uint64_t>(count);
            }
            struct stat source_status{};
            if (::fstat(source_fd.get(), &source_status) != 0 || source_status.st_nlink != 1)
                throw Error("ArtifactSourceHardlink");
        },
        [&](const ArtifactIdV1& id) {
            return PublicationTarget{ensure_object_parent(root_fd_, id.sha256), id.sha256 + ".blob"};
        });
}
void ArtifactStoreV1::verify(const ArtifactIdV1& id) const {
    validate_id(id);
    Fd object = open_object(root_fd_, id.sha256);
    const auto digest = verify_fd(object.get(), id.size_bytes);
    if (digest != id.sha256) throw Error("ArtifactObjectCorrupt");
}

ArtifactMetadataV1 ArtifactStoreV1::inspect(std::string_view sha256) const {
    if (!digest_text(sha256)) throw Error("ArtifactIdInvalid");
    Fd object = open_object(root_fd_, sha256);
    struct stat status{};
    if (::fstat(object.get(), &status) != 0 || status.st_size < 0)
        throw Error("ArtifactObjectCorrupt");
    const auto size = static_cast<std::uint64_t>(status.st_size);
    if (verify_fd(object.get(), size) != sha256) throw Error("ArtifactObjectCorrupt");
    return ArtifactMetadataV1{ArtifactIdV1{std::string(sha256), size}, {}, {}};
}

void ArtifactStoreV1::verify(std::string_view sha256) const {
    if (!digest_text(sha256)) throw Error("ArtifactIdInvalid");
    Fd object = open_object(root_fd_, sha256);
    struct stat status{};
    if (::fstat(object.get(), &status) != 0 || status.st_size < 0)
        throw Error("ArtifactObjectCorrupt");
    const auto size = static_cast<std::uint64_t>(status.st_size);
    const auto digest = verify_fd(object.get(), size);
    if (digest != sha256) throw Error("ArtifactObjectCorrupt");
}

ArtifactObjectCopyUsageV13 ArtifactStoreV1::copy_object_to_fd(
    const ArtifactIdV1& id,
    int destination_fd,
    std::uint64_t max_bytes,
    std::chrono::steady_clock::time_point deadline) const {
    validate_id(id);
    if (destination_fd < 0 || max_bytes < id.size_bytes)
        throw Error("ArtifactCopyDestinationInvalid");

    struct stat destination_status{};
    if (::fstat(destination_fd, &destination_status) != 0 ||
        !S_ISREG(destination_status.st_mode) || destination_status.st_size < 0 ||
        destination_status.st_uid != ::geteuid() || (destination_status.st_mode & 0077) != 0 ||
        destination_status.st_nlink != 1)
        throw Error("ArtifactCopyDestinationInvalid");
    const auto destination_size = static_cast<std::uint64_t>(destination_status.st_size);
    if (destination_size > max_bytes || id.size_bytes > max_bytes - destination_size)
        throw Error("ArtifactCopyLimitExceeded");
    if (::lseek(destination_fd, 0, SEEK_END) < 0)
        throw Error("ArtifactCopyDestinationInvalid");

    Fd object = open_object(root_fd_, id.sha256);
    struct stat source_status{};
    if (::fstat(object.get(), &source_status) != 0 || !S_ISREG(source_status.st_mode) ||
        source_status.st_size < 0 || source_status.st_nlink != 1 ||
        source_status.st_uid != ::geteuid() ||
        (source_status.st_mode & 0077) != 0 ||
        static_cast<std::uint64_t>(source_status.st_size) != id.size_bytes ||
        id.size_bytes > artifact_object_limit_v1)
        throw Error("ArtifactObjectCorrupt");
    if (::lseek(object.get(), 0, SEEK_SET) < 0)
        throw Error("ArtifactObjectCorrupt");

    Digest digest;
    std::array<char, 65536> buffer{};
    std::uint64_t total = 0;
    std::uint64_t chunks = 0;
    while (true) {
        check_copy_deadline(deadline);
        const auto count = ::read(object.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) throw Error("ArtifactObjectCorrupt");
        if (count == 0) break;
        if (static_cast<std::uint64_t>(count) > id.size_bytes - total)
            throw Error("ArtifactObjectCorrupt");
        digest.update(buffer.data(), static_cast<std::size_t>(count));
        std::size_t remaining = static_cast<std::size_t>(count);
        const char* cursor = buffer.data();
        while (remaining != 0) {
            check_copy_deadline(deadline);
            const auto written = ::write(destination_fd, cursor, remaining);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw Error("ArtifactCopyDestinationWriteFailed");
            cursor += written;
            remaining -= static_cast<std::size_t>(written);
        }
        total += static_cast<std::uint64_t>(count);
        ++chunks;
    }
    if (total != id.size_bytes || digest.finish() != id.sha256)
        throw Error("ArtifactObjectCorrupt");
    return ArtifactObjectCopyUsageV13{id, total, chunks};
}

bool ArtifactStoreV1::contains(const ArtifactIdV1& id) const {
    try {
        verify(id);
        return true;
    } catch (const Error& error) {
        const auto message = std::string_view(error.what());
        if (message == "ArtifactIdInvalid" || message == "ArtifactObjectMissing" ||
            message == "ArtifactObjectCorrupt" || message == "ArtifactStorePermissions") return false;
        throw;
    }
}

ArtifactMetadataV1 ArtifactStoreV1::inspect(const ArtifactIdV1& id) const {
    verify(id);
    return ArtifactMetadataV1{id, {}, {}};
}

void ArtifactStoreV1::validate_manifest_target(std::string_view manifest_id) const {
    if (!is_valid_manifest_id_v1(manifest_id)) throw Error("ArtifactManifestIdInvalid");
    Fd manifests = open_child_directory(root_fd_, "manifests", "ArtifactStoreSymlink");
    const auto name = std::string(manifest_id) + ".manifest";
    // O_PATH cannot block on a FIFO or invoke a device's open handler. fstat
    // checks this exact inode, rather than stat-then-open across a swap window.
    Fd target(::openat(manifests.get(), name.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW));
    if (!target) {
        if (errno == ENOENT) return;
        throw Error("ArtifactManifestConflict");
    }
    check_owner_only(target.get(), "ArtifactManifestConflict", true);
}

bool ArtifactStoreV1::manifest_entry_exists(std::string_view manifest_id) const {
    if (!is_valid_manifest_id_v1(manifest_id)) throw Error("ArtifactManifestIdInvalid");
    Fd manifests = open_child_directory(root_fd_, "manifests", "ArtifactStoreSymlink");
    const auto name = std::string(manifest_id) + ".manifest";
    struct stat status{};
    if (::fstatat(manifests.get(), name.c_str(), &status, AT_SYMLINK_NOFOLLOW) == 0) return true;
    if (errno == ENOENT) return false;
    throw Error("ArtifactManifestConflict");
}

std::string ArtifactStoreV1::publish_manifest(std::string_view manifest_id,
                                              const ArtifactManifestV1& manifest) {
    if (!is_valid_manifest_id_v1(manifest_id)) throw Error("ArtifactManifestIdInvalid");
    const auto bytes = encode_manifest_v1(manifest);
    Fd manifests = open_child_directory(root_fd_, "manifests", "ArtifactStoreSymlink");
    const auto name = std::string(manifest_id) + ".manifest";
    std::string temp_name;
    Fd temp = make_temp(manifests.get(), temp_name);
    try {
        write_all(temp.get(), bytes.data(), bytes.size());
        if (::fsync(temp.get()) != 0) throw Error("ArtifactManifestSyncFailed");
        temp = Fd(temp.release());
        if (::syscall(SYS_renameat2, manifests.get(), temp_name.c_str(), manifests.get(),
                      name.c_str(), RENAME_NOREPLACE) != 0) {
            if (errno != EEXIST) throw Error("ArtifactManifestPublishFailed");
            ::unlinkat(manifests.get(), temp_name.c_str(), 0);
            Fd existing(::openat(manifests.get(), name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
            if (!existing) throw Error("ArtifactManifestConflict");
            try {
                check_owner_only(existing.get(), "ArtifactManifestConflict", true);
            } catch (const Error&) {
                throw Error("ArtifactManifestConflict");
            }
            if (read_all(existing.get(), "ArtifactManifestTooLarge") != bytes)
                throw Error("ArtifactManifestConflict");
            return std::string(manifest_id);
        }
        if (::fsync(manifests.get()) != 0) throw Error("ArtifactManifestSyncFailed");
        return std::string(manifest_id);
    } catch (...) {
        ::unlinkat(manifests.get(), temp_name.c_str(), 0);
        throw;
    }
}

ArtifactManifestV1 ArtifactStoreV1::read_manifest(std::string_view manifest_id) const {
    if (!is_valid_manifest_id_v1(manifest_id)) throw Error("ArtifactManifestIdInvalid");
    Fd manifests = open_child_directory(root_fd_, "manifests", "ArtifactStoreSymlink");
    const auto name = std::string(manifest_id) + ".manifest";
    Fd file(::openat(manifests.get(), name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!file) {
        if (errno == ELOOP) throw Error("ArtifactManifestCorrupt");
        throw Error("ArtifactManifestMissing");
    }
    check_owner_only(file.get(), "ArtifactStorePermissions", true);
    return decode_manifest_v1(read_all(file.get(), "ArtifactManifestTooLarge"));
}

ArtifactResolvedManifestV1 ArtifactStoreV1::resolve_manifest(
    std::string_view manifest_id) const {
    if (!is_valid_manifest_id_v1(manifest_id)) throw Error("ArtifactManifestIdInvalid");
    const auto manifest = read_manifest(manifest_id);
    ArtifactResolvedManifestV1 resolved;
    resolved.manifest_id = std::string(manifest_id);
    resolved.run_id = manifest.run_id;
    resolved.node_id = manifest.node_id;
    resolved.attempt_id = manifest.attempt_id;
    resolved.manifest_status = manifest.status;
    resolved.result_code = manifest.result_code;
    resolved.objects.reserve(manifest.objects.size());
    for (const auto& object : manifest.objects) {
        verify(object.id);
        resolved.objects.push_back(ArtifactResolvedObjectV1{object.id, object.origin});
    }
    return resolved;
}

} // namespace bruce::artifact_v1
