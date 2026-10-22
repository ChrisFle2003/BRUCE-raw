#include "bruce/artifact/artifact_store_v3.hpp"
#include "bruce/evidence/bundle.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::artifact_v1;
using namespace bruce::artifact_v3;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {
bool fail_directory_sync = false;
unsigned failed_syncs = 0;
void require(bool value, const char* message) { if (!value) throw Error(message); }
template<class F> void rejects(F&& f, const char* message) {
    try { f(); } catch (const Error& e) {
        require(std::string_view(e.what()) == "PROTOCOL_ERROR", "internal diagnostic leaked");
        return;
    }
    throw Error(message);
}
struct Fixture {
    fs::path base;
    Fixture() {
        char pattern[] = "/tmp/bruce-v3-tests-XXXXXX";
        const auto p = ::mkdtemp(pattern);
        require(p, "mkdtemp failed"); base = p;
    }
    ~Fixture() { fs::remove_all(base); }
    fs::path store() const { return base / "store"; }
    ProviderConfigV3 config(std::string_view bytes) {
        const auto p = base / "providers.json";
        std::ofstream(p, std::ios::binary) << bytes;
        require(::chmod(p.c_str(), 0600) == 0, "chmod failed");
        return read_provider_config_v3(p);
    }
};
std::string config_bytes(std::string_view origin) {
    return "{\"contract\":\"bruce-artifact-store-providers-v3\",\"providers\":[{\"id\":\"https-a\",\"origin\":\"" +
           std::string(origin) + "\"}]}\n";
}
ProviderConfigV3 config() { return {{{"https-a", "https://example.org:8443/bruce"}}}; }
ArtifactIdV1 id(std::string_view bytes) { return {sha256(bytes), bytes.size()}; }
std::map<std::string, std::string> snapshot(const fs::path& root) {
    std::map<std::string, std::string> result;
    if (!fs::exists(root)) return result;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        const auto key = e.path().lexically_relative(root).string();
        if (e.is_regular_file()) {
            std::ifstream stream(e.path(), std::ios::binary);
            result[key] = std::string(std::istreambuf_iterator<char>(stream), {});
        } else result[key] = "<directory>";
    }
    return result;
}
HttpsTransportV3 response(std::string bytes, int status = 200) {
    return [bytes = std::move(bytes), status](const HttpsRequestV3&, const HttpsHeadersV3& headers,
                                             const HttpsBodyV3& body) {
        headers(status, bytes.size());
        for (std::size_t i = 0; i < bytes.size(); i += 65536)
            body(std::string_view(bytes).substr(i, 65536));
    };
}

