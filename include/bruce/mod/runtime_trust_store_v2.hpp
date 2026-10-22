#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bruce {

// The caller supplies UTC time. This contract never consults the process clock.
class RuntimeTrustClockV2 {
public:
    virtual ~RuntimeTrustClockV2() = default;
    [[nodiscard]] virtual std::string now_utc() const = 0;
};

struct RuntimeTrustGenerationV2 {
    std::string generation;
    std::string trust_bytes;
    std::string trust_sha256;
    std::string created_at;
    std::optional<std::string> comment;
    bool operator==(const RuntimeTrustGenerationV2&) const = default;
};

// generation 00000000000000000001 models an explicit V1 import. The caller
// passes the exact already-bound V1 bytes; this function performs no I/O.
RuntimeTrustGenerationV2 make_runtime_trust_generation_v2(
    std::string_view generation, std::string_view trust_bytes,
    const RuntimeTrustClockV2& clock, std::optional<std::string> comment = std::nullopt);
std::string render_runtime_trust_metadata_v2(const RuntimeTrustGenerationV2& generation);
RuntimeTrustGenerationV2 parse_runtime_trust_metadata_v2(std::string_view metadata,
                                                          std::string_view trust_bytes);
bool same_runtime_trust_identity_v2(const RuntimeTrustGenerationV2& left,
                                    const RuntimeTrustGenerationV2& right);

struct RuntimeTrustReferenceV2 {
    std::string generation;
    std::string trust_sha256;
    bool operator==(const RuntimeTrustReferenceV2&) const = default;
};
struct RuntimeTrustStateV2 {
    std::optional<RuntimeTrustReferenceV2> prepared;
    std::optional<RuntimeTrustReferenceV2> active;
    std::optional<RuntimeTrustReferenceV2> previous;
    bool operator==(const RuntimeTrustStateV2&) const = default;
};
std::string render_runtime_trust_state_v2(const RuntimeTrustStateV2& state);
RuntimeTrustStateV2 parse_runtime_trust_state_v2(std::string_view state);

enum class RuntimeInstalledModKindV2 { data, logic };
struct RuntimeActiveInstalledModV2 {
    RuntimeInstalledModKindV2 kind;
    std::string mod_id;
    std::string version;
    std::string signing_key_id;
    bool active = false;
};
struct RuntimeAffectedModV2 {
    RuntimeInstalledModKindV2 kind;
    std::string mod_id;
    std::string version;
    std::string signing_key_id;
    bool operator==(const RuntimeAffectedModV2&) const = default;
};
// Callers provide a snapshot of installed records after resolving package
// signing keys. Only active records enter the result; paths are never exposed.
std::vector<RuntimeAffectedModV2> runtime_mods_losing_trust_v2(
    const RuntimeTrustGenerationV2& current, const RuntimeTrustGenerationV2& candidate,
    std::span<const RuntimeActiveInstalledModV2> installed);

struct RuntimeTrustVerificationV2 {
    RuntimeTrustStateV2 state;
    std::vector<RuntimeAffectedModV2> active_incompatible;
    std::vector<RuntimeAffectedModV2> prepared_incompatible;
};

class RuntimeTrustStoreV2 {
public:
    // Validate existing state under a shared nonblocking lock, without recovery
    // or creating any root, lock, journal, snapshot or generation.
    static RuntimeTrustVerificationV2 read_only_status(const std::string& root);
    static RuntimeTrustStoreV2 open_existing(const std::string& root);
    static RuntimeTrustStoreV2 import_v1(const std::string& root,
        const RuntimeTrustClockV2& clock, std::optional<std::string> comment = std::nullopt);
    RuntimeTrustStateV2 state() const;
    RuntimeTrustVerificationV2 verify() const;
    void prepare(const std::string& external_truststore, const RuntimeTrustClockV2& clock,
                 std::optional<std::string> comment = std::nullopt);
    void activate();
    void rollback();
private:
    explicit RuntimeTrustStoreV2(std::string root) : root_(std::move(root)) {}
    std::string root_;
};

// Internal integration points. The caller already holds registry.lock. These
// preserve the one lock domain shared with InstalledModHostV2 and V1 registries.
namespace runtime_trust_store_detail {
inline constexpr std::string_view migration_fence = "BRUCE_RUNTIME_MOD_TRUST_MIGRATION_V2\n";
inline constexpr std::string_view migration_barrier = "BRUCE_TRUST_COORDINATOR_V2\n";
void recover_locked(const std::string& root, bool read_only = false);
std::string data_snapshot_locked(const std::string& root, bool read_only);
std::string logic_snapshot_locked(const std::string& root, bool read_only);
std::vector<RuntimeActiveInstalledModV2> data_packages_locked(const std::string& root,
    const std::vector<RuntimeTrustGenerationV2>& history);
std::vector<RuntimeActiveInstalledModV2> logic_packages_locked(const std::string& root,
    const std::vector<RuntimeTrustGenerationV2>& history);
struct TransitionPlanV2 {
    RuntimeTrustStateV2 next_state;
    std::string candidate_trust_bytes;
    std::vector<RuntimeAffectedModV2> affected;
};
// InstalledModHostV2 owns registry.lock around these calls.
bool coordinator_required_locked(const std::string& root);
std::string active_trust_bytes_locked(const std::string& root);
TransitionPlanV2 plan_transition_locked(const std::string& root, bool rollback,
                                       bool read_only_admission = false);
void import_v1_locked(const std::string& root, const RuntimeTrustClockV2& clock,
                      std::optional<std::string> comment, bool read_only_admission);
void prepare_locked(const std::string& root, const std::string& external,
                    const RuntimeTrustClockV2& clock, std::optional<std::string> comment,
                    bool read_only_admission);
RuntimeTrustVerificationV2 verify_for_host_locked(const std::string& root);
void commit_transition_locked(const std::string& root, bool rollback,
                              std::string next_data_snapshot,
                              std::string next_logic_snapshot);
}

} // namespace bruce
