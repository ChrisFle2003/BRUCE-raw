#pragma once
#include "bruce/application/exact_research.hpp"
#include <span>
namespace bruce {
enum class ResearchExecutor { affine_power, polynomial, rational, expression, portfolio };
struct ResearchStrategy {
    std::string_view id,default_planner;
    ResearchExecutor executor;
    std::span<const std::string_view> planners;
};
std::span<const ResearchStrategy> research_strategies();
const ResearchStrategy& research_strategy(std::string_view);
void validate_research_route(std::string_view strategy,std::string_view planner);
// Shared resource bounds, independent of sample parsing and execution.
void validate_exact_resource_limits(const ExactResearchTask&);
std::uint64_t validate_exact_task(ExactResearchTask&,const NumberDomain&);
std::string render_research_contract();
}
