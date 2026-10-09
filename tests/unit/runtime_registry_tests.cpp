#include "bruce/mod/runtime_registry.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <csignal>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

// Interrupt the actual publication path after the target first exists. The
// legacy link/unlink path stops before unlink; atomic rename stops before fsync.
// These interposed calls are confined to this test executable.
std::string interrupted_publication_target;
extern "C" int unlink(const char* path) {
    if (!interrupted_publication_target.empty() && std::filesystem::exists(interrupted_publication_target)) ::_exit(73);
    return ::unlinkat(AT_FDCWD, path, 0);
}
extern "C" int fsync(int fd) {
    if (!interrupted_publication_target.empty() && std::filesystem::exists(interrupted_publication_target)) ::_exit(73);
    return static_cast<int>(::syscall(SYS_fsync, fd));
}

namespace {
void require(bool value, const char* message) { if (!value) throw bruce::Error(message); }
template<class F> void rejected(F action) {
    try { action(); } catch (const bruce::Error&) { return; }
    throw bruce::Error("Unsafe registry operation accepted");
}
struct TemporaryRoot {
    std::string path;
    TemporaryRoot() { std::string p = "/tmp/bruce-runtime-registry-XXXXXX"; require(::mkdtemp(p.data()), "mkdtemp failed"); path = p; }
    ~TemporaryRoot() { std::filesystem::remove_all(path); }
};
// RFC 8032 published test vector, never a production signing secret.
const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
    0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
std::string trust_bytes() { return "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n"; }
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
std::string member(std::string_view name, std::string_view bytes) { return field(name) + field(bytes); }
std::string sign(std::string_view bytes) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    require(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1, "sign init failed");
    unsigned char signature[64]; std::size_t size = sizeof(signature);
    require(EVP_DigestSign(ctx.get(), signature, &size, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1, "sign failed");
    std::string output; constexpr char hex[] = "0123456789abcdef";
    for (auto b : signature) { output += hex[b >> 4]; output += hex[b & 15]; }
    return output;
}
bruce::RuntimeDataModPackageV1 package(std::string id = "user.power", unsigned address = 5,
    std::string verb = "hoch", std::string expected = "MATCHED", std::string version = "1.0.0") {
    const auto tree = "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[],\"claim_scope\":\"candidate_only\",\"id\":[1,1," + std::to_string(address) +
        "],\"key\":\"" + id + "\",\"route_pattern\":[\"<rational>\",\"" + verb + "\",\"<integer>\"],\"template_id\":\"" + id + ".v1\"}]}";
    const auto golden = "[{\"input\":\"3/4 " + verb + " 2\",\"status\":\"" + expected + "\",\"template_id\":\"" + (expected == "MATCHED" ? id + ".v1" : "") + "\"}]";
    const auto payload = member("tree.json", tree) + member("tests/golden-cases.json", golden);
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"" + id +
        "\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"" + version + "\"}";
    const auto unsigned_bytes = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    return bruce::unpack_runtime_data_mod_package_v1(unsigned_bytes + member("signature.ed25519", sign(unsigned_bytes)) + '\n');
}
void lifecycle() {
    TemporaryRoot temp;
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    const auto root = temp.path + "/mods";
    auto registry = bruce::RuntimeModRegistryV1::open(root, trust);
    const auto mod = package();
    registry.install(mod);
    require(registry.records().size() == 1 && registry.records()[0].state == bruce::RuntimeModStateV1::installed_inactive, "install must be inactive");
    require(bruce::LocalFiles::read(root + "/packages/" + mod.package_digest() + ".brucemod") == mod.bytes(), "package is not content-addressed");
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::unknown, "inactive mod routes");
    struct stat info{}; require(::stat(root.c_str(), &info) == 0 && (info.st_mode & 0777) == 0700, "root permissions unsafe");
    registry.activate("user.power", "1.0.0");
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::matched, "active mod missing");
    auto other_handle = bruce::RuntimeModRegistryV1::open_existing(root);
    other_handle.disable("user.power");
    require(registry.records()[0].state == bruce::RuntimeModStateV1::disabled && registry.generation() == 3, "stale handle failed to reload");
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::unknown, "disabled mod routes");
    require(bruce::RuntimeModRegistryV1::open(temp.path + "/other", trust).records().empty(), "roots share state");
    rejected([&] { registry.install(mod); });
    rejected([&] { registry.activate("missing", "1.0.0"); });
    rejected([&] { registry.disable("missing"); });
    rejected([&] { (void)bruce::RuntimeModRegistryV1::open(root, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n")); });
}
void conflicts() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust);
    const auto digest = registry.journal_digest();
    rejected([&] { registry.install(package("user.core-override", 1)); });
    rejected([&] { registry.install(package("user.bad-golden", 5, "hoch", "UNKNOWN")); });
    require(registry.generation() == 0 && registry.journal_digest() == digest, "failed install mutated state");
    registry.install(package()); registry.install(package("user.sibling", 5, "potenz"));
    registry.activate("user.power", "1.0.0");
    const auto before = registry.journal_digest();
    rejected([&] { registry.activate("user.sibling", "1.0.0"); });
    require(registry.generation() == 3 && registry.journal_digest() == before, "failed conflict changed generation");
    require(registry.active_tree().find({1,1,5})->owner_id == "user.power", "foreign owner overwritten");
}
void integrity_and_recovery() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust);
    const auto initial = bruce::LocalFiles::read(temp.path + "/registry.v1");
    const auto mod = package(); registry.install(mod); registry.activate("user.power", "1.0.0");
    const auto digest = registry.journal_digest();
    // A crash after durable journal append but before snapshot rename leaves a lagging snapshot.
    bruce::LocalFiles::replace_atomically(temp.path + "/registry.v1", initial);
    auto recovered = bruce::RuntimeModRegistryV1::open_existing(temp.path);
    require(recovered.generation() == 2 && recovered.journal_digest() == digest, "durable journal was not recovered");
    const auto journal = bruce::LocalFiles::read(temp.path + "/journal.v1");
    bruce::LocalFiles::replace_atomically(temp.path + "/journal.v1", journal + "27:partial");
    require(bruce::RuntimeModRegistryV1::open_existing(temp.path).generation() == 2, "incomplete append recovery failed");
    require(bruce::LocalFiles::read(temp.path + "/journal.v1") == journal, "incomplete append not truncated");
    auto mutation = mod.bytes(); mutation.at(mutation.find("hoch")) = 'X';
    bruce::LocalFiles::replace_atomically(temp.path + "/packages/" + mod.package_digest() + ".brucemod", mutation);
    rejected([&] { (void)recovered.active_tree(); });
    rejected([&] { recovered.activate("user.power", "1.0.0"); });
}
void upgrade_and_collision() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust);
    registry.install(package()); registry.activate("user.power", "1.0.0");
    registry.install(package("user.power", 5, "potenz", "MATCHED", "2.0.0"));
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::matched, "staged upgrade changed routing");
    registry.activate("user.power", "2.0.0");
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::unknown, "old version retained after upgrade");
    require(registry.active_tree().route(bruce::normalize_route_text_v1("3/4 potenz 2")).status == bruce::RouteStatusV1::matched, "new version not activated");
    const auto records = registry.records();
    require(records.size() == 2 && records[0].state == bruce::RuntimeModStateV1::disabled && records[1].state == bruce::RuntimeModStateV1::active, "upgrade lifecycle inconsistent");
    // A distinct address does not justify breaking an active mod's signed golden case.
    const auto before = registry.journal_digest();
    rejected([&] { registry.install(package("user.collision", 6, "potenz")); });
    require(registry.journal_digest() == before, "golden conflict modified state");
}
void store_tampering() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust);
    const auto mod = package(); registry.install(mod);
    const auto snapshot = bruce::LocalFiles::read(temp.path + "/registry.v1");
    bruce::LocalFiles::replace_atomically(temp.path + "/registry.v1", "arbitrary state");
    rejected([&] { (void)registry.snapshot(); });
    bruce::LocalFiles::replace_atomically(temp.path + "/registry.v1", snapshot);
    const auto journal = bruce::LocalFiles::read(temp.path + "/journal.v1");
    auto altered = journal; altered.at(altered.find("user.power")) = 'X';
    bruce::LocalFiles::replace_atomically(temp.path + "/journal.v1", altered);
    rejected([&] { (void)registry.snapshot(); });
    bruce::LocalFiles::replace_atomically(temp.path + "/journal.v1", journal);
    bruce::LocalFiles::replace_atomically(temp.path + "/truststore.pub", "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n");
    rejected([&] { registry.activate("user.power", "1.0.0"); });
    bruce::LocalFiles::replace_atomically(temp.path + "/truststore.pub", trust_bytes());
    const auto path = temp.path + "/packages/" + mod.package_digest() + ".brucemod";
    std::filesystem::rename(path, path + ".real");
    require(::symlink((path + ".real").c_str(), path.c_str()) == 0, "symlink failed");
    rejected([&] { registry.activate("user.power", "1.0.0"); });
    require(registry.generation() == 1, "failed activation changed generation");
    std::filesystem::remove(path);
    std::filesystem::rename(path + ".real", path);
    require(::link(path.c_str(), (path + ".foreign-link").c_str()) == 0, "hardlink failed");
    rejected([&] { registry.activate("user.power", "1.0.0"); });
    require(registry.generation() == 1, "multiply linked package activation changed generation");
}
void failed_append_recovery() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust); registry.install(package());
    const auto before = registry.journal_digest();
    const auto journal_size = std::filesystem::file_size(temp.path + "/journal.v1");
    const auto pid = ::fork(); require(pid >= 0, "fork failed");
    if (pid == 0) {
        std::signal(SIGXFSZ, SIG_IGN);
        const rlimit limit{journal_size + 64, journal_size + 64};
        if (::setrlimit(RLIMIT_FSIZE, &limit) != 0) ::_exit(2);
        try { registry.activate("user.power", "1.0.0"); ::_exit(3); } catch (const bruce::Error&) { ::_exit(0); }
    }
    int status; require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "partial journal write not observed");
    require(registry.generation() == 1 && registry.journal_digest() == before, "partial append became a commit");
    require(std::filesystem::file_size(temp.path + "/journal.v1") == journal_size, "partial append was not recovered");
    registry.activate("user.power", "1.0.0");
    require(registry.generation() == 2, "retry after recovery failed");
}
void interrupted_initial_publication() {
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    const auto mod = package();
    for (const std::string member_name : {"truststore.pub", "journal.v1", "package"}) {
        TemporaryRoot temp;
        const auto root = temp.path + "/mods";
        const bool is_package = member_name == "package";
        const auto target = root + (is_package ? "/packages/" + mod.package_digest() + ".brucemod" : "/" + member_name);
        if (is_package) (void)bruce::RuntimeModRegistryV1::open(root, trust);
        const auto child = ::fork(); require(child >= 0, "fork failed");
        if (child == 0) {
            interrupted_publication_target = target;
            try {
                auto registry = bruce::RuntimeModRegistryV1::open(root, trust);
                if (is_package) registry.install(mod);
            } catch (...) { ::_exit(2); }
            ::_exit(3);
        }
        int status;
        require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73,
            "publication interruption was not observed");
        // Retrying initialisation or installation must retain validated bytes
        // and a usable registry even when a process dies before directory sync.
        auto recovered = bruce::RuntimeModRegistryV1::open(root, trust);
        require(recovered.generation() == 0 && recovered.records().empty(), "publication created a premature state commit");
        recovered.install(mod);
        recovered.activate("user.power", "1.0.0");
        require(bruce::RuntimeModRegistryV1::open_existing(root).active_tree().route(
            bruce::normalize_route_text_v1("3/4 hoch 2")).status == bruce::RouteStatusV1::matched,
            "initial publication interruption made the registry unusable");
    }
}
void concurrency_and_permissions() {
    TemporaryRoot temp; const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    auto registry = bruce::RuntimeModRegistryV1::open(temp.path, trust);
    registry.install(package()); registry.install(package("user.square", 6, "quadratisch"));
    int start[2]; require(::pipe(start) == 0, "pipe failed"); pid_t children[2];
    for (int i = 0; i < 2; ++i) {
        children[i] = ::fork(); require(children[i] >= 0, "fork failed");
        if (children[i] == 0) {
            ::close(start[1]); char signal; if (::read(start[0], &signal, 1) != 1) ::_exit(2); ::close(start[0]);
            try { auto child = bruce::RuntimeModRegistryV1::open_existing(temp.path); child.activate(i == 0 ? "user.power" : "user.square", "1.0.0"); ::_exit(0); }
            catch (...) { ::_exit(1); }
        }
    }
    ::close(start[0]); require(::write(start[1], "go", 2) == 2, "race start failed"); ::close(start[1]);
    for (const auto pid : children) { int status; require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "concurrent activation lost"); }
    require(registry.generation() == 4 && registry.active_tree().find({1,1,5}) && registry.active_tree().find({1,1,6}), "concurrency lost state");
    require(::chmod(temp.path.c_str(), 0755) == 0, "chmod failed");
    rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(temp.path); });
    require(::chmod(temp.path.c_str(), 0700) == 0, "chmod failed");
    require(::chmod((temp.path + "/registry.v1").c_str(), 0644) == 0, "chmod failed");
    rejected([&] { (void)registry.records(); });
    require(::chmod((temp.path + "/registry.v1").c_str(), 0600) == 0, "chmod failed");
    const auto symlink_root = temp.path + "/linked";
    require(::symlink(temp.path.c_str(), symlink_root.c_str()) == 0, "symlink failed");
    rejected([&] { (void)bruce::RuntimeModRegistryV1::open_existing(symlink_root); });
}
}
int main() {
    try { interrupted_initial_publication(); lifecycle(); conflicts(); integrity_and_recovery(); upgrade_and_collision(); store_tampering(); failed_append_recovery(); concurrency_and_permissions(); std::cout << "runtime registry contracts passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
