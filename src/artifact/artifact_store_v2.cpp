#include "bruce/artifact/artifact_store_v2.hpp"
#include "bruce/core/json.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace bruce::artifact_v2 {
namespace {
[[noreturn]] void invalid() { throw Error("PROTOCOL_ERROR"); }

class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) ::close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    int get() const { return value_; }
    explicit operator bool() const { return value_ >= 0; }
private:
    int value_;
};

bool alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool valid_provider_id(std::string_view id) {
    if (id.empty() || id.size() > 128 || !alnum(static_cast<unsigned char>(id.front())))
        return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return alnum(c) || c == '-' || c == '_' || c == '.';
    });
}

std::vector<std::string> components(const std::filesystem::path& path) {
    const auto text = path.string();
    if (text.empty() || text.find('\0') != std::string::npos) invalid();
    std::vector<std::string> names;
    for (const auto& part : path) {
        const auto name = part.string();
        if (name == "..") invalid();
        if (name.empty() || name == "." || name == "/") continue;
        names.push_back(name);
    }
    if (names.empty()) invalid();
    return names;
}

void validate_root(const std::filesystem::path& path) {
    const auto text = path.string();
    if (!path.is_absolute() || text.ends_with('/') || text.find("//") != std::string::npos)
        invalid();
    (void)components(path);
    for (const auto& part : path) if (part == ".") invalid();
}

void validate_config(const ProviderConfigV2& config) {
    std::string_view previous;
    for (const auto& provider : config.providers) {
        if (!valid_provider_id(provider.id) || (!previous.empty() && provider.id <= previous)) invalid();
        validate_root(provider.root);
        previous = provider.id;
    }
}

void validate_expected(const artifact_v1::ArtifactIdV1& id) {
    if (id.sha256.size() != 64 || id.size_bytes > max_file_bytes) invalid();
    for (const unsigned char c : id.sha256)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) invalid();
}

struct stat checked_status(int fd, bool regular) {
    struct stat status{};
    if (::fstat(fd, &status) != 0 || status.st_uid != ::geteuid() ||
        (status.st_mode & 0077) != 0) invalid();
    if (regular) {
        if (!S_ISREG(status.st_mode) || status.st_nlink != 1 || status.st_size < 0 ||
            static_cast<std::uint64_t>(status.st_size) > max_file_bytes) invalid();
    } else if (!S_ISDIR(status.st_mode)) invalid();
    return status;
}

