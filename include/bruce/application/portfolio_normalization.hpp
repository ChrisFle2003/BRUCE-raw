#pragma once
#include "bruce/application/exact_research.hpp"
#include <map>
namespace bruce {
// Internal helpers for the versioned five-family portfolio only.
// Child results remain authoritative evidence even when union work stops early.
bool merge_normalized_portfolio(ExactResearchRun&,const ExactResearchRun&,unsigned,
    std::map<std::string,std::size_t>&,const std::function<bool(std::uint64_t)>&,EqualDomainCache* domain_cache=nullptr);
std::string render_normalized_portfolio(const ExactResearchRun&);
}
