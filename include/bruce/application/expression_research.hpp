#pragma once
#include "bruce/application/exact_research.hpp"
namespace bruce {
// Internal strategy entry; task validation is performed by exact_research.
ExactResearchRun run_expression_research(ExactResearchTask,const std::function<bool(std::uint64_t)>&);
std::string render_expression_research(const ExactResearchRun&);
}
