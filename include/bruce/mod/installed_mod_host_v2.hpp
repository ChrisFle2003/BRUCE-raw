#pragma once

#include "bruce/core/types.hpp"
#include "bruce/mod/logic_mod_registry_v1.hpp"
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include "bruce/mod/package_inspection_v2.hpp"
#include "bruce/mod/runtime_registry.hpp"
#include "bruce/mod/runtime_trust_store_v2.hpp"
#include "bruce/mod/unified_catalog_v2.hpp"

namespace bruce {

struct InstalledModSnapshotV2 {
    RuntimeModSnapshotV1 data;
    LogicModSnapshotV1 logic;
    bool operator==(const InstalledModSnapshotV2&) const = default;
};
struct InstalledModTrustTransitionV2 {
    RuntimeTrustStateV2 state;
    std::vector<RuntimeAffectedModV2> deactivated;
};
// A blocked switch carries only public identities from the host's already
// validated plan. Presentation belongs to the caller, never the host.
class InstalledModTrustBlockedV2 final : public Error {
public:
    explicit InstalledModTrustBlockedV2(std::vector<RuntimeAffectedModV2> affected)
        : Error("RuntimeTrustActiveModsIncompatible"), affected_(std::move(affected)) {}
    [[nodiscard]] const std::vector<RuntimeAffectedModV2>& affected() const noexcept { return affected_; }
private:
    std::vector<RuntimeAffectedModV2> affected_;
};
struct InstalledModRouteV2 {
    InstalledModSnapshotV2 snapshot;
    std::string registry_digest;
    RouteResultV1 route;
};
using InstalledDataModRouteV1 = RuntimeModRouteV1;
struct InstalledLogicModIdentityV2 {
    std::string mod_id;
    std::string version;
    std::string package_digest;
    std::string worker_sha256;
    std::string template_id;
    std::string capability;
    std::string registry_digest;
};
struct InstalledModRunV2 {
    InstalledLogicModIdentityV2 identity;
    LogicModRunV1 run;
};

class InstalledModHostV2 {
public:
    static RuntimeTrustVerificationV2 trust_status(const std::string& mod_root);
    // Select a root without opening registries or recovering state. Every trust
    // operation pins all components and acquires an existing lock itself.
    static InstalledModHostV2 for_trust_operations(const std::string& mod_root);
    static InstalledModHostV2 open(const std::string& mod_root,
                                   const RuntimeModTrustStoreV1& trust);
    static InstalledModHostV2 open_existing(const std::string& mod_root);
    static InstalledModHostV2 open_existing(const std::string& mod_root,
                                            const RuntimeModTrustStoreV1& trust);
    // Inventory reads never initialize/recover state and reject busy writers.
    // Lifecycle replies use read_only=false for the original blocking recovery
    // semantics after committing, so peer contention cannot turn success red.
    static std::vector<InstalledModProjectionV2> catalog_entries(const std::string& mod_root,
                                                                bool read_only = true);

    void install(const DetectedModPackageV2& package);
    void update(const DetectedModPackageV2& package);
    void activate(std::string_view mod_id, std::string_view version);
    void disable(std::string_view mod_id);
    void import_trust_v1(const RuntimeTrustClockV2& clock,
                         std::optional<std::string> comment = std::nullopt);
    void prepare_trust(const std::string& external_truststore,
                       const RuntimeTrustClockV2& clock,
                       std::optional<std::string> comment = std::nullopt);
    [[nodiscard]] RuntimeTrustVerificationV2 verify_trust() const;
    InstalledModTrustTransitionV2 activate_trust(bool deactivate_untrusted = false);
    InstalledModTrustTransitionV2 rollback_trust(bool deactivate_untrusted = false);
    [[nodiscard]] InstalledModSnapshotV2 snapshot() const;
    [[nodiscard]] InstalledDataModRouteV1 route_data(std::string_view text) const;
    [[nodiscard]] InstalledModRouteV2 route(std::string_view text) const;
    [[nodiscard]] InstalledModRunV2 run_logic(
        std::string_view mod_id, std::string_view template_id, std::string_view input,
        LogicModBudgetsV1 budgets,
        std::string_view capability = "worker.execute") const;

private:
    InstalledModHostV2(std::string root, RuntimeModTrustStoreV1 trust);
    InstalledModTrustTransitionV2 transition_trust(bool rollback,
                                                   bool deactivate_untrusted);
    std::string root_;
    RuntimeModTrustStoreV1 trust_;
};

} // namespace bruce
