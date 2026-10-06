#pragma once
#include "bruce/application/grid_spec.hpp"
#include "bruce/core/replay_control.hpp"
#include <deque>
#include <functional>

namespace bruce {
inline constexpr std::size_t max_grid_cache_bytes=1048576;
inline constexpr std::size_t max_grid_batch_queries=1024;
inline constexpr std::size_t max_grid_batch_input_bytes=262144;
struct GridCacheStats {
    std::uint64_t hits=0,misses=0,inserts=0,evictions=0,bypasses=0,entries=0,bytes=0,peak_bytes=0,evaluations=0;
};
struct CachedGridRun {GridRun run;std::string key_digest,cache_event;};
// Per-executor FIFO, at most 64 entries. Budget charges canonical key + V1
// result bundle bytes, not allocator overhead or total process memory.
class GridCache {
    struct Entry {std::string key;GridRun run;std::size_t bytes;};
    std::deque<Entry> entries_;
    std::size_t limit_;
    GridCacheStats stats_;
public:
    explicit GridCache(std::size_t bytes);
    CachedGridRun evaluate(GridTask);
    const GridCacheStats& stats() const {return stats_;}
};
struct GridBatchTask {
    GridSpec spec;
    std::vector<std::string> paths;
    std::size_t cache_bytes=65536;
    std::optional<std::size_t> query_steps,cancel_after_queries;
};
struct GridBatchRun {
    GridBatchTask task;
    std::vector<CachedGridRun> queries;
    std::vector<std::string> trace;
    GridCacheStats cache;
    std::string status="COMPLETED",stop_reason="Completed";
};
GridBatchRun evaluate_grid_batch(GridBatchTask,const std::function<bool(std::size_t)>& cancelled={});
std::string render_grid_batch(const GridBatchRun&);
std::string pack_grid_batch(const GridBatchRun&,const ReplayCheckpoint& checkpoint={},const ReplayCheckpoint& records={});
GridBatchRun replay_grid_batch(std::string_view,const ReplayCheckpoint& checkpoint={});
}
