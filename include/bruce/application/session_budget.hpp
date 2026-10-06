#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace bruce {
enum class ExecutionPhase;
struct SessionBudget {
    std::optional<std::uint64_t> max_checkpoints;
};
// Counts admitted control points, not mathematical operations, time or process RAM.
struct SessionUsage {
    std::optional<std::uint64_t> max_checkpoints;
    std::uint64_t checkpoints=0,executing=0,serializing=0,replaying=0;
    void consume(ExecutionPhase);
};
std::string render_session_usage(const SessionUsage&);
}