Fd open_directory(int parent, const std::string& name, bool owner_only) {
    Fd result(::openat(parent, name.c_str(),
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!result) invalid();
    if (owner_only) (void)checked_status(result.get(), false);
    return result;
}

// Ancestors such as /tmp may be shared, but no component may be a symlink.
// The allowlisted root and all digest-derived children are owner-only.
Fd open_tree(const std::filesystem::path& path, bool owner_only) {
    const auto names = components(path);
    Fd current(::open(path.is_absolute() ? "/" : ".",
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!current) invalid();
    for (const auto& name : names) current = open_directory(current.get(), name, false);
    if (owner_only) (void)checked_status(current.get(), false);
    return current;
}

Fd open_regular(int parent, const std::string& name) {
    // Reject special nodes before opening them; O_NONBLOCK also closes the
    // FIFO race between this check and openat, and fstat checks the opened inode.
    struct stat status{};
    if (::fstatat(parent, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) != 0 ||
        !S_ISREG(status.st_mode)) invalid();
    Fd result(::openat(parent, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (!result) invalid();
    (void)checked_status(result.get(), true);
    return result;
}

Fd open_config(const std::filesystem::path& path) {
    const auto names = components(path);
    Fd parent(::open(path.is_absolute() ? "/" : ".",
                      O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!parent) invalid();
    for (std::size_t index = 0; index + 1 < names.size(); ++index)
        parent = open_directory(parent.get(), names[index], false);
    return open_regular(parent.get(), names.back());
}

std::size_t read_chunk(int fd, char* buffer, std::size_t count) {
    ssize_t got;
    do { got = ::read(fd, buffer, count); } while (got < 0 && errno == EINTR);
    if (got < 0) invalid();
    return static_cast<std::size_t>(got);
}

void write_all(int fd, const char* bytes, std::size_t size) {
    while (size != 0) {
        const auto written = ::write(fd, bytes, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) invalid();
        bytes += written;
        size -= static_cast<std::size_t>(written);
    }
}

bool same_snapshot(const struct stat& before, const struct stat& after) {
    return before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
           before.st_size == after.st_size && before.st_mode == after.st_mode &&
           before.st_mtim.tv_sec == after.st_mtim.tv_sec &&
           before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
           before.st_ctim.tv_sec == after.st_ctim.tv_sec &&
           before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
}

std::string read_configuration(int fd) {
    const auto before = checked_status(fd, true);
    std::string bytes;
    std::array<char, 65536> buffer{};
    for (;;) {
        const auto count = read_chunk(fd, buffer.data(),
                                      std::min(buffer.size(), max_file_bytes - bytes.size() + 1));
        if (count == 0) break;
        if (count > max_file_bytes - bytes.size()) invalid();
        bytes.append(buffer.data(), count);
    }
    const auto after = checked_status(fd, true);
    if (!same_snapshot(before, after) || bytes.size() != static_cast<std::uint64_t>(after.st_size))
        invalid();
    return bytes;
}

class Parser {
public:
    explicit Parser(std::string_view bytes) : bytes_(bytes) {}
    void expect(std::string_view text) {
        if (!bytes_.substr(cursor_).starts_with(text)) invalid();
        cursor_ += text.size();
    }
    bool next(char c) const { return cursor_ < bytes_.size() && bytes_[cursor_] == c; }
    bool done() const { return cursor_ == bytes_.size(); }
    std::string string() {
        const auto start = cursor_;
        expect("\"");
        std::string value;
        while (cursor_ < bytes_.size()) {
            const auto c = static_cast<unsigned char>(bytes_[cursor_++]);
            if (c == '"') {
                if (json_string(value) != bytes_.substr(start, cursor_ - start)) invalid();
                return value;
            }
            if (c < 32) invalid();
            if (c != '\\') { value.push_back(static_cast<char>(c)); continue; }
            if (cursor_ == bytes_.size()) invalid();
            const char escape = bytes_[cursor_++];
            if (escape == '"' || escape == '\\') value.push_back(escape);
            else if (escape == 'u') {
                expect("00");
                unsigned byte = 0;
                for (unsigned index = 0; index < 2; ++index) {
                    if (cursor_ == bytes_.size()) invalid();
                    const char digit = bytes_[cursor_++];
                    if (digit >= '0' && digit <= '9') byte = byte * 16 + digit - '0';
                    else if (digit >= 'a' && digit <= 'f') byte = byte * 16 + digit - 'a' + 10;
                    else invalid();
                }
                // Canonical Unicode paths use UTF-8. Only control bytes are
                // emitted as \u00xx; decoding higher values as a single byte
                // would give a JSON code point a different filesystem meaning.
                if (byte >= 32) invalid();
                value.push_back(static_cast<char>(byte));
            } else invalid();
        }
        invalid();
    }
private:
    std::string_view bytes_;
    std::size_t cursor_{};
};

ProviderConfigV2 parse_configuration(std::string_view bytes) {
    Parser parser(bytes);
    parser.expect("{\"contract\":\"bruce-artifact-store-providers-v2\",\"providers\":[");
    ProviderConfigV2 config;
    if (!parser.next(']')) {
        for (;;) {
            parser.expect("{\"id\":");
            auto id = parser.string();
            parser.expect(",\"root\":");
            auto root = parser.string();
            parser.expect("}");
            config.providers.push_back({std::move(id), std::move(root)});
            if (!parser.next(',')) break;
            parser.expect(",");
        }
    }
    parser.expect("]}\n");
    if (!parser.done()) invalid();
    validate_config(config);
    return config;
}

class Staging {
public:
    Staging() {
        std::array<char, 44> pattern{};
        constexpr std::string_view name = "/tmp/bruce-artifact-fetch-v2-XXXXXX";
        std::copy(name.begin(), name.end(), pattern.begin());
        const char* created = ::mkdtemp(pattern.data());
        if (created == nullptr) invalid();
        directory_ = created;
    }
    ~Staging() {
        ::unlink((directory_ / "object").c_str());
        ::rmdir(directory_.c_str());
    }
    Staging(const Staging&) = delete;
    Staging& operator=(const Staging&) = delete;
    std::filesystem::path path() const { return directory_ / "object"; }
    Fd create_file() const {
        Fd directory = open_tree(directory_, true);
        Fd result(::openat(directory.get(), "object", O_WRONLY | O_CREAT | O_EXCL |
                           O_NOFOLLOW | O_CLOEXEC, 0600));
        if (!result) invalid();
        (void)checked_status(result.get(), true);
        return result;
    }
private:
    std::filesystem::path directory_;
};

void copy_verified(int source, int destination, const artifact_v1::ArtifactIdV1& expected) {
    const auto before = checked_status(source, true);
    if (static_cast<std::uint64_t>(before.st_size) != expected.size_bytes) invalid();
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!digest || EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1) invalid();
    std::array<char, 65536> buffer{};
    std::uint64_t total = 0;
    for (;;) {
        const auto count = read_chunk(source, buffer.data(), static_cast<std::size_t>(
            std::min<std::uint64_t>(buffer.size(), expected.size_bytes - total + 1)));
        if (count == 0) break;
        if (count > expected.size_bytes - total) invalid();
        write_all(destination, buffer.data(), count);
        if (EVP_DigestUpdate(digest.get(), buffer.data(), count) != 1) invalid();
        total += count;
    }
    const auto after = checked_status(source, true);
    if (total != expected.size_bytes || !same_snapshot(before, after)) invalid();
    std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
    unsigned size{};
    if (EVP_DigestFinal_ex(digest.get(), bytes.data(), &size) != 1 || size != 32) invalid();
    constexpr char hex[] = "0123456789abcdef";
    std::string actual;
    for (unsigned index = 0; index < size; ++index) {
        actual.push_back(hex[bytes[index] >> 4]);
        actual.push_back(hex[bytes[index] & 15]);
    }
    if (actual != expected.sha256 || ::fsync(destination) != 0) invalid();
}
} // namespace

ProviderConfigV2 read_provider_config_v2(const std::filesystem::path& path) {
    try {
        Fd file = open_config(path);
        return parse_configuration(read_configuration(file.get()));
    } catch (...) { invalid(); }
}

artifact_v1::ArtifactIdV1 fetch_to_store_v2(
    const ProviderConfigV2& config, std::string_view provider_id,
    const artifact_v1::ArtifactIdV1& expected, const std::filesystem::path& store) {
    try {
        validate_config(config);
        validate_expected(expected);
        if (!valid_provider_id(provider_id)) invalid();
        const auto found = std::find_if(config.providers.begin(), config.providers.end(),
            [&](const ProviderV2& provider) { return provider.id == provider_id; });
        if (found == config.providers.end()) invalid();
        Fd root = open_tree(found->root, true);
        Fd objects = open_directory(root.get(), "objects", true);
        Fd prefix = open_directory(objects.get(), expected.sha256.substr(0, 2), true);
        Fd source = open_regular(prefix.get(), expected.sha256 + ".blob");
        Staging staged;
        Fd output = staged.create_file();
        copy_verified(source.get(), output.get(), expected);
        auto local = artifact_v1::ArtifactStoreV1::open(store);
        try {
            return local.put_file(staged.path(), expected.sha256, expected.size_bytes);
        } catch (const Error& error) {
            if (std::string_view(error.what()) != "ArtifactPublishSyncFailed") throw;
            // V1 can report a directory synchronization error after its
            // atomic rename has committed. Resolve that ambiguous outcome
            // from a fresh integrity check instead of reporting a failed
            // fetch while leaving the completed object visible. An already
            // correct object is also a successful idempotent fetch. Never
            // remove an inode: it may predate this call or belong to a
            // concurrent publisher. No crash-durability guarantee is added.
            local.verify(expected);
            return expected;
        }
    } catch (...) { invalid(); }
}
} // namespace bruce::artifact_v2
