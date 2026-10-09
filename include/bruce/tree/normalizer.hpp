#pragma once
#include "bruce/token/task_spec.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
// Tokens preserve numeric spelling; normalization only folds ASCII case and whitespace.
struct NormalizedRouteInputV1 {
    std::string normalized_text;
    std::vector<std::string> tokens;
    std::string normalization_version = std::string(token_normalization_version_v1);
};
NormalizedRouteInputV1 normalize_route_text_v1(std::string_view text);
}
