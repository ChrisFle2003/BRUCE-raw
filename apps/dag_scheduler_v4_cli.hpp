#pragma once

#include "dag_scheduler_v3_cli.hpp"
#include "bruce/dag_scheduler/dag_scheduler_v4.hpp"

namespace bruce {
namespace dag_scheduler_v4_cli_detail {

inline std::string render(const DagSchedulerRunV2& run, std::string_view action,
                          std::string_view bundle_digest) {
    std::string report = "{\n  \"contract\": " +
        json_string("dag-scheduler-v4-" + std::string(action) + "-v1") +
        ",\n  \"action\": " + json_string(action) +
        ",\n  \"bundle_digest\": " + json_string(bundle_digest) +
        ",\n  \"scheduler_version\": " + json_string(run.spec.scheduler_version) +
        ",\n  \"dag_id\": " + json_string(run.spec.dag_id) +
        ",\n  \"max_started_nodes\": " + std::to_string(run.spec.max_started_nodes) +
        ",\n  \"max_total_receipt_bytes\": " + std::to_string(run.spec.max_total_receipt_bytes) +
        ",\n  \"receipts\": [";
    for (std::size_t i = 0; i < run.receipts.size(); ++i) {
        const auto& receipt = run.receipts[i];
        const auto node = std::find_if(run.spec.nodes.begin(), run.spec.nodes.end(),
            [&receipt](const auto& spec_node) { return spec_node.node_id == receipt.node_id; });
        if (node == run.spec.nodes.end()) throw Error("MissingDagSchedulerV4Node");
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
        report += "], \"outcome\": " + json_string(dag_scheduler_v3_cli_detail::outcome(receipt.outcome)) +
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
            json_string(dag_scheduler_v3_cli_detail::event(entry.event)) + ", \"node_id\": " + json_string(entry.node_id) + '}';
    }
    return report + "\n  ]\n}";
}

inline bool path_argument(const char* value) {
    const std::string_view path(value);
    return !path.empty() && !path.starts_with('-');
}

} // namespace dag_scheduler_v4_cli_detail

inline int dag_scheduler_v4_cli(int argc, char** argv) {
    const std::string_view action = argc > 2 ? argv[2] : "";
    const bool execute = action == "execute";
    const bool valid = execute ?
        argc == 6 && dag_scheduler_v4_cli_detail::path_argument(argv[3]) &&
            std::string_view(argv[4]) == "--output" &&
            dag_scheduler_v4_cli_detail::path_argument(argv[5]) :
        argc == 4 && (action == "inspect" || action == "replay") &&
            dag_scheduler_v4_cli_detail::path_argument(argv[3]);
    if (!valid) {
        std::cerr << "{\"error\":\"Usage: bruce dag-scheduler-v4 execute SPEC --output BUNDLE | inspect BUNDLE | replay BUNDLE\"}\n";
        return 2;
    }
    DagSchedulerRunV2 run;
    std::string bundle;
    if (execute) {
        const auto parsed = dag_scheduler_v3_cli_detail::parse_spec(argv[3]);
        run = dag_scheduler_v3_cli_detail::execute(parsed);
        bundle = pack_dag_scheduler_run_v4(run);
    } else {
        bundle = LocalFiles::read(argv[3], dag_scheduler_v4_max_bundle_bytes);
        run = action == "inspect" ?
            inspect_dag_scheduler_run_v4(bundle, dag_scheduler_v3_cli_detail::registry) :
            replay_dag_scheduler_run_v4(bundle, dag_scheduler_v3_cli_detail::registry);
    }
    // Both encoder and decoder verified the envelope before its digest is used.
    const auto digest = std::string_view(bundle).substr(bundle.find('\n') + 1, 64);
    const auto report = dag_scheduler_v4_cli_detail::render(run, action, digest);
    if (execute) LocalFiles::publish_new_atomically(argv[5], bundle);
    std::cout << report << '\n';
    return 0;
}

} // namespace bruce
