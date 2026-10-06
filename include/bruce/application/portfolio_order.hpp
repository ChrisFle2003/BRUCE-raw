#pragma once
#include "bruce/application/exact_research.hpp"
#include <array>
namespace bruce {
struct PortfolioOrder {
    std::vector<unsigned> strategies{0,1,2};
    std::string reason="insufficient_shape_samples";
};
// A bounded scheduling hint; this never excludes a family or verifies a hypothesis.
PortfolioOrder plan_portfolio_order(const ExactResearchTask&,const std::function<void(std::uint64_t)>& spend);
}
