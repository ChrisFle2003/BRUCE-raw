#pragma once
#include "bruce/core/types.hpp"
namespace bruce {
struct RankedCandidate {
    std::uint64_t id;
    std::int64_t score;
    std::uint64_t independent_evidence;
    bool hard_rejected;
};
struct Selection {
    std::vector<std::uint64_t> ids;
    bool ambiguous=false;
};
Selection select_candidates(const std::vector<RankedCandidate>& candidates);
}
