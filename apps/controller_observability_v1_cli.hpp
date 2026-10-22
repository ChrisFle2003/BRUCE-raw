#pragma once

#include "bruce/observability/controller_observability_cli_v1.hpp"
#include "dag_scheduler_v5_cli.hpp"
#include "bruce/observability/controller_observability_crosswalk_v1.hpp"
#include <iostream>
#include <span>
#include <vector>

namespace bruce {
namespace controller_observability_v1_cli_detail {
inline bool valid_path(std::string_view value) {
    return !value.empty() && !value.starts_with("--") && value.find_first_not_of(" \t\r\n\v\f") != std::string_view::npos;
}
inline std::string render(const ControllerObservationV1& value) {
    return render_controller_observation_v1(value) + "\n";
}
}
inline int controller_observability_v1_cli(int argc, char** argv) {
    using namespace controller_observability_v1_cli_detail;
    if (argc != 4 || !valid_path(argv[3]) || (std::string_view(argv[2]) != "v5" && std::string_view(argv[2]) != "v6")) {
        std::cerr << render_controller_observability_error_v1(); return 2;
    }
    try {
        if (std::string_view(argv[2]) == "v5") {
            const auto bundle = dag_scheduler_v5_cli_detail::normalized_inspect_bundle_path(argv[3]);
            const auto bytes = dag_scheduler_v5_cli_detail::inspect_bundle_file(bundle);
            const auto run_root = std::filesystem::path(bundle.string() + ".run");
            const auto inspection = worker_v1::inspect_worker_run_v1(run_root);
            // This existing V5 receipt checker binds the supplied BUNDLE bytes
            // to the sibling run evidence before the redacted projection.
            (void)dag_scheduler_v5_cli_detail::inspect_execute_report(bytes, run_root, inspection);
            std::cout << render(observe_v5_worker_run_v1(inspection));
        } else {
            std::cout << render(observe_logic_mod_artifact_dag_v6(read_controller_observability_v6_record_v1(argv[3])));
        }
        return 0;
    } catch (...) { std::cerr << render_controller_observability_error_v1(); return 2; }
}
} // namespace bruce
