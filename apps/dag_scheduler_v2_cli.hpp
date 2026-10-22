#pragma once

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/dag_scheduler/dag_scheduler_v2.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace bruce {
namespace dag_scheduler_v2_cli_detail {

inline DagSchedulerCallbackResultV2 ready(std::string code, std::string payload) {
    auto digest = sha256(payload);
    return {DagSchedulerCallbackStatusV2::ready, std::move(code),
            std::move(payload), std::move(digest)};
}

inline void require_input(const DagSchedulerInputV2& input, std::string_view producer,
                          std::string_view code, std::string_view payload) {
    if (input.producer_node_id != producer || input.result_code != code ||
        input.payload_bytes != payload || input.payload_digest != sha256(payload))
        throw Error("InvalidDagSchedulerV2DemoInput");
}

inline DagSchedulerCallbackResultV2 source(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (request != "source-request" || !inputs.empty())
        throw Error("InvalidDagSchedulerV2DemoSource");
    return ready("SOURCE_READY", "alpha");
}
inline DagSchedulerCallbackResultV2 transform(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (request != "transform-request" || inputs.size() != 1)
        throw Error("InvalidDagSchedulerV2DemoTransform");
    require_input(inputs[0], "a-source", "SOURCE_READY", "alpha");
    return ready("TRANSFORM_READY", inputs[0].payload_bytes + ":beta");
}
inline DagSchedulerCallbackResultV2 independent(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (request != "independent-request" || !inputs.empty())
        throw Error("InvalidDagSchedulerV2DemoIndependent");
    return ready("INDEPENDENT_READY", "gamma");
}
inline DagSchedulerCallbackResultV2 join(
        std::string_view request, std::span<const DagSchedulerInputV2> inputs) {
    if (request != "join-request" || inputs.size() != 2)
        throw Error("InvalidDagSchedulerV2DemoJoin");
    require_input(inputs[0], "b-transform", "TRANSFORM_READY", "alpha:beta");
    require_input(inputs[1], "c-independent", "INDEPENDENT_READY", "gamma");
    return ready("JOIN_READY", inputs[0].payload_bytes + '|' + inputs[1].payload_bytes);
}

inline DagSchedulerRunV2 demo() {
    const std::array<DagSchedulerCallbackEntryV2, 4> catalog{{
        {"independent-ready-v2", independent}, {"join-ready-v2", join},
        {"source-ready-v2", source}, {"transform-ready-v2", transform}}};
    const DagSchedulerSpecV2 spec{"dag-scheduler-v2", "flow-v2", {
        {"a-source", "source-ready-v2", {}, "source-request", sha256("source-request")},
        {"b-transform", "transform-ready-v2", {"a-source"},
         "transform-request", sha256("transform-request")},
        {"c-independent", "independent-ready-v2", {},
         "independent-request", sha256("independent-request")},
        {"d-join", "join-ready-v2", {"c-independent", "b-transform"},
         "join-request", sha256("join-request")}}, 4, 4096};
    return execute_dag_scheduler_v2(spec, catalog);
}

inline std::string_view outcome(DagSchedulerReceiptOutcomeV2 value) {
    switch (value) {
        case DagSchedulerReceiptOutcomeV2::ready: return "READY";
        case DagSchedulerReceiptOutcomeV2::failed: return "FAILED";
        case DagSchedulerReceiptOutcomeV2::blocked: return "BLOCKED";
        case DagSchedulerReceiptOutcomeV2::cancelled: return "CANCELLED";
    }
    throw Error("InvalidDagSchedulerV2DemoOutcome");
}
inline std::string_view event(DagSchedulerTraceEventV2 value) {
    switch (value) {
        case DagSchedulerTraceEventV2::validated: return "VALIDATED";
        case DagSchedulerTraceEventV2::started: return "STARTED";
        case DagSchedulerTraceEventV2::ready: return "READY";
        case DagSchedulerTraceEventV2::failed: return "FAILED";
        case DagSchedulerTraceEventV2::blocked: return "BLOCKED";
        case DagSchedulerTraceEventV2::cancelled: return "CANCELLED";
        case DagSchedulerTraceEventV2::completed: return "COMPLETED";
    }
    throw Error("InvalidDagSchedulerV2DemoTraceEvent");
}

inline std::string render(const DagSchedulerRunV2& run) {
    std::string report = "{\n  \"contract\": " + json_string("dag-scheduler-v2-cli-v1") +
        ",\n  \"scenario\": " + json_string("flow") +
        ",\n  \"scheduler_version\": " + json_string(run.spec.scheduler_version) +
        ",\n  \"dag_id\": " + json_string(run.spec.dag_id) +
        ",\n  \"max_started_nodes\": " + std::to_string(run.spec.max_started_nodes) +
        ",\n  \"max_total_receipt_bytes\": " + std::to_string(run.spec.max_total_receipt_bytes) +
        ",\n  \"receipts\": [";
    for (std::size_t i = 0; i < run.receipts.size(); ++i) {
        const auto& receipt = run.receipts[i];
        const auto node = std::find_if(run.spec.nodes.begin(), run.spec.nodes.end(),
            [&receipt](const auto& spec_node) { return spec_node.node_id == receipt.node_id; });
        if (node == run.spec.nodes.end()) throw Error("MissingDagSchedulerV2DemoNode");
        report += (i ? ",\n    " : "\n    ") + std::string("{\"node_id\": ") +
            json_string(receipt.node_id) + ", \"callback_id\": " +
            json_string(receipt.callback_id) + ", \"request_size\": " +
            std::to_string(node->request_bytes.size()) + ", \"request_digest\": " +
            json_string(receipt.request_digest) + ", \"input_refs\": [";
        for (std::size_t j = 0; j < receipt.input_refs.size(); ++j) {
            const auto& input = receipt.input_refs[j];
            if (j) report += ", ";
            report += "{\"producer_node_id\": " + json_string(input.producer_node_id) +
                ", \"result_code\": " + json_string(input.result_code) +
                ", \"payload_size\": " + std::to_string(input.payload_bytes.size()) +
                ", \"payload_digest\": " + json_string(input.payload_digest) + '}';
        }
        report += "], \"outcome\": " + json_string(outcome(receipt.outcome)) +
            ", \"failure_code\": " + json_string(receipt.failure_code) + ", \"output\": ";
        if (receipt.output) {
            const auto& output = *receipt.output;
            report += "{\"output_version\": " + json_string(output.output_version) +
                ", \"result_code\": " + json_string(output.result_code) +
                ", \"payload_size\": " + std::to_string(output.payload_bytes.size()) +
                ", \"payload_digest\": " + json_string(output.payload_digest) + '}';
        } else {
            report += "null";
        }
        report += '}';
    }
    report += "\n  ],\n  \"trace\": [";
    for (std::size_t i = 0; i < run.trace.size(); ++i) {
        const auto& entry = run.trace[i];
        report += (i ? ",\n    " : "\n    ") + std::string("{\"event\": ") +
            json_string(event(entry.event)) + ", \"node_id\": " + json_string(entry.node_id) + '}';
    }
    return report + "\n  ]\n}";
}

} // namespace dag_scheduler_v2_cli_detail

inline int dag_scheduler_v2_cli(int argc, char** argv) {
    if (argc != 4 || std::string_view(argv[2]) != "demo" ||
        std::string_view(argv[3]) != "flow") {
        std::cerr << "{\"error\":" << json_string("Usage: bruce dag-scheduler-v2 demo flow") << "}\n";
        return 2;
    }
    const auto run = dag_scheduler_v2_cli_detail::demo();
    std::cout << dag_scheduler_v2_cli_detail::render(run) << '\n';
    return 0;
}

} // namespace bruce
