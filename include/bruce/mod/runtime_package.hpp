#pragma once
#include "bruce/tree/registry.hpp"
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
inline constexpr std::size_t max_runtime_mod_package_bytes_v1 = 1024 * 1024;
struct RuntimeModManifestV1 {
    std::string mod_id;
    std::string version;
    std::string owner_scope;
    std::string signing_key_id;
    std::string payload_sha256;
    std::vector<std::string> capabilities;
    std::uint32_t schema_version = 1;
    std::uint32_t host_abi_version = 1;
    std::uint32_t tree_schema_version = 1;
    std::uint32_t token_normalization_version = 1;
};
class RuntimeModTrustStoreV1 {
public:
    const std::map<std::string, std::string>& public_keys() const noexcept { return public_keys_; }
private:
    std::map<std::string, std::string> public_keys_;
    friend RuntimeModTrustStoreV1 read_runtime_mod_truststore_v1(std::string_view);
};
struct RuntimeModGoldenCaseV1 {
    std::string input;
    RouteStatusV1 status = RouteStatusV1::unknown;
    std::string template_id;
};
// Unpacking checks the envelope, digest and manifest; tree data stays opaque.
// A tree or golden case can only be obtained after verification against a caller's
// explicit trust store. Package identity and signed bytes cannot be edited.
class RuntimeDataModPackageV1 {
public:
    const RuntimeModManifestV1& manifest() const noexcept { return manifest_; }
    const std::string& package_digest() const noexcept { return package_digest_; }
    const std::string& bytes() const noexcept { return bytes_; }
    TreeFragmentV1 tree_fragment(const RuntimeModTrustStoreV1&) const;
    std::vector<RuntimeModGoldenCaseV1> golden_cases(const RuntimeModTrustStoreV1&) const;
private:
    RuntimeDataModPackageV1() = default;
    RuntimeModManifestV1 manifest_;
    std::string bytes_, package_digest_, unsigned_bytes_, tree_bytes_, golden_bytes_, signature_hex_;
    friend RuntimeDataModPackageV1 unpack_runtime_data_mod_package_v1(std::string_view);
    friend void verify_runtime_data_mod_package_v1(const RuntimeDataModPackageV1&, const RuntimeModTrustStoreV1&);
};
RuntimeDataModPackageV1 unpack_runtime_data_mod_package_v1(std::string_view bytes);
// Argument is trust-store content, not a filesystem path.
RuntimeModTrustStoreV1 read_runtime_mod_truststore_v1(std::string_view bytes);
// Verifies Ed25519 first, then strict tree and golden-case schemas. Mount validity
// and golden-case execution require the installation's candidate tree (Task 4).
void verify_runtime_data_mod_package_v1(const RuntimeDataModPackageV1&, const RuntimeModTrustStoreV1&);
}
