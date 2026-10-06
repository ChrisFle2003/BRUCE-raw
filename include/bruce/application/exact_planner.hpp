#pragma once
#include "bruce/application/exact_research.hpp"

namespace bruce {
struct CoefficientPlan {
    std::string decision="fallback";
    std::uint64_t a_slot=0,b_slot=0;
    std::size_t second_sample=0;
};
// Called only after task validation. spend must throw before exceeding a budget.
CoefficientPlan plan_coefficients(const ExactResearchTask& task,unsigned power,
    const std::function<void(std::uint64_t)>& spend);
}
