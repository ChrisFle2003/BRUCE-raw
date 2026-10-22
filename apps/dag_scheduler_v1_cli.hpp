#pragma once

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/dag_scheduler/dag_scheduler_v1.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace bruce {
namespace dag_scheduler_v1_cli_detail {

// The public callback is a function pointer. Bind it to a run-local flag only
// for the synchronous run, restoring an earlier binding even if execution throws.
inline thread_local bool* cancellation_target = nullptr;
struct CancellationBinding {
    bool* previous;
    explicit CancellationBinding(bool& flag) : previous(cancellation_target) {
        cancellation_target = &flag;
    }
    ~CancellationBinding() { cancellation_target = previous; }
    CancellationBinding(const CancellationBinding&) = delete;
    CancellationBinding& operator=(const CancellationBinding&) = delete;
};

inline DagSchedulerCallbackResultV1 root(std::span<const DagSchedulerInputV1>) {
    return {DagSchedulerCallbackStatusV1::ready, "ROOT_READY"};
}
inline DagSchedulerCallbackResultV1 cancel_root(std::span<const DagSchedulerInputV1> inputs) {
    if (!cancellation_target) throw Error("MissingDagSchedulerDemoCancellationBinding");
    *cancellation_target = true;
    return root(inputs);
}
inline DagSchedulerCallbackResultV1 fail(std::span<const DagSchedulerInputV1>) {
    return {DagSchedulerCallbackStatusV1::failed, "EXPECTED_FAILURE"};
}
inline DagSchedulerCallbackResultV1 finish(std::span<const DagSchedulerInputV1>) {
    return {DagSchedulerCallbackStatusV1::ready, "FINISHED"};
}
inline DagSchedulerCallbackResultV1 independent(std::span<const DagSchedulerInputV1>) {
    return {DagSchedulerCallbackStatusV1::ready, "INDEPENDENT_READY"};
}
inline DagSchedulerCallbackResultV1 must_not_start(std::span<const DagSchedulerInputV1>) {
    throw Error("UnexpectedDagSchedulerDemoCallbackStart");
}

inline DagSchedulerRunV1 demo(std::string_view scenario) {
    if (scenario == "mixed") {
        const std::array<DagSchedulerCallbackEntryV1, 5> catalog{{
            {"fail-v1", fail}, {"finish-v1", finish},
            {"independent-ready-v1", independent}, {"must-not-start-v1", must_not_start},
            {"root-ready-v1", root}}};
        const DagSchedulerSpecV1 spec{"dag-scheduler-v1", "mixed-v1", {
            {"a-root", "root-ready-v1", {}},
            {"b-fail", "fail-v1", {"a-root"}},
            {"c-independent", "independent-ready-v1", {}},
            {"d-blocked", "must-not-start-v1", {"b-fail"}},
            {"e-finish", "finish-v1", {"c-independent"}}}, 5};
        return execute_dag_scheduler_v1(spec, catalog);
    }
    if (scenario == "budget") {
        const std::array<DagSchedulerCallbackEntryV1, 2> catalog{{
            {"must-not-start-v1", must_not_start}, {"root-ready-v1", root}}};
        const DagSchedulerSpecV1 spec{"dag-scheduler-v1", "budget-v1", {
            {"a-root", "root-ready-v1", {}},
            {"b-child", "must-not-start-v1", {"a-root"}},
            {"c-independent", "must-not-start-v1", {}}}, 1};
        return execute_dag_scheduler_v1(spec, catalog);
    }
    const std::array<DagSchedulerCallbackEntryV1, 2> catalog{{
        {"cancel-root-v1", cancel_root}, {"must-not-start-v1", must_not_start}}};
    const DagSchedulerSpecV1 spec{"dag-scheduler-v1", "cancelled-v1", {
        {"a-root", "cancel-root-v1", {}},
        {"b-child", "must-not-start-v1", {"a-root"}},
        {"c-independent", "must-not-start-v1", {}}}, 3};
    bool cancelled = false;
    const CancellationBinding binding(cancelled);
    return execute_dag_scheduler_v1(spec, catalog, {[&cancelled] { return cancelled; }});
}

inline std::string_view outcome(DagSchedulerReceiptOutcomeV1 value) {
    switch (value) {
        case DagSchedulerReceiptOutcomeV1::ready: return "READY";
        case DagSchedulerReceiptOutcomeV1::failed: return "FAILED";
        case DagSchedulerReceiptOutcomeV1::blocked: return "BLOCKED";
        case DagSchedulerReceiptOutcomeV1::cancelled: return "CANCELLED";
    }
    throw Error("InvalidDagSchedulerDemoOutcome");
}
inline std::string_view event(DagSchedulerTraceEventV1 value) {
    switch (value) {
        case DagSchedulerTraceEventV1::validated: return "VALIDATED";
        case DagSchedulerTraceEventV1::started: return "STARTED";
        case DagSchedulerTraceEventV1::ready: return "READY";
        case DagSchedulerTraceEventV1::failed: return "FAILED";
        case DagSchedulerTraceEventV1::blocked: return "BLOCKED";
        case DagSchedulerTraceEventV1::cancelled: return "CANCELLED";
        case DagSchedulerTraceEventV1::completed: return "COMPLETED";
    }
    throw Error("InvalidDagSchedulerDemoTraceEvent");
}

inline std::string render(std::string_view scenario, const DagSchedulerRunV1& run) {
    std::string report = "{\n  \"contract\": " + json_string("dag-scheduler-v1-cli-v1") +
        ",\n  \"scenario\": " + json_string(scenario) +
        ",\n  \"scheduler_version\": " + json_string(run.spec.scheduler_version) +
        ",\n  \"dag_id\": " + json_string(run.spec.dag_id) +
        ",\n  \"max_started_nodes\": " + std::to_string(run.spec.max_started_nodes) +
        ",\n  \"receipts\": [";
    for (std::size_t i = 0; i < run.receipts.size(); ++i) {
        const auto& receipt = run.receipts[i];
        report += (i ? ",\n    " : "\n    ") + std::string("{\"node_id\": ") +
            json_string(receipt.node_id) + ", \"callback_id\": " +
            json_string(receipt.callback_id) + ", \"input_refs\": [";
        for (std::size_t j = 0; j < receipt.input_refs.size(); ++j) {
            const auto& input = receipt.input_refs[j];
            if (j) report += ", ";
            report += "{\"producer_node_id\": " + json_string(input.producer_node_id) +
                ", \"result_code\": " + json_string(input.result_code) + '}';
        }
        report += "], \"outcome\": " + json_string(outcome(receipt.outcome)) +
            ", \"terminal_code\": " + json_string(receipt.terminal_code) + '}';
    }
    report += "\n  ],\n  \"trace\": [";
    for (std::size_t i = 0; i < run.trace.size(); ++i) {
        const auto& entry = run.trace[i];
        report += (i ? ",\n    " : "\n    ") + std::string("{\"event\": ") +
            json_string(event(entry.event)) + ", \"node_id\": " + json_string(entry.node_id) + '}';
    }
    return report + "\n  ]\n}";
}

} // namespace dag_scheduler_v1_cli_detail

inline int dag_scheduler_v1_cli(int argc, char** argv) {
    if (argc != 4 || std::string_view(argv[2]) != "demo" ||
        (std::string_view(argv[3]) != "mixed" && std::string_view(argv[3]) != "budget" &&
         std::string_view(argv[3]) != "cancelled")) {
        std::cerr << "{\"error\": " <<
            json_string("Usage: bruce dag-scheduler-v1 demo mixed|budget|cancelled") << "}\n";
        return 2;
    }
    const std::string_view scenario = argv[3];
    const auto run = dag_scheduler_v1_cli_detail::demo(scenario);
    std::cout << dag_scheduler_v1_cli_detail::render(scenario, run) << '\n';
    return 0;
}

} // namespace bruce
