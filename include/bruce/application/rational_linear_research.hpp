#pragma once
#include "bruce/application/exact_research.hpp"
namespace bruce {
ExactResearchRun run_rational_linear_research(ExactResearchTask,const std::function<bool(std::uint64_t)>&);
std::string render_rational_linear_research(const ExactResearchRun&);
}
