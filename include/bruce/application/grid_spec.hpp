#pragma once
#include "bruce/application/grid.hpp"

namespace bruce {
// The version fixes the supported shape and rules, not just a display name.
struct GridSpec {
    static constexpr std::string_view version="grid-spec-v1";
    static constexpr std::string_view explicit_stage_version="grid-spec-v2";
    static constexpr std::string_view engine="bruce-grid-v1";
    static constexpr std::string_view address_rule="local-8y-plus-x-path-v1";
    static constexpr char leaf_local_power_rule[]="LEAF_LOCAL_POWER_V1";
    static constexpr char scaled_exponential_rule[]="SCALED_EXPONENTIAL_V1";
    static constexpr std::string_view value_rule=leaf_local_power_rule;
    static constexpr std::string_view policy="local-u64-single-leaf-v1";
    static constexpr std::string_view domain="uint64";
    std::string mode="exact",zero_zero="one";
    unsigned max_bits=64;
    std::optional<std::uint64_t> modulus;
    std::string rule=leaf_local_power_rule;
    std::optional<std::uint64_t> stage=std::nullopt;
};
GridSpec grid_spec(const GridTask&);
GridTask grid_task(const GridSpec&,std::string path);
std::string canonical_grid_path(std::string_view);
std::string grid_cache_key(const GridTask&);
std::string render_grid_spec(const GridSpec&);
}
