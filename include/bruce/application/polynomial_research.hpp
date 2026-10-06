#pragma once
#include "bruce/application/exact_research.hpp"

namespace bruce {
// Internal strategy entry; exact_research validates the shared task first.
ExactResearchRun run_polynomial_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled);
std::string render_polynomial_research(const ExactResearchRun& run);
}
