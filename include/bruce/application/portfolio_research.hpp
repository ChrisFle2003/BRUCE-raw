#pragma once
#include "bruce/application/exact_research.hpp"
namespace bruce {
// Internal entry: caller validates the shared task before dispatch.
ExactResearchRun run_portfolio_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& experience_checkpoint={});
std::string render_portfolio_research(const ExactResearchRun& run);
}
