#include "bruce/artifact/artifact_store_v3.hpp"
#include "bruce/evidence/bundle.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../../third_party/cpp-httplib/httplib.h"

#include <array>
#include <charconv>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace bruce::artifact_v3 {
namespace {
using Clock = std::chrono::steady_clock;
[[noreturn]] void invalid() { throw Error("PROTOCOL_ERROR"); }
class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (value_ >= 0) ::close(value_);
        value_ = std::exchange(other.value_, -1); return *this;
    }
    int get() const { return value_; }
private:
    int value_;
};
bool alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
bool valid_id(std::string_view id) {
    return !id.empty() && id.size() <= 128 && alnum(id.front()) &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) { return alnum(c) || c == '-' || c == '_' || c == '.'; });
}
struct Origin { std::string host; int port{443}; std::string prefix; };
Origin parse_origin(std::string_view text) {
    if (!text.starts_with("https://") || text.size() > 2048) invalid();
    text.remove_prefix(8);
    const auto slash = text.find('/');
    auto authority = text.substr(0, slash);
    Origin result;
    if (slash != std::string_view::npos) {
        result.prefix = text.substr(slash);
        if (result.prefix.back() == '/') invalid();
        std::string_view path = result.prefix;
        path.remove_prefix(1);
        while (!path.empty()) {
            const auto end = path.find('/');
            const auto part = path.substr(0, end);
            if (part.empty() || part == "." || part == ".." ||
                !std::all_of(part.begin(), part.end(), [](unsigned char c) {
                    return alnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
                })) invalid();
            if (end == std::string_view::npos) break;
            path.remove_prefix(end + 1);
        }
    }
    const auto colon = authority.find(':');
    if (colon != std::string_view::npos) {
        const auto port = authority.substr(colon + 1);
        if (port.empty() || port.front() == '0') invalid();
        const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), result.port);
        if (error != std::errc{} || end != port.data() + port.size() || result.port < 1 || result.port > 65535) invalid();
        authority = authority.substr(0, colon);
    }
    if (authority.empty() || authority.size() > 253 || authority.back() == '.') invalid();
    auto host = authority;
    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-' ||
            !std::all_of(label.begin(), label.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
            })) invalid();
        if (dot == std::string_view::npos) break;
        host.remove_prefix(dot + 1);
    }
    // libc accepts abbreviated/octal/integer IPv4 aliases. Admit numeric hosts
    // only in canonical dotted-decimal form, so the allowlist has one meaning.
    in_addr address{};
    if (::inet_aton(std::string(authority).c_str(), &address) != 0) {
        char canonical[INET_ADDRSTRLEN]{};
        if (!::inet_ntop(AF_INET, &address, canonical, sizeof(canonical)) || authority != canonical) invalid();
    } else if (std::all_of(authority.begin(), authority.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; })) invalid();
    result.host = authority;
    return result;
}
void validate_config(const ProviderConfigV3& config) {
    std::string_view previous;
    for (const auto& p : config.providers) {
        if (!valid_id(p.id) || (!previous.empty() && p.id <= previous)) invalid();
        (void)parse_origin(p.origin); previous = p.id;
    }
}
struct stat checked_file(int fd) {
    struct stat s{};
    if (::fstat(fd, &s) != 0 || !S_ISREG(s.st_mode) || s.st_uid != ::geteuid() ||
        (s.st_mode & 0077) != 0 || s.st_nlink != 1 || s.st_size < 0 ||
        static_cast<std::uint64_t>(s.st_size) > max_file_bytes) invalid();
    return s;
}
std::string read_config(const std::filesystem::path& path) {
    if (path.empty() || path.string().find('\0') != std::string::npos) invalid();
    std::vector<std::string> names;
    for (const auto& part : path) {
        if (part == "..") invalid();
        if (part != "/" && part != "." && !part.empty()) names.push_back(part.string());
    }
    if (names.empty()) invalid();
    Fd dir(::open(path.is_absolute() ? "/" : ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW));
    if (dir.get() < 0) invalid();
    for (std::size_t i = 0; i + 1 < names.size(); ++i) {
        dir = Fd(::openat(dir.get(), names[i].c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK));
        if (dir.get() < 0) invalid();
    }
    struct stat node{};
    if (::fstatat(dir.get(), names.back().c_str(), &node, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(node.st_mode)) invalid();
    Fd file(::openat(dir.get(), names.back().c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    const auto before = checked_file(file.get());
    std::string bytes;
    std::array<char, 65536> buffer{};
    for (;;) {
        const auto count = ::read(file.get(), buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) invalid();
        if (count == 0) break;
        if (static_cast<std::size_t>(count) > max_file_bytes - bytes.size()) invalid();
        bytes.append(buffer.data(), count);
    }
    const auto after = checked_file(file.get());
    if (before.st_dev != after.st_dev || before.st_ino != after.st_ino || before.st_mode != after.st_mode ||
        before.st_size != after.st_size || bytes.size() != static_cast<std::uint64_t>(after.st_size) ||
        before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec) invalid();
    return bytes;
}
class Parser {
public:
    explicit Parser(std::string_view bytes) : bytes_(bytes) {}
    void expect(std::string_view text) {
        if (!bytes_.starts_with(text)) invalid();
        bytes_.remove_prefix(text.size());
    }
    bool next(char c) const { return !bytes_.empty() && bytes_.front() == c; }
    bool done() const { return bytes_.empty(); }
    std::string string() {
        expect("\"");
        const auto end = bytes_.find('"');
        if (end == std::string_view::npos) invalid();
        const auto value = bytes_.substr(0, end);
        // Every accepted ID and origin is plain ASCII and needs no JSON escape.
        if (!std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c <= 126 && c != '\\'; })) invalid();
        bytes_.remove_prefix(end + 1); return std::string(value);
    }
private:
    std::string_view bytes_;
};

enum class FrameType : std::uint32_t { connected = 1, headers = 2, body = 3, done = 4 };
struct Frame {
    FrameType type{};
    std::uint32_t count{};
    std::uint64_t length{};
};
constexpr std::uint64_t unknown_length = UINT64_MAX;
void send_bytes(int fd, const void* data, std::size_t size) {
    auto p = static_cast<const char*>(data);
    while (size != 0) {
        const auto sent = ::send(fd, p, size, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) invalid();
        p += sent; size -= static_cast<std::size_t>(sent);
    }
}
void send_frame(int fd, FrameType type, std::uint32_t count = 0, std::uint64_t length = 0) {
    const Frame frame{type, count, length}; send_bytes(fd, &frame, sizeof(frame));
}

[[noreturn]] void child_get(int ipc, const HttpsRequestV3& request, bool put, std::string_view upload) noexcept {
    try {
        // Inherited descriptors (including any caller store descriptors) must
        // not be accessible to the network child. Linux is the V1 platform.
        if ((ipc > 0 && ::syscall(SYS_close_range, 0U, static_cast<unsigned>(ipc - 1), 0U) != 0) ||
            ::syscall(SYS_close_range, static_cast<unsigned>(ipc + 1), UINT_MAX, 0U) != 0) ::_exit(1);
        bool success = false;
        {
            httplib::SSLClient client(request.host, request.port);
            client.enable_server_certificate_verification(true);
            client.enable_server_hostname_verification(true);
            client.set_follow_location(false);
            client.set_keep_alive(false);
            client.set_decompress(false);
            client.set_connection_timeout(5);
            client.set_read_timeout(30);
            client.set_write_timeout(30);
            client.set_max_timeout(std::chrono::seconds(30));
            // Socket options run before connect. Abort any implicit retry
            // before a second connection reaches the provider (also fail
            // closed if the first DNS address is unavailable).
            bool opened_socket = false;
            client.set_socket_options([&](socket_t socket) {
                if (opened_socket) invalid();
                opened_socket = true;
                httplib::default_socket_options(socket);
            });
            // httplib assembles request headers only after certificate AND
            // hostname verification, before sending HTTP bytes. This is the
            // point where the parent's connect deadline becomes the request
            // deadline. Never install a custom certificate verifier.
            bool wrote_request = false;
            client.set_header_writer([&](httplib::Stream& stream, httplib::Headers& headers) -> ssize_t {
                if (wrote_request) invalid();
                wrote_request = true;
                send_frame(ipc, FrameType::connected);
                return httplib::detail::write_headers(stream, headers);
            });
            std::uint64_t total = 0;
            const auto on_headers = [&](const httplib::Response& response) {
                    if ((put ? (response.status != 200 && response.status != 201 && response.status != 204) : response.status != 200) || response.get_header_value_count("Content-Length") > 1 ||
                        response.has_header("Content-Encoding")) return false;
                    std::uint64_t length = unknown_length;
                    if (response.has_header("Content-Length")) {
                        const auto text = response.get_header_value("Content-Length");
                        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), length);
                        if (error != std::errc{} || end != text.data() + text.size() || length > max_file_bytes) return false;
                    }
                    send_frame(ipc, FrameType::headers, response.status, length);
                    return true;
                };
            const auto on_body = [&](const char* data, std::size_t size) {
                    if (size > max_file_bytes - total) return false;
                    total += size;
                    while (size != 0) {
                        const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(65536, size));
                        send_frame(ipc, FrameType::body, count);
                        send_bytes(ipc, data, count); data += count; size -= count;
                    }
                    return true;
                };
            if (put) {
                httplib::Request outgoing;
                outgoing.method = "PUT"; outgoing.path = request.path;
                outgoing.body.assign(upload);
                outgoing.set_header("Content-Type", "application/octet-stream");
                outgoing.response_handler = on_headers;
                outgoing.content_receiver = [&](const char* data, std::size_t size, std::uint64_t, std::uint64_t) {
                    return on_body(data, size);
                };
                const auto result = client.send(outgoing);
                // httplib skips response_handler for 204 because no body is
                // permitted. Still validate and deliver its metadata exactly
                // once; never accept framing that would imply ignored bytes.
                if (result && result->status == 204) {
                    if (result->has_header("Transfer-Encoding") ||
                        (result->has_header("Content-Length") &&
                         result->get_header_value("Content-Length") != "0")) invalid();
                    if (!on_headers(*result)) invalid();
                }
                success = result && (result->status == 200 || result->status == 201 || result->status == 204);
            } else {
                const auto result = client.Get(request.path, on_headers, on_body);
                success = result && result->status == 200;
            }
        }
        if (!success) ::_exit(1);
        send_frame(ipc, FrameType::done);
        ::_exit(0);
    } catch (...) { ::_exit(1); }
}

