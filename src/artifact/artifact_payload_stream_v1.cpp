#include "bruce/artifact/artifact_payload_stream_v1.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace bruce::artifact_v1 {
namespace {
using Code = ArtifactPayloadStreamErrorCodeV15;
constexpr auto chunk_bytes = artifact_payload_stream_chunk_bytes_v15;
[[noreturn]] void fail(Code code) { throw ArtifactPayloadStreamErrorV15(code); }
const char* error_text(Code code) {
    switch (code) {
    case Code::invalid_destination: return "ArtifactPayloadStreamInvalidDestination";
    case Code::invalid_source: return "ArtifactPayloadStreamInvalidSource";
    case Code::limit_exceeded: return "ArtifactPayloadStreamLimitExceeded";
    case Code::deadline_exceeded: return "ArtifactPayloadStreamDeadlineExceeded";
    case Code::digest_size_mismatch: return "ArtifactPayloadStreamDigestSizeMismatch";
    case Code::seal_failure: return "ArtifactPayloadStreamSealFailure";
    case Code::mutation: return "ArtifactPayloadStreamMutation";
    }
    return "ArtifactPayloadStreamError";
}
class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(other.value_) { other.value_ = -1; }
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { if (value_ >= 0) ::close(value_); value_ = other.value_; other.value_ = -1; }
        return *this;
    }
    int get() const { return value_; }
    int release() { const int result = value_; value_ = -1; return result; }
private:
    int value_;
};
class Digest {
public:
    Digest() : ctx_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!ctx_ || EVP_DigestInit_ex(ctx_.get(), EVP_sha256(), nullptr) != 1) fail(Code::seal_failure);
    }
    void update(const void* bytes, std::size_t size) {
        if (EVP_DigestUpdate(ctx_.get(), bytes, size) != 1) fail(Code::seal_failure);
    }
    std::string finish() {
        std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{}; unsigned size{};
        if (EVP_DigestFinal_ex(ctx_.get(), bytes.data(), &size) != 1) fail(Code::seal_failure);
        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        for (unsigned i = 0; i < size; ++i) { result += hex[bytes[i] >> 4]; result += hex[bytes[i] & 15]; }
        return result;
    }
