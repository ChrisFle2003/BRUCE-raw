#pragma once
#include <functional>
#include <exception>
namespace bruce {
// A checkpoint either returns normally or throws; it never creates a partial replay result.
using ReplayCheckpoint=std::function<void()>;
namespace detail {
// Keep callback exceptions out of the mathematical executors' numeric-limit catches.
struct ReplayCheckpointRaised {std::exception_ptr original;};
inline bool observe_replay_checkpoint(const ReplayCheckpoint& checkpoint) {
    try {checkpoint();} catch(...) {throw ReplayCheckpointRaised{std::current_exception()};}
    return false;
}
}
}
