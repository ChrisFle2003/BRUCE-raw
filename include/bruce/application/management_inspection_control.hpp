#pragma once

#include "bruce/core/replay_control.hpp"
#include "bruce/core/types.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace bruce {

// Counts explicitly admitted read-only management inspection points. This
// controller is independent from session, gate and transfer contracts.
class ManagementInspectionControl {
public:
    std::optional<std::uint64_t> maximum;
    std::function<bool()> cancelled;

    void visit() {
        if (cancelled && cancelled()) throw Error("ManagementInspectionCancelled");
        if (maximum && used_ >= *maximum) {
            throw Error("ManagementCheckpointBudgetExceeded: checkpoints=" +
                        std::to_string(used_) + " limit=" + std::to_string(*maximum));
        }
        if (used_ == std::numeric_limits<std::uint64_t>::max()) {
            throw Error("ManagementCheckpointCounterOverflow");
        }
        ++used_;
    }

    ReplayCheckpoint callback() { return [this] { visit(); }; }

    std::uint64_t used() const { return used_; }

    std::string report() const {
        return "{\"model\":\"management-checkpoints-v1\",\"max_checkpoints\":" +
               (maximum ? std::to_string(*maximum) : "null") +
               ",\"checkpoints\":" + std::to_string(used_) + "}";
    }

private:
    std::uint64_t used_ = 0;
};

}
