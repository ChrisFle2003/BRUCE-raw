#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/core/types.hpp"
#include <optional>
#include <limits>
namespace bruce {
// Shared execution checkpoints for explicit transfers; not mathematical operations.
class TransferControl {
public:
    std::optional<std::uint64_t> maximum;
    std::function<bool()> cancelled;
    void visit() {
        if(cancelled && cancelled())throw Error("TransferCancelled");
        if(maximum && used_>=*maximum)throw Error("TransferCheckpointBudgetExceeded: checkpoints="+std::to_string(used_)+" limit="+std::to_string(*maximum));
        if(used_==std::numeric_limits<std::uint64_t>::max())throw Error("TransferCheckpointCounterOverflow");
        ++used_;
    }
    ReplayCheckpoint callback(){return [this]{visit();};}
    std::uint64_t used() const {return used_;}
    std::string report() const {
        return "{\"model\":\"transfer-checkpoints-v1\",\"max_checkpoints\":"+(maximum ? std::to_string(*maximum):"null")+",\"checkpoints\":"+std::to_string(used_)+"}";
    }
private:
    std::uint64_t used_=0;
};
}