private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx_;
};
bool identifier(std::string_view value) {
    if (value.empty() || value.size() > artifact_manifest_token_limit_v1 || value.find("..") != value.npos) return false;
    const auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    if (!alnum(value.front())) return false;
    for (char c : value) if (!alnum(c) && c != '-' && c != '_' && c != '.') return false;
    return true;
}
bool valid_digest(std::string_view value) {
    if (value.size() != 64) return false;
    for (char c : value) if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return false;
    return true;
}
std::uint64_t chunks(std::uint64_t size) { return size / chunk_bytes + (size % chunk_bytes != 0); }
std::string chunk_name(std::uint64_t index) {
    auto digits = std::to_string(index);
    return "chunk-" + std::string(20 - digits.size(), '0') + digits + ".bin";
}
std::string seal_bytes(const ArtifactPayloadStreamV15& s) {
    return "{\"sha256\":\"" + s.sha256 + "\",\"size_bytes\":" + std::to_string(s.size_bytes) +
        ",\"chunk_bytes\":" + std::to_string(s.chunk_bytes) + ",\"chunk_count\":" + std::to_string(s.chunk_count) + "}";
}
std::string header_bytes(const ArtifactPayloadStreamV15& s) {
    return "{\"stream_id\":\"" + s.stream_id + "\",\"sha256\":\"" + s.sha256 +
        "\",\"size_bytes\":" + std::to_string(s.size_bytes) + ",\"chunk_bytes\":" +
        std::to_string(s.chunk_bytes) + ",\"chunk_count\":" + std::to_string(s.chunk_count) + "}\n";
}
void check_deadline(std::chrono::steady_clock::time_point deadline) {
    if (std::chrono::steady_clock::now() >= deadline) fail(Code::deadline_exceeded);
}
struct stat metadata(int fd, Code code, bool directory = false) {
    struct stat s{};
    if (::fstat(fd, &s) != 0 || (directory ? !S_ISDIR(s.st_mode) : !S_ISREG(s.st_mode)) ||
        s.st_uid != ::geteuid() || (s.st_mode & 0077) != 0 || (!directory && s.st_nlink != 1) || s.st_size < 0)
        fail(code);
    return s;
}
bool same(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_mode == b.st_mode &&
        a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_nlink == b.st_nlink && a.st_size == b.st_size &&
        a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
void unchanged(int fd, const struct stat& before, Code code, bool directory = false) {
    if (!same(before, metadata(fd, code, directory))) fail(code);
}
void linked(int parent, const std::string& name, const struct stat& expected, Code code) {
    struct stat now{};
    if (::fstatat(parent, name.c_str(), &now, AT_SYMLINK_NOFOLLOW) != 0 || !same(expected, now)) fail(code);
}
// Traverse every component without following symlinks, including ancestors of
// the supplied root. Ancestors such as /tmp need not themselves be private.
Fd open_root(const std::filesystem::path& root) {
    if (root.empty() || root.native().find('\0') != std::string::npos) fail(Code::invalid_destination);
    Fd current(::open(root.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (current.get() < 0) fail(Code::invalid_destination);
    for (const auto& part : root.relative_path()) {
        const auto name = part.string();
        if (name.empty() || name == ".") continue;
        if (name == "..") fail(Code::invalid_destination);
        Fd next(::openat(current.get(), name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        if (next.get() < 0) fail(Code::invalid_destination);
        current = std::move(next);
    }
    metadata(current.get(), Code::invalid_destination, true);
    return current;
}
void root_still_linked(const std::filesystem::path& path, int fd) {
    const auto before = metadata(fd, Code::invalid_destination, true);
    auto current = open_root(path);
    const auto after = metadata(current.get(), Code::invalid_destination, true);
    if (before.st_dev != after.st_dev || before.st_ino != after.st_ino) fail(Code::mutation);
}
Fd open_file(int dir, const std::string& name, Code code) {
    // O_NONBLOCK ensures hostile FIFOs cannot block before the type check.
    Fd fd(::openat(dir, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if (fd.get() < 0) fail(code);
    metadata(fd.get(), code);
    return fd;
}
void sync_file(int fd, std::chrono::steady_clock::time_point deadline) {
    check_deadline(deadline);
    if (::fsync(fd) != 0) fail(Code::seal_failure);
}
void write_file(int dir, const std::string& name, std::string_view bytes,
                std::chrono::steady_clock::time_point deadline) {
    check_deadline(deadline);
    Fd file(::openat(dir, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (file.get() < 0) fail(Code::seal_failure);
    metadata(file.get(), Code::seal_failure);
    std::size_t offset{};
    while (offset < bytes.size()) {
        check_deadline(deadline);
        const auto n = ::write(file.get(), bytes.data() + offset, bytes.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fail(Code::seal_failure);
        offset += static_cast<std::size_t>(n);
    }
    sync_file(file.get(), deadline);
    const auto after = metadata(file.get(), Code::seal_failure);
    if (static_cast<std::uint64_t>(after.st_size) != bytes.size()) fail(Code::seal_failure);
    linked(dir, name, after, Code::mutation);
}
// Remove only entries beneath already opened directories. Unlink files and
// symlinks directly; recurse only after a no-follow directory open succeeds.
// Cleanup deliberately ignores the expired operation deadline and cannot throw
// while unwinding the original stream failure.
void remove_staging_entry(int parent, const char* name) noexcept {
    if (::unlinkat(parent, name, 0) == 0) return;
    Fd child(::openat(parent, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (child.get() >= 0) {
        const int child_fd = child.release();
        DIR* directory = ::fdopendir(child_fd);
        if (directory) {
            while (auto* entry = ::readdir(directory)) {
                const std::string_view entry_name(entry->d_name);
                if (entry_name != "." && entry_name != "..") remove_staging_entry(child_fd, entry->d_name);
            }
            ::closedir(directory);
        } else ::close(child_fd);
    }
    // Removing an empty directory needs access to its parent, not permission
    // to open the directory itself. A restrictive umask can create mode 000
    // staging, so attempt removal even when openat/fdopendir failed.
    if (::unlinkat(parent, name, AT_REMOVEDIR) != 0) {
        // A directory replaced with a symlink during cleanup is still unlinked
        // without following its target.
        ::unlinkat(parent, name, 0);
    }
}
class Staging {
public:
    explicit Staging(int root) : root_(root) {
        std::array<unsigned char, 16> random{};
        if (RAND_bytes(random.data(), random.size()) != 1) fail(Code::seal_failure);
        constexpr char hex[] = "0123456789abcdef"; name = ".stream-staging-";
        for (auto byte : random) { name += hex[byte >> 4]; name += hex[byte & 15]; }
        if (::mkdirat(root_, name.c_str(), 0700) != 0) fail(Code::invalid_destination);
        active = true;
    }
    ~Staging() {
        if (active) remove_staging_entry(root_, name.c_str());
    }

    std::string name;
    bool active = false;
private:
    int root_;
};
void verify_text(int dir, const std::string& name, const std::string& expected, Code mismatch,
                 std::chrono::steady_clock::time_point deadline) {
    check_deadline(deadline);
    auto fd = open_file(dir, name, Code::mutation);
    const auto before = metadata(fd.get(), Code::mutation);
    if (static_cast<std::uint64_t>(before.st_size) != expected.size()) fail(mismatch);
    std::array<char, 1024> bytes{};
    if (expected.size() > bytes.size()) fail(mismatch);
    std::size_t offset{};
    while (offset < expected.size()) {
        check_deadline(deadline);
        const auto n = ::read(fd.get(), bytes.data() + offset, expected.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fail(Code::mutation);
        offset += static_cast<std::size_t>(n);
    }
    if (std::string_view(bytes.data(), offset) != expected) fail(mismatch);
    unchanged(fd.get(), before, Code::mutation);
    linked(dir, name, before, Code::mutation);
}
// Fingerprint file identities and security-relevant metadata before and after
// the entire payload pass. A chunk changed after its own read must still fail;
// retaining one digest keeps verification memory independent of payload size.
std::string file_snapshots(int dir, const ArtifactPayloadStreamV15& stream,
                           std::chrono::steady_clock::time_point deadline) {
    Digest digest;
    auto snapshot = [&](const std::string& name) {
        check_deadline(deadline);
        auto fd = open_file(dir, name, Code::mutation);
        const auto s = metadata(fd.get(), Code::mutation);
        const std::array<std::uint64_t, 12> fields{
            static_cast<std::uint64_t>(s.st_dev), static_cast<std::uint64_t>(s.st_ino),
            static_cast<std::uint64_t>(s.st_mode), static_cast<std::uint64_t>(s.st_uid),
            static_cast<std::uint64_t>(s.st_gid), static_cast<std::uint64_t>(s.st_nlink),
            static_cast<std::uint64_t>(s.st_size), static_cast<std::uint64_t>(s.st_mtim.tv_sec),
            static_cast<std::uint64_t>(s.st_mtim.tv_nsec), static_cast<std::uint64_t>(s.st_ctim.tv_sec),
            static_cast<std::uint64_t>(s.st_ctim.tv_nsec), 0};
        digest.update(fields.data(), sizeof(fields));
        linked(dir, name, s, Code::mutation);
    };
    snapshot("header.jsonl"); snapshot("seal.json");
    for (std::uint64_t i = 0; i < stream.chunk_count; ++i) snapshot(chunk_name(i));
    return digest.finish();
}
void verify_directory(int dir, const ArtifactPayloadStreamV15& stream,
                      std::chrono::steady_clock::time_point deadline) {
    check_deadline(deadline);
    const auto before = metadata(dir, Code::mutation, true);
    // The bytes must exactly match the canonical serialization of independently
    // validated metadata. This rejects alternate JSON, duplicate/unknown keys,
    // reordered fields and noncanonical number representations without a parser.
    verify_text(dir, "seal.json", seal_bytes(stream), Code::seal_failure, deadline);
    verify_text(dir, "header.jsonl", header_bytes(stream), Code::mutation, deadline);
    check_deadline(deadline);
    Fd scan(::openat(dir, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (scan.get() < 0) fail(Code::mutation);
    const int scan_fd = scan.release();
    DIR* raw = ::fdopendir(scan_fd);
    if (!raw) { ::close(scan_fd); fail(Code::mutation); }
    const auto close_directory = [](DIR* d) { ::closedir(d); };
    std::unique_ptr<DIR, decltype(close_directory)> entries(raw, close_directory);
    std::uint64_t seen{};
    while (true) {
        check_deadline(deadline);
        errno = 0; auto* entry = ::readdir(entries.get());
        if (!entry) { if (errno != 0) fail(Code::mutation); break; }
        const std::string_view name(entry->d_name);
        if (name == "." || name == ".." || name == "header.jsonl" || name == "seal.json") continue;
        if (name.size() != 30 || name.substr(0, 6) != "chunk-" || name.substr(26) != ".bin") fail(Code::mutation);
        std::uint64_t index{};
        auto parsed = std::from_chars(name.data() + 6, name.data() + 26, index);
        if (parsed.ec != std::errc{} || parsed.ptr != name.data() + 26 || index >= stream.chunk_count || name != chunk_name(index)) fail(Code::mutation);
        ++seen;
    }
    if (seen != stream.chunk_count) fail(Code::mutation);
    const auto snapshots = file_snapshots(dir, stream, deadline);
    // Filesystems may assign the same timestamp to rapid successive writes.
    // A second content pass also catches changes to an earlier chunk made
    // while a later chunk was being read, without relying on clock precision.
    std::array<char, chunk_bytes> bytes{};
    for (unsigned pass = 0; pass < 2; ++pass) {
        Digest digest;
        std::uint64_t total{};
        for (std::uint64_t index = 0; index < stream.chunk_count; ++index) {
            check_deadline(deadline);
            const auto name = chunk_name(index); auto fd = open_file(dir, name, Code::mutation);
            const auto snapshot = metadata(fd.get(), Code::mutation);
            const auto size = index + 1 == stream.chunk_count ? stream.size_bytes - total : chunk_bytes;
            if (static_cast<std::uint64_t>(snapshot.st_size) != size) fail(Code::mutation);
            std::size_t offset{};
            while (offset < size) {
                check_deadline(deadline);
                const auto n = ::read(fd.get(), bytes.data() + offset, static_cast<std::size_t>(size) - offset);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) fail(Code::mutation);
                offset += static_cast<std::size_t>(n);
            }
            digest.update(bytes.data(), offset); total += offset;
            unchanged(fd.get(), snapshot, Code::mutation); linked(dir, name, snapshot, Code::mutation);
        }
        if (total != stream.size_bytes || digest.finish() != stream.sha256) fail(Code::mutation);
    }
    // Recheck header/seal after the payload pass as well as the directory's
    // identity, timestamps and membership throughout the verification window.
    verify_text(dir, "seal.json", seal_bytes(stream), Code::seal_failure, deadline);
    verify_text(dir, "header.jsonl", header_bytes(stream), Code::mutation, deadline);
    if (snapshots != file_snapshots(dir, stream, deadline)) fail(Code::mutation);
    unchanged(dir, before, Code::mutation, true);
}
void validate_stream(const ArtifactPayloadStreamV15& s) {
    if (!identifier(s.stream_id) || s.reference != "stream-" + s.stream_id) fail(Code::invalid_destination);
    if (!valid_digest(s.sha256) || s.chunk_bytes != chunk_bytes || s.chunk_count != chunks(s.size_bytes)) fail(Code::seal_failure);
}
} // namespace

ArtifactPayloadStreamErrorV15::ArtifactPayloadStreamErrorV15(ArtifactPayloadStreamErrorCodeV15 code)
    : Error(error_text(code)), code_(code) {}

ArtifactPayloadStreamV15 write_payload_stream_v15(
    int source_fd, const std::filesystem::path& attempt_root, std::string_view stream_id,
    const ArtifactIdV1& expected, std::uint64_t max_bytes,
    std::chrono::steady_clock::time_point deadline) {
    if (!identifier(stream_id)) fail(Code::invalid_destination);
    if (!valid_digest(expected.sha256)) fail(Code::digest_size_mismatch);
    check_deadline(deadline);
    if (expected.size_bytes > max_bytes) fail(Code::limit_exceeded);
    const auto source_before = metadata(source_fd, Code::invalid_source);
    const int flags = ::fcntl(source_fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) == O_WRONLY) fail(Code::invalid_source);
    auto root = open_root(attempt_root);
    ArtifactPayloadStreamV15 stream{std::string(stream_id), "stream-" + std::string(stream_id), expected.sha256,
                                    expected.size_bytes, chunk_bytes, chunks(expected.size_bytes)};
    struct stat existing{};
    if (::fstatat(root.get(), stream.reference.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) fail(Code::invalid_destination);
    check_deadline(deadline);
    Staging staging(root.get());
    Fd dir(::openat(root.get(), staging.name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (dir.get() < 0) fail(Code::invalid_destination);
    metadata(dir.get(), Code::invalid_destination, true);
    Digest digest; std::array<char, chunk_bytes> bytes{};
    std::uint64_t total{}, count{};
    bool eof = false;
    while (!eof) {
        std::size_t offset{};
        while (offset < bytes.size()) {
            check_deadline(deadline);
            const auto remaining = max_bytes - total;
            if (remaining == 0) {
                // A borrowed regular-file descriptor lets us establish EOF
                // without consuming even a probe byte past the byte budget.
                const auto position = ::lseek(source_fd, 0, SEEK_CUR);
                if (position < 0) fail(Code::invalid_source);
                unchanged(source_fd, source_before, Code::mutation);
                if (position < source_before.st_size) fail(Code::limit_exceeded);
                eof = true;
                break;
            }
            const auto request = static_cast<std::size_t>(std::min<std::uint64_t>(bytes.size() - offset, remaining));
            const auto n = ::read(source_fd, bytes.data() + offset, request);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0) fail(Code::invalid_source);
            if (n == 0) { eof = true; break; }
            if (static_cast<std::uint64_t>(n) > max_bytes - total) fail(Code::limit_exceeded);
            total += static_cast<std::uint64_t>(n); offset += static_cast<std::size_t>(n);
        }
        if (offset != 0) {
            digest.update(bytes.data(), offset);
            write_file(dir.get(), chunk_name(count++), std::string_view(bytes.data(), offset), deadline);
        }
    }
    unchanged(source_fd, source_before, Code::mutation);
    if (total != expected.size_bytes || count != stream.chunk_count || digest.finish() != expected.sha256) fail(Code::digest_size_mismatch);
    write_file(dir.get(), "header.jsonl", header_bytes(stream), deadline);
    write_file(dir.get(), "seal.json", seal_bytes(stream), deadline);
    sync_file(dir.get(), deadline);
    verify_directory(dir.get(), stream, deadline);
    unchanged(source_fd, source_before, Code::mutation);
    root_still_linked(attempt_root, root.get());
    linked(root.get(), staging.name, metadata(dir.get(), Code::mutation, true), Code::mutation);
    check_deadline(deadline);
    // Linux RENAME_NOREPLACE guarantees atomic adoption without replacing an
    // existing stream even when a competing writer races the initial check.
    if (::syscall(SYS_renameat2, root.get(), staging.name.c_str(), root.get(), stream.reference.c_str(), 1U) != 0)
        fail(Code::invalid_destination);
    staging.active = false;
    return stream;
}

void verify_payload_stream_v15(const ArtifactPayloadStreamV15& stream,
                               const std::filesystem::path& attempt_root) {
    verify_payload_stream_v15(stream, attempt_root, std::chrono::steady_clock::time_point::max());
}

void verify_payload_stream_v15(const ArtifactPayloadStreamV15& stream,
                               const std::filesystem::path& attempt_root,
                               std::chrono::steady_clock::time_point deadline) {
    check_deadline(deadline);
    validate_stream(stream);
    auto root = open_root(attempt_root);
    check_deadline(deadline);
    Fd dir(::openat(root.get(), stream.reference.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (dir.get() < 0) fail(Code::mutation);
    const auto snapshot = metadata(dir.get(), Code::mutation, true);
    verify_directory(dir.get(), stream, deadline);
    check_deadline(deadline);
    linked(root.get(), stream.reference, snapshot, Code::mutation);
    check_deadline(deadline);
    root_still_linked(attempt_root, root.get());
    check_deadline(deadline);
}
} // namespace bruce::artifact_v1
