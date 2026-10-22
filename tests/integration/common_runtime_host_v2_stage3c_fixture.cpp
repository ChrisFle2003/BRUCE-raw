#include "../../apps/dag_scheduler_v5_cli.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Test-only worker executable: exercise the real CLI, framing, supervisor,
// Batch Host, filesystem, and reports with deterministic worker outcomes.
int main(int argc, char** argv) {
    using namespace bruce;
    using namespace bruce::worker_v1;
    if (argc != 3 || std::string_view(argv[1]) != "worker-v1" ||
        std::string_view(argv[2]) != "child")
        return dag_scheduler_v5_cli(argc, argv);

    const auto request = decode_request_payload(decode_frame(
        dag_scheduler_v5_cli_detail::read_request_frame(), 1048576).payload);
    const bool first = request.attempt_id == "a-1" || request.attempt_id.ends_with("-1");
    const auto output = std::filesystem::path("attempts") / request.attempt_id / "output";
    std::ofstream(output / "fixture-started").put('1');
    if (request.node_id == "cancel_before_retry" && first) ::usleep(200000);
    if (request.node_id == "always_crash" ||
        (first && (request.node_id == "crash_once" || request.node_id == "slow_retry" ||
                   request.node_id == "cancel_before_retry")))
        std::_Exit(9);
    if (first && request.node_id == "protocol_once") {
        const std::string malformed = "not-a-worker-frame\n";
        (void)::write(STDOUT_FILENO, malformed.data(), malformed.size());
        return 0;
    }
    if (request.node_id == "slow_retry" || request.node_id == "a_slow") ::usleep(200000);
    WorkerResult result;
    result.status = WorkerStatus::READY;
    result.failure_code = FailureCode::READY;
    result.result_code = request.adapter_id == "join-v1" ? "JOINED_READY" :
        (request.adapter_id == "append-v1" ? "APPENDED_READY" : "LITERAL_READY");
    if (request.node_id == "failed") {
        result.status = WorkerStatus::FAILED;
        result.failure_code = FailureCode::FAILED;
        result.result_code = "FAILED";
    } else {
        const std::string payload = "OK";
        std::ofstream(output / "result.bin", std::ios::binary) << payload;
        if (::chmod((output / "result.bin").c_str(), 0600) != 0) return 2;
        result.payload_size = payload.size();
        result.payload_digest = sha256(payload);
        result.output_artifact_ref = ArtifactRef{"result", payload.size(), *result.payload_digest};
        result.usage.output_bytes = payload.size();
    }
    const auto bytes = encode_frame(FrameType::RESULT, encode_result_payload(result));
    std::size_t offset{};
    while (offset < bytes.size()) {
        const auto count = ::write(STDOUT_FILENO, bytes.data() + offset, bytes.size() - offset);
        if (count <= 0) return 2;
        offset += static_cast<std::size_t>(count);
    }
    return 0;
}
