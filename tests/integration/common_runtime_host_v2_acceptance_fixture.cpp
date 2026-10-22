#include "bruce/application/common_runtime_host_v2_acceptance.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

bruce::worker_v1::WorkerResult ready_result() {
    using namespace bruce;
    using namespace bruce::worker_v1;
    WorkerResult result;
    result.status = WorkerStatus::READY;
    result.failure_code = FailureCode::READY;
    result.result_code = "READY";
    result.usage.operations = 1;
    result.usage.output_bytes = 2;
    result.output_artifact_ref = ArtifactRef{"result", 2, sha256("ok")};
    result.payload_digest = sha256("ok");
    result.payload_size = 2;
    return result;
}

int publish_cancel(const std::filesystem::path& root) {
    const auto result = bruce::worker_v1::request_abort(root);
    std::cout << (result == bruce::worker_v1::AbortRequestResult::REQUESTED ?
                      "REQUESTED\n" : "ALREADY_REQUESTED\n");
    return 0;
}

int wait_at_boundary(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    (void)bruce::worker_v1::WorkerRunLog::create(root, "acceptance-fixture");

    // The line is the externally observable boundary.  No acceptance probe
    // occurs until the controller acknowledges that it has published the
    // normal worker-abort marker through the public protocol.
    std::cout << "ACCEPTANCE_BOUNDARY_V1\n" << std::flush;
    std::string command;
    if (!std::getline(std::cin, command) || command != "continue") return 2;

    const auto decision = bruce::decide_common_runtime_host_v2_acceptance(
        root, ready_result());
    const bool cancelled =
        decision.disposition == bruce::CommonRuntimeHostV2AcceptanceDisposition::CANCEL &&
        decision.result.status == bruce::worker_v1::WorkerStatus::CANCELLED &&
        decision.result.failure_code == bruce::worker_v1::FailureCode::CANCELLED;
    std::cout << "{\"disposition\":\"" << (cancelled ? "CANCEL" : "OTHER")
              << "\",\"host_exit_code\":" << decision.host_exit_code
              << ",\"append_cancel_requested\":"
              << (decision.append_cancel_requested ? "true" : "false")
              << ",\"append_artifact_accepted\":"
              << (decision.append_artifact_accepted ? "true" : "false")
              << ",\"has_reference\":"
              << (decision.result.output_artifact_ref ? "true" : "false")
              << ",\"has_digest\":"
              << (decision.result.payload_digest ? "true" : "false")
              << ",\"payload_size\":" << decision.result.payload_size << "}\n";
    return cancelled ? 1 : 3;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) return 2;
        const std::string action = argv[1];
        if (action == "cancel") return publish_cancel(argv[2]);
        if (action == "boundary") return wait_at_boundary(argv[2]);
        return 2;
    } catch (const std::exception&) {
        return 2;
    }
}
