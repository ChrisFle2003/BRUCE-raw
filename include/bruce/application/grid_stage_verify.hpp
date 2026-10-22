#pragma once
#include "bruce/application/grid_stage_sweep.hpp"

namespace bruce {
struct GridStageVerifyTask {
    std::string sweep_bundle;
};
struct GridStageVerifyRecord {
    std::uint64_t stage=0,expected=0,observed=0;
    bool matches=false;
};
struct GridStageVerifyRun {
    GridStageVerifyTask task;
    std::string mode,sweep_digest;
    std::string formula="100*(2^stage-1)";
    std::string claim_scope="finite-uint64-scaled-exponential-stage-domain-0..57";
    std::string status="VERIFIED_ON_DOMAIN";
    std::vector<GridStageVerifyRecord> records;
    std::optional<GridStageVerifyRecord> first_mismatch;
};
GridStageVerifyRun evaluate_grid_stage_verify(GridStageVerifyTask);
std::string render_grid_stage_verify(const GridStageVerifyRun&);
std::string pack_grid_stage_verify(const GridStageVerifyRun&,const ReplayCheckpoint& checkpoint={});
GridStageVerifyRun replay_grid_stage_verify(std::string_view,const ReplayCheckpoint& checkpoint={});
}
