#include "bruce/mod/logic_mod_payload_v1.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
void check(bool value, const char* why) { if (!value) throw bruce::Error(why); }
template<class F> void rejects(F f, std::string_view expected = {}) {
    try { f(); } catch (const bruce::Error& e) {
        if (!expected.empty() && expected != e.what()) throw bruce::Error(std::string("Wrong error: ") + e.what());
        return;
    }
    throw bruce::Error("Expected payload rejection");
}
void write(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc); file << bytes;
    file.close(); check(bool(file), "fixture write failed");
}
std::string field(std::string_view s) { return std::to_string(s.size()) + ':' + std::string(s); }
std::string member(std::string_view n, std::string_view s) { return field(n) + field(s); }
std::string sign(std::string_view bytes) {
    // Public RFC 8032 fixture key.
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    unsigned char out[64]; std::size_t n = sizeof(out);
    check(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 && EVP_DigestSign(ctx.get(), out, &n, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1, "sign failed");
    std::string result; for (auto b : out) { result += "0123456789abcdef"[b >> 4]; result += "0123456789abcdef"[b & 15]; } return result;
}
const std::string worker = "signed test worker";
std::string package() {
    const auto payload = member("tree.json", R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})") + member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    data += member("signature.ed25519", sign(data)) + '\n';
    const auto logic = "{\"capabilities\":[\"artifact.input.v1\",\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", worker);
    return raw + member("signature.ed25519", sign(raw)) + '\n';
}
struct Fixture {
    std::filesystem::path dir, root;
    bruce::LogicModRegistryV1 registry;
    Fixture() {
        char pattern[] = "/tmp/bruce-logic-payload-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp"); dir = pattern; root = dir / "store";
        write(dir / "worker.bin", worker); check(::chmod((dir / "worker.bin").c_str(), 0500) == 0, "chmod");
        const auto raw = package(); const auto id = bruce::sha256(raw);
        registry.discover(bruce::unpack_logic_mod_package_v1(raw), dir.string());
        registry.verify(id, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n"));
        registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    auto binding(std::string_view cap = "artifact.input.v1") { return registry.bind("user.logic", "1.0.0", "math.rational.power.v1", cap); }
    auto put(std::string_view name, std::string_view bytes, std::string status = "READY") {
        auto store = bruce::artifact_v1::ArtifactStoreV1::open(root); auto id = store.put_bytes(bytes);
        store.publish_manifest(name, {"run", "node", "attempt", status, "OK", {{id, "application/octet-stream", "opaque-origin"}}});
        return id;
    }
    auto resolve(std::string_view name, std::uint64_t index = 0) { return bruce::resolve_logic_mod_payload_v1(binding(), root, name, index); }
    auto object(const bruce::artifact_v1::ArtifactIdV1& id) { return root / "objects" / id.sha256.substr(0, 2) / (id.sha256 + ".blob"); }
};
void contracts() {
    Fixture f;
    rejects([&] { (void)bruce::resolve_logic_mod_payload_v1(f.binding("worker.execute"), f.root, "missing", 0); }, "LogicModPayloadCapabilityDenied");
    rejects([&] { (void)f.resolve("missing"); }); check(!std::filesystem::exists(f.root), "resolver created store");
    f.put("good", "abc"); auto good = f.resolve("good");
    check(good.manifest_id() == "good" && good.object_index() == 0 && good.size_bytes() == 3 && good.codec() == "artifact-bytes-v1", "identity missing");
    check(good.sha256() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "digest mismatch");
    check(good.authorization_identity() == f.binding().canonical_identity(), "authorization lost");
    check(good.canonical_identity() == "BRUCE_LOGIC_PAYLOAD_V1\n4:good1:064:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad1:317:artifact-bytes-v1\n", "canonical identity mismatch");
    char bytes[4]{}; check(::read(good.fd(), bytes, sizeof(bytes)) == 3 && std::string(bytes, 3) == "abc", "sealed copy unreadable");
    check((::fcntl(good.fd(), F_GETFL) & O_ACCMODE) == O_RDONLY, "payload descriptor writable");
    check(::write(good.fd(), "x", 1) == -1 && ::ftruncate(good.fd(), 0) == -1, "sealed bytes mutable");
    const int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
    check((::fcntl(good.fd(), F_GET_SEALS) & seals) == seals, "payload not fully sealed");
    check((::fcntl(good.fd(), F_GETFD) & FD_CLOEXEC) != 0, "payload leaks on exec");
    int owned = good.fd(); { auto moved = std::move(good); check(good.fd() == -1 && moved.fd() == owned, "move duplicated ownership"); }
    check(::fcntl(owned, F_GETFD) == -1 && errno == EBADF, "payload fd leaked");
    auto empty_id = f.put("empty", ""); (void)empty_id; auto empty = f.resolve("empty"); check(::read(empty.fd(), bytes, 1) == 0, "empty payload invalid");
    f.put("limit", std::string(1024 * 1024, 'x')); auto limit = f.resolve("limit");
    check(limit.size_bytes() == 1024 * 1024 && ::lseek(limit.fd(), 0, SEEK_END) == 1024 * 1024, "1MiB boundary rejected");
    f.put("oversize", std::string(1024 * 1024 + 1, 'x')); rejects([&] { (void)f.resolve("oversize"); }, "LogicModPayloadLimitExceeded");
    rejects([&] { (void)f.resolve("good", 1); }, "LogicModPayloadObjectIndexInvalid");
    rejects([&] { (void)f.resolve("../good"); }); rejects([&] { (void)f.resolve("missing"); });
    f.put("failed", "failed", "FAILED"); rejects([&] { (void)f.resolve("failed"); }, "LogicModPayloadManifestNotReady");
    auto corrupt = f.put("corrupt", "original"); write(f.object(corrupt), "mutated!"); rejects([&] { (void)f.resolve("corrupt"); });
    auto absent = f.put("absent", "absent"); std::filesystem::remove(f.object(absent)); rejects([&] { (void)f.resolve("absent"); });
    auto wrong = f.put("wrong-size", "size"); write(f.object(wrong), "s"); rejects([&] { (void)f.resolve("wrong-size"); });
    write(f.root / "manifests" / "good.manifest", "not a canonical manifest"); rejects([&] { (void)f.resolve("good"); });
    int status{}; check(::waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD, "resolver spawned child");
}
void fifo_contract(bool lock) {
    Fixture f;
    f.put("input", "abc");
    const auto target = lock ? f.root / "store.lock" : f.root / "manifests" / "input.manifest";
    check(std::filesystem::remove(target), "fixture replacement failed");
    check(::mkfifo(target.c_str(), 0600) == 0, "FIFO creation failed");
    const auto pid = ::fork();
    check(pid >= 0, "test fork failed");
    if (pid == 0) {
        // No writer exists: a blocking open would hang before the type check.
        // Isolate that regression so CTest receives a bounded failure.
        ::alarm(2);
        try {
            auto input = f.resolve("input");
            (void)input;
            ::_exit(2); // Returning any worker input is a contract violation.
        } catch (const bruce::Error& error) {
            ::_exit(std::string_view(error.what()) == "ArtifactStorePermissions" ? 0 : 3);
        } catch (...) { ::_exit(4); }
    }
    int status{};
    pid_t waited;
    do { waited = ::waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    check(waited == pid, "test child not reaped");
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          lock ? "FIFO lock blocked or returned input instead of controlled error"
               : "FIFO manifest blocked or returned input instead of controlled error");
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 1) contracts();
        else if (argc == 2 && std::string_view(argv[1]) == "--fifo-lock") fifo_contract(true);
        else if (argc == 2 && std::string_view(argv[1]) == "--fifo-manifest") fifo_contract(false);
        else throw bruce::Error("Unknown test selection");
        std::cout << "logic payload contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
