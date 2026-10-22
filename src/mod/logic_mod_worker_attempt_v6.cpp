#include "logic_mod_worker_attempt_v6.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <fstream>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/memfd.h>
#include <linux/seccomp.h>
#include <limits>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace bruce::logic_mod_worker_v1_internal {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto cancel_grace = std::chrono::milliseconds(1000);
constexpr auto term_grace = std::chrono::milliseconds(50);

struct Fd {
    int n{-1};
    explicit Fd(int value = -1) : n(value) {}
    ~Fd() { if (n >= 0) ::close(n); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : n(other.n) { other.n = -1; }
    Fd& operator=(Fd&& other) noexcept { if (this != &other) { if (n >= 0) ::close(n); n = other.n; other.n = -1; } return *this; }
};
void close_fd(int& fd) { if (fd >= 0) { ::close(fd); fd = -1; } }
void high_fd(Fd& fd) {
    if (fd.n < 0) throw Error("LogicWorkerSetupFailed");
    const int result = ::fcntl(fd.n, F_DUPFD_CLOEXEC, 10);
    if (result < 0) throw Error("LogicWorkerSetupFailed");
    ::close(fd.n);
    fd.n = result;
}
int sealed(std::string_view bytes, bool executable) {
    Fd fd(static_cast<int>(::syscall(SYS_memfd_create, "bruce-logic", MFD_CLOEXEC | MFD_ALLOW_SEALING)));
    high_fd(fd);
    std::size_t offset{};
    while (offset < bytes.size()) {
        const auto written = ::write(fd.n, bytes.data() + offset, bytes.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw Error("LogicWorkerSetupFailed");
        offset += static_cast<std::size_t>(written);
    }
    if (::fchmod(fd.n, executable ? 0500 : 0400) != 0 || ::lseek(fd.n, 0, SEEK_SET) != 0 ||
        ::fcntl(fd.n, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) != 0)
        throw Error("LogicWorkerSetupFailed");
    const int result = fd.n; fd.n = -1; return result;
}
void static_elf(std::string_view bytes) {
    if (bytes.size() < sizeof(Elf64_Ehdr)) throw Error("LogicWorkerRequiresStaticELF");
    Elf64_Ehdr header{}; std::memcpy(&header, bytes.data(), sizeof(header));
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_phentsize != sizeof(Elf64_Phdr) ||
        header.e_phnum > 128 || header.e_phoff > bytes.size() ||
        header.e_phnum > (bytes.size() - header.e_phoff) / sizeof(Elf64_Phdr))
        throw Error("LogicWorkerRequiresStaticELF");
    for (unsigned index{}; index < header.e_phnum; ++index) {
        Elf64_Phdr program{};
        std::memcpy(&program, bytes.data() + header.e_phoff + index * sizeof(program), sizeof(program));
        if (program.p_type == PT_INTERP) throw Error("LogicWorkerRequiresStaticELF");
    }
}
constexpr char execution_path[] = "";
int sandbox(bool has_payload) {
#if defined(__x86_64__)
    constexpr auto architecture = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
    constexpr auto architecture = AUDIT_ARCH_AARCH64;
#else
    constexpr auto architecture = 0U;
    (void)architecture;
    return -1;
#endif
    const sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, arch)), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, architecture, 1, 0), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_readlinkat, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#ifdef SYS_readlink
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_readlink, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_write, 0, 4), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[0])), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 6, 0, 1), BPF_STMT(BPF_RET | BPF_K, has_payload ? SECCOMP_RET_KILL_PROCESS : SECCOMP_RET_ALLOW), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_mmap, 0, 4), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[4])), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 6, 0, 1), BPF_STMT(BPF_RET | BPF_K, has_payload ? SECCOMP_RET_KILL_PROCESS : SECCOMP_RET_ALLOW), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
#define ALLOW(name) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_##name, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
        ALLOW(read), ALLOW(close), ALLOW(fstat), ALLOW(lseek), ALLOW(mprotect), ALLOW(munmap), ALLOW(mremap), ALLOW(brk), ALLOW(rt_sigaction), ALLOW(rt_sigprocmask), ALLOW(rt_sigreturn), ALLOW(sigaltstack), ALLOW(set_tid_address), ALLOW(set_robust_list), ALLOW(rseq), ALLOW(futex), ALLOW(clock_gettime), ALLOW(getrandom), ALLOW(getpid), ALLOW(gettid), ALLOW(sched_yield), ALLOW(exit), ALLOW(exit_group),
