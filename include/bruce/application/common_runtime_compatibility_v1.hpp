#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace bruce {

// Describes an existing control path. It grants no authority and does not
// choose or run a path.
struct CommonRuntimeCompatibilityV1 {
    std::string family;
    std::string budget_unit;
    std::string checkpoint_order;
    std::string error_envelope;
    std::string publication_boundary;
    std::string legacy_route;

    bool operator==(const CommonRuntimeCompatibilityV1&) const = default;
};

inline constexpr std::string_view common_runtime_compatibility_v1_contract =
    "bruce-common-runtime-compatibility-v1";
inline constexpr std::size_t common_runtime_compatibility_v1_max_identifier_bytes = 64;
inline constexpr std::size_t common_runtime_compatibility_v1_max_json_bytes = 768;

// Both operations throw bruce::Error for invalid or noncanonical data.
std::string encode_common_runtime_compatibility_v1(const CommonRuntimeCompatibilityV1& value);
CommonRuntimeCompatibilityV1 decode_common_runtime_compatibility_v1(std::string_view json);

// Exact, read-only descriptions of the existing control paths. Callers choose
// a family explicitly; validation never changes a descriptor or selects a path.
CommonRuntimeCompatibilityV1 control_family_compatibility_v1(std::string_view family);
void require_control_family_compatibility_v1(const CommonRuntimeCompatibilityV1& value);

}
