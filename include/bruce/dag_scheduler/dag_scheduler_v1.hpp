#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct DagSchedulerNodeSpecV1 {
    std::string node_id;
    std::string callback_id;
    std::vector<std::string> dependencies;
};

struct DagSchedulerSpecV1 {
    std::string scheduler_version{"dag-scheduler-v1"};
    std::string dag_id;
    std::vector<DagSchedulerNodeSpecV1> nodes;
    std::uint64_t max_started_nodes{};
};

struct DagSchedulerInputV1 {
    std::string producer_node_id;
    std::string result_code;
};

enum class DagSchedulerCallbackStatusV1 { ready, failed };
struct DagSchedulerCallbackResultV1 {
    DagSchedulerCallbackStatusV1 status{};
    std::string result_code;
};

using DagSchedulerCallbackFnV1 = DagSchedulerCallbackResultV1 (*)(
    std::span<const DagSchedulerInputV1>);
struct DagSchedulerCallbackEntryV1 {
    std::string_view callback_id;
    DagSchedulerCallbackFnV1 execute{};
};
using DagSchedulerCallbackRegistryV1 = std::span<const DagSchedulerCallbackEntryV1>;

enum class DagSchedulerReceiptOutcomeV1 { ready, failed, blocked, cancelled };
struct DagSchedulerReceiptV1 {
    std::string node_id;
    std::string callback_id;
    std::vector<DagSchedulerInputV1> input_refs;
    DagSchedulerReceiptOutcomeV1 outcome{};
    std::string terminal_code;
};

enum class DagSchedulerTraceEventV1 {
    validated, started, ready, failed, blocked, cancelled, completed
};
struct DagSchedulerTraceEntryV1 {
    DagSchedulerTraceEventV1 event;
    std::string node_id;
};

struct DagSchedulerControlV1 {
    std::function<bool()> cancelled;
};
struct DagSchedulerRunV1 {
    DagSchedulerSpecV1 spec;
    std::vector<DagSchedulerReceiptV1> receipts;
    std::vector<DagSchedulerTraceEntryV1> trace;
};

void validate_dag_scheduler_spec_v1(
    const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1);
DagSchedulerRunV1 execute_dag_scheduler_v1(
    const DagSchedulerSpecV1&, DagSchedulerCallbackRegistryV1,
    const DagSchedulerControlV1& = {});

}
