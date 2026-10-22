#include "bruce/mod/logic_mod_supervision_log_v6.hpp"

#include "bruce/core/types.hpp"

#include <cstdlib>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace bruce;

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
template <class F> void rejects(F&& f, const char* message) {
    try { f(); } catch (const Error& error) {
        require(std::string(error.what()) == "PROTOCOL_ERROR", message);
        return;
    }
    std::cerr << message << '\n'; std::exit(1);
}

mod::SupervisionEventV6 event(std::uint64_t seq, std::string name,
                               std::string status = "RUNNING") {
    mod::SupervisionEventV6 value;
    value.seq = seq;
    value.event = std::move(name);
    value.status = std::move(status);
    value.attempt_id = "attempt-1";
    value.node_id = "node-a";
    value.elapsed_ms = 7;
    value.peak_memory_availability = mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE;
    return value;
}

void append_terminal(mod::LogicModSupervisionLogV6& log, std::uint64_t& seq,
                     std::string reason = "READY_ACCEPTED", std::string status = "READY") {
    auto reaped = event(++seq, "REAPED", status);
    reaped.terminal_reason = reason;
    log.append(reaped);
    auto terminal = event(++seq, "ATTEMPT_FINISHED", status);
    terminal.terminal_reason = std::move(reason);
    log.append(terminal);
}

std::string read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::size_t open_fd_count() {
    DIR* directory = opendir("/proc/self/fd");
    require(directory != nullptr, "unable to inspect test FDs");
    std::size_t count{};
    while (readdir(directory) != nullptr) ++count;
    closedir(directory);
    return count;
}
}

