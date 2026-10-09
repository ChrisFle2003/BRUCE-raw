#pragma once
#include "bruce/mod/runtime_package.hpp"
#include <cstdint>

namespace bruce {
enum class RuntimeModStateV1 { installed_inactive, active, disabled, revoked };
struct RuntimeModRecordV1 {
    std::string mod_id;
    std::string version;
    std::string package_digest;
    RuntimeModStateV1 state = RuntimeModStateV1::installed_inactive;
    bool operator==(const RuntimeModRecordV1&) const = default;
};
struct RuntimeModSnapshotV1 {
    std::uint64_t generation = 0;
    std::vector<RuntimeModRecordV1> records;
    std::string journal_digest;
};
struct RuntimeModActiveViewV1 {
    RuntimeModSnapshotV1 snapshot;
    TreeRegistryV1 tree;
};
// Every operation reloads state while holding the installation's exclusive lock.
// The journal is the durable commit authority; registry.v1 is a recoverable snapshot.
// Trust is explicitly supplied at creation and pinned privately in the installation.
class RuntimeModRegistryV1 {
public:
    static RuntimeModRegistryV1 open(const std::string& mod_root, const RuntimeModTrustStoreV1&);
    static RuntimeModRegistryV1 open_existing(const std::string& mod_root);
    void install(const RuntimeDataModPackageV1&);
    void activate(std::string_view mod_id, std::string_view version);
    void disable(std::string_view mod_id);
    TreeRegistryV1 active_tree() const;
    RuntimeModActiveViewV1 active_view() const;
    RuntimeModSnapshotV1 snapshot() const;
    std::vector<RuntimeModRecordV1> records() const { return snapshot().records; }
    std::uint64_t generation() const { return snapshot().generation; }
    std::string journal_digest() const { return snapshot().journal_digest; }
private:
    RuntimeModRegistryV1(std::string root, RuntimeModTrustStoreV1 trust);
    std::string root_;
    RuntimeModTrustStoreV1 trust_;
};
}
