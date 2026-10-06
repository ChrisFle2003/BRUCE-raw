#pragma once
#include "bruce/application/portfolio_order.hpp"
#include "bruce/experience/store.hpp"
namespace bruce {
// Existing versioned context derivation; these hashes are not approval credentials.
std::string experience_training_key(const ExactResearchTask&);
std::string experience_context_key(const ExactResearchTask&,const std::string& shape);
void rank_experience(const ExperienceStore&,const ExactResearchTask&,PortfolioOrder&,
    std::vector<std::uint64_t>& votes,std::uint64_t& used,const std::function<void(std::uint64_t)>& spend);
}
