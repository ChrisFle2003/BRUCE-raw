#include "bruce/application/common_runtime_host_v2.hpp"
#include "bruce/application/common_runtime_host_v2_acceptance.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include "bruce/worker/worker_supervisor_v1.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <type_traits>
#include <sys/stat.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::worker_v1;
namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
std::string read_request() {
    std::string bytes;
    char c;
    while (bytes.find("\n\n") == std::string::npos && bytes.size() < 256) {
        if (::read(STDIN_FILENO, &c, 1) != 1) std::_Exit(2);
        bytes.push_back(c);
    }
    const auto at = bytes.find("payload-bytes=");
    if (at == std::string::npos) std::_Exit(3);
    const auto end = bytes.find('\n', at);
    const auto count = std::stoul(bytes.substr(at + 14, end - at - 14));
    for (std::size_t i = 0; i < count; ++i) {
        if (::read(STDIN_FILENO, &c, 1) != 1) std::_Exit(4);
        bytes.push_back(c);
    }
    return bytes;
}
void write_all(std::string_view data) {
    while (!data.empty()) {
        const auto n = ::write(STDOUT_FILENO, data.data(), data.size());
        if (n <= 0) std::_Exit(5);
        data.remove_prefix(static_cast<std::size_t>(n));
    }
}
int child() {
    const auto request = decode_request_payload(decode_frame(read_request(), 65536).payload);
    { std::ofstream out("child.pid"); out << ::getpid(); }
    if (request.node_id == "malformed") { write_all("not a frame"); return 0; }
    if (request.node_id == "crash") std::_Exit(42);
    if (request.node_id == "cooperative_cancel" || request.node_id == "ready_after_cancel" ||
        request.node_id == "malformed_after_cancel") {
        std::string input;
        char chunk[512];
        while (input.find("type=CANCEL\n") == std::string::npos) {
            const auto n = ::read(STDIN_FILENO, chunk, sizeof(chunk));
            if (n <= 0) std::_Exit(6);
            input.append(chunk, static_cast<std::size_t>(n));
        }
        { std::ofstream out("cancel.seen"); out << "yes"; }
        if (request.node_id == "ready_after_cancel") goto ready_result;
        if (request.node_id == "malformed_after_cancel") {
            write_all("not a frame");
            return 0;
        }
        WorkerResult cancelled;
        cancelled.status = WorkerStatus::CANCELLED;
        cancelled.failure_code = FailureCode::CANCELLED;
        cancelled.result_code = "CANCELLED";
        write_all(encode_frame(FrameType::RESULT, encode_result_payload(cancelled)));
        return 0;
    }
    if (request.node_id == "ignore_cancel" || request.node_id == "timeout") {
        if (request.node_id == "ignore_cancel") ::signal(SIGTERM, SIG_IGN);
        for (;;) ::pause();
    }
    if (request.node_id == "ready_on_cancel_log") {
        bool seen = false;
        for (int i = 0; i < 5000 && !seen; ++i) {
            std::ifstream in("control/run-log.jsonl");
            const std::string data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            seen = data.find("\"event\":\"CANCEL_REQUESTED\"") != std::string::npos;
            if (!seen) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!seen) std::_Exit(7);
    }
ready_result:
    WorkerResult result;
    result.status = WorkerStatus::READY;
    result.failure_code = FailureCode::READY;
    result.result_code = "READY";
    result.usage.operations = request.node_id == "operation_limit" ? 100 : 1;
    const std::string payload = request.node_id == "output_limit" ? std::string(32, 'x') : "ok";
    const auto ref = std::filesystem::path("attempts") / request.attempt_id / "output/result.bin";
    std::filesystem::create_directories(ref.parent_path());
    { std::ofstream out(ref, std::ios::binary); out << payload; }
    result.output_artifact_ref = ArtifactRef{"result", payload.size(), sha256(payload)};
    result.payload_digest = sha256(payload);
    result.payload_size = payload.size();
    result.usage.output_bytes = payload.size();
    if (request.node_id == "bad_digest") result.output_artifact_ref->sha256 = std::string(64, '0');
    if (request.node_id == "bad_size") result.output_artifact_ref->size_bytes++;
    if (request.node_id == "bad_reference") result.output_artifact_ref->reference = "other";
    write_all(encode_frame(FrameType::RESULT, encode_result_payload(result)));
    return 0;
}
worker_v1::Budget budget() { return {3000, 65536, 65536, 10, 268435456}; }
CommonRuntimeHostV2Request request(std::string mode, std::string id, worker_v1::Budget b = budget()) {
    WorkerRequest worker;
    worker.run_id = "run-1"; worker.attempt_id = std::move(id); worker.node_id = std::move(mode);
    worker.adapter_id = "literal-v1"; worker.adapter_version = "1";
    worker.request_digest = sha256("request"); worker.budget = b;
    return {std::move(worker)};
}
CommonRuntimeHostV2Context context(const std::filesystem::path& exe,
                                   const std::filesystem::path& root, worker_v1::Budget b = budget()) {
    return {root, exe, b, 1000};
}
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int pid(const std::filesystem::path& root) {
    std::ifstream in(root / "child.pid"); int value{}; in >> value; return value;
}
void reaped(const std::filesystem::path& root) {
    const int value = pid(root);
    require(value > 0, "child did not write PID");
    errno = 0;
    require(::kill(value, 0) == -1 && errno == ESRCH, "child process survived finish");
}
std::string events(const std::filesystem::path& root) {
    const auto data = read(root / "control/run-log.jsonl");
    std::string result;
    std::size_t pos{};
    while (pos < data.size()) {
        const auto end = data.find('\n', pos);
        require(end != std::string::npos, "log line lacks newline");
        const auto at = data.find("\"event\":\"", pos);
        require(at < end, "log line lacks event");
        const auto begin = at + 9;
        const auto close = data.find('"', begin);
        result += data.substr(begin, close - begin) + ",";
        pos = end + 1;
    }
    return result;
}
void wait_for_pid(const std::filesystem::path& root) {
    for (int i = 0; i < 200 && pid(root) == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(pid(root) > 0, "child did not start");
}
WorkerResult canonical_ready_result() {
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
void initialize_acceptance_root(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    (void)WorkerRunLog::create(root, "run-1");
}
void require_stripped(const WorkerResult& result, const char* message) {
    require(!result.output_artifact_ref && !result.payload_digest && result.payload_size == 0,
            message);
}
void acceptance_decision_contracts(const std::filesystem::path& base) {
    {
        const auto root = base / "acceptance_valid_cancel";
        initialize_acceptance_root(root);
        require(request_abort(root) == AbortRequestResult::REQUESTED,
                "could not publish valid final-boundary cancel marker");
        const auto decision = decide_common_runtime_host_v2_acceptance(
            root, canonical_ready_result());
        require(decision.disposition == CommonRuntimeHostV2AcceptanceDisposition::CANCEL &&
                decision.result.status == WorkerStatus::CANCELLED &&
                decision.result.failure_code == FailureCode::CANCELLED &&
                decision.result.usage.operations == 1 &&
                decision.result.usage.output_bytes == 2 &&
                decision.host_exit_code == 1,
                "valid final-boundary cancel marker did not cancel READY");
        require_stripped(decision.result,
                         "final-boundary cancellation leaked artifact metadata");
        require(decision.append_cancel_requested && !decision.append_artifact_accepted,
                "final-boundary cancellation selected the wrong journal events");
    }
    {
        const auto root = base / "acceptance_invalid_marker";
        initialize_acceptance_root(root);
        { std::ofstream marker(root / "control/cancel.request"); marker << "bad marker\n"; }
        std::filesystem::permissions(root / "control/cancel.request",
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);
        const auto decision = decide_common_runtime_host_v2_acceptance(
            root, canonical_ready_result());
        require(decision.disposition == CommonRuntimeHostV2AcceptanceDisposition::PROTOCOL_ERROR &&
                decision.result.failure_code == FailureCode::PROTOCOL_ERROR &&
                decision.result.usage.operations == 1 &&
                decision.result.usage.output_bytes == 2 &&
                decision.host_exit_code == 1,
                "invalid final-boundary marker was not a protocol error");
        require_stripped(decision.result,
                         "invalid final-boundary marker leaked artifact metadata");
        require(!decision.append_cancel_requested && !decision.append_artifact_accepted,
                "invalid final-boundary marker selected a cancel or acceptance event");
    }
    {
        const auto root = base / "acceptance_normal";
        initialize_acceptance_root(root);
        const auto decision = decide_common_runtime_host_v2_acceptance(
            root, canonical_ready_result());
        require(decision.disposition == CommonRuntimeHostV2AcceptanceDisposition::ACCEPT &&
                decision.result.failure_code == FailureCode::READY &&
                decision.result.output_artifact_ref && decision.result.payload_digest &&
                decision.result.payload_size == 2 && decision.host_exit_code == 0,
                "marker-free final-boundary READY was not accepted");
        require(!decision.append_cancel_requested && decision.append_artifact_accepted,
                "marker-free final-boundary READY selected the wrong journal events");
    }
    {
        const auto root = base / "acceptance_marker_after_probe";
        initialize_acceptance_root(root);
        const auto decision = decide_common_runtime_host_v2_acceptance(
            root, canonical_ready_result());
        require(request_abort(root) == AbortRequestResult::REQUESTED,
                "could not publish marker after final acceptance probe");
        require(decision.disposition == CommonRuntimeHostV2AcceptanceDisposition::ACCEPT &&
                decision.result.failure_code == FailureCode::READY &&
                decision.host_exit_code == 0 && decision.append_artifact_accepted &&
                !decision.append_cancel_requested,
                "marker published after successful final probe retroactively changed decision");
    }
}
}
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "worker-v1" &&
        std::string_view(argv[2]) == "child") return child();
    static_assert(!std::is_copy_constructible_v<CommonRuntimeHostV2Run>);
    static_assert(std::is_move_constructible_v<CommonRuntimeHostV2Run>);
    const auto base = std::filesystem::temp_directory_path() /
        ("bruce-common-runtime-host-v2-" + std::to_string(::getpid()));
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    acceptance_decision_contracts(base);
    const auto exe = std::filesystem::canonical(argv[0]);
    const auto run = [&](std::string mode, std::string id, worker_v1::Budget b = budget()) {
        auto root = base / id;
        auto handle = start_single_worker_v2(request(std::move(mode), id, b), context(exe, root, b));
        auto outcome = finish_single_worker_v2(handle);
        require(poll_single_worker_v2(handle).has_value(), "finished poll lost outcome");
        require(finish_single_worker_v2(handle).result.failure_code == outcome.result.failure_code,
                "repeat finish changed result");
        return std::pair{outcome, root};
    };
    {
        auto root = base / "invalid_context";
        auto wrong = budget(); wrong.max_operations++;
        auto handle = start_single_worker_v2(request("ready", "invalid_context"), context(exe, root, wrong));
        require(finish_single_worker_v2(handle).result.failure_code == FailureCode::INVALID_REQUEST,
                "budget mismatch was accepted");
        require(!std::filesystem::exists(root / "child.pid"), "invalid context started child");
        auto foreign = context("/bin/true", base / "foreign");
        auto bad = start_single_worker_v2(request("ready", "foreign"), foreign);
        require(finish_single_worker_v2(bad).host_exit_code == 1, "foreign executable accepted");
        require(!std::filesystem::exists(foreign.run_root / "child.pid"), "foreign child started");
        auto bad_grace = context(exe, base / "bad_grace"); bad_grace.grace_ms = 999;
        auto grace = start_single_worker_v2(request("ready", "bad_grace"), bad_grace);
        require(finish_single_worker_v2(grace).result.failure_code == FailureCode::INVALID_REQUEST,
                "noncanonical grace accepted");
        auto invalid_request = request("ready", "invalid_request");
        invalid_request.worker_request.request_digest = "invalid";
        auto invalid = start_single_worker_v2(invalid_request, context(exe, base / "invalid_request"));
        require(finish_single_worker_v2(invalid).result.failure_code == FailureCode::INVALID_REQUEST,
                "invalid Worker V1 request accepted");
        require(!std::filesystem::exists(base / "invalid_request/child.pid"), "invalid request started child");
        const auto special_root = base / "special_mode";
        std::filesystem::create_directory(special_root);
        require(::chmod(special_root.c_str(), 02700) == 0, "could not set special root mode");
        auto special = start_single_worker_v2(request("ready", "special_mode"), context(exe, special_root));
        require(finish_single_worker_v2(special).result.failure_code == FailureCode::INVALID_REQUEST &&
                !std::filesystem::exists(special_root / "child.pid"), "mode 2700 root accepted");
    }
    {
        auto [out, root] = run("ready", "ready");
        require(out.host_exit_code == 0 && out.result.failure_code == FailureCode::READY,
                "valid READY result rejected");
        require(out.result.output_artifact_ref && out.result.output_artifact_ref->reference == "result" &&
                out.result.output_artifact_ref->sha256 == *out.result.payload_digest &&
                out.result.output_artifact_ref->size_bytes == out.result.payload_size,
                "READY projection is not canonical");
        require(events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,PROCESS_EXITED,RESULT_RECEIVED,ARTIFACT_ACCEPTED,RUN_FINISHED,",
                "READY lifecycle order is wrong");
        const auto data = read(root / "control/run-log.jsonl");
        for (int seq = 1; seq <= 7; ++seq)
            require(data.find("\"seq\":" + std::to_string(seq) + ",") != std::string::npos,
                    "READY log sequence is not monotonic");
        require(data.find("\"schema\":\"worker-run-log-v1\"") != std::string::npos &&
                data.find("\"reference\":\"attempts/ready/output/result.bin\"") != std::string::npos,
                "READY log does not use canonical event schema or private reference");
        (void)WorkerRunLog::create(root, "run-1");
        reaped(root);
        bool rejected_replay = false;
        try { (void)start_single_worker_v2(request("ready", "ready"), context(exe, root)); }
        catch (const Error&) { rejected_replay = true; }
        require(rejected_replay, "completed log allowed a second worker attempt");
    }
    {
        const auto root = base / "marker_after_durable_acceptance";
        auto handle = start_single_worker_v2(
            request("ready", "marker_after_durable_acceptance"), context(exe, root));
        const auto accepted = finish_single_worker_v2(handle);
        require(accepted.host_exit_code == 0 &&
                accepted.result.failure_code == FailureCode::READY,
                "late-marker control did not first accept READY");
        const auto accepted_events = events(root);
        require(request_abort(root) == AbortRequestResult::REQUESTED,
                "could not publish marker after durable acceptance");
        const auto repeated = finish_single_worker_v2(handle);
        require(repeated.host_exit_code == 0 &&
                repeated.result.failure_code == FailureCode::READY &&
                repeated.result.output_artifact_ref && repeated.result.payload_digest,
                "marker after durable acceptance changed terminal result");
        require(events(root) == accepted_events &&
                events(root).find("CANCEL_REQUESTED") == std::string::npos,
                "marker after durable acceptance appended a cancellation event");
        reaped(root);
    }
    for (const auto& mode : {"bad_digest", "bad_size", "bad_reference", "malformed"}) {
        auto [out, root] = run(mode, mode);
        const auto expected = std::string_view(mode) == "malformed" ||
            std::string_view(mode) == "bad_reference" ? FailureCode::PROTOCOL_ERROR :
            FailureCode::DIGEST_MISMATCH;
        require(out.result.failure_code == expected, "invalid READY classification is wrong");
        require(out.host_exit_code == 1 && !out.result.output_artifact_ref && !out.result.payload_digest &&
                out.result.payload_size == 0, "noncanonical READY leaked artifact metadata");
        require(events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "noncanonical READY accepted artifact");
        reaped(root);
    }
    {
        const auto root = base / "continued_log";
        auto log = WorkerRunLog::create(root, "run-1");
        std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        WorkerLogEvent created;
        created.seq = 7; created.ts_unix_ms = 17;
        created.event = "RUN_CREATED"; created.status = "READY";
        log.append(created);
        auto handle = start_single_worker_v2(request("ready", "continued_log"), context(exe, root));
        require(finish_single_worker_v2(handle).host_exit_code == 0,
                "existing RUN_CREATED prevented valid completion");
        const auto data = read(root / "control/run-log.jsonl");
        require(data.find("\"seq\":8,") != std::string::npos &&
                events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,PROCESS_EXITED,RESULT_RECEIVED,ARTIFACT_ACCEPTED,RUN_FINISHED,",
                "existing log event was duplicated or sequence restarted");
        reaped(root);
    }
    {
        auto root = base / "cooperative_cancel";
        auto handle = start_single_worker_v2(request("cooperative_cancel", "cooperative_cancel"), context(exe, root));
        wait_for_pid(root);
        request_cancel_v2(handle);
        request_cancel_v2(handle);
        auto out = finish_single_worker_v2(handle);
        require(out.result.failure_code == FailureCode::CANCELLED && out.host_exit_code == 1,
                "cooperative cancel classification is wrong");
        require(std::filesystem::exists(root / "cancel.seen"), "cooperative CANCEL frame was not received");
        require(events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,CANCEL_REQUESTED,PROCESS_EXITED,RESULT_RECEIVED,RUN_FINISHED,",
                "cooperative cancellation lifecycle is wrong");
        reaped(root);
    }
    {
        auto root = base / "ignore_cancel";
        auto handle = start_single_worker_v2(request("ignore_cancel", "ignore_cancel"), context(exe, root));
        wait_for_pid(root);
        const auto started = std::chrono::steady_clock::now();
        request_cancel_v2(handle);
        auto out = finish_single_worker_v2(handle);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        require(out.result.failure_code == FailureCode::CANCELLED, "ignored CANCEL misclassified");
        require(elapsed >= std::chrono::milliseconds(1000) && elapsed < std::chrono::milliseconds(2500),
                "ignored CANCEL did not use fixed grace");
        reaped(root);
    }
    {
        auto root = base / "ready_after_cancel";
        auto handle = start_single_worker_v2(request("ready_after_cancel", "ready_after_cancel"), context(exe, root));
        wait_for_pid(root);
        request_cancel_v2(handle);
        auto out = finish_single_worker_v2(handle);
        require(out.result.failure_code == FailureCode::CANCELLED &&
                !out.result.output_artifact_ref && !out.result.payload_digest && out.result.payload_size == 0,
                "late READY after host cancellation was accepted");
        require(events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "late READY produced ARTIFACT_ACCEPTED");
        reaped(root);
    }
    {
        auto root = base / "malformed_after_cancel";
        auto handle = start_single_worker_v2(
            request("malformed_after_cancel", "malformed_after_cancel"), context(exe, root));
        wait_for_pid(root);
        request_cancel_v2(handle);
        auto out = finish_single_worker_v2(handle);
        require(out.result.failure_code == FailureCode::CANCELLED &&
                !out.result.output_artifact_ref && !out.result.payload_digest,
                "late malformed result was not normalized by host cancellation");
        require(events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,CANCEL_REQUESTED,PROCESS_EXITED,RESULT_RECEIVED,RUN_FINISHED,",
                "late malformed cancellation lifecycle has duplicates or accepted artifact");
        reaped(root);
    }
    {
        auto root = base / "ready_before_cancel_poll";
        auto handle = start_single_worker_v2(request("ready", "ready_before_cancel_poll"),
                                              context(exe, root));
        wait_for_pid(root);
        bool child_exited = false;
        for (int i = 0; i < 500 && !child_exited; ++i) {
            std::ifstream status("/proc/" + std::to_string(pid(root)) + "/status");
            const std::string data{std::istreambuf_iterator<char>(status),
                                   std::istreambuf_iterator<char>()};
            child_exited = data.find("State:\tZ") != std::string::npos;
            if (!child_exited) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(child_exited, "worker did not finish before cancellation probe");
        request_cancel_v2(handle);
        const auto out = finish_single_worker_v2(handle);
        require(out.host_exit_code == 1 && out.result.failure_code == FailureCode::CANCELLED &&
                !out.result.output_artifact_ref && !out.result.payload_digest,
                "finished but unpolled READY was accepted after cancellation");
        require(events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,CANCEL_REQUESTED,PROCESS_EXITED,RESULT_RECEIVED,RUN_FINISHED,",
                "finished worker cancellation lifecycle is wrong");
        reaped(root);
    }
    {
        auto root = base / "ready_on_cancel_log";
        auto handle = start_single_worker_v2(request("ready_on_cancel_log", "ready_on_cancel_log"), context(exe, root));
        wait_for_pid(root);
        request_cancel_v2(handle);
        auto out = finish_single_worker_v2(handle);
        require(out.result.failure_code == FailureCode::CANCELLED && !out.result.output_artifact_ref,
                "READY emitted after host cancellation log was accepted");
        require(events(root) == "RUN_CREATED,ATTEMPT_STARTED,STATUS,CANCEL_REQUESTED,PROCESS_EXITED,RESULT_RECEIVED,RUN_FINISHED,",
                "late READY cancellation lifecycle has duplicates or accepted artifact");
        reaped(root);
    }
    {
        auto root = base / "invalid_marker";
        auto handle = start_single_worker_v2(request("ignore_cancel", "invalid_marker"), context(exe, root));
        wait_for_pid(root);
        { std::ofstream marker(root / "control/cancel.request"); marker << "bad marker\n"; }
        std::filesystem::permissions(root / "control/cancel.request", std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
        auto out = finish_single_worker_v2(handle);
        require(out.result.failure_code == FailureCode::PROTOCOL_ERROR && out.host_exit_code == 1,
                "invalid abort marker did not become terminal host error");
        require(events(root) ==
                "RUN_CREATED,ATTEMPT_STARTED,STATUS,PROCESS_EXITED,RESULT_RECEIVED,RUN_FINISHED,",
                "invalid marker claimed cancellation, accepted an artifact, or did not finish");
        reaped(root);
    }
    {
        const auto alias = base / "executable_alias";
        std::filesystem::create_symlink(exe, alias);
        auto root = base / "symlink_executable";
        auto handle = start_single_worker_v2(request("ready", "symlink_executable"), context(alias, root));
        std::filesystem::remove(alias);
        std::filesystem::create_symlink("/bin/true", alias);
        require(finish_single_worker_v2(handle).result.failure_code == FailureCode::READY,
                "changing executable alias after start changed worker identity");
        reaped(root);
    }
    {
        auto short_budget = budget(); short_budget.max_wall_ms = 80;
        auto [out, root] = run("timeout", "timeout", short_budget);
        require(out.result.failure_code == FailureCode::TIMEOUT, "timeout misclassified");
        require(events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "timeout accepted an artifact");
        reaped(root);
    }
    {
        auto [out, root] = run("crash", "crash");
        require(out.result.failure_code == FailureCode::PROCESS_CRASHED, "crash misclassified");
        reaped(root);
    }
    {
        auto small = budget(); small.max_output_bytes = 8;
        auto [out, root] = run("output_limit", "output_limit", small);
        require(out.result.failure_code == FailureCode::OUTPUT_LIMIT_EXCEEDED,
                "output limit misclassified");
        reaped(root);
    }
    {
        auto [out, root] = run("operation_limit", "operation_limit");
        require(out.result.failure_code == FailureCode::OPERATION_BUDGET_EXCEEDED,
                "operation limit misclassified");
        reaped(root);
    }
    std::filesystem::remove_all(base);
    std::cout << "common runtime host v2 contracts passed\n";
}
