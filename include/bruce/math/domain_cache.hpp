#pragma once
#include "bruce/math/expression_normal_form.hpp"
#include <array>
namespace bruce {
struct DomainCacheStats {
    std::uint64_t hits=0,misses=0,bypasses=0,inserts=0,evictions=0,entries=0,key_bytes=0,operations=0;
};
// Run-local, fixed modular policy and immutable number domain. Stores only
// completed EQUAL decisions, never UNKNOWN, DIFFERENT or unfinished work.
class EqualDomainCache {
    const NumberDomain domain_;
    std::array<std::string,64> keys_{};
    DomainCacheStats stats_;
public:
    explicit EqualDomainCache(const NumberDomain& domain):domain_(domain) {}
    const DomainCacheStats& stats() const {return stats_;}
    RationalDomainComparison compare(const std::vector<ExactValue>& left,
        const std::vector<ExactValue>& right,const ExpressionSpend& spend);
};
}
