#include "bruce/mod/logic_mod_plugin_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
template<class F> void rejected(F action, std::string_view error = {}) {
    try { action(); } catch (const bruce::Error& e) {
        if (!error.empty() && e.what() != error) throw bruce::Error(std::string("Wrong rejection: ") + e.what());
        return;
    }
    throw bruce::Error("Unsafe logic package accepted");
}
// Public RFC 8032 vector, never a production signing secret.
const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
    0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
std::string sign(std::string_view bytes, const unsigned char* signing_seed = seed) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, signing_seed, sizeof(seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    require(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1, "Signing init failed");
    unsigned char signature[64]; std::size_t n = sizeof(signature);
    require(EVP_DigestSign(ctx.get(), signature, &n, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1 && n == 64, "Signing failed");
    std::string result; constexpr char hex[] = "0123456789abcdef";
    for (auto b : signature) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
std::string member(std::string_view name, std::string_view bytes) {
    return std::to_string(name.size()) + ':' + std::string(name) + std::to_string(bytes.size()) + ':' + std::string(bytes);
}
const std::string worker = "signed worker bytes\n";
const std::string tree = R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})";
const std::string golden = R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])";
const std::string trust_bytes = "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n";
std::string data_package(std::string version, std::string cases) {
    auto payload = member("tree.json", tree) + member("tests/golden-cases.json", cases);
    auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"" + version + "\"}";
    auto bytes = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    return bytes + member("signature.ed25519", sign(bytes)) + '\n';
}
std::string package(std::string version = "1.0.0", std::string cases = golden,
                    std::string path = "worker.bin", std::string capability = "worker.execute",
                    std::string templates = "[\"math.rational.power.v1\"]", std::string capabilities = {}) {
    if (capabilities.empty()) capabilities = "[\"" + capability + "\"]";
    auto logic = "{\"capabilities\":" + capabilities + ",\"template_ids\":" + templates + ",\"worker_path\":\"" + path + "\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    auto bytes = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data_package(version, cases)) + member("logic.json", logic) + member(path, worker);
    return bytes + member("signature.ed25519", sign(bytes)) + '\n';
}
struct Directory {
    std::string path;
    Directory() { char pattern[] = "/tmp/bruce-logic-v1-XXXXXX"; const auto p = ::mkdtemp(pattern); require(p, "mkdtemp failed"); path = p; write(worker); }
    ~Directory() { std::filesystem::remove_all(path); }
    void write(std::string_view bytes) { std::ofstream out(path + "/worker.bin", std::ios::binary); out << bytes; out.close(); require(::chmod((path + "/worker.bin").c_str(), 0700) == 0, "chmod failed"); }
};
// Reject accidental broadening (missing execute, unknown rights), reordering,
// and duplicates while preserving both old capability sets.
void capability_contracts() {
    const std::vector<std::string> names = {"artifact.input.v1", "artifact.output.v1", "worker.execute", "network"};
    for (unsigned mask = 0; mask < 16; ++mask) {
        std::string json = "[";
        std::vector<std::string> wanted;
        for (unsigned bit = 0; bit < names.size(); ++bit) if (mask & (1u << bit)) {
            if (!wanted.empty()) json += ',';
            json += '"' + names[bit] + '"'; wanted.push_back(names[bit]);
        }
        json += ']';
        const auto raw = package("1.0.0", golden, "worker.bin", "worker.execute", "[\"math.rational.power.v1\"]", json);
        if (mask < 4 || mask >= 8) {
            rejected([&] { (void)bruce::unpack_logic_mod_package_v1(raw); }, "UnsupportedLogicModCapability");
            continue;
        }
        const auto parsed = bruce::unpack_logic_mod_package_v1(raw);
        require(parsed.capabilities() == wanted, "Capability set changed");
        Directory dir;
        bruce::LogicModRegistryV1 registry;
        registry.discover(parsed, dir.path);
        registry.verify(parsed.package_digest(), bruce::read_runtime_mod_truststore_v1(trust_bytes));
        registry.test_golden(parsed.package_digest(), bruce::core_tree_registry_v1());
        registry.install(parsed.package_digest()); registry.activate(parsed.package_digest());
        for (const auto& cap : wanted) {
            const auto bound = registry.bind("user.logic", "1.0.0", "math.rational.power.v1", cap);
            require(bound.capability() == cap && bound.registry_digest() == registry.digest(), "Fresh capability identity lost");
        }
        if (!(mask & 2)) rejected([&] { (void)registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "artifact.output.v1"); });
    }
    for (const auto* json : {"[\"worker.execute\",\"artifact.input.v1\"]", "[\"worker.execute\",\"artifact.output.v1\"]",
             "[\"artifact.output.v1\",\"artifact.input.v1\",\"worker.execute\"]", "[\"worker.execute\",\"worker.execute\"]",
             "[\"artifact.output.v1\",\"artifact.output.v1\",\"worker.execute\"]", "[\"artifact.input.v1\",\"artifact.input.v1\",\"worker.execute\"]"})
        rejected([&] { (void)bruce::unpack_logic_mod_package_v1(package("1.0.0", golden, "worker.bin", "worker.execute", "[\"math.rational.power.v1\"]", json)); }, "UnsupportedLogicModCapability");
}
void package_contracts() {
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes);
    const auto raw = package();
    const auto parsed = bruce::unpack_logic_mod_package_v1(raw);
    bruce::verify_logic_mod_package_v1(parsed, trust);
    require(parsed.manifest().mod_id == "user.logic" && parsed.package_digest() == bruce::sha256(raw), "Package identity lost");
    require(parsed.worker_sha256() == bruce::sha256(worker), "Worker identity lost");
    require(parsed.worker_bytes(trust) == worker, "Verified extraction changed worker bytes");
    rejected([&] { (void)parsed.worker_bytes(bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n")); }, "RuntimeModSigningKeyUntrusted");
    rejected([&] { bruce::verify_logic_mod_package_v1(parsed, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n")); }, "RuntimeModSigningKeyUntrusted");
    auto mutated = raw; mutated[mutated.find("signed worker bytes")] = 'X';
    rejected([&] { bruce::verify_logic_mod_package_v1(bruce::unpack_logic_mod_package_v1(mutated), trust); });
    mutated = raw; mutated[mutated.find("user.logic")] = 'X';
    rejected([&] { bruce::verify_logic_mod_package_v1(bruce::unpack_logic_mod_package_v1(mutated), trust); }, "RuntimeModSignatureInvalid");
    mutated = raw; mutated[mutated.size() - 2] = mutated[mutated.size() - 2] == '0' ? '1' : '0';
    rejected([&] { bruce::verify_logic_mod_package_v1(bruce::unpack_logic_mod_package_v1(mutated), trust); }, "RuntimeModSignatureInvalid");
    // A valid outer signature cannot authorize an invalid nested signature.
    mutated = raw;
    const auto inner_signature = mutated.find("signature.ed25519") + std::string("signature.ed25519").size() + 4;
    mutated[inner_signature] = mutated[inner_signature] == '0' ? '1' : '0';
    const auto outer_signature = mutated.rfind("17:signature.ed25519");
    mutated = mutated.substr(0, outer_signature) + member("signature.ed25519", sign(mutated.substr(0, outer_signature))) + '\n';
    rejected([&] { bruce::verify_logic_mod_package_v1(bruce::unpack_logic_mod_package_v1(mutated), trust); }, "RuntimeModSignatureInvalid");
    // Both keys are trusted, but only the manifest's signer may sign the outer.
    const unsigned char second_seed[] = {0x4c,0xcd,0x08,0x9b,0x28,0xff,0x96,0xda,0x9d,0xb6,0xc3,0x46,0xec,0x11,0x4e,0x0f,0x5b,0x8a,0x31,0x9f,0x35,0xab,0xa6,0x24,0xda,0x8c,0xf6,0xed,0x4f,0xb8,0xa6,0xfb};
    const auto two_keys = bruce::read_runtime_mod_truststore_v1(trust_bytes + "test-second:3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c\n");
    const auto unsigned_outer = raw.substr(0, raw.rfind("17:signature.ed25519"));
    mutated = unsigned_outer + member("signature.ed25519", sign(unsigned_outer, second_seed)) + '\n';
    rejected([&] { bruce::verify_logic_mod_package_v1(bruce::unpack_logic_mod_package_v1(mutated), two_keys); }, "RuntimeModSignatureInvalid");
    for (const auto& path : {"../worker.bin", "/worker.bin", "sub/worker.bin", ".", ".."})
        rejected([&] { (void)bruce::unpack_logic_mod_package_v1(package("1", golden, path)); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(package("1", golden, "worker.bin", "network")); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(package("1", golden, "worker.bin", "worker.execute", "[]")); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(package("1", golden, "worker.bin", "worker.execute", "[\"x\",\"x\"]")); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(raw + member("extra", worker)); });
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(raw); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(data_package("1", golden)); });
    rejected([&] { (void)bruce::unpack_logic_mod_package_v1(std::string(1024 * 1024 + 1, 'x')); });
}
void state_contracts() {
    Directory dir;
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes);
    const auto parsed = bruce::unpack_logic_mod_package_v1(package());
    bruce::LogicModRegistryV1 registry;
    registry.discover(parsed, dir.path);
    const auto id = parsed.package_digest();
    require(registry.state(id) == bruce::LogicModStateV1::discovered, "Discovery activates");
    rejected([&] { registry.install(id); });
    rejected([&] { registry.activate(id); });
    registry.verify(id, trust);
    require(registry.state(id) == bruce::LogicModStateV1::verified, "Verification state missing");
    rejected([&] { registry.install(id); });
    registry.test_golden(id, bruce::core_tree_registry_v1());
    require(registry.state(id) == bruce::LogicModStateV1::golden_tested, "Golden gate missing");
    registry.install(id);
    require(registry.state(id) == bruce::LogicModStateV1::installed_inactive, "Install activates");
    rejected([&] { (void)registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "worker.execute"); });
    const auto inactive_digest = registry.digest();
    registry.activate(id);
    const auto binding = registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "worker.execute");
    require(binding.package_digest() == id && binding.worker_bytes() == worker && binding.registry_digest() == registry.digest(), "Binding loses signed identities");
    require(binding.registry_digest() != inactive_digest, "Registry digest ignores activation");
    require(binding.template_id() == "math.rational.power.v1" && binding.mod_id() == "user.logic" && binding.version() == "1.0.0", "Binding identity mismatch");
    rejected([&] { (void)registry.bind("user.logic", "1.0.0", "other", "worker.execute"); });
    rejected([&] { (void)registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "network"); });
    rejected([&] { (void)registry.bind("user.logic", "wrong", "math.rational.power.v1", "worker.execute"); });
    const auto update = bruce::unpack_logic_mod_package_v1(package("2.0.0"));
    const auto next = update.package_digest(); registry.discover(update, dir.path); registry.verify(next, trust);
    registry.test_golden(next, bruce::core_tree_registry_v1()); registry.install(next);
    rejected([&] { registry.activate(next); });
    require(registry.state(id) == bruce::LogicModStateV1::active && registry.state(next) == bruce::LogicModStateV1::installed_inactive, "Update replaced active version");
    registry.disable(id);
    rejected([&] { (void)registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "worker.execute"); });
    registry.activate(next); registry.revoke(next);
    rejected([&] { registry.activate(next); });
    rejected([&] { (void)registry.bind("user.logic", "2.0.0", "math.rational.power.v1", "worker.execute"); });
    registry.activate(id);
    dir.write("tampered");
    rejected([&] { (void)registry.bind("user.logic", "1.0.0", "math.rational.power.v1", "worker.execute"); }, "LogicModWorkerDigestMismatch");
    require(binding.worker_bytes() == worker, "Issued binding mutable through file changes");
    registry.disable(id);
    rejected([&] { registry.activate(id); }, "LogicModWorkerDigestMismatch");
    require(registry.state(id) == bruce::LogicModStateV1::disabled, "Failed activation changed state");
}
void golden_and_files() {
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes);
    Directory dir;
    const auto invalid = bruce::unpack_logic_mod_package_v1(package("1", R"([{"input":"unmatched","status":"MATCHED","template_id":"math.rational.power.v1"}])"));
    bruce::LogicModRegistryV1 registry; registry.discover(invalid, dir.path); registry.verify(invalid.package_digest(), trust);
    rejected([&] { registry.test_golden(invalid.package_digest(), bruce::core_tree_registry_v1()); }, "LogicModGoldenCaseFailed");
    require(registry.state(invalid.package_digest()) == bruce::LogicModStateV1::verified, "Failed golden changed state");
    const auto parsed = bruce::unpack_logic_mod_package_v1(package());
    auto fails_file = [&] {
        bruce::LogicModRegistryV1 files; files.discover(parsed, dir.path);
        rejected([&] { files.verify(parsed.package_digest(), trust); });
        require(files.state(parsed.package_digest()) == bruce::LogicModStateV1::discovered, "Failed verification changed state");
    };
    ::chmod((dir.path + "/worker.bin").c_str(), 0755); fails_file();
    ::chmod((dir.path + "/worker.bin").c_str(), 0600); fails_file();
    std::filesystem::remove(dir.path + "/worker.bin");
    ::mkfifo((dir.path + "/worker.bin").c_str(), 0700); fails_file();
    std::filesystem::remove(dir.path + "/worker.bin");
    std::filesystem::create_directory(dir.path + "/worker.bin"); fails_file();
    std::filesystem::remove(dir.path + "/worker.bin");
    Directory other; std::filesystem::create_symlink(other.path + "/worker.bin", dir.path + "/worker.bin"); fails_file();
    std::filesystem::remove(dir.path + "/worker.bin");
    std::filesystem::create_hard_link(other.path + "/worker.bin", dir.path + "/worker.bin"); fails_file();
    std::filesystem::remove(dir.path + "/worker.bin"); dir.write(worker);
    ::chmod(dir.path.c_str(), 0755); fails_file(); ::chmod(dir.path.c_str(), 0700);
    ::chmod((dir.path + "/worker.bin").c_str(), 0500);
    bruce::LogicModRegistryV1 readonly;
    readonly.discover(parsed, dir.path); readonly.verify(parsed.package_digest(), trust);
    require(readonly.state(parsed.package_digest()) == bruce::LogicModStateV1::verified, "Owner read/execute worker rejected");
    const auto missing = bruce::unpack_logic_mod_package_v1(package("2", golden, "worker.bin", "worker.execute", "[\"unbound.template\"]"));
    registry.discover(missing, dir.path);
    rejected([&] { registry.verify(missing.package_digest(), trust); });
}
}
int main() {
    try { capability_contracts(); package_contracts(); state_contracts(); golden_and_files(); std::cout << "logic mod plugin v1 contracts passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