#ifdef SYS_arch_prctl
        ALLOW(arch_prctl),
#endif
#undef ALLOW
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_prlimit64, 0, 10), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[0])), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, 0), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[2])), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, 0), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[2]) + 4), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_execveat, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_USER_NOTIF), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_sendmsg, 0, 4), BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[0])), BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 4, 0, 1), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS), BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)
    };
    const sock_fprog program{static_cast<unsigned short>(std::size(filter)), const_cast<sock_filter*>(filter)};
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return -1;
    return static_cast<int>(::syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER, &program));
}
bool transfer_listener(int listener) {
    char marker = 'L'; iovec iov{&marker, 1}; alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{}; message.msg_iov = &iov; message.msg_iovlen = 1; message.msg_control = control; message.msg_controllen = sizeof(control);
    auto* header = CMSG_FIRSTHDR(&message); header->cmsg_level = SOL_SOCKET; header->cmsg_type = SCM_RIGHTS; header->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(header), &listener, sizeof(listener));
    return ::sendmsg(4, &message, MSG_NOSIGNAL) == 1;
}
int receive_listener(int channel) {
    char marker{}; iovec iov{&marker, 1}; alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{}; message.msg_iov = &iov; message.msg_iovlen = 1; message.msg_control = control; message.msg_controllen = sizeof(control);
    const auto received = ::recvmsg(channel, &message, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (received < 0 && (errno == EAGAIN || errno == EINTR)) return -1;
    if (received == 0) return -2;
    auto* header = CMSG_FIRSTHDR(&message);
    if (received != 1 || marker != 'L' || (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) || !header ||
        header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS || header->cmsg_len != CMSG_LEN(sizeof(int)))
        throw Error("LogicExecGateSetupFailed");
    int listener{-1}; std::memcpy(&listener, CMSG_DATA(header), sizeof(listener)); return listener;
}
bool authorize_initial_exec(int listener, pid_t child, bool& granted) {
    pollfd ready{listener, POLLIN, 0}; if (::poll(&ready, 1, 0) <= 0 || !(ready.revents & POLLIN)) return true;
    seccomp_notif notification{};
    if (::ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &notification) != 0) {
        if (errno == EINTR || errno == ENOENT) return true;
        throw Error("LogicExecGateFailed");
    }
    if (granted || notification.pid != static_cast<unsigned>(child) || notification.data.nr != SYS_execveat ||
        notification.data.args[0] != 3 || notification.data.args[1] != reinterpret_cast<std::uintptr_t>(execution_path) ||
        notification.data.args[4] != AT_EMPTY_PATH) return false;
    seccomp_notif_resp response{}; response.id = notification.id; response.flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE; granted = true;
    if (::ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &response) != 0) throw Error("LogicExecGateFailed");
    return true;
}
bool limit(int resource, rlim_t value) { const rlimit limits{value, value}; return ::setrlimit(resource, &limits) == 0; }
struct Directory {
    char path[40] = "/tmp/bruce-logic-run-XXXXXX";
    Directory() { if (!::mkdtemp(path)) throw Error("LogicWorkerSetupFailed"); }
    ~Directory() { ::rmdir(path); }
};
std::optional<std::uint64_t> proc_peak(pid_t pid) {
    std::ifstream input("/proc/" + std::to_string(pid) + "/status");
    if (!input) return std::nullopt;
    std::string line; std::optional<std::uint64_t> maximum;
    while (std::getline(input, line)) {
        if (!line.starts_with("VmHWM:") && !line.starts_with("VmRSS:")) continue;
        const auto begin = line.find_first_of("0123456789"); if (begin == std::string::npos) continue;
        char* end{}; errno = 0; const auto kib = std::strtoull(line.c_str() + begin, &end, 10);
        if (errno == 0 && end != line.c_str() + begin && kib <= std::numeric_limits<std::uint64_t>::max() / 1024) {
            const auto bytes = static_cast<std::uint64_t>(kib) * 1024;
            maximum = maximum ? std::max(*maximum, bytes) : bytes;
        }
    }
    return maximum;
}
LogicModAttemptSignalHookV6 signal_hook{};
bool force_pdeath_failure{};
bool force_preexec_exit_with_error_descendant{};
bool force_preexec_descendant_output{};
bool force_reader_eagain_once{};
bool force_start_error_eagain_once{};
int signal_group_once(pid_t pgid, int signal) {
    if (pgid <= 0) return 0;
    return signal_hook ? signal_hook(-pgid, signal) : ::kill(-pgid, signal);
}
void cleanup_group(pid_t pgid) noexcept { (void)signal_group_once(pgid, SIGKILL); }
} // namespace

