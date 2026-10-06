#include "bruce/compatibility/selection.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace bruce {
Selection select_candidates(const std::vector<RankedCandidate>& candidates) {
    Selection result;
    std::optional<std::tuple<std::int64_t,std::uint64_t>> best;
    std::set<std::uint64_t> seen;
    for (const auto& candidate:candidates) {
        if (!seen.insert(candidate.id).second) throw Error("DuplicateCandidateId");
        if (candidate.hard_rejected) continue;
        const auto rank=std::make_tuple(candidate.score,candidate.independent_evidence);
        if (!best || rank>*best) { best=rank; result.ids.clear(); }
        if (rank==*best) result.ids.push_back(candidate.id);
    }
    std::sort(result.ids.begin(),result.ids.end());
    result.ambiguous=result.ids.size()>1;
    return result;
}
}
