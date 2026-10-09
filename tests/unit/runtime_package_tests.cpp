#include "bruce/mod/runtime_package.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/tree/normalizer.hpp"
#include <openssl/evp.h>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
const std::string public_key = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
// Public RFC 8032 test vector seed, NOT a production secret or trusted signing key.
const unsigned char test_seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
    0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
const std::string tree = "{\"mount\":[1,1],\"nodes\":[{\"aliases\":[\"hoch\"],\"claim_scope\":\"candidate_only\",\"id\":[1,1,5],\"key\":\"power\",\"route_pattern\":[\"<rational>\",\"hoch\",\"<integer>\"],\"template_id\":\"math.rational.power.v1\"}]}";
const std::string golden = "[{\"input\":\"3/4 hoch 2\",\"status\":\"MATCHED\",\"template_id\":\"math.rational.power.v1\"}]";
std::string field(std::string_view v) { return std::to_string(v.size()) + ":" + std::string(v); }
std::string member(std::string_view name, std::string_view data) { return field(name) + field(data); }
std::string trust_bytes() { return "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\ntest-rfc8032:" + public_key + "\n"; }
std::string sign(std::string_view message) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, test_seed, sizeof(test_seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    require(key && ctx && EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1, "Test key init failed");
    unsigned char signature[64]; std::size_t length = sizeof(signature);
    require(EVP_DigestSign(ctx.get(), signature, &length, reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1 && length == 64, "Test signing failed");
    constexpr char hex[] = "0123456789abcdef";
    std::string output;
    for (auto byte : signature) { output += hex[byte >> 4]; output += hex[byte & 15]; }
    return output;
}
std::string package(std::string content = tree, std::string capability = "tree.extend", std::string cases = golden, std::string manifest_prefix = "") {
    const auto payload = member("tree.json", content) + member("tests/golden-cases.json", cases);
    const auto manifest = "{" + manifest_prefix + "\"capabilities\":[\"dataset.read\",\"" + capability + "\"],\"host_abi_version\":1,\"mod_id\":\"user.math-rational-power\",\"owner_scope\":\"user\",\"payload_sha256\":\"" + bruce::sha256(payload) + "\",\"schema_version\":1,\"signing_key_id\":\"test-rfc8032\",\"token_normalization_version\":1,\"tree_schema_version\":1,\"version\":\"1.0.0\"}";
    const auto unsigned_bytes = "BRUCE_RUNTIME_DATA_MOD_V1\n" + member("manifest.json", manifest) + payload;
    return unsigned_bytes + member("signature.ed25519", sign(unsigned_bytes)) + '\n';
}
template<class F> void rejected(F action, std::string_view error = {}) {
    try { action(); } catch (const bruce::Error& e) {
        if (!error.empty()) require(e.what() == error, "Wrong rejection stage");
        return;
    }
    throw bruce::Error("Unsafe package accepted");
}
void contracts() {
    const auto trust = bruce::read_runtime_mod_truststore_v1(trust_bytes());
    const auto raw = package();
    const auto parsed = bruce::unpack_runtime_data_mod_package_v1(raw);
    bruce::verify_runtime_data_mod_package_v1(parsed, trust);
    require(parsed.manifest().mod_id == "user.math-rational-power" && parsed.manifest().version == "1.0.0", "Wrong manifest identity");
    require(parsed.package_digest() == bruce::sha256(raw) && parsed.bytes() == raw, "Package identity is not bound to complete bytes");
    const auto fragment = parsed.tree_fragment(trust);
    require(fragment.mount == bruce::TreeAddressV1{1,1} && fragment.nodes.size() == 1, "Wrong fragment");
    const auto registry = bruce::core_tree_registry_v1().mount(fragment, parsed.manifest().mod_id);
    const auto route = registry.route(bruce::normalize_route_text_v1("3/4 hoch 2"));
    require(route.status == bruce::RouteStatusV1::matched && route.candidates[0].id == bruce::TreeAddressV1{1,1,5}, "Verified package route failed");

    auto mutation = raw; mutation.at(mutation.find("hoch")) = 'H';
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(mutation); }, "RuntimeModPayloadDigestMismatch");
    const auto no_trust = bruce::read_runtime_mod_truststore_v1("BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n");
    rejected([&] { bruce::verify_runtime_data_mod_package_v1(parsed, no_trust); }, "RuntimeModSigningKeyUntrusted");
    rejected([&] { (void)parsed.tree_fragment(no_trust); }, "RuntimeModSigningKeyUntrusted");
    auto bad_signature = raw; bad_signature.at(bad_signature.size() - 2) = 'z';
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(bad_signature); }, "InvalidRuntimeModSignatureEncoding");
    auto wrong_signature = raw; auto& last_hex = wrong_signature.at(wrong_signature.size() - 2); last_hex = last_hex == '0' ? '1' : '0';
    rejected([&] { bruce::verify_runtime_data_mod_package_v1(bruce::unpack_runtime_data_mod_package_v1(wrong_signature), trust); }, "RuntimeModSignatureInvalid");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(package(tree, "worker.execute")); }, "UnsupportedRuntimeModCapability");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(std::string(1024 * 1024 + 1, 'x')); }, "RuntimeModPackageTooLarge");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(std::string(1024 * 1024, 'x')); }, "InvalidRuntimeModContainer");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(raw + member("worker.so", "executable")); });
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1("BRUCE_RUNTIME_DATA_MOD_V1\n99999999999999999999999999999:"); });
    // Invalid tree bytes must remain opaque until signature/trust verification.
    const auto malformed_tree = bruce::unpack_runtime_data_mod_package_v1(package("not-json"));
    rejected([&] { (void)malformed_tree.tree_fragment(no_trust); }, "RuntimeModSigningKeyUntrusted");
    rejected([&] { (void)malformed_tree.tree_fragment(trust); });
    auto unverified_malformed_tree = package("not-json");
    auto& malformed_signature = unverified_malformed_tree.at(unverified_malformed_tree.size() - 2);
    malformed_signature = malformed_signature == '0' ? '1' : '0';
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(unverified_malformed_tree).tree_fragment(trust); }, "RuntimeModSignatureInvalid");
    for (const auto& invalid_tree : {
        "{\"mount\":[1,1],\"mount\":[1,1],\"nodes\":[]}",
        "{ \"mount\":[1,1],\"nodes\":[]}",
        "{\"mount\":[1,1],\"nodes\":[],\"worker\":\"exec\"}",
        "{\"mount\":[1,1],\"nodes\":[],\"z\":true}",
        "{\"mount\":[1,1],\"nodes\":[],\"z\":\"\\u00ff\"}",
        "{\"mount\":[4294967296],\"nodes\":[]}"}) {
        rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(package(invalid_tree)).tree_fragment(trust); });
    }
    auto embedded_worker = tree;
    embedded_worker.insert(embedded_worker.find("\"template_id\""), "\"shell\":\"execute\",");
    rejected([&] { bruce::verify_runtime_data_mod_package_v1(bruce::unpack_runtime_data_mod_package_v1(package(embedded_worker)), trust); });
    auto bad_utf8 = tree;
    bad_utf8.at(bad_utf8.find("hoch")) = static_cast<char>(0xff);
    rejected([&] { bruce::verify_runtime_data_mod_package_v1(bruce::unpack_runtime_data_mod_package_v1(package(bad_utf8)), trust); }, "NoncanonicalRuntimeModJson");
    auto escaped_non_ascii = tree;
    escaped_non_ascii.replace(escaped_non_ascii.find("hoch"), 4, "\\u00ff");
    rejected([&] { bruce::verify_runtime_data_mod_package_v1(bruce::unpack_runtime_data_mod_package_v1(package(escaped_non_ascii)), trust); });
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(package(tree, "tree.extend", golden, "\"capabilities\":[],")); }, "DuplicateRuntimeModJsonKey");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(package(tree, "tree.extend", golden, "\"binary\":\"worker\",")); }, "InvalidRuntimeModSchema");
    auto future_abi = raw;
    future_abi.at(future_abi.find("\"host_abi_version\":1") + 19) = '2';
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(future_abi); }, "UnsupportedRuntimeModVersion");
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(raw.substr(0, raw.size() - 1)); });
    // A signed canonical package exactly at the inclusive 1 MiB boundary works.
    auto padded_tree = tree;
    std::string aliases = "[";
    for (unsigned i = 0; i < 255; ++i) { if (i != 0) aliases += ','; aliases += '"' + std::string(4096, 'a') + '"'; }
    aliases += ",\"PADDING\"]";
    padded_tree.replace(padded_tree.find("[\"hoch\"]"), 8, aliases);
    auto bounded_package = package(padded_tree);
    const auto missing_bytes = 1024 * 1024 - bounded_package.size();
    // Member-length decimal widths remain six digits after filling this tail.
    padded_tree.replace(padded_tree.find("PADDING"), 7, std::string(7 + missing_bytes, 'p'));
    bounded_package = package(padded_tree);
    require(bounded_package.size() == 1024 * 1024, "Boundary vector has wrong byte count");
    bruce::verify_runtime_data_mod_package_v1(bruce::unpack_runtime_data_mod_package_v1(bounded_package), trust);
    rejected([&] { (void)bruce::read_runtime_mod_truststore_v1(trust_bytes() + "test-rfc8032:" + public_key + "\n"); });
    rejected([&] { (void)bruce::unpack_runtime_data_mod_package_v1(package(tree, "tree.extend", "[{\"input\":\"a\",\"status\":\"MAYBE\",\"template_id\":\"x\"}]")).tree_fragment(trust); });
    std::ifstream fixture(std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/math-rational-power-v1.brucemod", std::ios::binary);
    const std::string fixture_bytes((std::istreambuf_iterator<char>(fixture)), {});
    require(fixture_bytes == raw, "Checked-in fixture differs from signed contract vector");
    std::ifstream trust_fixture(std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/test-truststore-v1.pub", std::ios::binary);
    const std::string fixture_trust((std::istreambuf_iterator<char>(trust_fixture)), {});
    require(fixture_trust == trust_bytes(), "Checked-in trust fixture differs from public vector");
}
}
int main(int argc, char**) {
    try {
        if (argc == 2) { std::cout << package(); return 0; }
        contracts(); std::cout << "runtime package contracts passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
