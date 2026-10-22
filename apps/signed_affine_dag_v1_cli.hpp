#pragma once

#include "bruce/core/json.hpp"
#include "bruce/signed_affine_dag/signed_affine_dag_v1.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace bruce {
namespace signed_affine_dag_v1_cli_detail {

inline SignedAffineDagSpecV1 demo_spec(bool cancellation) {
    return {{{"z-double", "source", "double-v1", std::nullopt, cancellation ? -1 : 1},
             {"source", std::nullopt, "", 2, 1},
             {"a-add", "source", "add-two-v1", std::nullopt, 1}}};
}

inline std::string render(std::string_view scenario, const SignedAffineDagResultV1& result) {
    std::string report = "{\n  \"contract\": " + json_string("signed-affine-dag-v1-cli-v1") +
        ",\n  \"scenario\": " + json_string(scenario) + ",\n  \"states\": [";
    for (std::size_t i = 0; i < result.states.size(); ++i) {
        const auto& state = result.states[i];
        if (i) report += ',';
        report += "\n    {\"value\": " + std::to_string(state.value) +
            ", \"weight_balance\": " + std::to_string(state.weight_balance) + ", \"paths\": [";
        for (std::size_t j = 0; j < state.paths.size(); ++j) {
            const auto& path = state.paths[j];
            if (j) report += ',';
            report += "\n      {\"node_id\": " + json_string(path.node_id) +
                ", \"predecessor_node_id\": " + (path.predecessor_node_id.empty() ?
                    "null" : json_string(path.predecessor_node_id)) +
                ", \"operator_id\": " + json_string(path.operator_id) +
                ", \"input_value\": " + std::to_string(path.input_value) +
                ", \"output_value\": " + std::to_string(path.output_value) +
                ", \"path_weight\": " + std::to_string(path.path_weight) + '}';
        }
        report += "\n    ]}";
    }
    return report + "\n  ]\n}";
}

} // namespace signed_affine_dag_v1_cli_detail

inline int signed_affine_dag_v1_cli(int argc, char** argv) {
    if (argc != 4 || std::string_view(argv[2]) != "demo" ||
        (std::string_view(argv[3]) != "merge" && std::string_view(argv[3]) != "cancellation"))
        throw Error("Usage: bruce signed-affine-dag-v1 demo merge|cancellation");
    const std::string_view scenario = argv[3];
    const auto result = execute_signed_affine_dag_v1(
        signed_affine_dag_v1_cli_detail::demo_spec(scenario == "cancellation"));
    std::cout << signed_affine_dag_v1_cli_detail::render(scenario, result) << '\n';
    return 0;
}

} // namespace bruce
