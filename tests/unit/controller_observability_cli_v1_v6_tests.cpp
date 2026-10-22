#include "../../apps/controller_observability_v1_cli.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"
#include <openssl/evp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }
std::string field(std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); }
std::string member(std::string_view key, std::string_view value) { return field(key) + field(value); }
std::string read(const std::filesystem::path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
void write(const std::filesystem::path& path, std::string_view bytes) { std::ofstream out(path, std::ios::binary); out << bytes; out.close(); check(static_cast<bool>(out), "fixture write"); check(::chmod(path.c_str(), 0600) == 0, "fixture mode"); }
std::string sign(std::string_view bytes) {
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    auto* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed)); auto* ctx = EVP_MD_CTX_new(); unsigned char out[64]; std::size_t size = sizeof(out);
    check(key && ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 && EVP_DigestSign(ctx, out, &size, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1, "sign");
    EVP_MD_CTX_free(ctx); EVP_PKEY_free(key); std::string result; for (unsigned char byte : out) { result += "0123456789abcdef"[byte >> 4]; result += "0123456789abcdef"[byte & 15]; } return result;
}
std::string package(std::string_view worker) {
    const auto payload = member("tree.json", R"({"mount":[1,1],"nodes":[{"aliases":["hoch"],"claim_scope":"candidate_only","id":[1,1,5],"key":"power","route_pattern":["<rational>","hoch","<integer>"],"template_id":"math.rational.power.v1"}]})") + member("tests/golden-cases.json", R"([{"input":"3/4 hoch 2","status":"MATCHED","template_id":"math.rational.power.v1"}])");
    const auto manifest = "{\"capabilities\":[\"dataset.read\",\"tree.extend\"],\"host_abi_version\":1,\"mod_id\":\"user.logic\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    auto data = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload; data += member("signature.ed25519", sign(data)) + '\n';
    const auto logic = "{\"capabilities\":[\"artifact.output.v1\",\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" + bruce::sha256(worker) + "\"}";
    auto raw = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) + member("logic.json", logic) + member("worker.bin", worker); return raw + member("signature.ed25519", sign(raw)) + '\n';
}
struct Fixture {
    std::filesystem::path dir, store; bruce::LogicModRegistryV1 registry;
    Fixture() {
        char pattern[] = "/tmp/bruce-observe-v6-XXXXXX"; check(::mkdtemp(pattern), "mkdtemp"); dir = pattern; store = dir / "store";
        const auto root = dir / "pkg"; std::filesystem::create_directory(root); check(::chmod(root.c_str(), 0700) == 0, "root mode"); const auto worker = read(BRUCE_DAG_WORKER); write(root / "worker.bin", worker); check(::chmod((root / "worker.bin").c_str(), 0500) == 0, "worker mode");
        const auto raw = package(worker); const auto id = bruce::sha256(raw); registry.discover(bruce::unpack_logic_mod_package_v1(raw), root.string()); registry.verify(id, bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\n")); registry.test_golden(id, bruce::core_tree_registry_v1()); registry.install(id); registry.activate(id);
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    bruce::LogicModDagNodeV2 node() { return {"a", registry, "user.logic", "1.0.0", "math.rational.power.v1", "3/4 hoch 2", {}, std::nullopt, {store, "a-out"}}; }
};
void v6_real_record_succeeds_through_public_cli() {
    Fixture fixture; bruce::LogicModArtifactDagControlV6 control; control.report_root = fixture.dir / "report"; const auto run = bruce::run_logic_mod_artifact_dag_v6({fixture.node()}, fixture.store, std::string(64, 'a'), control); check(run.accepted, "real V6 fixture run");
    const auto record = fixture.dir / "record"; write(record, bruce::logic_mod_artifact_dag_record_v6(run));
    std::string a0 = "bruce", a1 = "controller-observability-v1", a2 = "v6", a3 = record.string(); char* argv[] = {a0.data(), a1.data(), a2.data(), a3.data()}; std::ostringstream out, err; auto* oldout = std::cout.rdbuf(out.rdbuf()); auto* olderr = std::cerr.rdbuf(err.rdbuf()); const auto status = bruce::controller_observability_v1_cli(4, argv); std::cout.rdbuf(oldout); std::cerr.rdbuf(olderr);
    check(status == 0 && err.str().empty(), "public V6 CLI failed"); check(out.str() == "{\"schema\":\"controller-observability-crosswalk-v1\",\"source\":1,\"id\":\"redacted-logic-mod-artifact-dag-v6\",\"phase\":2,\"terminal\":1,\"cancellation\":0,\"success\":0,\"publication\":2,\"durable_evidence\":2,\"diagnostic\":2}\n", "public V6 CLI output");
}
}
int main() { try { v6_real_record_succeeds_through_public_cli(); std::cout << "controller observability v6 fixture passed\n"; } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