int main() {
    const auto base = std::filesystem::temp_directory_path() / "bruce-logic-mod-supervision-log-v6-tests";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    std::filesystem::permissions(base, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    const auto report = base / "report";
    auto log = mod::LogicModSupervisionLogV6::create(report, "run-1");
    std::uint64_t seq{};
    auto started = event(++seq, "ATTEMPT_STARTED");
    started.wall_time_limit_ms = 100;
    started.address_space_limit_bytes = 2048;
    started.input_limit_bytes = 5;
    started.output_limit_bytes = 9;
    log.append(started);
    log.append(event(++seq, "CANCEL_SENT", "CANCELLED"));
    log.append(event(++seq, "TERM_SENT", "CANCELLED"));
    log.append(event(++seq, "KILL_SENT", "CANCELLED"));
    append_terminal(log, seq, "HOST_CANCELLED", "CANCELLED");

    const auto log_path = std::filesystem::path(report.string() + ".run") / "control/supervision-v6.jsonl";
    const auto bytes = read(log_path);
    const std::string expected_prefix =
        "{\"schema\":\"logic-mod-supervision-v6\",\"run_id\":\"run-1\",\"attempt_id\":\"attempt-1\",\"node_id\":\"node-a\",\"seq\":1,\"event\":\"ATTEMPT_STARTED\",\"status\":\"RUNNING\",\"elapsed_ms\":7,\"wall_time_limit_ms\":100,\"address_space_limit_bytes\":2048,\"input_limit_bytes\":5,\"output_limit_bytes\":9,\"peak_memory_availability\":\"UNAVAILABLE\"}\n";
    require(bytes.rfind(expected_prefix, 0) == 0, "canonical start event changed");
    require(bytes.find("peak_memory_bytes") == std::string::npos, "unavailable peak serialized as a value");
    const auto events = mod::validate_logic_mod_supervision_log_v6(report, "run-1");
    require(events.size() == seq && events.back().event == "ATTEMPT_FINISHED", "valid log rejected");

    rejects([&] { log.append(event(seq, "ATTEMPT_STARTED")); }, "duplicate global sequence accepted");
    auto forbidden = event(++seq, "ATTEMPT_STARTED"); forbidden.forbidden_data = "payload";
    rejects([&] { log.append(forbidden); }, "forbidden diagnostic data serialized");
    auto bad_peak = event(++seq, "ATTEMPT_STARTED");
    bad_peak.peak_memory_bytes = 0;
    rejects([&] { log.append(bad_peak); }, "unavailable peak with value accepted");

    const auto malformed = base / "malformed";
    std::filesystem::create_directories(std::filesystem::path(malformed.string() + ".run") / "control");
    std::filesystem::permissions(std::filesystem::path(malformed.string() + ".run"), std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    std::filesystem::permissions(std::filesystem::path(malformed.string() + ".run") / "control", std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    { std::ofstream out(std::filesystem::path(malformed.string() + ".run") / "control/supervision-v6.jsonl"); out << "{broken"; }
    std::filesystem::permissions(std::filesystem::path(malformed.string() + ".run") / "control/supervision-v6.jsonl",
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
    rejects([&] { (void)mod::LogicModSupervisionLogV6::create(malformed, "run-1"); }, "truncated historical JSON accepted");

    const auto symlink = base / "symlink";
    std::filesystem::create_directories(std::filesystem::path(symlink.string() + ".run") / "control");
    std::filesystem::permissions(std::filesystem::path(symlink.string() + ".run"), std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    std::filesystem::permissions(std::filesystem::path(symlink.string() + ".run") / "control", std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    const auto outside = base / "outside"; { std::ofstream out(outside); out << "safe"; }
    std::filesystem::create_symlink(outside, std::filesystem::path(symlink.string() + ".run") / "control/supervision-v6.jsonl");
    rejects([&] { (void)mod::LogicModSupervisionLogV6::create(symlink, "run-1"); }, "symlink target accepted");

    for (const auto* kind : {"fifo", "hardlink", "directory"}) {
        const auto hostile = base / kind;
        const auto control = std::filesystem::path(hostile.string() + ".run") / "control";
        std::filesystem::create_directories(control);
        std::filesystem::permissions(std::filesystem::path(hostile.string() + ".run"), std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        std::filesystem::permissions(control, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        const auto target = control / "supervision-v6.jsonl";
        if (std::string_view(kind) == "fifo") {
            require(::mkfifo(target.c_str(), 0600) == 0, "unable to make FIFO fixture");
        } else if (std::string_view(kind) == "hardlink") {
            const auto source = base / "hardlink-source"; { std::ofstream out(source); out << "x"; }
            std::filesystem::permissions(source, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::replace);
            std::filesystem::create_hard_link(source, target);
        } else {
            std::filesystem::create_directory(target);
            std::filesystem::permissions(target, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
        }
        rejects([&] { (void)mod::LogicModSupervisionLogV6::create(hostile, "run-1"); }, "hostile target accepted");
    }

    const auto inspector_fifo = base / "inspector-fifo";
    const auto inspector_control = std::filesystem::path(inspector_fifo.string() + ".run") / "control";
    std::filesystem::create_directories(inspector_control);
    std::filesystem::permissions(std::filesystem::path(inspector_fifo.string() + ".run"), std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    std::filesystem::permissions(inspector_control, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    require(::mkfifo((inspector_control / "supervision-v6.jsonl").c_str(), 0600) == 0,
            "unable to make inspector FIFO fixture");
    const pid_t fifo_child = fork();
    require(fifo_child >= 0, "unable to fork inspector FIFO probe");
    if (fifo_child == 0) {
        alarm(1);
        try { (void)mod::validate_logic_mod_supervision_log_v6(inspector_fifo, "run-1"); }
        catch (const Error&) { _exit(0); }
        _exit(1);
    }
    int fifo_status{};
    require(waitpid(fifo_child, &fifo_status, 0) == fifo_child && WIFEXITED(fifo_status) && WEXITSTATUS(fifo_status) == 0,
            "read-only inspector blocked on FIFO or accepted it");

    const auto permissive = base / "permissive";
    auto permissive_log = mod::LogicModSupervisionLogV6::create(permissive, "run-1");
    const auto permissive_path = std::filesystem::path(permissive.string() + ".run") / "control/supervision-v6.jsonl";
    std::filesystem::permissions(permissive_path, std::filesystem::perms::group_read, std::filesystem::perm_options::add);
    rejects([&] { permissive_log.append(event(1, "ATTEMPT_STARTED")); }, "permissive replacement target accepted");

    const auto readonly = base / "readonly";
    auto readonly_log = mod::LogicModSupervisionLogV6::create(readonly, "run-1");
    readonly_log.append(event(1, "ATTEMPT_STARTED"));
    const auto readonly_path = std::filesystem::path(readonly.string() + ".run") / "control/supervision-v6.jsonl";
    std::filesystem::permissions(readonly_path, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);
    require(mod::validate_logic_mod_supervision_log_v6(readonly, "run-1").size() == 1,
            "read-only inspector rejected owner-only log");

    const auto before_rejections = open_fd_count();
    for (unsigned i = 0; i != 32; ++i) {
        const auto leak = base / ("leak-" + std::to_string(i));
        auto leak_log = mod::LogicModSupervisionLogV6::create(leak, "run-1");
        const auto leak_path = std::filesystem::path(leak.string() + ".run") / "control/supervision-v6.jsonl";
        std::filesystem::permissions(leak_path, std::filesystem::perms::group_read,
                                     std::filesystem::perm_options::add);
        rejects([&] { leak_log.append(event(1, "ATTEMPT_STARTED")); }, "permissive append target accepted");
    }
    require(open_fd_count() == before_rejections, "rejected targets leaked file descriptors");

    const auto order = base / "order";
    auto order_log = mod::LogicModSupervisionLogV6::create(order, "run-1");
    rejects([&] { order_log.append(event(1, "REAPED", "READY")); }, "reap before start accepted");
    order_log.append(event(1, "ATTEMPT_STARTED"));
    rejects([&] { order_log.append(event(2, "TERM_SENT", "CANCELLED")); }, "TERM before CANCEL accepted");
    order_log.append(event(2, "CANCEL_SENT", "CANCELLED"));
    rejects([&] { order_log.append(event(3, "CANCEL_SENT", "CANCELLED")); }, "duplicate CANCEL accepted");
    order_log.append(event(3, "TERM_SENT", "CANCELLED"));
    rejects([&] { order_log.append(event(4, "TERM_SENT", "CANCELLED")); }, "duplicate TERM accepted");
    auto changed_node = event(4, "KILL_SENT", "CANCELLED"); changed_node.node_id = "node-b";
    rejects([&] { order_log.append(changed_node); }, "node identity changed within attempt");
    order_log.append(event(4, "KILL_SENT", "CANCELLED"));
    rejects([&] { order_log.append(event(5, "KILL_SENT", "CANCELLED")); }, "duplicate KILL accepted");

    for (const auto* reason : {"WORKER_FAILED", "WALL_TIME_LIMIT", "MEMORY_LIMIT", "INPUT_LIMIT", "OUTPUT_LIMIT",
                               "PROCESS_START_FAILED", "PROCESS_CRASHED", "PROTOCOL_ERROR", "DIGEST_MISMATCH",
                               "SUPERVISION_UNAVAILABLE"}) {
        const auto terminal_root = base / (std::string("terminal-") + reason);
        auto terminal_log = mod::LogicModSupervisionLogV6::create(terminal_root, "run-1");
        terminal_log.append(event(1, "ATTEMPT_STARTED"));
        const std::string status = std::string_view(reason) == "SUPERVISION_UNAVAILABLE" ? "SUPERVISION_FAILED" : "FAILED";
        std::uint64_t terminal_seq = 1;
        append_terminal(terminal_log, terminal_seq, reason, status);
    }

    const auto unknown = base / "unknown";
    auto unknown_log = mod::LogicModSupervisionLogV6::create(unknown, "run-1");
    unknown_log.append(event(1, "ATTEMPT_STARTED"));
    const auto unknown_path = std::filesystem::path(unknown.string() + ".run") / "control/supervision-v6.jsonl";
    auto unknown_bytes = read(unknown_path);
    unknown_bytes.insert(unknown_bytes.size() - 2, ",\"unexpected\":1");
    { std::ofstream out(unknown_path, std::ios::binary | std::ios::trunc); out << unknown_bytes; }
    rejects([&] { (void)mod::validate_logic_mod_supervision_log_v6(unknown, "run-1"); }, "unknown historical field accepted");

    const auto malformed_append = base / "malformed-append";
    auto malformed_append_log = mod::LogicModSupervisionLogV6::create(malformed_append, "run-1");
    malformed_append_log.append(event(1, "ATTEMPT_STARTED"));
    const auto malformed_append_path = std::filesystem::path(malformed_append.string() + ".run") / "control/supervision-v6.jsonl";
    const auto good_history = read(malformed_append_path);
    { std::ofstream out(malformed_append_path, std::ios::binary | std::ios::app); out << "{broken"; }
    rejects([&] { malformed_append_log.append(event(2, "CANCEL_SENT", "CANCELLED")); }, "malformed append history accepted");
    { std::ofstream out(malformed_append_path, std::ios::binary | std::ios::trunc); out << good_history; }
    auto reopened = mod::LogicModSupervisionLogV6::create(malformed_append, "run-1");
    reopened.append(event(2, "CANCEL_SENT", "CANCELLED"));

    const auto replacement = base / "replacement";
    auto replacement_log = mod::LogicModSupervisionLogV6::create(replacement, "run-1");
    replacement_log.append(event(1, "ATTEMPT_STARTED"));
    const auto replacement_path = std::filesystem::path(replacement.string() + ".run") / "control/supervision-v6.jsonl";
    const auto stale_path = replacement_path.parent_path() / "stale.jsonl";
    std::filesystem::rename(replacement_path, stale_path);
    { std::ofstream out(replacement_path, std::ios::binary); out << read(stale_path); }
    std::filesystem::permissions(replacement_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
    replacement_log.append(event(2, "CANCEL_SENT", "CANCELLED"));
    require(mod::validate_logic_mod_supervision_log_v6(replacement, "run-1").size() == 2,
            "visible replacement did not receive append");
    require(read(stale_path).find("\"seq\":2") == std::string::npos, "append wrote stale renamed inode");

    const auto altered = base / "altered-history";
    auto altered_log = mod::LogicModSupervisionLogV6::create(altered, "run-1");
    altered_log.append(event(1, "ATTEMPT_STARTED"));
    const auto altered_path = std::filesystem::path(altered.string() + ".run") / "control/supervision-v6.jsonl";
    auto altered_bytes = read(altered_path);
    const auto node_at = altered_bytes.find("node-a");
    require(node_at != std::string::npos, "missing node in altered-history fixture");
    altered_bytes.replace(node_at, 6, "node-b"); // Same file length, count and last seq.
    { std::ofstream out(altered_path, std::ios::binary | std::ios::trunc); out << altered_bytes; }
    rejects([&] { altered_log.append(event(2, "CANCEL_SENT", "CANCELLED")); },
            "same-size altered cached history accepted");

    std::filesystem::remove_all(base);
    std::cout << "logic-mod supervision log v6 contracts passed\n";
}
