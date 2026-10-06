#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/application/grid_batch.hpp"
namespace bruce {
struct GridResumeRun {
    std::string parent;
    std::size_t additional_queries=0,generation=0;
    std::uint64_t audit_evaluations=0,new_evaluations=0;
    GridBatchRun batch;
};
GridResumeRun resume_grid_batch(std::string parent,std::size_t additional,
    const std::function<bool(std::size_t)>& cancelled={},const ReplayCheckpoint& import_checkpoint={});
std::string render_grid_resume(const GridResumeRun&);
std::string pack_grid_resume(const GridResumeRun&,const ReplayCheckpoint& checkpoint={},const ReplayCheckpoint& records={});
GridResumeRun replay_grid_resume(std::string_view,const ReplayCheckpoint& checkpoint={});
GridBatchRun replay_grid_source(std::string_view,const ReplayCheckpoint& checkpoint={});
}
