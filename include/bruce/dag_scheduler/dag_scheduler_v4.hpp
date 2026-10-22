#pragma once

#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include <cstddef>

namespace bruce {

inline constexpr std::size_t dag_scheduler_v4_max_bundle_bytes = 5 * 1024 * 1024;

// The supplied registry is trusted code: exactly append-v1, join-v1, literal-v1
// in that order. Bundle data can never introduce additional callbacks.
std::string pack_dag_scheduler_run_v4(const DagSchedulerRunV2&);
DagSchedulerRunV2 inspect_dag_scheduler_run_v4(
    std::string_view, DagSchedulerCallbackRegistryV2);
DagSchedulerRunV2 replay_dag_scheduler_run_v4(
    std::string_view, DagSchedulerCallbackRegistryV2);

}
