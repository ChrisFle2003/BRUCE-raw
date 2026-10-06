#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/coordinate/address.hpp"

namespace bruce {
struct GridTask {
    std::string path,mode="exact",zero_zero="one";
    unsigned max_bits=64;
    std::optional<std::uint64_t> modulus;
};
struct GridRun {
    GridTask task;
    GridPath path;
    bool stopped=false;
    std::uint64_t value=0;
    std::vector<std::string> trace;
};
GridRun evaluate_grid(GridTask);
std::string render_grid(const GridRun&);
std::string pack_grid(const GridRun&,const ReplayCheckpoint& checkpoint={});
GridRun replay_grid(std::string_view,const ReplayCheckpoint& checkpoint={});
}
