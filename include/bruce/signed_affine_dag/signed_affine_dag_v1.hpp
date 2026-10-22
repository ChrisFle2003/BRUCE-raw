#pragma once

#include "bruce/core/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct AffineOperatorV1 {
    std::string_view id;
    std::int64_t a, b;
};

struct SignedAffineExecutionNodeV1 {
    std::string node_id;
    std::optional<std::string> predecessor_node_id;
    std::string operator_id;
    std::optional<std::int64_t> source_value;
    std::int64_t path_weight{};
};

struct SignedAffineDagSpecV1 {
    std::vector<SignedAffineExecutionNodeV1> nodes;
};

struct SignedAffinePathRecordV1 {
    std::string node_id, predecessor_node_id, operator_id;
    std::int64_t input_value{}, output_value{}, path_weight{};
};

struct SignedAffineSemanticStateV1 {
    std::int64_t value{}, weight_balance{};
    std::vector<SignedAffinePathRecordV1> paths;
};

struct SignedAffineDagResultV1 {
    std::vector<SignedAffineSemanticStateV1> states;
};

void validate_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec);
SignedAffineDagResultV1 execute_signed_affine_dag_v1(const SignedAffineDagSpecV1& spec);

} // namespace bruce
