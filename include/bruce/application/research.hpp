#pragma once
#include "bruce/core/types.hpp"
#include "bruce/core/json.hpp"
#include <functional>
#include "bruce/core/replay_control.hpp"
namespace bruce {
struct Hypothesis {
    std::uint64_t id;
    std::int64_t a,b;
    std::string status;
};
struct Run {
    Task task;
    std::string status, stop_reason="Completed";
    std::uint64_t candidates=0, operations=0, range_rejections=0;
    std::vector<Hypothesis> hypotheses;
    std::vector<std::string> trace;
};
void validate_task(const Task& task);
Run research(Task task, const std::function<bool(std::uint64_t)>& cancelled={});
std::string render_run(const Run& run);
std::string pack_run(const Run& run,const ReplayCheckpoint& checkpoint={});
Run replay(std::string_view bundle,const ReplayCheckpoint& checkpoint={});
std::vector<Sample> read_samples(const std::string& path);
}
