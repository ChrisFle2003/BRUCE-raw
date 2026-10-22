#pragma once

#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/mod/logic_mod_plugin_v1.hpp"

namespace bruce {
inline constexpr std::uint64_t logic_mod_result_limit_v1 = 16384;

// Host-only configuration. Neither this path nor the prepared Store is passed
// to the worker; published identities contain no local filesystem information.
struct LogicModOutputV1 {
    std::filesystem::path store;
    std::string manifest_id;
};

struct LogicModPublishedOutputV1 {
    std::string manifest_id;
    std::uint64_t object_index = 0;
    std::string sha256;
    std::uint64_t size_bytes = 0;
    std::string codec = "logic-mod-result-text-v1";
    std::string canonical_identity() const;
};

// Owns the Store exclusively in the host. No descriptor accessor is exposed.
class PreparedLogicModOutputV1 {
public:
    PreparedLogicModOutputV1(const PreparedLogicModOutputV1&) = delete;
    PreparedLogicModOutputV1& operator=(const PreparedLogicModOutputV1&) = delete;
    PreparedLogicModOutputV1(PreparedLogicModOutputV1&&) noexcept = default;
    PreparedLogicModOutputV1& operator=(PreparedLogicModOutputV1&&) noexcept = default;
private:
    PreparedLogicModOutputV1() = default;
    artifact_v1::ArtifactStoreV1 store_;
    std::string manifest_id_;
    friend PreparedLogicModOutputV1 prepare_logic_mod_output_v1(const LogicModOutputV1&);
    friend LogicModPublishedOutputV1 publish_logic_mod_result_artifact_v1(
        PreparedLogicModOutputV1&, const LogicModBindingV1&, std::string_view);
};

// Validate the ID before opening/creating the local Store.
PreparedLogicModOutputV1 prepare_logic_mod_output_v1(const LogicModOutputV1&);

// Requires a fresh registry-issued artifact.output.v1 snapshot. The caller
// issues it alongside the run's worker binding and calls only after acceptance.
LogicModPublishedOutputV1 publish_logic_mod_result_artifact_v1(
    PreparedLogicModOutputV1&, const LogicModBindingV1& output_binding,
    std::string_view result);

// Fresh read-only resolution; verifies fixed READY metadata and the recorded
// identity, then proves exact result bytes through SHA-256 and length.
void verify_logic_mod_result_artifact_v1(const LogicModOutputV1&,
    const LogicModPublishedOutputV1&, std::string_view expected_result);
}
