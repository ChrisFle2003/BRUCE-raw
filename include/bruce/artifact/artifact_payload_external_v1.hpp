#pragma once

#include "bruce/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bruce::artifact_v1 {

inline constexpr std::string_view artifact_payload_external_contract_v14 =
    "bruce-dag-scheduler-v5-external-inputs-v14";
inline constexpr std::size_t artifact_payload_external_input_limit_v14 = 15;

struct ExternalInputRefV14 {
    std::string manifest_id;
    std::uint64_t object_index{};
    std::string sha256;
    std::uint64_t size_bytes{};
    std::string source_run_id;
    std::string source_node_id;
    std::string source_attempt_id;
    bool operator==(const ExternalInputRefV14&) const = default;
};

struct ExternalInputBindingV14 {
    std::string node_id;
    std::vector<ExternalInputRefV14> inputs;
    bool operator==(const ExternalInputBindingV14&) const = default;
};

struct ExternalInputsSidecarV14 {
    std::string contract;
    std::vector<ExternalInputBindingV14> bindings;
    bool operator==(const ExternalInputsSidecarV14&) const = default;
};

struct ExternalInputsFileV14 {
    ExternalInputsSidecarV14 sidecar;
    std::string digest;
    std::uint64_t size_bytes{};
    bool operator==(const ExternalInputsFileV14&) const = default;
};

enum class ExternalInputsErrorCodeV14 {
    file_missing,
    file_too_large,
    file_not_regular,
    file_symlink,
    file_permissions,
    file_changed,
    invalid_json,
    invalid_contract,
    invalid_field,
    duplicate_field,
    invalid_identifier,
    invalid_digest,
    invalid_number,
    binding_invalid,
    duplicate_binding,
    duplicate_object,
    input_limit_exceeded,
};

class ExternalInputsErrorV14 final : public Error {
public:
    explicit ExternalInputsErrorV14(ExternalInputsErrorCodeV14 code);
    ExternalInputsErrorCodeV14 code() const noexcept { return code_; }

private:
    ExternalInputsErrorCodeV14 code_;
};

// Parse the exact canonical V1.4 sidecar bytes. The parser accepts no
// whitespace, alternate escapes, unknown fields, duplicate fields, or omitted
// values. It never interprets a JSON value as a filesystem path.
ExternalInputsSidecarV14 parse_external_inputs_v14(std::string_view bytes);

// Read, authenticate, and parse one owner-only sidecar. The returned digest is
// over the exact bytes read from the file; no path or file bytes are retained.
ExternalInputsFileV14 read_external_inputs_v14(const std::filesystem::path& path);

// Compute the public digest representation used by the report layer.
std::string digest_external_inputs_v14(std::string_view bytes);

} // namespace bruce::artifact_v1