struct LogicModPreparedWorkerV6::State {
    Fd executable, request, payload;
    LogicModBudgetsV1 budgets{};
    bool has_payload{};
    std::unique_ptr<Directory> directory;
};
struct LogicModWorkerAttemptV6::State {
    pid_t pid{-1}, pgid{-1}; Fd reader, channel, listener, start_error, test_output_ready;
    std::unique_ptr<Directory> directory;
    Clock::time_point started{Clock::now()}, deadline{};
    Clock::time_point cancel_started{}, term_started{};
    LogicModBudgetsV1 budgets{};
    std::string response; int exit_status{};
    bool child_exited{}, eof{}, exec_granted{}, cancel_requested{}, group_bound{}, group_signal_armed{}, term_sent{}, kill_sent{}, reaped{};
    std::optional<std::uint64_t> peak;
    std::optional<LogicModAttemptOutcomeV6> outcome;
    ~State() noexcept {
        if (pid <= 0) return;
        if (!reaped) {
            if (group_bound && group_signal_armed) cleanup_group(pgid);
            else if (!group_bound) (void)::kill(pid, SIGKILL);
            int status{}; while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }
    }
};

LogicModPreparedWorkerV6::LogicModPreparedWorkerV6() = default;
LogicModPreparedWorkerV6::LogicModPreparedWorkerV6(std::unique_ptr<State> state) : state_(std::move(state)) {}
LogicModPreparedWorkerV6::~LogicModPreparedWorkerV6() = default;
LogicModPreparedWorkerV6::LogicModPreparedWorkerV6(LogicModPreparedWorkerV6&&) noexcept = default;
LogicModPreparedWorkerV6& LogicModPreparedWorkerV6::operator=(LogicModPreparedWorkerV6&&) noexcept = default;
LogicModWorkerAttemptV6::LogicModWorkerAttemptV6() = default;
LogicModWorkerAttemptV6::LogicModWorkerAttemptV6(std::unique_ptr<State> state) : state_(std::move(state)) {}
LogicModWorkerAttemptV6::~LogicModWorkerAttemptV6() = default;
LogicModWorkerAttemptV6::LogicModWorkerAttemptV6(LogicModWorkerAttemptV6&&) noexcept = default;
LogicModWorkerAttemptV6& LogicModWorkerAttemptV6::operator=(LogicModWorkerAttemptV6&&) noexcept = default;

std::optional<std::uint64_t> logic_mod_peak_from_rusage_kib_v6(std::uint64_t kib) {
    if (kib == 0 || kib > std::numeric_limits<std::uint64_t>::max() / 1024) return std::nullopt;
    return kib * 1024;
}
bool cleanup_logic_mod_process_group_v6_for_test(pid_t pgid) { cleanup_group(pgid); return true; }
void set_logic_mod_attempt_signal_hook_v6_for_test(LogicModAttemptSignalHookV6 hook) { signal_hook = hook; }
void set_logic_mod_attempt_force_pdeath_failure_v6_for_test(bool enabled) { force_pdeath_failure = enabled; }
void set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(bool enabled) {
    force_preexec_exit_with_error_descendant = enabled;
}
void set_logic_mod_attempt_force_preexec_descendant_output_v6_for_test(bool enabled) {
    force_preexec_descendant_output = enabled;
}
void set_logic_mod_attempt_force_reader_eagain_once_v6_for_test(bool enabled) { force_reader_eagain_once = enabled; }
void set_logic_mod_attempt_force_start_error_eagain_once_v6_for_test(bool enabled) { force_start_error_eagain_once = enabled; }
bool logic_mod_attempt_leader_exit_pending_v6_for_test(const LogicModWorkerAttemptV6& attempt) {
    if (!attempt.state_ || attempt.state_->reaped || attempt.state_->pid <= 0) return false;
    siginfo_t info{};
    return ::waitid(P_PID, static_cast<id_t>(attempt.state_->pid), &info, WEXITED | WNOHANG | WNOWAIT) == 0 &&
           info.si_pid == attempt.state_->pid;
}
bool logic_mod_attempt_wait_for_output_ready_v6_for_test(LogicModWorkerAttemptV6& attempt, int timeout_ms) {
    if (!attempt.state_ || attempt.state_->test_output_ready.n < 0) return false;
    pollfd ready{attempt.state_->test_output_ready.n, POLLIN | POLLHUP, 0};
    if (::poll(&ready, 1, timeout_ms) <= 0) return false;
    char marker{};
    const auto read = ::read(attempt.state_->test_output_ready.n, &marker, 1);
    attempt.state_->test_output_ready = Fd{};
    return read == 1 && marker == 'O';
}