class Child {
public:
    explicit Child(pid_t pid) : pid_(pid) {}
    ~Child() {
        if (pid_ > 0) {
            ::kill(pid_, SIGKILL);
            while (::waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {}
        }
    }
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    bool finished() {
        int status{};
        const auto result = ::waitpid(pid_, &status, WNOHANG);
        if (result < 0 && errno == EINTR) return false;
        if (result < 0) { pid_ = -1; invalid(); }
        if (result == 0) return false;
        pid_ = -1;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) invalid();
        return true;
    }
private:
    pid_t pid_;
};
void receive_bytes(int fd, void* destination, std::size_t size, Clock::time_point deadline) {
    auto data = static_cast<char*>(destination);
    while (size != 0) {
        const auto now = Clock::now();
        if (now >= deadline) invalid();
        const auto timeout = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
        pollfd p{fd, POLLIN, 0};
        const auto ready = ::poll(&p, 1, static_cast<int>(std::min<std::int64_t>(timeout, 30000)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0 || Clock::now() >= deadline) invalid();
        const auto got = ::recv(fd, data, size, MSG_DONTWAIT);
        if (got < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (got <= 0) invalid();
        data += got; size -= static_cast<std::size_t>(got);
    }
}
} // namespace

ProviderConfigV3 read_provider_config_v3(const std::filesystem::path& path) {
    try {
        const auto bytes = read_config(path);
        Parser parser(bytes);
        parser.expect("{\"contract\":\"bruce-artifact-store-providers-v3\",\"providers\":[");
        ProviderConfigV3 result;
        if (!parser.next(']')) for (;;) {
            parser.expect("{\"id\":"); auto id = parser.string();
            parser.expect(",\"origin\":"); auto origin = parser.string(); parser.expect("}");
            result.providers.push_back({std::move(id), std::move(origin)});
            if (!parser.next(',')) break;
            parser.expect(",");
        }
        parser.expect("]}\n");
        if (!parser.done()) invalid();
        validate_config(result);
        return result;
    } catch (...) { invalid(); }
}

static void https_exchange_v3(const HttpsRequestV3& request, const HttpsHeadersV3& headers, const HttpsBodyV3& body, bool put, std::string_view upload) {
    try {
        const auto start = Clock::now();
        const auto deadline = std::min(request.deadline, start + std::chrono::seconds(30));
        const auto connect_deadline = std::min({request.connect_deadline, deadline, start + std::chrono::seconds(5)});
        if (start >= connect_deadline || upload.size() > max_file_bytes) invalid();
        int pair[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair) != 0) invalid();
        Fd parent(pair[0]), child(pair[1]);
        const auto pid = ::fork();
        if (pid < 0) invalid();
        if (pid == 0) child_get(child.get(), request, put, upload);
        Child process(pid);
        child = Fd();
        bool connected = false, have_headers = false;
        std::uint64_t total = 0;
        std::array<char, 65536> buffer{};
        for (;;) {
            Frame frame{};
            receive_bytes(parent.get(), &frame, sizeof(frame), connected ? deadline : connect_deadline);
            if (frame.type == FrameType::connected) {
                if (connected || frame.count != 0 || frame.length != 0) invalid();
                connected = true;
            } else if (frame.type == FrameType::headers) {
                if (!connected || have_headers || (put ? (frame.count != 200 && frame.count != 201 && frame.count != 204) : frame.count != 200) ||
                    (frame.length != unknown_length && frame.length > max_file_bytes)) invalid();
                have_headers = true;
                headers(static_cast<int>(frame.count), frame.length == unknown_length ? std::nullopt : std::optional(frame.length));
            } else if (frame.type == FrameType::body) {
                if (!have_headers || frame.count == 0 || frame.count > buffer.size() ||
                    frame.count > max_file_bytes - total || frame.length != 0) invalid();
                receive_bytes(parent.get(), buffer.data(), frame.count, deadline);
                total += frame.count; body(std::string_view(buffer.data(), frame.count));
            } else if (frame.type == FrameType::done) {
                if (!have_headers || frame.count != 0 || frame.length != 0) invalid();
                // A completion frame is not success until the child exits cleanly.
                while (!process.finished()) {
                    if (Clock::now() >= deadline) invalid();
                    ::poll(nullptr, 0, 1);
                }
                if (Clock::now() >= deadline) invalid();
                return;
            } else invalid();
        }
    } catch (...) { invalid(); }
}

