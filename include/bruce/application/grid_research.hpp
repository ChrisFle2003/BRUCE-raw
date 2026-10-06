#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/application/exact_research.hpp"

namespace bruce {
inline constexpr std::size_t grid_research_source_limit=2*1024*1024;
struct GridResearchRun {
    std::string training_source,holdout_source,prefix;
    unsigned exponent=0;
    ExactResearchRun research;
    std::string axis="x";
    unsigned base=0;
    std::optional<std::uint64_t> total_operations;
    std::uint64_t requested_operations=0;
    bool budget_experience=false;
};
GridResearchRun research_from_grid(ExactResearchTask,std::string training,std::string holdout={},
    const std::function<bool(std::uint64_t)>& cancelled={},std::string axis="x",
    std::optional<std::uint64_t> total_operations={},const ReplayCheckpoint& experience_checkpoint={},const ReplayCheckpoint& grid_checkpoint={});
std::string render_grid_research(const GridResearchRun&);
std::string pack_grid_research(const GridResearchRun&,const ReplayCheckpoint& checkpoint={});
std::string pack_grid_research_records(const GridResearchRun&,const ReplayCheckpoint&,const ReplayCheckpoint&);
GridResearchRun replay_grid_research(std::string_view,std::uint64_t operation_limit=10000000,
    const std::function<void(std::string_view)>& admit_research={},const ReplayCheckpoint& checkpoint={});
}
