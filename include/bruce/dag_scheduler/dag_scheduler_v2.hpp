#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct DagSchedulerNodeSpecV2 {
    std::string node_id;
    std::string callback_id;
    std::vector<std::string> dependencies;
    std::string request_bytes;
    std::string request_digest;
};
struct DagSchedulerSpecV2 {
    std::string scheduler_version{"dag-scheduler-v2"};
    std::string dag_id;
    std::vector<DagSchedulerNodeSpecV2> nodes;
    std::uint64_t max_started_nodes{};
    std::uint64_t max_total_receipt_bytes{};
};
struct DagSchedulerInputV2 {
    std::string producer_node_id;
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};
enum class DagSchedulerCallbackStatusV2 { ready, failed };
struct DagSchedulerCallbackResultV2 {
    DagSchedulerCallbackStatusV2 status{};
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};
using DagSchedulerCallbackFnV2 = DagSchedulerCallbackResultV2 (*)(
    std::string_view request_bytes, std::span<const DagSchedulerInputV2>);
struct DagSchedulerCallbackEntryV2 {
    std::string_view callback_id;
    DagSchedulerCallbackFnV2 execute{};
};
using DagSchedulerCallbackRegistryV2 = std::span<const DagSchedulerCallbackEntryV2>;

struct DagSchedulerOutputRefV2 {
    std::string output_version{"dag-scheduler-output-v2"};
    std::string result_code;
    std::string payload_bytes;
    std::string payload_digest;
};
enum class DagSchedulerReceiptOutcomeV2 { ready, failed, blocked, cancelled };
struct DagSchedulerReceiptV2 {
    std::string node_id;
    std::string callback_id;
    std::string request_digest;
    std::vector<DagSchedulerInputV2> input_refs;
    DagSchedulerReceiptOutcomeV2 outcome{};
    std::optional<DagSchedulerOutputRefV2> output;
    std::string failure_code;
};
enum class DagSchedulerTraceEventV2 {
    validated, started, ready, failed, blocked, cancelled, completed
};
struct DagSchedulerTraceEntryV2 {
    DagSchedulerTraceEventV2 event;
    std::string node_id;
};
struct DagSchedulerControlV2 {
    std::function<bool()> cancelled;
};
struct DagSchedulerRunV2 {
    DagSchedulerSpecV2 spec;
    std::vector<DagSchedulerReceiptV2> receipts;
    std::vector<DagSchedulerTraceEntryV2> trace;
};

void validate_dag_scheduler_spec_v2(
    const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2);
DagSchedulerRunV2 execute_dag_scheduler_v2(
    const DagSchedulerSpecV2&, DagSchedulerCallbackRegistryV2,
    const DagSchedulerControlV2& = {});

}
