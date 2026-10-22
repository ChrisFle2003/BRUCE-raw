#pragma once

#include "bruce/mod/logic_mod_plugin_v1.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct LogicModRecordV1 {
    std::string mod_id;
    std::string version;
    std::string package_digest;
    std::string worker_sha256;
    LogicModStateV1 state = LogicModStateV1::installed_inactive;
    bool operator==(const LogicModRecordV1&) const = default;
};

struct LogicModSnapshotV1 {
    std::uint64_t generation = 0;
    std::vector<LogicModRecordV1> records;
    std::string journal_digest;
    bool operator==(const LogicModSnapshotV1&) const = default;
};

class DurableLogicModRegistryV1 {
public:
    static DurableLogicModRegistryV1 open(const std::string& mod_root,
                                          const RuntimeModTrustStoreV1& trust);
    static DurableLogicModRegistryV1 open_existing(const std::string& mod_root);

    void install(const LogicModPackageV1& package);
    void activate(std::string_view mod_id, std::string_view version);
    void disable(std::string_view mod_id);
    [[nodiscard]] LogicModSnapshotV1 snapshot() const;
#ifdef BRUCE_TRUST_V1_FIXTURE_API
    [[nodiscard]] LogicModBindingV1 bind(std::string_view mod_id, std::string_view version,
                                         std::string_view template_id,
                                         std::string_view capability) const;
#endif
private:
    friend class InstalledModHostV2;
    static LogicModSnapshotV1 read_only_snapshot(const std::string& root);
    DurableLogicModRegistryV1(std::string root, RuntimeModTrustStoreV1 trust);
    void install_locked(const LogicModPackageV1& package);
    void update_locked(const LogicModPackageV1& package);
    void activate_locked(std::string_view mod_id, std::string_view version);
    void disable_locked(std::string_view mod_id);
    LogicModSnapshotV1 snapshot_locked() const;
    LogicModBindingV1 bind_locked(std::string_view mod_id, std::string_view version,
                                  std::string_view template_id,
                                  std::string_view capability) const;
    std::string root_;
    RuntimeModTrustStoreV1 trust_;
};

} // namespace bruce