LogicModPreparedWorkerV6 prepare_logic_mod_worker_v6(std::string_view worker_bytes, std::string_view request_frame,
    LogicModBudgetsV1 budgets, bool has_payload, int payload_fd) {
    static_elf(worker_bytes);
    if (!budgets.wall_ms || !budgets.memory_bytes || !budgets.output_bytes)
        throw Error("LogicWorkerSetupFailed");
    auto state = std::make_unique<LogicModPreparedWorkerV6::State>();
    state->budgets = budgets; state->has_payload = has_payload; state->directory = std::make_unique<Directory>();
    state->executable = Fd(sealed(worker_bytes, true)); state->request = Fd(sealed(request_frame, false));
    if (has_payload) { const int copy = ::fcntl(payload_fd, F_DUPFD_CLOEXEC, 10); if (copy < 0) throw Error("LogicWorkerSetupFailed"); state->payload = Fd(copy); }
    return LogicModPreparedWorkerV6(std::move(state));
}

LogicModWorkerAttemptV6 start_logic_mod_worker_attempt_v6(LogicModPreparedWorkerV6&& prepared) {
    if (!prepared.state_) throw Error("InvalidLogicModPreparedWorker");
    auto setup = std::move(prepared.state_);
    auto directory = std::move(setup->directory);
    int pipe_fds[2]{-1, -1}; int channel_fds[2]{-1, -1}; int error_fds[2]{-1, -1}; int test_ready_fds[2]{-1, -1};
    if (::pipe2(pipe_fds, O_CLOEXEC) != 0 || ::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, channel_fds) != 0 || ::pipe2(error_fds, O_CLOEXEC) != 0 ||
        (force_preexec_descendant_output && ::pipe2(test_ready_fds, O_CLOEXEC) != 0)) {
        close_fd(pipe_fds[0]); close_fd(pipe_fds[1]); close_fd(channel_fds[0]); close_fd(channel_fds[1]); close_fd(error_fds[0]); close_fd(error_fds[1]); close_fd(test_ready_fds[0]); close_fd(test_ready_fds[1]); throw Error("LogicWorkerSetupFailed");
    }
    Fd reader(pipe_fds[0]), writer(pipe_fds[1]), channel(channel_fds[0]), child_channel(channel_fds[1]),
        start_error(error_fds[0]), child_error(error_fds[1]), test_ready(test_ready_fds[0]), child_test_ready(test_ready_fds[1]);
    pipe_fds[0] = pipe_fds[1] = channel_fds[0] = channel_fds[1] = error_fds[0] = error_fds[1] = test_ready_fds[0] = test_ready_fds[1] = -1;
    high_fd(reader); high_fd(writer); high_fd(channel); high_fd(child_channel); high_fd(start_error); high_fd(child_error);
    if (test_ready.n >= 0) { high_fd(test_ready); high_fd(child_test_ready); }
    auto state = std::make_unique<LogicModWorkerAttemptV6::State>();
    state->reader = std::move(reader); state->channel = std::move(channel); state->start_error = std::move(start_error); state->test_output_ready = std::move(test_ready);
    state->directory = std::move(directory); state->budgets = setup->budgets;
    state->started = Clock::now(); state->deadline = state->started + std::chrono::milliseconds(setup->budgets.wall_ms);
    const auto parent = ::getpid();
    const auto pid = ::fork();
    if (pid < 0) throw Error("LogicWorkerSpawnFailed");
    if (pid == 0) {
        const auto fail_from = [](int fd) { const int saved = errno; (void)::write(fd, &saved, sizeof(saved)); _exit(125); };
        if (::dup3(child_error.n, 7, O_CLOEXEC) < 0) fail_from(child_error.n);
        const auto fail_child = [] { const int saved = errno; (void)::write(7, &saved, sizeof(saved)); _exit(125); };
        if (::setpgid(0, 0) != 0) fail_child();
        if (force_preexec_exit_with_error_descendant) {
            const auto descendant = ::fork();
            if (descendant < 0) fail_child();
            if (descendant == 0) {
                for (int fd = 0; fd != 64; ++fd)
                    if (fd != 7 && (!force_preexec_descendant_output || (fd != writer.n && fd != child_test_ready.n))) ::close(fd);
                if (force_preexec_descendant_output) {
                    static constexpr char output[] = "exceeds-output-budget";
                    (void)::write(writer.n, output, sizeof(output) - 1);
                    static constexpr char marker = 'O';
                    (void)::write(child_test_ready.n, &marker, 1);
                    ::close(child_test_ready.n);
                }
                for (;;) ::pause();
            }
            _exit(125);
        }
        if (force_pdeath_failure || ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || ::getppid() != parent ||
            ::dup2(setup->request.n, 0) < 0 || ::dup2(writer.n, 1) < 0 || ::dup2(writer.n, 2) < 0 ||
            ::dup3(setup->executable.n, 3, O_CLOEXEC) < 0 || ::dup3(child_channel.n, 4, O_CLOEXEC) < 0 ||
            (setup->has_payload && ::dup2(setup->payload.n, 6) < 0) ||
            ::syscall(SYS_close_range, 8U, UINT_MAX, 0U) != 0 ||
            ::chdir(state->directory->path) != 0 || !limit(RLIMIT_AS, setup->budgets.memory_bytes) ||
            !limit(RLIMIT_CPU, (setup->budgets.wall_ms + 999) / 1000) || !limit(RLIMIT_FSIZE, 0) || !limit(RLIMIT_CORE, 0)) fail_child();
        if ((::close(5) != 0 && errno != EBADF) || (!setup->has_payload && ::close(6) != 0 && errno != EBADF)) fail_child();
        // FD 7 carries only pre-exec errors and must survive exec failure;
        // fd 8 is temporarily needed for the seccomp listener transfer.
        if (!limit(RLIMIT_NOFILE, 9)) fail_child();
        const int listener = sandbox(setup->has_payload);
        if (listener < 0 || !transfer_listener(listener)) fail_child();
        ::close(listener); ::close(4);
        char name[] = "logic-worker-v1"; char* const args[] = {name, nullptr}; char* const environment[] = {nullptr};
        ::syscall(SYS_execveat, 3, execution_path, args, environment, AT_EMPTY_PATH); fail_child();
    }
    state->pid = pid; state->pgid = pid;
    writer = Fd{}; child_channel = Fd{}; child_error = Fd{}; child_test_ready = Fd{};
    if (::setpgid(pid, pid) != 0 && errno != EACCES) { (void)::kill(pid, SIGKILL); throw Error("LogicSupervisionUnavailable"); }
    if (::getpgid(pid) != pid) { (void)::kill(pid, SIGKILL); throw Error("LogicSupervisionUnavailable"); }
    state->group_bound = true; state->group_signal_armed = true;
    const int reader_flags = ::fcntl(state->reader.n, F_GETFL);
    if (reader_flags < 0 || ::fcntl(state->reader.n, F_SETFL, reader_flags | O_NONBLOCK) != 0) {
        throw Error("LogicWorkerSetupFailed");
    }
    const int error_flags = ::fcntl(state->start_error.n, F_GETFL);
    if (error_flags < 0 || ::fcntl(state->start_error.n, F_SETFL, error_flags | O_NONBLOCK) != 0) {
        throw Error("LogicWorkerSetupFailed");
    }
    return LogicModWorkerAttemptV6(std::move(state));
}