void https_get_v3(const HttpsRequestV3& request, const HttpsHeadersV3& headers, const HttpsBodyV3& body) {
    https_exchange_v3(request, headers, body, false, {});
}
void https_put_v3(const HttpsRequestV3& request, std::string_view bytes,
    const HttpsHeadersV3& headers, const HttpsBodyV3& body) {
    https_exchange_v3(request, headers, body, true, bytes);
}
HttpsRequestV3 https_request_v3(std::string_view remote, std::string_view path, Clock::time_point deadline) {
    const auto origin = parse_origin(remote);
    if (path.empty() || path.front() != '/' ||
        !std::all_of(path.begin(), path.end(), [](unsigned char c) {
            return alnum(c) || c == '/' || c == '-' || c == '_' || c == '.';
        }) || path.find("/../") != std::string_view::npos || path.find("/./") != std::string_view::npos ||
        path.ends_with("/..") || path.ends_with("/.") || path.find("//") != std::string_view::npos) invalid();
    return {origin.host, origin.port, origin.prefix + std::string(path),
            std::min(deadline, Clock::now() + std::chrono::seconds(5)), deadline};
}

artifact_v1::ArtifactIdV1 fetch_to_store_v3(
    const ProviderConfigV3& config, std::string_view provider_id,
    const artifact_v1::ArtifactIdV1& expected, const std::filesystem::path& store,
    const HttpsTransportV3& transport) {
    try {
        validate_config(config);
        if (!valid_id(provider_id) || expected.sha256.size() != 64 || expected.size_bytes > max_file_bytes ||
            !std::all_of(expected.sha256.begin(), expected.sha256.end(), [](unsigned char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            })) invalid();
        const auto provider = std::find_if(config.providers.begin(), config.providers.end(),
            [&](const auto& p) { return p.id == provider_id; });
        if (provider == config.providers.end()) invalid();
        const auto origin = parse_origin(provider->origin);
        const auto start = Clock::now();
        const HttpsRequestV3 request{origin.host, origin.port,
            origin.prefix + "/objects/" + expected.sha256.substr(0, 2) + "/" + expected.sha256 + ".blob",
            start + std::chrono::seconds(5), start + std::chrono::seconds(30)};
        std::string bytes;
        bool have_headers = false;
        transport(request, [&](int status, std::optional<std::uint64_t> length) {
            if (Clock::now() >= request.deadline || have_headers || status != 200 ||
                (length && *length != expected.size_bytes)) invalid();
            have_headers = true;
        }, [&](std::string_view chunk) {
            if (Clock::now() >= request.deadline || !have_headers || chunk.size() > expected.size_bytes - bytes.size()) invalid();
            bytes.append(chunk);
        });
        if (Clock::now() >= request.deadline || !have_headers || bytes.size() != expected.size_bytes ||
            sha256(bytes) != expected.sha256) invalid();
        auto local = artifact_v1::ArtifactStoreV1::open(store);
        try { return local.put_bytes(bytes); }
        catch (const Error& error) {
            if (std::string_view(error.what()) != "ArtifactPublishSyncFailed") throw;
            local.verify(expected);
            return expected;
        }
    } catch (...) { invalid(); }
}
} // namespace bruce::artifact_v3
