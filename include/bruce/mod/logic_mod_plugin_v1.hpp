#pragma once
#include "bruce/mod/runtime_package.hpp"
#include <mutex>
#include <optional>

namespace bruce {
// Separate envelope; the embedded Runtime Data Mod bytes and schemas are unchanged.
// Total envelope size (including the sole worker artifact) is at most 1 MiB.
class LogicModPackageV1 {
public:
    const RuntimeModManifestV1& manifest() const noexcept { return data_.manifest(); }
    const std::string& package_digest() const noexcept { return package_digest_; }
    const std::string& bytes() const noexcept { return bytes_; }
    const std::string& worker_path() const noexcept { return worker_path_; }
    const std::string& worker_sha256() const noexcept { return worker_sha256_; }
    const std::vector<std::string>& template_ids() const noexcept { return template_ids_; }
    const std::vector<std::string>& capabilities() const noexcept { return capabilities_; }
    TreeFragmentV1 tree_fragment(const RuntimeModTrustStoreV1&) const;
    std::vector<RuntimeModGoldenCaseV1> golden_cases(const RuntimeModTrustStoreV1&) const;
    // Installation may extract exactly these bytes only after trust verification.
    std::string worker_bytes(const RuntimeModTrustStoreV1&) const;
private:
    explicit LogicModPackageV1(RuntimeDataModPackageV1 data) : data_(std::move(data)) {}
    RuntimeDataModPackageV1 data_;
    std::string bytes_, package_digest_, unsigned_bytes_, signature_hex_;
    std::string worker_path_, worker_sha256_, worker_bytes_;
    std::vector<std::string> template_ids_, capabilities_;
    friend LogicModPackageV1 unpack_logic_mod_package_v1(std::string_view);
    friend void verify_logic_mod_package_v1(const LogicModPackageV1&, const RuntimeModTrustStoreV1&);
    friend class LogicModRegistryV1;
    friend class DurableLogicModRegistryV1;
};
LogicModPackageV1 unpack_logic_mod_package_v1(std::string_view);
void verify_logic_mod_package_v1(const LogicModPackageV1&, const RuntimeModTrustStoreV1&);

enum class LogicModStateV1 { discovered, verified, golden_tested, installed_inactive, active, disabled, revoked };

// An immutable request-time snapshot. A runner must execute these pinned bytes,
// never reopen a pathname, and request a fresh binding for each run. Its validity
// is the registry state at issuance, not a perpetual authorization after revoke.
class LogicModBindingV1 {
public:
    const std::string& mod_id() const noexcept { return mod_id_; }
    const std::string& version() const noexcept { return version_; }
    const std::string& package_digest() const noexcept { return package_digest_; }
    const std::string& worker_sha256() const noexcept { return worker_sha256_; }
    const std::string& template_id() const noexcept { return template_id_; }
    const std::string& capability() const noexcept { return capability_; }
    const std::string& registry_digest() const noexcept { return registry_digest_; }
    const std::string& worker_bytes() const noexcept { return worker_bytes_; }
    std::string canonical_identity() const;
private:
    LogicModBindingV1() = default;
    std::string mod_id_, version_, package_digest_, worker_sha256_, template_id_, capability_, registry_digest_, worker_bytes_;
    friend class LogicModRegistryV1;
    friend class DurableLogicModRegistryV1;
};

// In-memory registry. No persistence, extraction, execution or implicit updates.
// Every mutation and binding issuance is atomic with respect to this instance.
class LogicModRegistryV1 {
public:
    void discover(LogicModPackageV1 package, std::string package_directory);
    void verify(std::string_view package_digest, const RuntimeModTrustStoreV1& trust);
    void test_golden(std::string_view package_digest, const TreeRegistryV1& base);
    void install(std::string_view package_digest);
    void activate(std::string_view package_digest);
    void disable(std::string_view package_digest);
    void revoke(std::string_view package_digest);
    LogicModStateV1 state(std::string_view package_digest) const;
    std::string digest() const;
    LogicModBindingV1 bind(std::string_view mod_id, std::string_view version,
                           std::string_view template_id, std::string_view capability) const;
private:
    struct Record {
        LogicModPackageV1 package;
        std::string directory;
        LogicModStateV1 state = LogicModStateV1::discovered;
        std::optional<RuntimeModTrustStoreV1> trust;
        std::string golden_tree_digest;
    };
    Record& find(std::string_view);
    const Record& find(std::string_view) const;
    std::string digest_locked() const;
    mutable std::mutex mutex_;
    std::map<std::string, Record> records_;
};
}
