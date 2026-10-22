#include "../../apps/dag_scheduler_v5_cli.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Process { int status{}; std::string output; };
Process invoke(const std::string& executable, const std::vector<std::string>& arguments) {
    int fd[2]; if (::pipe(fd)) throw std::runtime_error("pipe");
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::close(fd[0]); ::dup2(fd[1], 1); ::dup2(fd[1], 2); ::close(fd[1]);
        std::vector<char*> argv{const_cast<char*>(executable.c_str())};
        for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr); ::execv(executable.c_str(), argv.data()); std::_Exit(127);
    }
    ::close(fd[1]); Process result; char chunk[4096]; ssize_t n;
    while ((n = ::read(fd[0], chunk, sizeof(chunk))) > 0) result.output.append(chunk, n);
    ::close(fd[0]); int status{}; ::waitpid(pid, &status, 0);
    result.status = WIFEXITED(status) ? WEXITSTATUS(status) : 128; return result;
}
void write(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream out(path, std::ios::binary); out.write(bytes.data(), bytes.size());
    require(static_cast<bool>(out), "write failed");
}
std::string run_real_child_control(const std::string& executable,
                                   std::string_view input) {
    int input_pipe[2], output_pipe[2];
    if (::pipe(input_pipe) || ::pipe(output_pipe)) throw std::runtime_error("child pipes");
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::dup2(input_pipe[0], STDIN_FILENO);
        ::dup2(output_pipe[1], STDOUT_FILENO);
        ::close(input_pipe[0]); ::close(input_pipe[1]);
        ::close(output_pipe[0]); ::close(output_pipe[1]);
        const char* args[] = {executable.c_str(), "worker-v1", "child", nullptr};
        ::execv(executable.c_str(), const_cast<char* const*>(args));
        std::_Exit(127);
    }
    ::close(input_pipe[0]); ::close(output_pipe[1]);
    std::size_t sent{};
    while (sent < input.size()) {
        const auto count = ::write(input_pipe[1], input.data() + sent, input.size() - sent);
        if (count <= 0) throw std::runtime_error("child input");
        sent += static_cast<std::size_t>(count);
    }
    ::close(input_pipe[1]);
    std::string output;
    char buffer[4096];
    for (ssize_t count{}; (count = ::read(output_pipe[0], buffer, sizeof(buffer))) > 0;)
        output.append(buffer, static_cast<std::size_t>(count));
    ::close(output_pipe[0]);
    int status{};
    ::waitpid(pid, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "real child failed");
    return output;
}
}
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "worker-v1" &&
        std::string_view(argv[2]) == "child") {
        using namespace bruce::worker_v1;
        const auto request_frame = decode_frame(bruce::dag_scheduler_v5_cli_detail::read_request_frame(), 1048576);
        const auto request = decode_request_payload(request_frame.payload);
        const auto payload = request.node_id == "named-node" ? request.node_id : "bad";
        write("attempts/a-1/output/result.bin", payload);
        WorkerResult result;
        result.status = WorkerStatus::READY;
        result.failure_code = FailureCode::READY;
        result.result_code = "READY";
        result.payload_size = payload.size();
        result.payload_digest = bruce::sha256(payload);
        result.output_artifact_ref = ArtifactRef{"result", payload.size(),
            request.node_id == "named-node" ? bruce::sha256(payload) : std::string(64, '0')};
        const auto frame = encode_frame(FrameType::RESULT, encode_result_payload(result));
        (void)::write(STDOUT_FILENO, frame.data(), frame.size());
        return 0;
    }
    require(argc == 2, "expected bruce executable");
    const std::string executable = std::filesystem::canonical(argv[1]);
    using namespace bruce::dag_scheduler_v5_cli_detail;
    require(usage == "Usage: bruce dag-scheduler-v5 execute SPEC --output BUNDLE [--retry N] [--max-wall-ms N] [--max-input-bytes N] [--max-output-bytes N] [--max-operations N] [--max-memory-bytes N] [--workers N] [--handoff-v1]", "usage text changed");
    auto defaults = parse_options({});
    require(defaults.retry == 0 && defaults.budget.max_wall_ms == 30000 &&
            defaults.budget.max_input_bytes == 65536 && defaults.budget.max_output_bytes == 65536 &&
            defaults.budget.max_operations == 1000000 && defaults.budget.max_memory_bytes == 268435456 &&
            defaults.workers == 1, "defaults changed");
    auto maxima = parse_options({"--retry", "3", "--max-wall-ms", "600000", "--max-input-bytes", "1048576", "--max-output-bytes", "5242880", "--max-operations", "10000000", "--max-memory-bytes", "2147483648", "--workers", "4"});
    require(maxima.retry == 3 && maxima.budget.max_wall_ms == 600000 && maxima.budget.max_input_bytes == 1048576 && maxima.budget.max_output_bytes == 5242880 && maxima.budget.max_operations == 10000000 && maxima.budget.max_memory_bytes == 2147483648ULL && maxima.workers == 4, "maxima changed");
    for (const auto& option : std::vector<std::pair<std::string,std::string>>{{"--retry","4"},{"--max-wall-ms","600001"},{"--max-input-bytes","1048577"},{"--max-output-bytes","5242881"},{"--max-operations","10000001"},{"--max-memory-bytes","2147483649"},{"--workers","5"}})
        for (const auto& invalid : {option.second, std::string("-1"), std::string("garbage")}) {
            bool rejected = false; try { (void)parse_options({option.first, invalid}); } catch (const bruce::Error&) { rejected = true; }
            require(rejected, "invalid option accepted");
        }
    {
        V5NodeState cancelled;
        mark_cancelled(cancelled);
        require(cancelled.terminal && !cancelled.accepted && cancelled.attempt_id.empty() &&
                cancelled.result.status == bruce::worker_v1::WorkerStatus::CANCELLED &&
                cancelled.result.failure_code == bruce::worker_v1::FailureCode::CANCELLED &&
                !retryable(cancelled.result.failure_code),
                "host cancellation state is retryable or leaks attempt metadata");
    }
    {
        using namespace bruce::worker_v1;
        WorkerRequest control_request;
        control_request.run_id = "run-control";
        control_request.attempt_id = "a-control";
        control_request.node_id = "control-node";
        control_request.adapter_id = "literal-v1";
        control_request.adapter_version = "1";
        control_request.request_digest = bruce::sha256("x");
        control_request.input_artifact_refs = {{"inputs/request.bin", 1, control_request.request_digest}};
        control_request.budget = defaults.budget;
        const auto wire = encode_frame(FrameType::REQUEST, encode_request_payload(control_request)) +
            encode_frame(FrameType::STATUS_QUERY, "") +
            encode_frame(FrameType::CANCEL, "");
        const auto output = run_real_child_control(executable, wire);
        std::size_t cursor{};
        const auto next_frame = [&]() {
            const auto separator = output.find("\n\n", cursor);
            require(separator != std::string::npos, "real child control frame missing");
            const auto marker = output.find("\npayload-bytes=", cursor);
            const auto begin = marker + std::string_view("\npayload-bytes=").size();
            const auto end = output.find('\n', begin);
            std::size_t size{};
            const auto [last, ec] = std::from_chars(output.data() + begin,
                output.data() + end, size);
            require(ec == std::errc{} && last == output.data() + end, "real child frame size");
            const auto total = separator + 2 + size;
            const auto frame = decode_frame(std::string_view(output).substr(cursor, total - cursor), 65536);
            cursor = total;
            return frame;
        };
        require(next_frame().type == FrameType::STATUS, "real child did not answer status query");
        require(next_frame().type == FrameType::ACK, "real child did not acknowledge cancel");
        const auto cancelled = decode_result_payload(next_frame().payload);
        require(cancelled.status == WorkerStatus::CANCELLED &&
                cancelled.failure_code == FailureCode::CANCELLED,
                "real child did not honor cooperative cancel");
        WorkerResult completed;
        completed.status = WorkerStatus::READY;
        completed.failure_code = FailureCode::READY;
        completed.result_code = "READY";
        const auto late_cancelled = finalize_child_result(completed, true);
        require(late_cancelled.status == WorkerStatus::CANCELLED &&
                late_cancelled.failure_code == FailureCode::CANCELLED,
                "late cancel did not override completed child result");
    }
    for (const auto& name : {"--max-wall-ms","--max-input-bytes","--max-output-bytes","--max-operations","--max-memory-bytes","--workers"}) {
        bool rejected = false; try { (void)parse_options({name,"0"}); } catch (const bruce::Error&) { rejected = true; }
        require(rejected, "zero budget accepted");
    }
    require(invoke(executable, {"--help"}).output.find("worker-v1 child") == std::string::npos, "internal route advertised");
    require(invoke(executable, {"--help"}).output.find(
        "bruce dag-scheduler-v5 execute SPEC --output BUNDLE [--retry N] [--max-wall-ms N]\n"
        "    [--max-input-bytes N] [--max-output-bytes N] [--max-operations N]\n"
        "    [--max-memory-bytes N] [--workers N] [--handoff-v1]\n"
        "bruce dag-scheduler-v5 inspect BUNDLE [--handoff-v1]\n") != std::string::npos,
        "V5 help text changed");
    const auto invalid_usage = invoke(executable, {"dag-scheduler-v5"});
    require(invalid_usage.status == 2 && invalid_usage.output.find(usage) != std::string::npos, "usage mismatch");
    std::string root_pattern = (std::filesystem::temp_directory_path() / "bruce-v5-unit-XXXXXX").string();
    require(::mkdtemp(root_pattern.data()) != nullptr, "cannot create test directory");
    const std::filesystem::path root(root_pattern);
    const auto spec = root / "spec"; const auto bundle = root / "bundle";
    write(spec, "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=one\nmax-started-nodes=1\nmax-total-receipt-bytes=4096\n\nnode=a\ndepends=\ncallback=literal-v1\nrequest-hex=73656e74696e656c2d7061796c6f6164\n");
    {
        std::vector<std::filesystem::path> occupied;
        for (unsigned number = 0; number < 16; ++number) {
            const auto path = root / (".bruce-handoff-v1-" + std::to_string(::getpid()) +
                                      "-" + std::to_string(number));
            write(path, "foreign-entry");
            occupied.push_back(path);
        }
        const auto collision_bundle = root / "collision-bundle";
        std::ostringstream captured;
        auto* original_stdout = std::cout.rdbuf(captured.rdbuf());
        bool rejected = false;
        try {
            (void)execute(spec, collision_bundle, defaults, executable,
                false, nullptr, nullptr, {}, nullptr, false, false, true);
        } catch (const bruce::Error& error) {
            rejected = std::string_view(error.what()) == "HANDOFF_RECORD_INVALID";
        }
        std::cout.rdbuf(original_stdout);
        require(rejected && captured.str().empty(),
                "exhausted temporary names did not fail with empty stdout");
        require(std::filesystem::exists(collision_bundle),
                "legacy report was lost after handoff adoption failure");
        for (const auto& path : occupied)
            require(bruce::LocalFiles::read(path.string(), 64) == "foreign-entry",
                    "exhausted temporary names removed a foreign entry");
        require(!std::filesystem::exists(collision_bundle.string() + ".handoff-v1.json"),
                "exhausted temporary names published a sidecar");
    }
    const auto run = invoke(executable, {"dag-scheduler-v5", "execute", spec.string(), "--output", bundle.string()});
    require(run.status == 0, "literal CLI failed");
    require(run.output.find("sentinel-payload") == std::string::npos, "payload leaked on stdout");
    require(run.output.find("\"reference\"") != std::string::npos && std::filesystem::exists(bundle), "final reference absent");
    require(run.output.find("\"retry\":0") != std::string::npos &&
            run.output.find("\"workers\":1") != std::string::npos &&
            run.output.find("\"max_wall_ms\":30000") != std::string::npos,
            "default metadata absent");
    const auto report = bruce::LocalFiles::read(bundle.string(), 65536);
    require(report.find("sentinel-payload") == std::string::npos, "payload leaked in bundle");
    require(!std::filesystem::exists(bundle.string() + ".handoff-v1.json"),
            "plain execute unexpectedly created a handoff sidecar");
    const auto handoff_bundle = root / "handoff-bundle";
    const auto handoff = invoke(executable, {"dag-scheduler-v5", "execute", spec.string(),
        "--output", handoff_bundle.string(), "--handoff-v1"});
    require(handoff.status == 0 && handoff.output == bruce::LocalFiles::read(handoff_bundle.string(), 65536),
            "handoff opt-in changed the public execute report");
    const auto handoff_sidecar = bruce::LocalFiles::read(handoff_bundle.string() + ".handoff-v1.json", 65536);
    require(handoff_sidecar.find("\"contract\":\"bruce-execute-handoff-v1\"") == 1,
            "handoff opt-in did not publish the canonical sidecar");
    const auto artifact = bundle.string() + ".run/attempts/a-1/output/result.bin";
    require(bruce::LocalFiles::read(artifact, 65536) == "sentinel-payload", "literal artifact incorrect");
    const auto log = bruce::LocalFiles::read(bundle.string() + ".run/control/run-log.jsonl", 65536);
    std::size_t prior{};
    for (const auto* event : {"RUN_CREATED", "ATTEMPT_STARTED", "RESULT_RECEIVED",
                               "ARTIFACT_ACCEPTED", "RUN_FINISHED"}) {
        const auto pos = log.find(std::string("\"event\":\"") + event + "\"", prior);
        require(pos != std::string::npos, "missing or unordered run event");
        prior = pos + 1;
    }
    require(log.find("sentinel-payload") == std::string::npos, "payload leaked to run log");
    require((std::filesystem::status(bundle.string() + ".run").permissions() &
             std::filesystem::perms::mask) == std::filesystem::perms::owner_all,
            "run root is not private");
    const auto inspected = invoke(executable, {"dag-scheduler-v5", "inspect", bundle.string()});
    require(inspected.status == 0 && inspected.output.find(
                "{\"contract\":\"dag-scheduler-v5-inspect-v1\",\"action\":\"inspect\",\"status\":\"VALID\"") == 0,
            "V5 inspect route did not return the stable valid report");
    const auto inspected_again = invoke(executable, {"dag-scheduler-v5", "inspect", bundle.string()});
    require(inspected.output == inspected_again.output, "V5 inspect report is not byte-stable");
    const auto inspect_usage = invoke(executable, {"dag-scheduler-v5", "inspect"});
    require(inspect_usage.status == 2 && inspect_usage.output ==
                "{\"error\":\"Usage: bruce dag-scheduler-v5 inspect BUNDLE [--handoff-v1]\"}\n",
            "V5 inspect usage contract changed");
    const auto cancel_live_bundle = root / "cancel-live-bundle";
    const auto cancel_live_run = std::filesystem::path(cancel_live_bundle.string() + ".run");
    std::filesystem::create_directories(cancel_live_run / "control");
    std::filesystem::create_directories(cancel_live_run / "inputs");
    std::filesystem::create_directories(cancel_live_run / "attempts");
    for (const auto& directory : {cancel_live_run, cancel_live_run / "control",
                                  cancel_live_run / "inputs", cancel_live_run / "attempts"})
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    auto cancel_log = bruce::worker_v1::WorkerRunLog::create(cancel_live_run, "run-cancel-live");
    bruce::worker_v1::WorkerLogEvent cancel_created;
    cancel_created.run_id = "run-cancel-live";
    cancel_created.seq = 1;
    cancel_created.ts_unix_ms = 1;
    cancel_created.event = "RUN_CREATED";
    cancel_created.status = "READY";
    cancel_log.append(cancel_created);
    const auto cancel_requested = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                       cancel_live_bundle.string()});
    require(cancel_requested.status == 0 && cancel_requested.output ==
                "{\"contract\":\"dag-scheduler-v5-cancel-v1\",\"action\":\"cancel\",\"status\":\"REQUESTED\"}\n",
            "cancel did not create the canonical request");
    require(bruce::LocalFiles::read((cancel_live_run / "control/cancel.request").string(), 128) ==
                bruce::worker_v1::kMarker,
            "cancel marker bytes changed");
    const auto cancel_again = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                   cancel_live_bundle.string()});
    require(cancel_again.status == 0 && cancel_again.output ==
                "{\"contract\":\"dag-scheduler-v5-cancel-v1\",\"action\":\"cancel\",\"status\":\"ALREADY_REQUESTED\"}\n",
            "cancel was not idempotent");
    const auto cancel_transient_bundle = root / "cancel-transient-bundle";
    const auto cancel_transient_run = std::filesystem::path(cancel_transient_bundle.string() + ".run");
    std::filesystem::create_directories(cancel_transient_run / "control");
    std::filesystem::create_directories(cancel_transient_run / "inputs");
    std::filesystem::create_directories(cancel_transient_run / "attempts");
    for (const auto& directory : {cancel_transient_run, cancel_transient_run / "control",
                                  cancel_transient_run / "inputs", cancel_transient_run / "attempts"})
        std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    auto transient_log = bruce::worker_v1::WorkerRunLog::create(
        cancel_transient_run, "run-cancel-transient");
    auto transient = cancel_created;
    transient.run_id = "run-cancel-transient";
    transient_log.append(transient);
    transient.seq = 2; transient.attempt_id = "a-1";
    transient.event = "ATTEMPT_STARTED"; transient.status = "RUNNING";
    transient_log.append(transient);
    transient.seq = 3; transient.event = "STATUS";
    transient_log.append(transient);
    transient.seq = 4; transient.event = "PROCESS_EXITED"; transient.status = "READY";
    transient_log.append(transient);
    const auto cancel_transient = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                       cancel_transient_bundle.string()});
    require(cancel_transient.status == 0 && cancel_transient.output ==
                "{\"contract\":\"dag-scheduler-v5-cancel-v1\",\"action\":\"cancel\",\"status\":\"REQUESTED\"}\n",
            "cancel rejected a durably logged live transition");
    const auto finished_bundle_bytes = bruce::LocalFiles::read(bundle.string(), 65536);
    bruce::LocalFiles::replace_atomically(bundle.string(), "{}\n");
    const auto cancel_invalid_finished = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                              bundle.string()});
    require(cancel_invalid_finished.status == 1 && cancel_invalid_finished.output ==
                "{\"error\":\"CANCEL_TARGET_UNAVAILABLE\"}\n",
            "cancel treated an invalid public bundle as a finished run");
    bruce::LocalFiles::replace_atomically(bundle.string(), finished_bundle_bytes);
    const auto cancel_finished = invoke(executable, {"dag-scheduler-v5", "cancel", bundle.string()});
    require(cancel_finished.status == 0 && cancel_finished.output ==
                "{\"contract\":\"dag-scheduler-v5-cancel-v1\",\"action\":\"cancel\",\"status\":\"ALREADY_FINISHED\"}\n" &&
            !std::filesystem::exists(std::filesystem::path(bundle.string() + ".run/control/cancel.request")),
            "finished run did not take precedence over cancellation");
    const auto cancel_missing = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                    (root / "missing-bundle").string()});
    require(cancel_missing.status == 1 && cancel_missing.output ==
                "{\"error\":\"CANCEL_TARGET_UNAVAILABLE\"}\n",
            "missing cancel target was not controlled");
    const auto cancel_bad_syntax = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                       cancel_live_bundle.string(), "--force"});
    require(cancel_bad_syntax.status == 2 && cancel_bad_syntax.output ==
                "{\"error\":\"PROTOCOL_ERROR\"}\n",
            "cancel accepted extra arguments");
    const auto cancel_run_target = invoke(executable, {"dag-scheduler-v5", "cancel",
                                                       cancel_live_run.string()});
    require(cancel_run_target.status == 2 && cancel_run_target.output ==
                "{\"error\":\"PROTOCOL_ERROR\"}\n",
            "cancel accepted a private run target");
    for (const auto& run_suffix : {std::string(".run/"), std::string(".run//")}) {
        const auto trailing_run_target = invoke(executable, {"dag-scheduler-v5", "cancel",
            (cancel_live_bundle.string() + run_suffix)});
        require(trailing_run_target.status == 2 && trailing_run_target.output ==
                    "{\"error\":\"PROTOCOL_ERROR\"}\n",
                "cancel accepted a trailing-slash private run target");
    }
    const auto tiny_bundle = root / "tiny-bundle";
    const auto tiny = invoke(executable, {"dag-scheduler-v5", "execute", spec.string(),
        "--output", tiny_bundle.string(), "--max-output-bytes", "1"});
    require(tiny.status == 1 && tiny.output.find("\"failure_code\":\"OUTPUT_LIMIT_EXCEEDED\"") != std::string::npos,
        "output budget failure collapsed to invalid request");
    require(!std::filesystem::exists(tiny_bundle.string() + ".run/attempts/a-1/output/result.bin"),
        "over-budget artifact was created");
    const auto named_spec = root / "named-spec";
    write(named_spec, "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=named\nmax-started-nodes=1\n"
        "max-total-receipt-bytes=4096\n\nnode=named-node\ndepends=\n"
        "callback=literal-v1\nrequest-hex=78\n");
    const auto named_bundle = root / "named-bundle";
    const auto named_status = execute(named_spec, named_bundle, defaults, std::filesystem::canonical(argv[0]));
    require(named_status == 0, "named-node request was not forwarded");
    const auto named_report = bruce::LocalFiles::read(named_bundle.string(), 65536);
    require(named_report.find("\"node_id\":\"named-node\"") != std::string::npos &&
            bruce::LocalFiles::read(named_bundle.string() + ".run/attempts/a-1/output/result.bin", 65536) == "named-node",
            "child request node identity differs from report");
    const auto append_root = root / "append.run";
    std::filesystem::create_directory(append_root);
    std::filesystem::permissions(append_root, std::filesystem::perms::owner_all);
    std::filesystem::create_directory(append_root / "inputs");
    write(append_root / "inputs/request.bin", ":tail");
    write(append_root / "inputs/source.bin", "head");
    bruce::worker_v1::WorkerRequest request;
    request.run_id = "run-append"; request.attempt_id = "a-1"; request.node_id = "node-1";
    request.adapter_id = "append-v1"; request.adapter_version = "1";
    request.request_digest = bruce::sha256(":tail");
    request.input_artifact_refs = {{"inputs/request.bin", 5, request.request_digest},
                                   {"inputs/source.bin", 4, bruce::sha256("head")}};
    request.budget = defaults.budget;
    auto append_attempt = bruce::worker_v1::start_attempt(request, {executable, append_root, defaults.budget, 1000});
    const auto append_result = bruce::worker_v1::finish_attempt(append_attempt).result;
    require(append_result.failure_code == bruce::worker_v1::FailureCode::READY &&
            append_result.result_code == "APPENDED_READY" &&
            bruce::LocalFiles::read((append_root / "attempts/a-1/output/result.bin").string(), 65536) == "head:tail",
            "append worker execution failed");
    const auto mismatch_root = root / "mismatch.run";
    std::filesystem::create_directory(mismatch_root);
    std::filesystem::permissions(mismatch_root, std::filesystem::perms::owner_all);
    auto mismatch_request = request;
    mismatch_request.input_artifact_refs.clear();
    auto mismatch_attempt = bruce::worker_v1::start_attempt(mismatch_request, {
        std::filesystem::canonical(argv[0]), mismatch_root, defaults.budget, 1000});
    require(bruce::worker_v1::finish_attempt(mismatch_attempt).result.failure_code ==
                bruce::worker_v1::FailureCode::DIGEST_MISMATCH,
            "mismatched output digest accepted");
    require(retryable(bruce::worker_v1::FailureCode::PROCESS_CRASHED) &&
            retryable(bruce::worker_v1::FailureCode::TIMEOUT) &&
            retryable(bruce::worker_v1::FailureCode::PROTOCOL_ERROR) &&
            !retryable(bruce::worker_v1::FailureCode::FAILED) &&
            !retryable(bruce::worker_v1::FailureCode::BLOCKED) &&
            !retryable(bruce::worker_v1::FailureCode::CANCELLED) &&
            !retryable(bruce::worker_v1::FailureCode::DIGEST_MISMATCH) &&
            !retryable(bruce::worker_v1::FailureCode::PROCESS_START_FAILED),
            "retry classification changed");
    const auto multi_spec = root / "multi-spec";
    write(multi_spec,
        "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=multi\nmax-started-nodes=8\n"
        "max-total-receipt-bytes=4096\n\nnode=a\ndepends=\ncallback=literal-v1\n"
        "request-hex=41\n\nnode=b\ndepends=\ncallback=literal-v1\nrequest-hex=42\n\n"
        "node=c\ndepends=a,b\ncallback=join-v1\nrequest-hex=2d\n");
    const auto multi_bundle = root / "multi-bundle";
    const auto multi_status = invoke(executable, {"dag-scheduler-v5", "execute",
        multi_spec.string(), "--output", multi_bundle.string(), "--workers", "2"});
    require(multi_status.status == 0, "multi-node DAG failed");
    const auto multi_report = bruce::LocalFiles::read(multi_bundle.string(), 65536);
    require(multi_report.find("\"status\":\"READY\"") != std::string::npos &&
            multi_report.find("\"node_id\":\"a\"") <
            multi_report.find("\"node_id\":\"b\"") &&
            multi_report.find("\"node_id\":\"b\"") <
            multi_report.find("\"node_id\":\"c\""),
            "multi-node receipt order is not canonical");
    require(multi_report.find("A-B") == std::string::npos, "payload leaked in multi report");
    require(multi_report.find("\"usage\":{") != std::string::npos &&
            multi_report.find("\"attempts\":[") != std::string::npos &&
            multi_report.find("attempts/a-1-1/output") == std::string::npos &&
            multi_report.find("\"reference\":\"artifact/a\"") != std::string::npos,
            "multi-node metadata contract changed");
    const auto multi_log = bruce::LocalFiles::read(
        multi_bundle.string() + ".run/control/run-log.jsonl", 65536);
    require(multi_log.find("\"attempt_id\":\"a-3-1\"") != std::string::npos &&
            multi_log.find("\"event\":\"ATTEMPT_STARTED\"") <
            multi_log.find("\"attempt_id\":\"a-3-1\""),
            "dependent node was not started through the run log");
    const auto artifact_store = root / "artifact-store";
    const auto single_publish = publish_v5_artifacts_v1(bundle, artifact_store);
    require(single_publish.run_id.starts_with("run-") &&
            single_publish.artifacts.size() == 1 &&
            single_publish.artifacts.front().node_id == "a" &&
            single_publish.artifacts.front().public_reference == "artifact/a",
            "single publish import binding mismatch");
    const auto single_publish_again = publish_v5_artifacts_v1(bundle, artifact_store);
    require(single_publish_again.artifacts.front().manifest_id ==
                single_publish.artifacts.front().manifest_id,
            "single publish was not idempotent");
    const auto single_manifest = bruce::artifact_v1::ArtifactStoreV1::open(artifact_store)
        .read_manifest(single_publish.artifacts.front().manifest_id);
    require(single_manifest.run_id == single_publish.run_id && single_manifest.node_id == "a" &&
            single_manifest.attempt_id == single_publish.artifacts.front().attempt_id &&
            single_manifest.objects.size() == 1 &&
            single_manifest.objects.front().origin == "artifact/a",
            "single publish manifest mismatch");
    const auto original_bundle = bruce::LocalFiles::read(bundle.string(), 65536);
    auto node_tampered_bundle = original_bundle;
    const auto node_marker = std::string("\"node_id\":\"a\"");
    const auto node_marker_at = node_tampered_bundle.find(node_marker);
    require(node_marker_at != std::string::npos, "single node marker missing");
    node_tampered_bundle.replace(node_marker_at, node_marker.size(), "\"node_id\":\"b\"");
    write(bundle, node_tampered_bundle);
    bool node_tamper_rejected = false;
    try { (void)publish_v5_artifacts_v1(bundle, root / "node-tamper-store"); }
    catch (const bruce::Error& error) { node_tamper_rejected = std::string(error.what()) == "PROTOCOL_ERROR"; }
    write(bundle, original_bundle);
    require(node_tamper_rejected, "node id detached from checked input binding");
    auto result_tampered_bundle = original_bundle;
    const auto result_marker = std::string("\"result_code\":\"LITERAL_READY\"");
    const auto result_marker_at = result_tampered_bundle.find(result_marker);
    require(result_marker_at != std::string::npos, "single result marker missing");
    result_tampered_bundle.replace(result_marker_at, result_marker.size(), "\"result_code\":\"FORGED_READY\"");
    write(bundle, result_tampered_bundle);
    bool result_tamper_rejected = false;
    try { (void)publish_v5_artifacts_v1(bundle, root / "result-tamper-store"); }
    catch (const bruce::Error& error) { result_tamper_rejected = std::string(error.what()) == "PROTOCOL_ERROR"; }
    write(bundle, original_bundle);
    require(result_tamper_rejected, "accepted result code was not bound");
    bool store_path_rejected = false;
    try { (void)publish_v5_artifacts_v1(bundle, bundle); }
    catch (const bruce::Error& error) { store_path_rejected = std::string(error.what()) == "PROTOCOL_ERROR"; }
    require(store_path_rejected, "bundle path was accepted as artifact store");
    const auto empty_spec = root / "empty-spec";
    write(empty_spec, "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=empty\nmax-started-nodes=1\n"
        "max-total-receipt-bytes=4096\n\nnode=empty\ndepends=\ncallback=literal-v1\nrequest-hex=\n");
    const auto empty_bundle = root / "empty-bundle";
    require(invoke(executable, {"dag-scheduler-v5", "execute", empty_spec.string(),
        "--output", empty_bundle.string()}).status == 0, "empty payload execute failed");
    const auto empty_store = root / "empty-artifact-store";
    const auto empty_publish = publish_v5_artifacts_v1(empty_bundle, empty_store);
    require(empty_publish.artifacts.size() == 1 && empty_publish.artifacts.front().object.size_bytes == 0,
            "zero-sized payload was rejected");
    const auto multi_store = root / "multi-artifact-store";
    const auto multi_publish = publish_v5_artifacts_v1(multi_bundle, multi_store);
    require(multi_publish.artifacts.size() == 3 &&
            multi_publish.artifacts[0].node_id == "a" &&
            multi_publish.artifacts[1].node_id == "b" &&
            multi_publish.artifacts[2].node_id == "c",
            "multi-node publish order mismatch");
    const auto same_spec = root / "same-input-spec";
    write(same_spec,
        "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=same\nmax-started-nodes=8\n"
        "max-total-receipt-bytes=4096\n\nnode=a\ndepends=\ncallback=literal-v1\n"
        "request-hex=41\n\nnode=b\ndepends=\ncallback=literal-v1\nrequest-hex=41\n\n"
        "node=c\ndepends=a,b\ncallback=join-v1\nrequest-hex=2d\n");
    const auto same_bundle = root / "same-input-bundle";
    require(invoke(executable, {"dag-scheduler-v5", "execute", same_spec.string(),
        "--output", same_bundle.string(), "--workers", "2"}).status == 0,
        "same-input multi-node execute failed");
    const auto same_original = bruce::LocalFiles::read(same_bundle.string(), 65536);
    auto same_swapped = same_original;
    const auto a_marker = std::string("\"node_id\":\"a\"");
    const auto b_marker = std::string("\"node_id\":\"b\"");
    const auto a_at = same_swapped.find(a_marker);
    const auto b_at = same_swapped.find(b_marker);
    require(a_at != std::string::npos && b_at != std::string::npos, "same-input node markers missing");
    same_swapped.replace(a_at, a_marker.size(), "\"node_id\":\"__swap__\"");
    const auto b_after = same_swapped.find(b_marker);
    same_swapped.replace(b_after, b_marker.size(), a_marker);
    const auto swap_at = same_swapped.find("\"node_id\":\"__swap__\"");
    same_swapped.replace(swap_at, std::string("\"node_id\":\"__swap__\"").size(), b_marker);
    write(same_bundle, same_swapped);
    bool same_order_rejected = false;
    try { (void)publish_v5_artifacts_v1(same_bundle, root / "same-order-store"); }
    catch (const bruce::Error& error) { same_order_rejected = std::string(error.what()) == "PROTOCOL_ERROR"; }
    write(same_bundle, same_original);
    require(same_order_rejected, "same-input receipt order was not bound to sorted inputs");
    const auto blocked_spec = root / "blocked-spec";
    write(blocked_spec,
        "BRUCE_DAG_SCHEDULER_SPEC_V3\ndag-id=blocked\nmax-started-nodes=8\n"
        "max-total-receipt-bytes=4096\n\nnode=a\ndepends=\ncallback=literal-v1\n"
        "request-hex=4142\n\nnode=b\ndepends=\ncallback=literal-v1\nrequest-hex=42\n\n"
        "node=c\ndepends=a,b\ncallback=join-v1\nrequest-hex=2d\n");
    const auto blocked_bundle = root / "blocked-bundle";
    const auto blocked_status = invoke(executable, {"dag-scheduler-v5", "execute",
        blocked_spec.string(), "--output", blocked_bundle.string(), "--workers", "2",
        "--max-output-bytes", "1"});
    require(blocked_status.status == 1 &&
            blocked_status.output.find("\"node_id\":\"c\"") != std::string::npos &&
            blocked_status.output.find("\"failure_code\":\"BLOCKED\"") != std::string::npos &&
            blocked_status.output.find("\"node_id\":\"b\"") != std::string::npos &&
            blocked_status.output.find("\"node_id\":\"b\",\"callback_id\":\"literal-v1\"") != std::string::npos,
            "failed predecessor did not block dependent node");
    bool blocked_publish_rejected = false;
    try { (void)publish_v5_artifacts_v1(root / "blocked-bundle", root / "blocked-store"); }
    catch (const bruce::Error& error) {
        blocked_publish_rejected = std::string(error.what()) == "PROTOCOL_ERROR";
    }
    require(blocked_publish_rejected && !std::filesystem::exists(root / "blocked-store"),
            "failed run was publishable or store opened before inspection");
    std::filesystem::remove_all(root);
    std::cout << "dag scheduler v5 CLI contracts passed\n";
}
