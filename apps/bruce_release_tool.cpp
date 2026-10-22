// Linux-only offline release publication. No private material enters diagnostics.
#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/release/release_signature_v1.hpp"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <array>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
enum class Failure { arguments, contract, verification, file, internal };
[[noreturn]] void fail(Failure reason = Failure::file) { throw reason; }
void require(bool condition, Failure reason = Failure::contract) { if (!condition) fail(reason); }
void file_require(bool condition) { require(condition, Failure::file); }
const char* public_code(Failure reason) {
    switch (reason) {
        case Failure::arguments: return "ReleaseToolInvalidArguments";
        case Failure::contract: return "ReleaseToolInvalidContract";
        case Failure::verification: return "ReleaseToolVerificationFailed";
        case Failure::file: return "ReleaseToolUnsafeFile";
        case Failure::internal: return "ReleaseToolFailed";
    }
    return "ReleaseToolFailed";
}
int report_failure(Failure reason) {
    std::cerr << "{\"contract\":\"bruce-release-tool-report-v1\",\"error\":\""
              << public_code(reason) << "\",\"ok\":false}\n";
    return 2;
}
struct Fd {
    int value;
    explicit Fd(int v) : value(v) { file_require(v >= 0); }
    ~Fd() { ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value(std::exchange(other.value, -1)) {}
};
// Resolve every directory component with O_NOFOLLOW, retaining the final fd.
Fd directory(const std::string& path) {
    file_require(!path.empty());
    Fd current(::open(path.front() == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    std::size_t start = path.front() == '/' ? 1 : 0;
    while (start < path.size()) {
        const auto end = path.find('/', start);
        const auto component = path.substr(start, end == std::string::npos ? end : end - start);
        file_require(component != "..");
        if (!component.empty() && component != ".") {
            Fd next(::openat(current.value, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
            std::swap(current.value, next.value);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return current;
}
struct Location {
    Fd parent;
    std::string name;
    explicit Location(const std::string& path)
        : parent(directory(path.find_last_of('/') == std::string::npos ? "." :
                           path.substr(0, path.find_last_of('/') + 1))),
          name(path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1)) {
        file_require(!name.empty() && name != "." && name != ".." && name.size() <= 255);
    }
};
struct stat metadata(int fd) { struct stat s{}; file_require(::fstat(fd, &s) == 0); return s; }
bool same_snapshot(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size &&
        a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
void safe_publication_parent(int fd) {
    const auto s = metadata(fd);
    // An untrusted directory owner or a writable non-sticky parent could
    // substitute the staging entry between its inode check and renameat2.
    file_require((s.st_uid == ::geteuid() || s.st_uid == 0) &&
            ((s.st_mode & 0022) == 0 || (s.st_mode & S_ISVTX) != 0));
}
Fd regular(int parent, const std::string& name) {
    Fd fd(::openat(parent, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    file_require(S_ISREG(metadata(fd.value).st_mode));
    return fd;
}
std::string read_at(int parent, const std::string& name, std::size_t limit) {
    auto fd = regular(parent, name);
    const auto before = metadata(fd.value);
    file_require(before.st_size >= 0 && static_cast<std::uint64_t>(before.st_size) <= limit);
    std::string bytes;
    std::array<char, 65536> block{};
    while (true) {
        const auto count = ::read(fd.value, block.data(), block.size());
        if (count < 0 && errno == EINTR) continue;
        file_require(count >= 0);
        if (!count) break;
        file_require(static_cast<std::size_t>(count) <= limit - bytes.size());
        bytes.append(block.data(), static_cast<std::size_t>(count));
    }
    file_require(same_snapshot(before, metadata(fd.value)) && bytes.size() == static_cast<std::size_t>(before.st_size));
    return bytes;
}
std::string read_path(const std::string& path, std::size_t limit) {
    Location location(path);
    return read_at(location.parent.value, location.name, limit);
}
bool hex(std::string_view s, std::size_t n) {
    if (s.size() != n) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
unsigned nibble(char c) { return c <= '9' ? static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'a' + 10); }
struct Secret {
    std::array<char, 256> bytes{};
    std::array<unsigned char, 32> seed{};
    ~Secret() { OPENSSL_cleanse(bytes.data(), bytes.size()); OPENSSL_cleanse(seed.data(), seed.size()); }
    Secret(const Secret&) = delete;
    Secret& operator=(const Secret&) = delete;
    Secret() = default;
    void load(const std::string& path) {
        Location location(path);
        auto fd = regular(location.parent.value, location.name);
        const auto before = metadata(fd.value);
        constexpr std::string_view header = "BRUCE_RELEASE_PRIVATE_KEY_V1\n";
        constexpr auto size = header.size() + 64 + 1;
        file_require(before.st_uid == ::geteuid() && (before.st_mode & 07777) == 0600 && before.st_nlink == 1);
        require(before.st_size == static_cast<off_t>(size));
        std::size_t used = 0;
        while (true) {
            require(used < bytes.size());
            const auto count = ::read(fd.value, bytes.data() + used, bytes.size() - used);
            if (count < 0 && errno == EINTR) continue;
            file_require(count >= 0);
            if (!count) break;
            used += static_cast<std::size_t>(count);
        }
        const auto after = metadata(fd.value);
        file_require(same_snapshot(before, after) && after.st_uid == ::geteuid() &&
                (after.st_mode & 07777) == 0600 && after.st_nlink == 1 && used == size);
        const std::string_view contents(bytes.data(), used);
        require(contents.starts_with(header) && contents.back() == '\n');
        const auto digits = contents.substr(header.size(), 64);
        require(hex(digits, 64));
        for (std::size_t i = 0; i < seed.size(); ++i)
            seed[i] = static_cast<unsigned char>((nibble(digits[i * 2]) << 4) | nibble(digits[i * 2 + 1]));
    }
};

// The audit producer emits pretty JSON, unlike the canonical release contracts.
// Parse the complete bounded document, rejecting duplicates at every depth.
struct Json {
    char kind = 0;
    std::string text;
    std::map<std::string, Json> object;
    std::vector<Json> array;
};
class AuditParser {
    std::string_view input;
    std::size_t pos = 0, nodes = 0;
    void ws() { while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\n' || input[pos] == '\r' || input[pos] == '\t')) ++pos; }
    char next() { require(pos < input.size()); return input[pos++]; }
    char peek() { ws(); require(pos < input.size()); return input[pos]; }
    std::string string() {
        require(next() == '"');
        std::string result;
        while (true) {
            const auto c = static_cast<unsigned char>(next());
            if (c == '"') return result;
            require(c >= 32);
            if (c >= 128) {
                const unsigned width = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 :
                    c >= 0xf0 && c <= 0xf4 ? 4 : 0;
                require(width != 0);
                result += static_cast<char>(c);
                for (unsigned i = 1; i < width; ++i) {
                    const auto continuation = static_cast<unsigned char>(next());
                    require(continuation >= 0x80 && continuation <= 0xbf);
                    if (i == 1) require(!(c == 0xe0 && continuation < 0xa0) &&
                        !(c == 0xed && continuation >= 0xa0) && !(c == 0xf0 && continuation < 0x90) &&
                        !(c == 0xf4 && continuation >= 0x90));
                    result += static_cast<char>(continuation);
                }
                continue;
            }
            if (c != '\\') { result += static_cast<char>(c); continue; }
            const char escaped = next();
            if (escaped == '"' || escaped == '\\' || escaped == '/') result += escaped;
            else if (escaped == 'b') result += '\b';
            else if (escaped == 'f') result += '\f';
            else if (escaped == 'n') result += '\n';
            else if (escaped == 'r') result += '\r';
            else if (escaped == 't') result += '\t';
            else {
                require(escaped == 'u');
                unsigned code = 0;
                for (int i = 0; i < 4; ++i) {
                    char h = next();
                    if (h >= 'A' && h <= 'F') h = static_cast<char>(h - 'A' + 'a');
                    require((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f'));
                    code = (code << 4) | nibble(h);
                }
                // Audit fields are ASCII or direct UTF-8 from ensure_ascii=False.
                require(code < 128);
                result += static_cast<char>(code);
            }
        }
    }
    Json value(unsigned depth) {
        require(depth <= 32 && ++nodes <= 16384);
        const char c = peek();
        Json result;
        result.kind = c;
        if (c == '"') { result.text = string(); return result; }
        if (c == '{' || c == '[') {
            next();
            const char close = c == '{' ? '}' : ']';
            if (peek() == close) { next(); return result; }
            while (true) {
                if (c == '{') {
                    ws(); auto name = string(); require(peek() == ':'); next();
                    auto child = value(depth + 1);
                    require(result.object.emplace(std::move(name), std::move(child)).second);
                } else result.array.push_back(value(depth + 1));
                const char delimiter = peek(); next();
                if (delimiter == close) return result;
                require(delimiter == ',');
            }
        }
        if (c == 't' || c == 'f' || c == 'n') {
            const std::string_view literal = c == 't' ? "true" : c == 'f' ? "false" : "null";
            require(input.substr(pos, literal.size()) == literal); pos += literal.size(); return result;
        }
        // The producer uses only nonnegative integers (not floats).
        require(c >= '0' && c <= '9');
        result.kind = '0';
        do { result.text += next(); } while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9');
        require(result.text.size() == 1 || result.text.front() != '0');
        return result;
    }
public:
    explicit AuditParser(std::string_view bytes) : input(bytes) {}
    Json parse() { auto result = value(0); ws(); require(pos == input.size()); return result; }
};
const Json& field(const Json& value, const char* name) {
    require(value.kind == '{'); const auto found = value.object.find(name);
    require(found != value.object.end()); return found->second;
}
std::string text(const Json& value) { require(value.kind == '"'); return value.text; }
void successful_tree(const Json& tree, const std::string& version) {
    require(text(field(tree, "version")) == version);
    const auto& info = field(tree, "build_info");
    require(text(field(info, "contract")) == "bruce-build-info-v1" &&
            text(field(info, "version")) == version && !text(field(info, "build_type")).empty());
    require(hex(text(field(tree, "binary_sha256")), 64));
    require(field(tree, "research_export_replay").kind == 't' && field(tree, "budget_failure_no_export").kind == 't');
    const auto& fixtures = field(tree, "historical_fixtures");
    require(fixtures.kind == '[' && !fixtures.array.empty());
    for (const auto& fixture : fixtures.array)
        require(!text(field(fixture, "name")).empty() && hex(text(field(fixture, "sha256")), 64));
}
void archive_version(const bruce::ReleaseSignatureEnvelopeV1& e) {
    const auto prefix = "bruce-" + e.version + "-";
    const auto cpack_prefix = "bruce-core-" + e.version + "-";
    require(((e.archive.starts_with(prefix) && e.archive.size() > prefix.size() + 7) ||
             (e.archive.starts_with(cpack_prefix) && e.archive.size() > cpack_prefix.size() + 7)) &&
            e.archive.ends_with(".tar.gz"));
}
std::string manifest(const bruce::ReleaseSignatureEnvelopeV1& e) {
    return "{\"archive\":" + bruce::json_string(e.archive) + ",\"archive_sha256\":" + bruce::json_string(e.archive_sha256) +
        ",\"contract\":\"bruce-release-set-v1\",\"key_id\":" + bruce::json_string(e.key_id) +
        ",\"version\":" + bruce::json_string(e.version) + "}\n";
}
constexpr std::size_t archive_limit = 512ULL * 1024 * 1024;
struct Stage {
    int parent;
    std::string name;
    Fd fd;
    std::vector<std::string> owned;
    bool published = false;
    static std::string create(int parent) {
        std::array<unsigned char, 16> random{};
        require(RAND_bytes(random.data(), random.size()) == 1, Failure::verification);
        constexpr char digits[] = "0123456789abcdef";
        std::string name = ".bruce-release-";
        for (auto byte : random) { name += digits[byte >> 4]; name += digits[byte & 15]; }
        file_require(::mkdirat(parent, name.c_str(), 0700) == 0);
        return name;
    }
    static int open_created(int parent, const std::string& name) {
        const int fd = ::openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        // Even an unusually restrictive umask must not leak an empty stage.
        if (fd < 0) { ::unlinkat(parent, name.c_str(), AT_REMOVEDIR); fail(); }
        return fd;
    }
    explicit Stage(int p) : parent(p), name(create(p)),
        fd(open_created(p, name)) {}
    ~Stage() {
        if (published) return;
        for (const auto& file : owned) ::unlinkat(fd.value, file.c_str(), 0);
        struct stat entry{};
        const auto own = metadata_noexcept();
        if (::fstatat(parent, name.c_str(), &entry, AT_SYMLINK_NOFOLLOW) == 0 &&
            own.st_ino == entry.st_ino && own.st_dev == entry.st_dev)
            ::unlinkat(parent, name.c_str(), AT_REMOVEDIR);
    }
    struct stat metadata_noexcept() const { struct stat s{}; ::fstat(fd.value, &s); return s; }
    void write(const std::string& name, std::string_view bytes) {
        // Reserve ownership bookkeeping before creating the file.
        owned.push_back(name);
        int raw = ::openat(fd.value, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (raw < 0) { owned.pop_back(); fail(); }
        Fd out(raw);
        while (!bytes.empty()) {
            const auto count = ::write(out.value, bytes.data(), bytes.size());
            if (count < 0 && errno == EINTR) continue;
            file_require(count > 0); bytes.remove_prefix(static_cast<std::size_t>(count));
        }
        file_require(::fsync(out.value) == 0);
    }
    void publish(const std::string& target) {
        safe_publication_parent(parent);
        file_require(::fsync(fd.value) == 0 && ::fsync(parent) == 0);
        struct stat entry{};
        const auto own = metadata(fd.value);
        file_require(::fstatat(parent, name.c_str(), &entry, AT_SYMLINK_NOFOLLOW) == 0 &&
                own.st_dev == entry.st_dev && own.st_ino == entry.st_ino);
        // Linux renameat2 is essential: a preflight existence check alone races.
        file_require(::syscall(SYS_renameat2, parent, name.c_str(), parent, target.c_str(), 1 /* RENAME_NOREPLACE */) == 0);
        published = true;
        file_require(::fsync(parent) == 0);
    }
};
using Options = std::map<std::string, std::string>;
bruce::ReleaseSignatureEnvelopeV1 sign(const Options& o) {
    Location archive(o.at("--archive")), output(o.at("--output-dir"));
    safe_publication_parent(output.parent.value);
    struct stat collision{};
    file_require(::fstatat(output.parent.value, output.name.c_str(), &collision, AT_SYMLINK_NOFOLLOW) < 0 && errno == ENOENT);
    const auto audit = AuditParser(read_path(o.at("--audit-report"), 1024 * 1024)).parse();
    require(text(field(audit, "contract")) == "bruce-install-audit-v1");
    const auto& package = field(audit, "package");
    const auto pin = bruce::parse_release_public_key_v1(read_path(o.at("--trusted-public-key"), 4096));
    bruce::ReleaseSignatureEnvelopeV1 envelope{"bruce-release-signature-v1", text(field(package, "version")),
        text(field(package, "archive_name")), text(field(package, "archive_sha256")), pin.key_id, std::string(128, '0')};
    (void)bruce::render_release_signature_v1(envelope); archive_version(envelope);
    require(archive.name == envelope.archive);
    successful_tree(package, envelope.version); successful_tree(field(audit, "installation"), envelope.version);
    const auto bytes = read_at(archive.parent.value, archive.name, archive_limit);
    require(bruce::sha256(bytes) == envelope.archive_sha256, Failure::verification);
    {
        Secret secret; secret.load(o.at("--private-key"));
        envelope = bruce::sign_release_signature_v1(envelope, secret.seed, pin);
    }
    bruce::verify_release_signature_v1(envelope, pin, pin);
    Stage stage(output.parent.value);
    stage.write(envelope.archive, bytes);
    stage.write(envelope.archive + ".sha256", envelope.archive_sha256 + "  " + envelope.archive + "\n");
    stage.write(envelope.archive + ".sig.json", bruce::render_release_signature_v1(envelope));
    stage.write("release-key-v1.json", bruce::render_release_public_key_v1(pin));
    stage.write("release-set-v1.json", manifest(envelope));
    stage.publish(output.name);
    return envelope;
}
bruce::ReleaseSignatureEnvelopeV1 verify(const Options& o) {
    auto dir = directory(o.at("--release-dir"));
    const auto pin = bruce::parse_release_public_key_v1(read_path(o.at("--trusted-public-key"), 4096));
    const auto key_bytes = read_at(dir.value, "release-key-v1.json", 4096);
    const auto bundled = bruce::parse_release_public_key_v1(key_bytes);
    require(key_bytes == bruce::render_release_public_key_v1(pin), Failure::verification);
    const auto commit_bytes = read_at(dir.value, "release-set-v1.json", 4096);
    const auto commit = AuditParser(commit_bytes).parse();
    // Validate the archive basename through the signature contract before openat.
    bruce::ReleaseSignatureEnvelopeV1 hint{"bruce-release-signature-v1", text(field(commit, "version")),
        text(field(commit, "archive")), text(field(commit, "archive_sha256")), text(field(commit, "key_id")), std::string(128, '0')};
    (void)bruce::render_release_signature_v1(hint);
    require(commit_bytes == manifest(hint));
    const auto envelope = bruce::parse_release_signature_v1(read_at(dir.value, hint.archive + ".sig.json", 4096));
    require(commit_bytes == manifest(envelope)); archive_version(envelope);
    const auto checksum = read_at(dir.value, envelope.archive + ".sha256", 1024);
    require(checksum == envelope.archive_sha256 + "  " + envelope.archive + "\n");
    require(bruce::sha256(read_at(dir.value, envelope.archive, archive_limit)) == envelope.archive_sha256,
            Failure::verification);
    bruce::verify_release_signature_v1(envelope, bundled, pin);
    return envelope;
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "{\"contract\":\"bruce-release-tool-help-v1\",\"usage\":\"sign --archive ARCHIVE --audit-report AUDIT --private-key KEY --trusted-public-key PIN --output-dir NEW-DIR | verify --release-dir DIR --trusted-public-key PIN\"}\n";
            return 0;
        }
        require(argc >= 2, Failure::arguments);
        const std::string action = argv[1];
        require(action == "sign" || action == "verify", Failure::arguments);
        const std::vector<std::string> allowed = action == "sign" ?
            std::vector<std::string>{"--archive", "--audit-report", "--private-key", "--trusted-public-key", "--output-dir"} :
            std::vector<std::string>{"--release-dir", "--trusted-public-key"};
        Options options;
        for (int i = 2; i < argc; i += 2) {
            require(i + 1 < argc, Failure::arguments);
            const std::string name = argv[i], value = argv[i + 1];
            bool known = false; for (const auto& key : allowed) if (name == key) known = true;
            require(known && !value.empty() && !value.starts_with("--") && options.emplace(name, value).second,
                    Failure::arguments);
        }
        require(options.size() == allowed.size(), Failure::arguments);
        const auto e = action == "sign" ? sign(options) : verify(options);
        std::cout << "{\"action\":" << bruce::json_string(action) << ",\"archive\":" << bruce::json_string(e.archive)
            << ",\"archive_sha256\":" << bruce::json_string(e.archive_sha256)
            << ",\"contract\":\"bruce-release-tool-report-v1\",\"key_id\":" << bruce::json_string(e.key_id)
            << ",\"ok\":true,\"version\":" << bruce::json_string(e.version) << "}\n";
        return 0;
    } catch (Failure reason) {
        return report_failure(reason);
    } catch (const bruce::Error& error) {
        // Only fixed library identities select a public category. Exception
        // text is never emitted, including unknown future library errors.
        const std::string_view code = error.what();
        if (code == "InvalidReleaseSignatureV1") return report_failure(Failure::contract);
        if (code == "ReleaseKeyUntrusted" || code == "ReleaseSignatureInvalid" || code == "ReleaseCryptoFailure")
            return report_failure(Failure::verification);
        return report_failure(Failure::internal);
    } catch (...) {
        return report_failure(Failure::internal);
    }
}