namespace {
void signal_owned_group(LogicModWorkerAttemptV6::State& state, int signal) {
    if (state.group_bound && state.group_signal_armed) (void)signal_group_once(state.pgid, signal);
}
void reap(LogicModWorkerAttemptV6::State& state) {
    if (state.reaped || state.pid <= 0) return;
    rusage usage{}; const auto waited = ::wait4(state.pid, &state.exit_status, WNOHANG, &usage);
    if (waited == 0) return;
    if (waited < 0) { if (errno == EINTR) return; throw Error("LogicSupervisionUnavailable"); }
    state.child_exited = true; state.reaped = true;
    state.group_signal_armed = false;
    if (const auto bytes = logic_mod_peak_from_rusage_kib_v6(static_cast<std::uint64_t>(usage.ru_maxrss)))
        state.peak = state.peak ? std::max(*state.peak, *bytes) : bytes;
}
bool exit_pending_without_reap(const LogicModWorkerAttemptV6::State& state) {
    if (state.reaped || state.pid <= 0) return false;
    siginfo_t info{};
    for (;;) {
        if (::waitid(P_PID, static_cast<id_t>(state.pid), &info, WEXITED | WNOHANG | WNOWAIT) == 0)
            return info.si_pid == state.pid;
        if (errno != EINTR) throw Error("LogicSupervisionUnavailable");
    }
}
void reap_blocking(LogicModWorkerAttemptV6::State& state) {
    while (!state.reaped) {
        reap(state);
        if (!state.reaped) ::usleep(1000);
    }
}
LogicModAttemptOutcomeV6 terminal(LogicModWorkerAttemptV6::State& state, std::string reason) {
    LogicModAttemptOutcomeV6 out; out.response_frame = std::move(state.response); out.response_digest = sha256(out.response_frame);
    out.terminal_reason = std::move(reason); out.elapsed_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - state.started).count());
    out.peak_memory_bytes = state.peak; out.peak_memory_availability = state.peak ? LogicModPeakMemoryAvailabilityV6::OBSERVED : LogicModPeakMemoryAvailabilityV6::UNAVAILABLE;
    out.reaped = state.reaped;
    out.cancel_requested = state.cancel_requested;
    out.term_sent = state.term_sent;
    out.kill_sent = state.kill_sent;
    if (out.terminal_reason == "READY") out.worker_status.clear();
    else if (state.child_exited && WIFSIGNALED(state.exit_status)) out.worker_status = "WORKER_SIGNAL_" + std::to_string(WTERMSIG(state.exit_status));
    else if (state.child_exited && WIFEXITED(state.exit_status)) out.worker_status = "WORKER_EXIT_" + std::to_string(WEXITSTATUS(state.exit_status));
    else out.worker_status = out.terminal_reason;
    state.outcome = out; return out;
}
}

