#include "../../apps/dag_scheduler_v5_cli.hpp"

#include <fstream>
#include <unistd.h>

// Only the worker failure timing is synthetic. Successful workers execute the
// production Append/Join/Literal adapters with real staged payload inputs.
int main(int argc, char** argv) {
    using namespace bruce;
    using namespace bruce::worker_v1;
    if (argc == 3 && std::string_view(argv[1]) == "fixture-inspect") {
        try {
            std::cout << dag_scheduler_v5_cli_detail::render_inspection_report(
                inspect_worker_run_v1(argv[2]));
            return 0;
        } catch (...) { return 2; }
    }
    if (argc != 3 || std::string_view(argv[1]) != "worker-v1" ||
        std::string_view(argv[2]) != "child")
    {
        const auto pid = std::to_string(::getpid());
        ::setenv("BRUCE_TEST_STAGE3D_HOST_PID", pid.c_str(), 1);
        return dag_scheduler_v5_cli(argc, argv);
    }
    const auto request = decode_request_payload(decode_frame(
        dag_scheduler_v5_cli_detail::read_request_frame(), 1048576).payload);
    const bool first = request.attempt_id == "a-1" || request.attempt_id.ends_with("-1");
    const auto output = std::filesystem::path("attempts") / request.attempt_id / "output";
    std::ofstream(output / "fixture-started").put('1');
    if (request.node_id == "cancel_before_retry" && first) ::usleep(200000);
    if (request.node_id == "always_crash" ||
        (first && (request.node_id == "crash_once" || request.node_id == "retry_consumer" ||
                   request.node_id == "slow_retry" || request.node_id == "cancel_before_retry")))
        std::_Exit(9);
    if (request.node_id == "slow_retry" || request.node_id == "a_slow") ::usleep(200000);
    WorkerResult result;
    try { result = dag_scheduler_v5_cli_detail::execute_child_request(request); }
    catch (const Error& error) {
        result = dag_scheduler_v5_cli_detail::failed_child_result(
            dag_scheduler_v5_cli_detail::child_error_code(error.what()));
    }
    dag_scheduler_v5_cli_detail::write_all(STDOUT_FILENO,
        encode_frame(FrameType::RESULT, encode_result_payload(result)));
    return 0;
}
