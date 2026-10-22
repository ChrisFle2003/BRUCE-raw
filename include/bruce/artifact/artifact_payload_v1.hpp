#pragma once

#include "bruce/artifact/artifact_store_v1.hpp"

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

inline constexpr std::size_t artifact_payload_input_object_limit_v13 = 16;
inline constexpr std::string_view artifact_payload_contract_v13 = "BRUCE_ARTIFACT_PAYLOAD_V13";

// Opaque, metadata-only identity for one object on a direct DAG edge. This
// value deliberately contains no filesystem path, URL, provider, or origin.
struct ArtifactInputRefV13 {
    std::string manifest_id;
    std::uint64_t object_index{};
    std::string sha256;
    std::uint64_t size_bytes{};
    bool operator==(const ArtifactInputRefV13&) const = default;
};

// Provenance is supplied by the already validated direct DAG edge. It is kept
// outside ArtifactInputRefV13 so the persisted reference remains exactly four
// fields while selection still enforces the manifest binding.
struct ArtifactPayloadProvenanceV13 {
    std::string run_id;
    std::string node_id;
    std::string attempt_id;
    bool operator==(const ArtifactPayloadProvenanceV13&) const = default;
};

enum class ArtifactPayloadErrorCodeV13 {
    invalid_reference,
    manifest_invalid,
    manifest_not_ready,
    object_index_out_of_range,
    reference_mismatch,
    provenance_mismatch,
    object_limit_exceeded,
    object_verification_failed,
};

class ArtifactPayloadErrorV13 final : public Error {
public:
    explicit ArtifactPayloadErrorV13(ArtifactPayloadErrorCodeV13 code);
    ArtifactPayloadErrorCodeV13 code() const noexcept { return code_; }

private:
    ArtifactPayloadErrorCodeV13 code_;
};

// Metadata returned after the selected object has passed the complete
// Artifact Store verification. No payload bytes or store paths are returned.
struct ArtifactPayloadSelectionV13 {
    ArtifactInputRefV13 reference;
    ArtifactIdV1 object;
    std::string run_id;
    std::string node_id;
    std::string attempt_id;
    std::string result_code;
    std::string origin;
    bool operator==(const ArtifactPayloadSelectionV13&) const = default;
};

enum class ArtifactPayloadMaterializationErrorV13 {
    destination_invalid,
    destination_not_empty,
    input_limit_exceeded,
    object_limit_exceeded,
    deadline_exceeded,
    object_copy_failed,
};

class ArtifactPayloadMaterializationExceptionV13 final : public Error {
public:
    explicit ArtifactPayloadMaterializationExceptionV13(
        ArtifactPayloadMaterializationErrorV13 code);
    ArtifactPayloadMaterializationErrorV13 code() const noexcept { return code_; }

private:
    ArtifactPayloadMaterializationErrorV13 code_;
};

struct ArtifactPayloadMaterializedObjectV13 {
    ArtifactInputRefV13 reference;
    ArtifactIdV1 object;
    std::uint64_t bytes_written{};
    std::uint64_t chunks{};
    bool operator==(const ArtifactPayloadMaterializedObjectV13&) const = default;
};

struct ArtifactPayloadMaterializationV13 {
    std::uint64_t object_count{};
    std::uint64_t bytes_written{};
    std::vector<ArtifactPayloadMaterializedObjectV13> objects;
    bool operator==(const ArtifactPayloadMaterializationV13&) const = default;
};

bool is_valid_artifact_input_ref_v13(const ArtifactInputRefV13& reference) noexcept;
void validate_artifact_input_ref_v13(const ArtifactInputRefV13& reference);

// Resolve one object from a canonical READY manifest, enforce the direct-edge
// provenance supplied by the caller, and fully verify the content-addressed
// object. This operation is read-only and never returns object bytes.
ArtifactPayloadSelectionV13 select_artifact_input_v13(
    const ArtifactStoreV1& store,
    const ArtifactInputRefV13& reference,
    const ArtifactPayloadProvenanceV13& provenance);

// Materialize already-selected direct inputs in deterministic order into one
// caller-created private Attempt file.  All selected objects are copied before
// success is returned; any error rewinds and truncates the destination so a
// partial payload cannot be accepted by a caller.
ArtifactPayloadMaterializationV13 materialize_artifact_inputs_v13(
    const ArtifactStoreV1& store,
    const std::vector<ArtifactPayloadSelectionV13>& inputs,
    int destination_fd,
    std::uint64_t max_input_bytes,
    std::chrono::steady_clock::time_point deadline);

} // namespace bruce::artifact_v1
