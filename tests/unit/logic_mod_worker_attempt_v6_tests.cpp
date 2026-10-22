#include "logic_mod_worker_attempt_v6.hpp"

#include "bruce/core/types.hpp"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
static_assert(std::is_nothrow_destructible_v<bruce::logic_mod_worker_v1_internal::LogicModWorkerAttemptV6>);
void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
std::string read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
std::string field(std::string_view value) {
    return std::to_string(value.size()) + ':' + std::string(value);
}
std::string request(std::string_view input) {
    return field("BRUCE_LOGIC_REQUEST_V1\n" + field("binding") + field("digest") +
                 field(input) + field("budget"));
}
int fd_count() {
    DIR* directory = ::opendir("/proc/self/fd");
    check(directory != nullptr, "cannot inspect test descriptors");
    int count{};
    while (::readdir(directory)) ++count;
    ::closedir(directory);
    return count;
}
int signal_calls{};
int count_signal(pid_t, int) { ++signal_calls; return 0; }
int count_and_signal(pid_t target, int signal) { ++signal_calls; return ::kill(target, signal); }
void wait_for_terminal(bruce::logic_mod_worker_v1_internal::LogicModWorkerAttemptV6& attempt,
                       bruce::logic_mod_worker_v1_internal::LogicModAttemptOutcomeV6& outcome) {
    for (int i = 0; i != 2000; ++i) {
        if (const auto polled = bruce::logic_mod_worker_v1_internal::poll_logic_mod_worker_attempt_v6(attempt)) {
            outcome = *polled; return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    outcome = bruce::logic_mod_worker_v1_internal::finish_logic_mod_worker_attempt_v6(attempt);
}
bool wait_for_leader_exit_pending(bruce::logic_mod_worker_v1_internal::LogicModWorkerAttemptV6& attempt) {
    using namespace bruce::logic_mod_worker_v1_internal;
    for (int i = 0; i != 500; ++i) {
        if (logic_mod_attempt_leader_exit_pending_v6_for_test(attempt)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
void contracts(const char* worker) {
    using namespace bruce::logic_mod_worker_v1_internal;
    const auto bytes = read(worker);
    bruce::LogicModBudgetsV1 budgets{250, 32 * 1024 * 1024, 4096, 65536};
    const int saved_five = ::fcntl(5, F_DUPFD_CLOEXEC, 30);
    const int saved_six = ::fcntl(6, F_DUPFD_CLOEXEC, 30);
    const int sentinel = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    check(sentinel >= 0 && ::dup2(sentinel, 5) == 5 && ::dup2(sentinel, 6) == 6, "cannot install inherited fd sentinels");
    if (sentinel != 5 && sentinel != 6) ::close(sentinel);
    auto prepared = prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, false, -1);
    auto attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    LogicModAttemptOutcomeV6 accepted;
    wait_for_terminal(attempt, accepted);
    check(accepted.terminal_reason == "READY", ("valid attempt was not ready: " + accepted.terminal_reason + "/" + accepted.worker_status).c_str());
    check(accepted.response_frame.find("BRUCE_LOGIC_RESULT_V1") != std::string::npos, "response frame missing");
    check(accepted.peak_memory_availability == LogicModPeakMemoryAvailabilityV6::OBSERVED ||
          accepted.peak_memory_availability == LogicModPeakMemoryAvailabilityV6::UNAVAILABLE,
          "started attempt fabricated not-applicable peak");
    check(accepted.reaped, "attempt did not report reaped");
    if (saved_five >= 0) { check(::dup2(saved_five, 5) == 5, "cannot restore fd5"); ::close(saved_five); } else ::close(5);
    if (saved_six >= 0) { check(::dup2(saved_six, 6) == 6, "cannot restore fd6"); ::close(saved_six); } else ::close(6);
    set_logic_mod_attempt_signal_hook_v6_for_test(count_signal);
    attempt = LogicModWorkerAttemptV6{};
    check(signal_calls == 0, "terminal reaped attempt signalled a later PGID");
    set_logic_mod_attempt_signal_hook_v6_for_test(nullptr);

    prepared = prepare_logic_mod_worker_v6(bytes, request("timeout"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    LogicModAttemptOutcomeV6 timed_out;
    wait_for_terminal(attempt, timed_out);
    check(timed_out.terminal_reason == "WALL_TIME_LIMIT", "deadline did not classify wall time");
    check(timed_out.reaped, "deadline attempt was not reaped");
    check(timed_out.elapsed_ms < 800, "legacy hard deadline gained cancel grace");

    budgets.wall_ms = 3000;
    prepared = prepare_logic_mod_worker_v6(bytes, request("timeout"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    cancel_logic_mod_worker_attempt_v6(attempt);
    LogicModAttemptOutcomeV6 cancelled;
    wait_for_terminal(attempt, cancelled);
    check(cancelled.terminal_reason == "HOST_CANCELLED", "cancel did not win");
    check(cancelled.reaped, "cancelled attempt was not reaped");

    // A host cancellation is causally latched.  The later wall deadline must
    // not relabel that same attempt while it is still in the fixed grace.
    budgets.wall_ms = 1005;
    prepared = prepare_logic_mod_worker_v6(bytes, request("timeout"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    cancel_logic_mod_worker_attempt_v6(attempt);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto latched = finish_logic_mod_worker_attempt_v6(attempt);
    check(latched.terminal_reason == "HOST_CANCELLED", "deadline rewrote latched host cancel");

    // prepare owns all just-created memfds before payload duplication.  A
    // failing optional-FD setup therefore cannot leak descriptors.
    const auto before = fd_count();
    for (int i = 0; i != 32; ++i) {
        try { (void)prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, true, -1); }
        catch (const bruce::Error&) { continue; }
        check(false, "invalid payload fd unexpectedly prepared worker");
    }
    check(fd_count() == before, "failed prepare leaked descriptors");
    check(!logic_mod_peak_from_rusage_kib_v6(0).has_value(), "zero rusage fabricated observed peak");
    check(logic_mod_peak_from_rusage_kib_v6(1) == std::optional<std::uint64_t>{1024}, "rusage peak conversion changed");

    // The direct leader is intentionally reaped before cleanup.  Its child
    // keeps the response-pipe write end open, reproducing the only state in
    // which EOF could otherwise hang after the leader is gone.
    int held_pipe[2]{-1, -1};
    check(::pipe(held_pipe) == 0, "cannot create descendant pipe");
    const auto leader = ::fork();
    check(leader >= 0, "cannot fork descendant leader");
    if (leader == 0) {
        ::close(held_pipe[0]);
        check(::setpgid(0, 0) == 0, "cannot form descendant group");
        const auto descendant = ::fork();
        check(descendant >= 0, "cannot fork descendant");
        if (descendant == 0) { for (;;) ::pause(); }
        _exit(0);
    }
    ::close(held_pipe[1]);
    siginfo_t pending{};
    while (::waitid(P_PID, leader, &pending, WEXITED | WNOWAIT) != 0 && errno == EINTR) {}
    check(pending.si_pid == leader, "cannot observe descendant leader without reaping");
    check(cleanup_logic_mod_process_group_v6_for_test(leader), "reaped leader group was not cleaned");
    int leader_status{};
    check(::waitpid(leader, &leader_status, 0) == leader && WIFEXITED(leader_status), "cannot reap descendant leader");
    char end{};
    check(::read(held_pipe[0], &end, 1) == 0, "group descendant retained response pipe");
    ::close(held_pipe[0]);

    // EOF alone is not evidence that a group is empty: this descendant closes
    // every inherited pipe before the leader exits.  Cleanup must still occur
    // while the leader remains an unreaped zombie which pins its PGID.
    int descendant_pid_pipe[2]{-1, -1};
    check(::pipe(descendant_pid_pipe) == 0, "cannot create closed-fd descendant pipe");
    const auto closed_fd_leader = ::fork();
    check(closed_fd_leader >= 0, "cannot fork closed-fd descendant leader");
    if (closed_fd_leader == 0) {
        ::close(descendant_pid_pipe[0]);
        check(::setpgid(0, 0) == 0, "cannot form closed-fd descendant group");
        const auto descendant = ::fork();
        check(descendant >= 0, "cannot fork closed-fd descendant");
        if (descendant == 0) {
            for (int fd = 0; fd != 64; ++fd) if (fd != descendant_pid_pipe[1]) ::close(fd);
            const auto own_pid = ::getpid();
            (void)::write(descendant_pid_pipe[1], &own_pid, sizeof(own_pid));
            ::close(descendant_pid_pipe[1]);
            for (;;) ::pause();
        }
        ::close(descendant_pid_pipe[1]);
        _exit(0);
    }
    ::close(descendant_pid_pipe[1]);
    pid_t closed_fd_descendant{-1};
    check(::read(descendant_pid_pipe[0], &closed_fd_descendant, sizeof(closed_fd_descendant)) ==
              static_cast<ssize_t>(sizeof(closed_fd_descendant)),
          "closed-fd descendant did not report its pid");
    ::close(descendant_pid_pipe[0]);
    siginfo_t closed_fd_pending{};
    while (::waitid(P_PID, closed_fd_leader, &closed_fd_pending, WEXITED | WNOWAIT) != 0 && errno == EINTR) {}
    check(closed_fd_pending.si_pid == closed_fd_leader, "cannot observe closed-fd leader without reaping");
    check(cleanup_logic_mod_process_group_v6_for_test(closed_fd_leader), "closed-fd descendant group was not cleaned");
    int closed_fd_leader_status{};
    check(::waitpid(closed_fd_leader, &closed_fd_leader_status, 0) == closed_fd_leader && WIFEXITED(closed_fd_leader_status),
          "cannot reap closed-fd descendant leader");
    bool closed_fd_descendant_gone{};
    for (int i = 0; i != 500; ++i) {
        if (::kill(closed_fd_descendant, 0) != 0 && errno == ESRCH) { closed_fd_descendant_gone = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(closed_fd_descendant_gone, "closed-fd group descendant survived cleanup");

    // This ELF remains structurally static for preflight, but the kernel
    // rejects its altered architecture at execveat.  It is a pre-exec start
    // failure, never a worker crash.
    auto wrong_machine = bytes;
    Elf64_Ehdr header{};
    std::memcpy(&header, wrong_machine.data(), sizeof(header));
    header.e_machine = 0;
    std::memcpy(wrong_machine.data(), &header, sizeof(header));
    prepared = prepare_logic_mod_worker_v6(wrong_machine, request("3/4 hoch 2"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    const auto malformed = finish_logic_mod_worker_attempt_v6(attempt);
    check(malformed.terminal_reason == "PROCESS_START_FAILED", "exec failure was not classified as start failure");
    check(malformed.reaped, "exec failure was not reaped");

    // The error channel is installed before PDEATHSIG.  A pre-opened writable
    // FD 7 must therefore remain untouched when that early setup fails.
    const int saved_seven = ::fcntl(7, F_DUPFD_CLOEXEC, 30);
    int ambient_seven[2]{-1, -1};
    check(::pipe2(ambient_seven, O_CLOEXEC | O_NONBLOCK) == 0, "cannot create fd7 sentinel");
    const int ambient_reader = ::fcntl(ambient_seven[0], F_DUPFD_CLOEXEC, 20);
    const int ambient_writer = ::fcntl(ambient_seven[1], F_DUPFD_CLOEXEC, 20);
    ::close(ambient_seven[0]);
    ::close(ambient_seven[1]);
    check(ambient_reader >= 0 && ambient_writer >= 0 && ::dup2(ambient_writer, 7) == 7,
          "cannot install fd7 sentinel");
    if (ambient_writer != 7) ::close(ambient_writer);
    set_logic_mod_attempt_force_pdeath_failure_v6_for_test(true);
    prepared = prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    const auto pdeath_failure = finish_logic_mod_worker_attempt_v6(attempt);
    set_logic_mod_attempt_force_pdeath_failure_v6_for_test(false);
    check(pdeath_failure.terminal_reason == "PROCESS_START_FAILED", "early PDEATHSIG failure was not controlled");
    char sentinel_byte{};
    check(::read(ambient_reader, &sentinel_byte, 1) == -1 && errno == EAGAIN, "early child setup wrote ambient fd7");
    ::close(ambient_reader);
    if (saved_seven >= 0) { check(::dup2(saved_seven, 7) == 7, "cannot restore fd7"); ::close(saved_seven); } else ::close(7);

    // A malformed pre-exec path can exit(125) without writing an error while
    // a descendant retains only FD 7.  The closed gate socket must not bypass
    // the normal lifecycle: leader observation kills that group and produces
    // a controlled, bounded start failure instead of waiting for FD7 EOF.
    budgets.wall_ms = 250;
    set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(true);
    prepared = prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(false);
    // Force the first response read to observe EAGAIN.  The next poll sees
    // gate EOF plus FD7 EOF after group cleanup, and must drain the response
    // EOF before deciding the leader's controlled exit(125).
    set_logic_mod_attempt_force_reader_eagain_once_v6_for_test(true);
    const auto orphaned_error_channel = finish_logic_mod_worker_attempt_v6(attempt);
    check(orphaned_error_channel.terminal_reason == "PROCESS_START_FAILED",
          "held pre-exec error channel was not classified as start failure");
    check(orphaned_error_channel.reaped, "held pre-exec error channel was not reaped");
    check(orphaned_error_channel.elapsed_ms < 800, "held pre-exec error channel bypassed bounded supervision");

    // Delay the actual start-error integer until after a first poll has
    // observed/reaped exit(125).  The active group cleanup signals once; the
    // later error classification must not signal the now-disarmed numeric PGID.
    signal_calls = 0;
    set_logic_mod_attempt_signal_hook_v6_for_test(count_and_signal);
    budgets.output_bytes = 4096;
    set_logic_mod_attempt_force_pdeath_failure_v6_for_test(true);
    prepared = prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    set_logic_mod_attempt_force_pdeath_failure_v6_for_test(false);
    check(wait_for_leader_exit_pending(attempt), "delayed start-error leader did not become observable zombie");
    set_logic_mod_attempt_force_start_error_eagain_once_v6_for_test(true);
    set_logic_mod_attempt_force_reader_eagain_once_v6_for_test(true);
    check(!poll_logic_mod_worker_attempt_v6(attempt).has_value(), "delayed start error did not require second poll");
    const int start_error_active_signals = signal_calls;
    check(start_error_active_signals == 1, "active delayed-start-error cleanup did not signal once");
    const auto delayed_start_error = finish_logic_mod_worker_attempt_v6(attempt);
    check(delayed_start_error.terminal_reason == "PROCESS_START_FAILED" && delayed_start_error.reaped,
          "delayed start error did not terminate cleanly");
    check(signal_calls == start_error_active_signals, "delayed start error signalled a disarmed group");
    set_logic_mod_attempt_signal_hook_v6_for_test(nullptr);

    // The same ordering applies to an output-limit decision.  The descendant
    // writes before cleanup, while the first forced EAGAIN lets leader reaping
    // disarm the group.  Draining that buffered output must not signal again.
    signal_calls = 0;
    set_logic_mod_attempt_signal_hook_v6_for_test(count_and_signal);
    budgets.output_bytes = 1;
    set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(true);
    set_logic_mod_attempt_force_preexec_descendant_output_v6_for_test(true);
    prepared = prepare_logic_mod_worker_v6(bytes, request("3/4 hoch 2"), budgets, false, -1);
    attempt = start_logic_mod_worker_attempt_v6(std::move(prepared));
    set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(false);
    set_logic_mod_attempt_force_preexec_descendant_output_v6_for_test(false);
    check(logic_mod_attempt_wait_for_output_ready_v6_for_test(attempt, 500),
          "output descendant did not confirm buffered output");
    check(wait_for_leader_exit_pending(attempt), "delayed output leader did not become observable zombie");
    set_logic_mod_attempt_force_reader_eagain_once_v6_for_test(true);
    check(!poll_logic_mod_worker_attempt_v6(attempt).has_value(), "delayed output limit did not require second poll");
    const int output_active_signals = signal_calls;
    check(output_active_signals == 1, "active delayed-output cleanup did not signal once");
    const auto delayed_output = finish_logic_mod_worker_attempt_v6(attempt);
    check(delayed_output.terminal_reason == "OUTPUT_LIMIT" && delayed_output.reaped,
          "delayed output limit did not terminate cleanly");
    check(signal_calls == output_active_signals, "delayed output limit signalled a disarmed group");
    set_logic_mod_attempt_signal_hook_v6_for_test(nullptr);
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "worker path required");
        contracts(argv[1]);
        std::cout << "logic mod worker attempt v6 contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
