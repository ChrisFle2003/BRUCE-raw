#include "bruce/application/management_inspection_control.hpp"

#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw bruce::Error(message);
}

template<class Function>
std::string error_of(Function&& function) {
    try {
        function();
    } catch (const bruce::Error& error) {
        return error.what();
    }
    throw bruce::Error("Expected management inspection error");
}
}

int main() {
    try {
        using namespace bruce;
        ManagementInspectionControl unbounded;
        require(unbounded.report() ==
                    "{\"model\":\"management-checkpoints-v1\",\"max_checkpoints\":null,\"checkpoints\":0}",
                "Unbounded report changed");

        ManagementInspectionControl bounded;
        bounded.maximum = 2;
        auto callback = bounded.callback();
        callback();
        callback();
        require(bounded.used() == 2, "Checkpoint count changed");
        require(error_of([&] { bounded.visit(); }) ==
                    "ManagementCheckpointBudgetExceeded: checkpoints=2 limit=2",
                "Budget diagnostic or precedence changed");

        ManagementInspectionControl cancelled;
        cancelled.maximum = 0;
        cancelled.cancelled = [] { return true; };
        require(error_of([&] { cancelled.visit(); }) == "ManagementInspectionCancelled",
                "Cancellation did not win over zero budget");

        ManagementInspectionControl exhausted;
        exhausted.maximum = 0;
        exhausted.cancelled = [] { return false; };
        require(error_of([&] { exhausted.visit(); }) ==
                    "ManagementCheckpointBudgetExceeded: checkpoints=0 limit=0",
                "Zero budget diagnostic changed");

        ManagementInspectionControl maximum;
        maximum.maximum = std::numeric_limits<std::uint64_t>::max();
        maximum.visit();
        require(maximum.used() == 1, "Maximum uint64 budget rejected");
        std::cout << "Management inspection priority, usage and counter bounds verified\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
