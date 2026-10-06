#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/compatibility/snapshot.hpp"
namespace bruce {
struct CompatibilityTask {
    std::string rules, candidates, origin;
    std::int64_t at=0;
    ZBudget budget;
};
struct CompatibilityRun {
    CompatibilityTask task;
    std::string snapshot_digest, origin_status;
    ZResult result;
};
CompatibilityRun run_compatibility(CompatibilityTask task,const std::function<bool(std::uint64_t)>& cancelled={},const ReplayCheckpoint& source_checkpoint={},const ReplayCheckpoint& rules_checkpoint={});
std::string render_compatibility(const CompatibilityRun& run);
std::string pack_compatibility(const CompatibilityRun& run,const ReplayCheckpoint& checkpoint={});
std::string pack_compatibility_records(const CompatibilityRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records);
CompatibilityRun replay_compatibility(std::string_view bundle,const ReplayCheckpoint& checkpoint={});
std::string replay_output(std::string_view bundle);
}
