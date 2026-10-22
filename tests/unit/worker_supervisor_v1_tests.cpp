#include "bruce/worker/worker_supervisor_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace bruce::worker_v1;
namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
template<class F> void rejects_invalid_handle(F&& operation, const char* message) {
    try { operation(); }
    catch (const bruce::Error&) { return; }
    require(false, message);
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
    if (count > 65536) std::_Exit(4);
    for (std::size_t i = 0; i < count; ++i) {
        if (::read(STDIN_FILENO, &c, 1) != 1) std::_Exit(5);
        bytes.push_back(c);
    }
    return bytes;
}
void write_all(std::string_view data) {
    while (!data.empty()) {
        const auto n = ::write(STDOUT_FILENO, data.data(), data.size());
        if (n <= 0) std::_Exit(6);
        data.remove_prefix(static_cast<std::size_t>(n));
    }
}
int child() {
    const auto request = decode_request_payload(decode_frame(read_request(), 65536).payload);
    { std::ofstream out("child.pid"); out << ::getpid(); }
    if (request.node_id == "crash") std::_Exit(42);
    if (request.node_id == "malformed") { write_all("not a frame"); return 0; }
    if (request.node_id == "failed_result") {
        WorkerResult failed;
        failed.status = WorkerStatus::FAILED;
        failed.failure_code = FailureCode::FAILED;
        failed.result_code = "FAILED";
        write_all(encode_frame(FrameType::RESULT, encode_result_payload(failed)));
        return 0;
    }
    if (request.node_id == "status") write_all(encode_frame(FrameType::STATUS, "RUNNING"));
    if (request.node_id == "slow_ready")
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (request.node_id == "flood") {
        const auto status = encode_frame(FrameType::STATUS, "RUNNING");
        for (;;) write_all(status);
    }
    if (request.node_id == "cooperative_cancel") {
        std::string input;
        char chunk[512];
        while (input.find("type=CANCEL\n") == std::string::npos) {
            const auto n = ::read(STDIN_FILENO, chunk, sizeof(chunk));
            if (n <= 0) std::_Exit(7);
            input.append(chunk, static_cast<std::size_t>(n));
            if (input.size() > 65536) std::_Exit(8);
        }
        { std::ofstream out("cancel.seen"); out << "yes"; }
        WorkerResult cancelled;
        cancelled.status = WorkerStatus::CANCELLED;
        cancelled.failure_code = FailureCode::CANCELLED;
        cancelled.result_code = "CANCELLED";
        write_all(encode_frame(FrameType::RESULT, encode_result_payload(cancelled)));
        return 0;
    }
    if (request.node_id == "hold_stdout" || request.node_id == "closed_stdout_desc") {
        const auto descendant = ::fork();
        if (descendant == 0) {
            ::signal(SIGTERM, SIG_IGN);
            if (request.node_id == "closed_stdout_desc") ::close(STDOUT_FILENO);
            for (;;) ::pause();
        }
        { std::ofstream out("descendant.pid"); out << descendant; }
    }
    if (request.node_id == "timeout" || request.node_id == "ignore_cancel" ||
        request.node_id == "term_descendant") {
        if (request.node_id == "ignore_cancel" || request.node_id == "term_descendant") {
            const auto descendant = ::fork();
            if (descendant == 0) {
                ::signal(SIGTERM, SIG_IGN);
                for (;;) ::pause();
            }
            { std::ofstream out("descendant.pid"); out << descendant; }
            if (request.node_id == "ignore_cancel") ::signal(SIGTERM, SIG_IGN);
        }
        for (;;) ::pause();
    }
    WorkerResult result;
    result.status = WorkerStatus::READY;
    result.failure_code = FailureCode::READY;
    result.result_code = "READY";
    result.usage.operations = 1;
    if (request.node_id == "operation_limit") result.usage.operations = 100;
    std::string payload = request.node_id == "output_limit" ? std::string(32, 'x') : "ok";
    std::filesystem::create_directories(std::filesystem::path("attempts") / request.attempt_id / "output");
    const auto ref = (std::filesystem::path("attempts") / request.attempt_id / "output/result.bin").generic_string();
    { std::ofstream out(ref, std::ios::binary); out << payload; }
    result.output_artifact_ref = ArtifactRef{"result", payload.size(), bruce::sha256(payload)};
    result.payload_digest = bruce::sha256(payload);
    result.payload_size = payload.size();
    result.usage.output_bytes = payload.size();
    if (request.node_id == "bad_digest") result.output_artifact_ref->sha256 = std::string(64, '0');
    write_all(encode_frame(FrameType::RESULT, encode_result_payload(result)));
    return 0;
}
WorkerRequest request(std::string mode, std::string id) {
    WorkerRequest value;
    value.run_id = "run-1"; value.attempt_id = std::move(id); value.node_id = std::move(mode);
    value.adapter_id = "literal-v1"; value.adapter_version = "1";
    value.request_digest = bruce::sha256("request");
    value.budget = Budget{3000, 65536, 65536, 10, 268435456};
    return value;
}
SupervisorOptions options(const std::filesystem::path& exe, const std::filesystem::path& root) {
    return {exe, root, Budget{3000, 65536, 65536, 10, 268435456}, 1000};
}
int read_pid(const std::filesystem::path& root, const char* name) {
    std::ifstream in(root / name); int pid = 0; in >> pid; return pid;
}
int watchdog_pid(int worker_pid) {
    const auto id = std::to_string(::getpid());
    std::ifstream in("/proc/" + id + "/task/" + id + "/children");
    int child{};
    while (in >> child) if (child != worker_pid) return child;
    return 0;
}
void reaped(int pid, const char* message) {
    require(pid > 0, "helper did not write PID");
    errno = 0;
    require(::kill(pid, 0) == -1 && errno == ESRCH, message);
}
bool not_running(int pid) {
    if (pid <= 0) return false;
    if (::kill(pid, 0) != 0 && errno == ESRCH) return true;
    std::ifstream in("/proc/" + std::to_string(pid) + "/stat");
    std::string line; std::getline(in, line);
    const auto end = line.rfind(") ");
    return end != std::string::npos && end + 2 < line.size() && line[end + 2] == 'Z';
}
void descendant_gone(int pid, const char* message) {
    for (int i = 0; i < 100 && !not_running(pid); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(not_running(pid), message);
}
}
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "worker-v1" && std::string_view(argv[2]) == "child") return child();
    const auto root = std::filesystem::temp_directory_path() /
        ("bruce-worker-supervisor-v1-" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto exe = std::filesystem::canonical(argv[0]);
    {
        WorkerAttemptHandle invalid;
        WorkerAttemptHandle moved(std::move(invalid));
        rejects_invalid_handle([&] { (void)poll_attempt(invalid); }, "moved-from poll accepted");
        rejects_invalid_handle([&] { (void)finish_attempt(moved); }, "default finish accepted");
        rejects_invalid_handle([&] { request_cancel(moved); }, "default cancellation accepted");
    }
    const auto run = [&](std::string mode, std::string id, Budget budget) {
        auto opt = options(exe, root / id); opt.budgets = budget;
        auto req = request(std::move(mode), id); req.budget = budget;
        auto attempt = start_attempt(req, opt);
        return std::pair{finish_attempt(attempt), opt.run_root};
    };
    const Budget normal{3000, 65536, 65536, 10, 268435456};
    {
        auto [out, path] = run("ready", "ready", normal);
        if (out.result.failure_code != FailureCode::READY)
            std::cerr << "ready code=" << static_cast<int>(out.result.failure_code) << '\n';
        require(out.result.status == WorkerStatus::READY && out.result.failure_code == FailureCode::READY,
                "valid READY result rejected");
        require(out.result.usage.peak_memory_bytes > 0, "peak memory diagnostic missing");
        reaped(read_pid(path, "child.pid"), "READY child not reaped");
    }
    {
        auto opt = options(exe, root / "finished_before_cancel");
        auto attempt = start_attempt(request("ready", "finished_before_cancel"), opt);
        int pid = 0;
        for (int i = 0; i < 100 && pid == 0; ++i) {
            pid = read_pid(opt.run_root, "child.pid");
            if (pid == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (int i = 0; i < 100 && !not_running(pid); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(not_running(pid), "helper had not exited before cancellation test");
        request_cancel(attempt);
        require(finish_attempt(attempt).result.failure_code == FailureCode::READY,
                "late cancellation discarded completed READY result");
    }
    {
        auto opt = options(exe, root / "failed_before_cancel");
        auto attempt = start_attempt(request("failed_result", "failed_before_cancel"), opt);
        int pid = 0;
        for (int i = 0; i < 100 && pid == 0; ++i) {
            pid = read_pid(opt.run_root, "child.pid");
            if (pid == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (int i = 0; i < 100 && !not_running(pid); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(not_running(pid), "failed helper had not exited before cancellation test");
        request_cancel(attempt);
        require(finish_attempt(attempt).result.failure_code == FailureCode::FAILED,
                "late cancellation discarded completed FAILED result");
    }
    {
        auto [out, path] = run("status", "status", normal);
        require(out.result.failure_code == FailureCode::READY,
                "STATUS before RESULT rejected");
        reaped(read_pid(path, "child.pid"), "status child not reaped");
    }
    {
        auto fast = normal; fast.max_wall_ms = 200;
        auto opt = options(exe, root / "completed_before_poll"); opt.budgets = fast;
        auto req = request("ready", "completed_before_poll"); req.budget = fast;
        auto attempt = start_attempt(req, opt);
        for (int i = 0; i < 100 && !std::filesystem::exists(opt.run_root / "child.pid"); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        require(finish_attempt(attempt).result.failure_code == FailureCode::READY,
                "completed result incorrectly timed out before first poll");
    }
    {
        auto slow_budget = normal; slow_budget.max_wall_ms = 100;
        auto opt = options(exe, root / "slow_ready"); opt.budgets = slow_budget;
        auto req = request("slow_ready", "slow_ready"); req.budget = slow_budget;
        auto attempt = start_attempt(req, opt);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        require(finish_attempt(attempt).result.failure_code == FailureCode::TIMEOUT,
                "over-budget completed result escaped late first poll");
    }
    {
        auto budget = normal; budget.max_wall_ms = 1000;
        auto opt = options(exe, root / "delayed_watchdog"); opt.budgets = budget;
        auto req = request("slow_ready", "delayed_watchdog"); req.budget = budget;
        auto attempt = start_attempt(req, opt);
        int worker = 0;
        for (int i = 0; i < 100 && worker == 0; ++i) {
            worker = read_pid(opt.run_root, "child.pid");
            if (worker == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const int watcher = watchdog_pid(worker);
        require(worker > 0 && watcher > 0 && ::kill(watcher, SIGSTOP) == 0,
                "could not pause watchdog for reaping-order test");
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        require(!poll_attempt(attempt).has_value(), "worker completed before watchdog observation");
        require(::kill(worker, 0) == 0, "worker reaped before watchdog observation");
        require(::kill(watcher, SIGCONT) == 0, "could not resume watchdog");
        require(finish_attempt(attempt).result.failure_code == FailureCode::READY,
                "delayed watchdog lost valid READY result");
        reaped(worker, "worker remained after delayed watchdog completion");
        reaped(watcher, "watchdog remained after delayed completion");
    }
    {
        auto [out, path] = run("flood", "flood", normal);
        require(out.result.failure_code == FailureCode::PROTOCOL_ERROR,
                "unbounded STATUS flood escaped protocol limit");
        reaped(read_pid(path, "child.pid"), "flood child not reaped");
    }
    {
        auto [out, path] = run("malformed", "malformed", normal);
        require(out.result.failure_code == FailureCode::PROTOCOL_ERROR, "malformed result misclassified");
        reaped(read_pid(path, "child.pid"), "malformed child not reaped");
    }
    {
        auto [out, path] = run("crash", "crash", normal);
        require(out.result.failure_code == FailureCode::PROCESS_CRASHED, "crash misclassified");
        reaped(read_pid(path, "child.pid"), "crashed child not reaped");
    }
    {
        auto short_budget = normal; short_budget.max_wall_ms = 80;
        auto [out, path] = run("timeout", "timeout", short_budget);
        require(out.result.failure_code == FailureCode::TIMEOUT, "timeout misclassified");
        reaped(read_pid(path, "child.pid"), "timed-out child not reaped");
    }
    {
        auto short_budget = normal; short_budget.max_wall_ms = 80;
        auto opt = options(exe, root / "timeout_descendant"); opt.budgets = short_budget;
        auto req = request("term_descendant", "timeout_descendant"); req.budget = short_budget;
        auto attempt = start_attempt(req, opt);
        for (int i = 0; i < 100 && read_pid(opt.run_root, "descendant.pid") == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const int worker = read_pid(opt.run_root, "child.pid");
        const int watcher = watchdog_pid(worker);
        require(watcher > 0, "timeout watchdog not observable");
        require(finish_attempt(attempt).result.failure_code == FailureCode::TIMEOUT,
                "watchdog TERM/KILL timeout misclassified");
        reaped(worker, "timed-out process-group leader not reaped");
        reaped(watcher, "timeout watchdog not reaped");
        descendant_gone(read_pid(opt.run_root, "descendant.pid"),
                        "descendant survived watchdog TERM/KILL sequence");
    }
    {
        auto small = normal; small.max_output_bytes = 8;
        auto [out, path] = run("output_limit", "output_limit", small);
        require(out.result.failure_code == FailureCode::OUTPUT_LIMIT_EXCEEDED, "output limit misclassified");
        reaped(read_pid(path, "child.pid"), "output-limit child not reaped");
    }
    {
        auto [out, path] = run("operation_limit", "operation_limit", normal);
        require(out.result.failure_code == FailureCode::OPERATION_BUDGET_EXCEEDED, "operation limit misclassified");
        reaped(read_pid(path, "child.pid"), "operation-limit child not reaped");
    }
    {
        auto [out, path] = run("bad_digest", "bad_digest", normal);
        require(out.result.failure_code == FailureCode::DIGEST_MISMATCH, "artifact digest misclassified");
        reaped(read_pid(path, "child.pid"), "digest-mismatch child not reaped");
    }
    for (const auto& mode : {"hold_stdout", "closed_stdout_desc"}) {
        auto short_budget = normal; short_budget.max_wall_ms = 500;
        auto [out, path] = run(mode, mode, short_budget);
        require(out.result.failure_code == FailureCode::READY,
                "reaped main child left descendant or waited for inherited stdout");
        reaped(read_pid(path, "child.pid"), "main child not reaped after descendant");
        descendant_gone(read_pid(path, "descendant.pid"), "descendant survived completed attempt");
    }
    {
        const auto run_root = root / "symlink_run";
        const auto outside = root / "outside_attempts";
        std::filesystem::create_directories(run_root);
        std::filesystem::permissions(run_root, std::filesystem::perms::owner_all);
        std::filesystem::create_directories(outside);
        std::filesystem::create_directory_symlink(outside, run_root / "attempts");
        auto attempt = start_attempt(request("ready", "symlink"), options(exe, run_root));
        require(finish_attempt(attempt).result.failure_code != FailureCode::READY,
                "attempts symlink outside run root accepted");
        require(!std::filesystem::exists(outside / "symlink"),
                "attempt directory was created outside run root");
    }
    {
        auto req = request("ready", "escaping_input");
        req.input_artifact_refs.push_back({"../outside", 0, bruce::sha256("")});
        auto attempt = start_attempt(req, options(exe, root / "escaping_input"));
        require(finish_attempt(attempt).result.failure_code == FailureCode::INVALID_REQUEST,
                "escaping input reference misclassified as input-byte limit");
    }
    {
        auto req = request("ready", "invalid_digest");
        req.input_artifact_refs.push_back({"inputs/item", 0, "not-a-sha256"});
        auto attempt = start_attempt(req, options(exe, root / "invalid_digest"));
        require(finish_attempt(attempt).result.failure_code == FailureCode::INVALID_REQUEST,
                "invalid input digest misclassified as input-byte limit");
    }
    {
        auto req = request("ready", "oversized_input");
        req.input_artifact_refs.push_back({"inputs/item", normal.max_input_bytes + 1, bruce::sha256("")});
        auto attempt = start_attempt(req, options(exe, root / "oversized_input"));
        require(finish_attempt(attempt).result.failure_code == FailureCode::INPUT_LIMIT_EXCEEDED,
                "actual input-byte overflow not classified as input limit");
    }
    {
        const auto run_root = root / "escaping_oversized_input";
        std::filesystem::create_directories(run_root / "inputs");
        std::filesystem::permissions(run_root, std::filesystem::perms::owner_all);
        const auto outside = root / "outside-input";
        { std::ofstream file(outside); file << "outside"; }
        std::filesystem::create_symlink(outside, run_root / "inputs/item");
        auto req = request("ready", "escaping_oversized_input");
        req.input_artifact_refs.push_back({"inputs/item", normal.max_input_bytes + 1, bruce::sha256("")});
        auto attempt = start_attempt(req, options(exe, run_root));
        require(finish_attempt(attempt).result.failure_code == FailureCode::INVALID_REQUEST,
                "escaping oversized input was classified as byte overflow");
    }
    {
        auto opt = options(exe, root / "cooperative");
        auto attempt = start_attempt(request("cooperative_cancel", "cooperative"), opt);
        for (int i = 0; i < 100 && !std::filesystem::exists(opt.run_root / "child.pid"); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        request_cancel(attempt);
        for (int i = 0; i < 100 && !std::filesystem::exists(opt.run_root / "cancel.seen"); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(std::filesystem::exists(opt.run_root / "cancel.seen"),
                "request_cancel did not send cooperative frame before polling");
        const auto started = std::chrono::steady_clock::now();
        const auto out = finish_attempt(attempt);
        require(out.result.failure_code == FailureCode::CANCELLED,
                "cooperative CANCEL result rejected");
        require(std::filesystem::exists(opt.run_root / "cancel.seen"),
                "cooperative CANCEL was not delivered before finish");
        require(std::chrono::steady_clock::now() - started < std::chrono::milliseconds(500),
                "cooperative cancellation waited for hard grace");
    }
    {
        auto opt = options(exe, root / "cancel");
        auto attempt = start_attempt(request("ignore_cancel", "cancel"), opt);
        for (int i = 0; i < 100 && read_pid(opt.run_root, "descendant.pid") == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const auto started = std::chrono::steady_clock::now();
        request_cancel(attempt);
        const auto out = finish_attempt(attempt);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (out.result.failure_code != FailureCode::CANCELLED)
            std::cerr << "cancel code=" << static_cast<int>(out.result.failure_code)
                      << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << '\n';
        require(out.result.failure_code == FailureCode::CANCELLED, "ignored CANCEL misclassified");
        require(elapsed >= std::chrono::milliseconds(1000), "fixed grace period shortened");
        require(elapsed < std::chrono::milliseconds(2500), "cancellation exceeded bounded grace");
        reaped(read_pid(opt.run_root, "child.pid"), "cancelled child not reaped");
        descendant_gone(read_pid(opt.run_root, "descendant.pid"),
                        "descendant survived process-group cancellation");
    }
    {
        auto short_budget = normal; short_budget.max_wall_ms = 300;
        auto opt = options(exe, root / "cancel_near_deadline"); opt.budgets = short_budget;
        auto req = request("ignore_cancel", "cancel_near_deadline"); req.budget = short_budget;
        auto attempt = start_attempt(req, opt);
        for (int i = 0; i < 100 && read_pid(opt.run_root, "descendant.pid") == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto started = std::chrono::steady_clock::now();
        request_cancel(attempt);
        const auto out = finish_attempt(attempt);
        const auto elapsed = std::chrono::steady_clock::now() - started;
        require(out.result.failure_code == FailureCode::CANCELLED,
                "wall watchdog overrode requested cancellation");
        require(elapsed >= std::chrono::milliseconds(1000),
                "wall watchdog shortened the fixed cancellation grace");
        descendant_gone(read_pid(opt.run_root, "descendant.pid"),
                        "descendant survived short-budget cancellation");
    }
    std::filesystem::remove_all(root);
    std::cout << "worker supervisor v1 contracts passed\n";
}