std::optional<LogicModAttemptOutcomeV6> poll_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt) {
    if (!attempt.state_) throw Error("InvalidLogicModWorkerAttempt");
    auto& state = *attempt.state_;
    if (state.outcome) return state.outcome;
    bool start_error_open = state.start_error.n >= 0;
    if (start_error_open) {
        int child_setup_error{};
        ssize_t setup_read{};
        if (force_start_error_eagain_once) {
            force_start_error_eagain_once = false;
            errno = EAGAIN;
            setup_read = -1;
        } else {
            setup_read = ::read(state.start_error.n, &child_setup_error, sizeof(child_setup_error));
        }
        if (setup_read == static_cast<ssize_t>(sizeof(child_setup_error))) {
            signal_owned_group(state, SIGKILL);
            reap_blocking(state);
            return terminal(state, "PROCESS_START_FAILED");
        }
        if (setup_read == 0) { state.start_error = Fd{}; start_error_open = false; }
        // FD 7 remains open until either fail_child reports a setup error or
        // exec closes it atomically (CLOEXEC).  The seccomp exec gate itself
        // runs before exec and therefore must still be serviced while this
        // channel is open.  A closed gate channel is deferred below until FD
        // 7 resolves the race in favor of a setup error or successful exec.
        else if (setup_read < 0 && (errno == EAGAIN || errno == EINTR)) {}
        else if (setup_read < 0 && errno != EAGAIN && errno != EINTR) {
            signal_owned_group(state, SIGKILL);
            reap_blocking(state);
            return terminal(state, "SUPERVISION_UNAVAILABLE");
        }
    }
    if (const auto observed = proc_peak(state.pid)) state.peak = state.peak ? std::max(*state.peak, *observed) : observed;
    bool gate_eof{};
    if (state.listener.n < 0) {
        const int received_listener = receive_listener(state.channel.n);
        if (received_listener == -2) {
            gate_eof = true;
            // If FD 7 is still open, its owner can be a pre-exec descendant.
            // Continue through output, wait4 and deadline supervision below;
            // the direct leader may already have exited with the start-failure
            // code even though neither channel will reach EOF on its own.
        }
        if (received_listener >= 0) state.listener = Fd(received_listener);
    }
    if (state.listener.n >= 0 && !authorize_initial_exec(state.listener.n, state.pid, state.exec_granted)) {
        signal_owned_group(state, SIGKILL); reap_blocking(state); return terminal(state, "EXEC_DENIED");
    }
    char bytes[4096];
    for (;;) {
        if (force_reader_eagain_once) {
            force_reader_eagain_once = false;
            break;
        }
        const auto count = ::read(state.reader.n, bytes, sizeof(bytes));
        if (count > 0) {
            if (state.response.size() + static_cast<std::size_t>(count) > state.budgets.output_bytes) {
                signal_owned_group(state, SIGKILL); reap_blocking(state); return terminal(state, "OUTPUT_LIMIT");
            }
            state.response.append(bytes, static_cast<std::size_t>(count)); continue;
        }
        if (count == 0) state.eof = true;
        else if (errno != EAGAIN && errno != EINTR) { signal_owned_group(state, SIGKILL); reap_blocking(state); return terminal(state, "PROCESS_CRASHED"); }
        break;
    }
    // The response bytes have been drained above.  Once the leader is a
    // zombie its PID/PGID is still identity-pinned, so remove every possible
    // descendant before the single wait4 disarms future group signalling.
    if (state.group_signal_armed && !state.kill_sent && exit_pending_without_reap(state)) {
        cleanup_group(state.pgid);
        state.kill_sent = true;
    }
    reap(state);
    if (state.child_exited && state.eof) {
        if (state.cancel_requested) return terminal(state, "HOST_CANCELLED");
        if (!state.exec_granted && WIFEXITED(state.exit_status) && WEXITSTATUS(state.exit_status) == 125)
            return terminal(state, "PROCESS_START_FAILED");
        if (!WIFEXITED(state.exit_status) || WEXITSTATUS(state.exit_status) != 0) return terminal(state, "PROCESS_CRASHED");
        return terminal(state, "READY");
    }
    // A gate EOF is a setup failure only while the direct leader is still
    // non-terminal.  Once it has been reaped, drain the response pipe first:
    // group cleanup can make EOF observable only on this later poll, and an
    // exit(125) then has the more precise PROCESS_START_FAILED classification.
    if (gate_eof && !start_error_open && !state.reaped)
        throw Error("LogicExecGateSetupFailed");
    const auto now = Clock::now();
    if (!state.child_exited && !state.cancel_requested && now >= state.deadline) {
        cleanup_group(state.pgid);
        reap_blocking(state);
        return terminal(state, "WALL_TIME_LIMIT");
    }
    if (state.cancel_requested && !state.child_exited && state.cancel_started == Clock::time_point{}) state.cancel_started = now;
    if (state.cancel_requested && !state.child_exited && now - state.cancel_started >= cancel_grace && !state.term_sent) { signal_owned_group(state, SIGTERM); state.term_sent = true; state.term_started = now; }
    if (state.term_sent && !state.child_exited && now - state.term_started >= term_grace && !state.kill_sent) { signal_owned_group(state, SIGKILL); state.kill_sent = true; }
    return std::nullopt;
}

void cancel_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt) {
    if (!attempt.state_) throw Error("InvalidLogicModWorkerAttempt");
    auto& state = *attempt.state_;
    // Logic-Mod has no cancel frame.  This is host-only intent: polling waits
    // the fixed grace, then sends TERM and finally KILL to the owned group.
    if (!state.outcome && !state.cancel_requested) { state.cancel_requested = true; state.cancel_started = Clock::now(); }
}

LogicModAttemptOutcomeV6 finish_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt) {
    if (!attempt.state_) throw Error("InvalidLogicModWorkerAttempt");
    for (;;) {
        if (const auto outcome = poll_logic_mod_worker_attempt_v6(attempt)) return *outcome;
        ::usleep(1000);
    }
}

} // namespace bruce::logic_mod_worker_v1_internal
