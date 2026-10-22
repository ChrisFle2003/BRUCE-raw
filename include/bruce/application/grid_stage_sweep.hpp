#pragma once
#include "bruce/application/grid_batch.hpp"

namespace bruce {
struct GridStageSweepTask {
    std::string path,mode="exact";
    std::uint64_t from_stage=0,to_stage=0;
    unsigned max_bits=64;
    std::optional<std::uint64_t> modulus;
    std::size_t cache_bytes=65536;
    bool debug=false;
};
struct GridStageSweepRecord {
    std::size_t index=0;
    std::uint64_t stage=0;
    GridRun run;
    std::string key_digest,cache_event,status="EVALUATED";
};
struct GridStageSweepRun {
    GridStageSweepTask task;
    GridSpec spec;
    std::vector<GridStageSweepRecord> records;
    GridCacheStats cache;
    std::string status="COMPLETED",stop_reason="Completed";
    std::optional<std::uint64_t> stopped_stage;
};
GridStageSweepRun evaluate_grid_stage_sweep(GridStageSweepTask);
std::string render_grid_stage_sweep(const GridStageSweepRun&);
std::string pack_grid_stage_sweep(const GridStageSweepRun&,const ReplayCheckpoint& checkpoint={});
GridStageSweepRun replay_grid_stage_sweep(std::string_view,const ReplayCheckpoint& checkpoint={});
}
