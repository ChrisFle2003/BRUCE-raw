#include "bruce/mod/package_inspection_v2.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
void require(bool condition, const char* message) { if (!condition) throw bruce::Error(message); }
template<class F> void rejected(F action, std::string_view expected = {}) {
    try { action(); } catch (const bruce::Error& error) {
        if (!expected.empty()) require(error.what() == expected, "Wrong package inspection error");
        return;
    }
    throw bruce::Error("Invalid package accepted");
}
std::string read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(input)), {}};
}
std::string member(std::string_view name, std::string_view bytes) {
    return std::to_string(name.size()) + ':' + std::string(name) +
        std::to_string(bytes.size()) + ':' + std::string(bytes);
}
std::string sign(std::string_view bytes) {
    const unsigned char seed[] = {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
        0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, sizeof(seed)), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    require(key && context && EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) == 1,
            "Test signing init failed");
    unsigned char signature[64]; std::size_t size = sizeof(signature);
    require(EVP_DigestSign(context.get(), signature, &size,
        reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) == 1 && size == 64,
        "Test signing failed");
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (const auto byte : signature) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
std::string logic_package(const std::string& data) {
    const std::string worker = "signed worker bytes\n";
    const std::string logic = "{\"capabilities\":[\"worker.execute\"],\"template_ids\":[\"math.rational.power.v1\"],\"worker_path\":\"worker.bin\",\"worker_sha256\":\"" +
        bruce::sha256(worker) + "\"}";
    const auto unsigned_bytes = "BRUCE_LOGIC_MOD_PLUGIN_V1\n" + member("data.brucemod", data) +
        member("logic.json", logic) + member("worker.bin", worker);
    return unsigned_bytes + member("signature.ed25519", sign(unsigned_bytes)) + '\n';
}
}

int main() {
    try {
        const auto data_bytes = read(std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/math-rational-power-v1.brucemod");
        const auto trust = bruce::read_runtime_mod_truststore_v1(
            read(std::string(BRUCE_RUNTIME_FIXTURE_DIR) + "/test-truststore-v1.pub"));
        const auto data = bruce::detect_mod_package_v2(data_bytes);
        require(data.kind() == bruce::ModPackageKindV2::data, "Data package misclassified");
        const auto data_info = bruce::inspect_mod_package_v2(data);
        require(data_info.mod_id == "user.math-rational-power" &&
                data_info.capabilities == std::vector<std::string>{"dataset.read", "tree.extend"},
                "Data inspection mismatch");
        bruce::verify_mod_package_v2(data, trust);

        const auto logic_bytes = logic_package(data_bytes);
        const auto logic = bruce::detect_mod_package_v2(logic_bytes);
        require(logic.kind() == bruce::ModPackageKindV2::logic, "Logic package misclassified");
        const auto logic_info = bruce::inspect_mod_package_v2(logic);
        require(logic_info.mod_id == "user.math-rational-power" &&
                logic_info.capabilities == std::vector<std::string>{"worker.execute"},
                "Logic inspection mismatch");
        bruce::verify_mod_package_v2(logic, trust);
        require(bruce::render_mod_package_inspection_v2(logic_info).starts_with(
            "{\"contract\":\"mod-package-inspection-v2\",\"kind\":\"logic\""),
            "Inspection contract mismatch");
        const auto expected_verification = "{\"contract\":\"mod-package-verification-v2\",\"kind\":\"logic\",\"mod_id\":\"user.math-rational-power\",\"version\":\"1.0.0\",\"package_digest\":\"" +
            logic_info.package_digest + "\",\"capabilities\":[\"worker.execute\"],\"verified\":true}";
        require(bruce::render_mod_package_verification_v2(logic_info) == expected_verification,
                "Verification contract mismatch");

        rejected([&] { (void)bruce::detect_mod_package_v2(""); }, "InvalidModPackageEnvelope");
        rejected([&] { (void)bruce::detect_mod_package_v2("BRUCE_RUNTIME_DATA_MOD_V1"); },
                 "InvalidModPackageEnvelope");
        rejected([&] { (void)bruce::detect_mod_package_v2(std::string(1024 * 1024 + 1, 'x')); },
                 "RuntimeModPackageTooLarge");
        rejected([&] { (void)bruce::detect_mod_package_v2(data_bytes + logic_bytes); });
        std::cout << "package inspection v2 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