// Reject URL aliases and schema variations before any transport or publication.
void canonical_configuration() {
    Fixture f;
    for (const auto origin : {"https://example.org", "https://localhost:443/bruce",
                              "https://127.0.0.1:8443/a-b/c_d.x~y"}) {
        const auto c = f.config(config_bytes(origin));
        require(c.providers.size() == 1 && c.providers[0].origin == origin,
                "canonical origin changed");
    }
    for (const auto origin : {"http://example.org", "HTTPS://example.org", "https://EXAMPLE.org",
            "https://", "https://example.org/", "https://example.org//x", "https://example.org/a/../b",
            "https://example.org/./x", "https://example.org/x/", "https://user@example.org",
            "https://example.org?x", "https://example.org#x", "https://example.org/%2e",
            "https://example.org/a\\b", "https://example.org:0", "https://example.org:0443",
            "https://example.org:65536", "https://example.org:", "https://[::1]",
            "https://.example.org", "https://example..org", "https://example.org.",
            "https://-host.org", "https://host-.org", "https://a_b.org", "https://host/a b",
            "https://127.1", "https://0177.0.0.1", "https://2130706433"}) {
        rejects([&] { (void)f.config(config_bytes(origin)); }, "unsafe origin accepted");
    }
    const auto good = config_bytes("https://example.org");
    for (const auto& bad : {good.substr(0, good.size()-1), good + "\n", " " + good,
        std::string("{\"providers\":[],\"contract\":\"bruce-artifact-store-providers-v3\"}\n"),
        std::string("{\"contract\":\"bruce-artifact-store-providers-v3\",\"providers\":[{\"id\":\"b\",\"origin\":\"https://a\"},{\"id\":\"a\",\"origin\":\"https://a\"}]}\n"),
        std::string("{\"contract\":\"bruce-artifact-store-providers-v3\",\"providers\":[{\"id\":\"a\",\"origin\":\"https://a\"},{\"id\":\"a\",\"origin\":\"https://a\"}]}\n")})
        rejects([&] { (void)f.config(bad); }, "noncanonical config accepted");
    require(f.config("{\"contract\":\"bruce-artifact-store-providers-v3\",\"providers\":[]}\n").providers.empty(),
            "empty config rejected");
}
void unsafe_configuration_files() {
    Fixture f;
    (void)f.config(config_bytes("https://example.org"));
    const auto p = f.base / "providers.json";
    ::chmod(p.c_str(), 0644);
    rejects([&] { (void)read_provider_config_v3(p); }, "public config accepted");
    ::chmod(p.c_str(), 0600);
    fs::create_hard_link(p, f.base / "link");
    rejects([&] { (void)read_provider_config_v3(p); }, "hardlink accepted");
    fs::remove(f.base / "link");
    fs::rename(p, f.base / "original");
    fs::create_symlink(f.base / "original", p);
    rejects([&] { (void)read_provider_config_v3(p); }, "symlink accepted");
    fs::remove(p);
    require(::mkfifo(p.c_str(), 0600) == 0, "fifo failed");
    rejects([&] { (void)read_provider_config_v3(p); }, "fifo accepted");
    fs::remove(p);
    fs::create_directory(p);
    rejects([&] { (void)read_provider_config_v3(p); }, "directory accepted");
    fs::create_directory_symlink(f.base, f.base / "alias");
    rejects([&] { (void)read_provider_config_v3(f.base / "alias" / "original"); }, "ancestor alias accepted");
    rejects([&] { (void)read_provider_config_v3(f.base / ".." / f.base.filename() / "original"); }, "traversal accepted");
    fs::resize_file(f.base / "original", max_file_bytes + 1);
    rejects([&] { (void)read_provider_config_v3(f.base / "original"); }, "large config accepted");
}
// A wrong route, missing budget, repeated GET, or unverified publication breaks this test.
void success_route_and_idempotence() {
    Fixture f;
    for (const auto& bytes : {std::string{}, std::string("small\0payload", 13), std::string(max_file_bytes, 'm')}) {
        const auto expected = id(bytes);
        unsigned calls = 0;
        HttpsTransportV3 transport = [&](const HttpsRequestV3& request, const HttpsHeadersV3& headers,
                                         const HttpsBodyV3& body) {
            ++calls;
            require(request.host == "example.org" && request.port == 8443 &&
                    request.path == "/bruce/objects/" + expected.sha256.substr(0, 2) + "/" + expected.sha256 + ".blob",
                    "wrong fixed route");
            const auto now = Clock::now();
            require(request.connect_deadline > now + std::chrono::seconds(4) &&
                    request.connect_deadline <= now + std::chrono::seconds(5) &&
                    request.deadline > now + std::chrono::seconds(29) &&
                    request.deadline <= now + std::chrono::seconds(30), "missing fixed deadlines");
            response(bytes)(request, headers, body);
        };
        require(fetch_to_store_v3(config(), "https-a", expected, f.store(), transport) == expected,
                "fetch identity changed");
        ArtifactStoreV1::open_existing(f.store()).verify(expected);
        const auto before = snapshot(f.store());
        require(fetch_to_store_v3(config(), "https-a", expected, f.store(), transport) == expected,
                "repeat identity changed");
        require(calls == 2 && snapshot(f.store()) == before, "fetch retried or changed existing store");
    }
}
// All protocol/transport/integrity failures must leave existing bytes untouched.
void failure_preserves_store() {
    Fixture f;
    auto local = ArtifactStoreV1::open(f.store());
    const auto retained = local.put_bytes("retained");
    const auto before = snapshot(f.store());
    const auto expected = id("payload");
    const auto fail = [&](const HttpsTransportV3& transport, const ArtifactIdV1& object) {
        rejects([&] { (void)fetch_to_store_v3(config(), "https-a", object, f.store(), transport); }, "bad fetch accepted");
        require(snapshot(f.store()) == before, "failed fetch changed store");
        rejects([&] { (void)fetch_to_store_v3(config(), "https-a", object, f.base / "absent", transport); }, "bad fetch accepted");
        require(!fs::exists(f.base / "absent"), "failed fetch created store");
    };
    for (int status : {199, 201, 204, 301, 302, 307, 308, 404, 500}) fail(response("payload", status), expected);
    fail(response("corrupt"), expected);
    fail(response("payload"), {expected.sha256, 6});
    fail(response("payload"), {expected.sha256, 8});
    for (const auto diagnostic : {"TLS certificate failure", "TLS hostname failure", "timeout", "truncated stream"})
        fail([&](const HttpsRequestV3&, const HttpsHeadersV3&, const HttpsBodyV3&) { throw std::runtime_error(diagnostic); }, expected);
    fail([](const HttpsRequestV3&, const HttpsHeadersV3& h, const HttpsBodyV3& b) {
        h(200, 7); b("payload"); throw std::runtime_error("truncated final HTTP framing");
    }, expected);
    fail([](const HttpsRequestV3&, const HttpsHeadersV3& h, const HttpsBodyV3& b) {
        h(200, std::nullopt); b("payload"); b("extra");
    }, expected);
    fail([](const HttpsRequestV3&, const HttpsHeadersV3&, const HttpsBodyV3& b) { b("payload"); }, expected);
    fail([](const HttpsRequestV3&, const HttpsHeadersV3& h, const HttpsBodyV3&) { h(200, max_file_bytes + 1); }, expected);
    fail([](const HttpsRequestV3&, const HttpsHeadersV3& h, const HttpsBodyV3& b) {
        h(200, std::nullopt); h(200, std::nullopt); b("payload");
    }, expected);
    local.verify(retained);
    unsigned calls = 0;
    HttpsTransportV3 must_not_run = [&](const HttpsRequestV3&, const HttpsHeadersV3&, const HttpsBodyV3&) { ++calls; };
    for (const auto& object : {ArtifactIdV1{"../bad", 1}, ArtifactIdV1{std::string(64,'A'), 1},
                               ArtifactIdV1{expected.sha256, max_file_bytes + 1}})
        fail(must_not_run, object);
    rejects([&] { (void)fetch_to_store_v3(config(), "missing", expected, f.store(), must_not_run); }, "unknown provider accepted");
    auto bad = config(); bad.providers[0].origin = "http://example.org";
    rejects([&] { (void)fetch_to_store_v3(bad, "https-a", expected, f.store(), must_not_run); }, "programmatic unsafe origin accepted");
    require(calls == 0, "invalid request invoked network");
}
void publication_sync_recovery() {
    Fixture f;
    const auto expected = id("sync-payload");
    fail_directory_sync = true;
    const auto actual = fetch_to_store_v3(config(), "https-a", expected, f.store(), response("sync-payload"));
    fail_directory_sync = false;
    require(failed_syncs == 1 && actual == expected, "post-rename sync did not recover");
    ArtifactStoreV1::open_existing(f.store()).verify(expected);
}
// Real TCP peer never answers TLS: the supervised helper must be killed/reaped,
// without waiting for httplib's per-operation timeout or leaving a child behind.
void production_adapter_bounds_handshake() {
    int listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    require(listener >= 0, "socket failed");
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
            ::listen(listener, 1) == 0, "listen failed");
    socklen_t length = sizeof(address);
    require(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "getsockname failed");
    for (bool connect_limit : {true, false}) {
        const auto start = Clock::now();
        HttpsRequestV3 request{"127.0.0.1", ntohs(address.sin_port), "/objects/aa/test.blob",
            start + (connect_limit ? std::chrono::milliseconds(100) : std::chrono::seconds(5)),
            start + (connect_limit ? std::chrono::seconds(30) : std::chrono::milliseconds(100))};
        rejects([&] { https_get_v3(request, [](int, auto) {}, [](auto) {}); }, "stalled TLS accepted");
        const auto elapsed = Clock::now() - start;
        require(elapsed >= std::chrono::milliseconds(75), "deadline test failed before exercising stalled TLS");
        require(elapsed < std::chrono::seconds(2), "absolute deadline not enforced");
        int status{};
        require(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "network helper not reaped");
    }
    const auto start = Clock::now();
    HttpsRequestV3 upload{"127.0.0.1", ntohs(address.sin_port), "/manifests/example.mirror",
        start + std::chrono::milliseconds(100), start + std::chrono::seconds(30)};
    rejects([&] { https_put_v3(upload, "payload", [](int, auto) {}, [](auto) {}); }, "stalled PUT TLS accepted");
    require(Clock::now() - start < std::chrono::seconds(2), "PUT absolute deadline not enforced");
    int status{};
    require(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "PUT helper not reaped");
    ::close(listener);
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    struct stat status{};
    if (fail_directory_sync && ::fstat(fd, &status) == 0 && S_ISDIR(status.st_mode)) {
        ++failed_syncs; errno = EIO; return -1;
    }
    return __real_fsync(fd);
}
int main() {
    ::alarm(90);
    try {
        canonical_configuration(); unsafe_configuration_files(); success_route_and_idempotence();
        failure_preserves_store(); publication_sync_recovery(); production_adapter_bounds_handshake();
        std::cout << "artifact store V3 contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
