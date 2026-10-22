#include "bruce/worker/worker_supervisor_v1.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <climits>
#include <limits>
#include <poll.h>
#include <pthread.h>
#include <sstream>
#include <string_view>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace bruce::worker_v1 {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t max_metadata = 1048576;
constexpr std::size_t max_result_frame = 65536;
constexpr std::size_t max_poll_read_bytes = 32768;
constexpr std::size_t max_poll_control_frames = 32;
constexpr std::size_t max_attempt_control_frames = 1024;
constexpr std::size_t max_attempt_control_bytes = 1048576;
constexpr auto fixed_grace = std::chrono::milliseconds(1000);
constexpr auto term_grace = std::chrono::milliseconds(50);

bool token(std::string_view value) {
    if (value.empty() || value.size() > 128) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
bool digest(std::string_view value) {
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool relative_ref(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& part : path) if (part == ".." || part == ".") return false;
    return true;
}
bool same_budget(const Budget& a, const Budget& b) {
    return a.max_wall_ms == b.max_wall_ms && a.max_input_bytes == b.max_input_bytes &&
           a.max_output_bytes == b.max_output_bytes && a.max_operations == b.max_operations &&
           a.max_memory_bytes == b.max_memory_bytes;
}
bool valid_budget(const Budget& b) {
    return b.max_wall_ms > 0 && b.max_wall_ms <= 600000 &&
           b.max_input_bytes > 0 && b.max_input_bytes <= 1048576 &&
           b.max_output_bytes > 0 && b.max_output_bytes <= 5242880 &&
           b.max_operations > 0 && b.max_operations <= 10000000 &&
           b.max_memory_bytes > 0 && b.max_memory_bytes <= 2147483648ULL;
}
void field(std::string& out, std::string_view name, std::string_view value) {
    if (value.size() > max_metadata - 32 || out.size() > max_metadata - 32 - value.size())
        throw bruce::Error("InvalidWorkerPayload");
    out.append(name).push_back('=');
    out.append(std::to_string(value.size())).push_back('\n');
    out.append(value);
}
void number(std::string& out, std::string_view name, std::uint64_t value) {
    field(out, name, std::to_string(value));
}
class Reader {
public:
    explicit Reader(std::string_view data) : data_(data) {
        if (data.size() > max_metadata) throw bruce::Error("InvalidWorkerPayload");
    }
    std::string_view field(std::string_view name) {
        if (!data_.substr(pos_).starts_with(name) || pos_ + name.size() >= data_.size() ||
            data_[pos_ + name.size()] != '=') throw bruce::Error("InvalidWorkerPayload");
        pos_ += name.size() + 1;
        const auto end = data_.find('\n', pos_);
        if (end == std::string_view::npos || end - pos_ > 8) throw bruce::Error("InvalidWorkerPayload");
        const auto len = parse_uint(data_.substr(pos_, end - pos_));
        pos_ = end + 1;
        if (len > data_.size() - pos_) throw bruce::Error("InvalidWorkerPayload");
        const auto result = data_.substr(pos_, static_cast<std::size_t>(len));
        pos_ += static_cast<std::size_t>(len);
        return result;
    }
    std::uint64_t number(std::string_view name) { return parse_uint(field(name)); }
    void finish() const { if (pos_ != data_.size()) throw bruce::Error("InvalidWorkerPayload"); }
private:
    static std::uint64_t parse_uint(std::string_view text) {
        if (text.empty() || (text.size() > 1 && text.front() == '0')) throw bruce::Error("InvalidWorkerPayload");
        std::uint64_t value{};
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || end != text.data() + text.size()) throw bruce::Error("InvalidWorkerPayload");
        return value;
    }
    std::string_view data_;
    std::size_t pos_{};
};

std::string_view status_name(WorkerStatus value) {
    switch (value) {
    case WorkerStatus::READY: return "READY";
    case WorkerStatus::FAILED: return "FAILED";
    case WorkerStatus::BLOCKED: return "BLOCKED";
    case WorkerStatus::CANCELLED: return "CANCELLED";
    }
    throw bruce::Error("InvalidWorkerStatus");
}
WorkerStatus parse_status(std::string_view value) {
    for (const auto status : {WorkerStatus::READY, WorkerStatus::FAILED,
                              WorkerStatus::BLOCKED, WorkerStatus::CANCELLED})
        if (status_name(status) == value) return status;
    throw bruce::Error("InvalidWorkerStatus");
}
std::string_view code_name(FailureCode value) {
    switch (value) {
    case FailureCode::NONE: return "NONE";
    case FailureCode::READY: return "READY";
    case FailureCode::FAILED: return "FAILED";
    case FailureCode::BLOCKED: return "BLOCKED";
    case FailureCode::INVALID_REQUEST: return "INVALID_REQUEST";
    case FailureCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
    case FailureCode::PROCESS_START_FAILED: return "PROCESS_START_FAILED";
    case FailureCode::PROCESS_CRASHED: return "PROCESS_CRASHED";
    case FailureCode::TIMEOUT: return "TIMEOUT";
    case FailureCode::CANCELLED: return "CANCELLED";
    case FailureCode::INPUT_LIMIT_EXCEEDED: return "INPUT_LIMIT_EXCEEDED";
    case FailureCode::OUTPUT_LIMIT_EXCEEDED: return "OUTPUT_LIMIT_EXCEEDED";
    case FailureCode::OPERATION_BUDGET_EXCEEDED: return "OPERATION_BUDGET_EXCEEDED";
    case FailureCode::MEMORY_LIMIT_EXCEEDED: return "MEMORY_LIMIT_EXCEEDED";
    case FailureCode::DIGEST_MISMATCH: return "DIGEST_MISMATCH";
    }
    throw bruce::Error("InvalidWorkerFailureCode");
}
FailureCode parse_code(std::string_view value) {
    for (const auto code : {FailureCode::NONE, FailureCode::READY, FailureCode::FAILED,
                            FailureCode::BLOCKED, FailureCode::INVALID_REQUEST,
                            FailureCode::PROTOCOL_ERROR, FailureCode::PROCESS_START_FAILED,
                            FailureCode::PROCESS_CRASHED, FailureCode::TIMEOUT,
                            FailureCode::CANCELLED, FailureCode::INPUT_LIMIT_EXCEEDED,
                            FailureCode::OUTPUT_LIMIT_EXCEEDED, FailureCode::OPERATION_BUDGET_EXCEEDED,
                            FailureCode::MEMORY_LIMIT_EXCEEDED, FailureCode::DIGEST_MISMATCH})
        if (code_name(code) == value) return code;
    throw bruce::Error("InvalidWorkerFailureCode");
}
void close_fd(int& fd) { if (fd >= 0) { ::close(fd); fd = -1; } }
void nonblock(int fd) { (void)::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK); }
void cloexec(int fd) { (void)::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC); }
void signal_group(pid_t pid, int signal) { if (pid > 0) (void)::kill(-pid, signal); }
ssize_t write_pipe(int fd, const char* data, std::size_t size) {
    sigset_t block{}, old{}, pending{};
    ::sigemptyset(&block);
    ::sigaddset(&block, SIGPIPE);
    if (::pthread_sigmask(SIG_BLOCK, &block, &old) != 0) return -1;
    (void)::sigpending(&pending);
    const bool already_pending = ::sigismember(&pending, SIGPIPE) == 1;
    const auto n = ::write(fd, data, size);
    const int saved_errno = errno;
    if (n < 0 && saved_errno == EPIPE && !already_pending) {
        const timespec zero{};
        (void)::sigtimedwait(&block, nullptr, &zero);
    }
    (void)::pthread_sigmask(SIG_SETMASK, &old, nullptr);
    errno = saved_errno;
    return n;
}
std::uint64_t elapsed_ms(Clock::time_point since) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count());
}
WorkerAttemptOutcome failure(FailureCode code) {
    WorkerAttemptOutcome out;
    out.result.status = code == FailureCode::CANCELLED ? WorkerStatus::CANCELLED : WorkerStatus::FAILED;
    out.result.failure_code = code;
    out.result.result_code = std::string(code_name(code));
    return out;
}
bool read_regular_fd(int fd, std::uint64_t cap, std::string& bytes) {
    if (fd < 0) return false;
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        static_cast<std::uint64_t>(st.st_size) > cap) { ::close(fd); return false; }
    bytes.clear();
    bytes.reserve(static_cast<std::size_t>(st.st_size));
    char chunk[8192];
    while (bytes.size() <= cap) {
        const auto n = ::read(fd, chunk, sizeof(chunk));
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; ::close(fd); return false; }
        bytes.append(chunk, static_cast<std::size_t>(n));
    }
    ::close(fd);
    return bytes.size() <= cap;
}
bool read_artifact(const std::filesystem::path& path, std::uint64_t cap,
                   std::string& bytes) {
    return read_regular_fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK),
                           cap, bytes);
}
int open_output(const std::filesystem::path& root, std::string_view attempt_id) {
    const int root_fd = ::open(root.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) return -1;
    const int attempts_fd = ::openat(root_fd, "attempts", O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    ::close(root_fd);
    if (attempts_fd < 0) return -1;
    const std::string id(attempt_id);
    const int attempt_fd = ::openat(attempts_fd, id.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    ::close(attempts_fd);
    if (attempt_fd < 0) return -1;
    const int output_fd = ::openat(attempt_fd, "output", O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    ::close(attempt_fd);
    if (output_fd < 0) return -1;
    const int result_fd = ::openat(output_fd, "result.bin", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    ::close(output_fd);
    return result_fd;
}
int mkdir_private_at(int parent, const char* name, bool existing_allowed) {
    if (::mkdirat(parent, name, 0700) != 0 &&
        (errno != EEXIST || !existing_allowed)) return -1;
    const int fd = ::openat(parent, name, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) return -1;
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() ||
        (st.st_mode & 0777) != 0700) { ::close(fd); return -1; }
    return fd;
}
bool make_attempt_dirs(const std::filesystem::path& root, std::string_view attempt_id) {
    const int root_fd = ::open(root.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) return false;
    const int attempts_fd = mkdir_private_at(root_fd, "attempts", true);
    ::close(root_fd);
    if (attempts_fd < 0) return false;
    const std::string id(attempt_id);
    const int attempt_fd = mkdir_private_at(attempts_fd, id.c_str(), false);
    ::close(attempts_fd);
    if (attempt_fd < 0) return false;
    const int output_fd = mkdir_private_at(attempt_fd, "output", false);
    const int control_fd = mkdir_private_at(attempt_fd, "control", false);
    ::close(attempt_fd);
    if (output_fd >= 0) ::close(output_fd);
    if (control_fd >= 0) ::close(control_fd);
    return output_fd >= 0 && control_fd >= 0;
}
std::uint64_t proc_peak_bytes(pid_t pid) {
    std::ifstream in("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    std::uint64_t peak{};
    while (std::getline(in, line)) {
        if (!line.starts_with("VmHWM:") && !line.starts_with("VmRSS:")) continue;
        const auto first = line.find_first_of("0123456789");
        if (first == std::string::npos) continue;
        std::uint64_t kib{};
        const auto [end, ec] = std::from_chars(line.data() + first, line.data() + line.size(), kib);
        if (ec == std::errc{} && end != line.data() + first &&
            kib <= std::numeric_limits<std::uint64_t>::max() / 1024)
            peak = std::max(peak, kib * 1024);
    }
    return peak;
}
bool group_has_live_members(pid_t group) {
    std::error_code ec;
    std::filesystem::directory_iterator it("/proc", ec), end;
    if (ec) return true;
    for (; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; }))
            continue;
        std::ifstream in(it->path() / "stat");
        std::string line; std::getline(in, line);
        const auto close = line.rfind(") ");
        if (close == std::string::npos) continue;
        std::istringstream fields(line.substr(close + 2));
        char state{}; long parent{}, process_group{};
        if (fields >> state >> parent >> process_group && process_group == group &&
            state != 'Z' && state != 'X') return true;
    }
    return static_cast<bool>(ec);
}
bool cleanup_group(pid_t group) {
    signal_group(group, SIGKILL);
    const auto deadline = Clock::now() + std::chrono::milliseconds(500);
    do {
        if (!group_has_live_members(group)) return true;
        ::usleep(1000);
        signal_group(group, SIGKILL);
    } while (Clock::now() < deadline);
    return !group_has_live_members(group);
}
// A separate process enforces the monotonic wall deadline even if the parent
// does not poll.  Its control pipe takes precedence over an expiring deadline.
[[noreturn]] void run_watchdog(pid_t worker, pid_t parent, Clock::time_point deadline,
                               int control, int result) {
    if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || ::getppid() != parent) std::_Exit(1);
    const int pidfd = static_cast<int>(::syscall(SYS_pidfd_open, worker, 0));
    std::array<int, 3> retained{control, result, pidfd};
    std::sort(retained.begin(), retained.end());
    unsigned first{};
    bool closed = true;
    for (const int fd : retained) {
        if (fd < 0) continue;
        if (first < static_cast<unsigned>(fd) &&
            ::syscall(SYS_close_range, first, static_cast<unsigned>(fd - 1), 0U) != 0)
            closed = false;
        first = static_cast<unsigned>(fd + 1);
    }
    if (::syscall(SYS_close_range, first, UINT_MAX, 0U) != 0) closed = false;
    if (!closed) {
        struct rlimit fd_limit{};
        const auto maxfd = ::getrlimit(RLIMIT_NOFILE, &fd_limit) == 0 ?
            std::min<rlim_t>(fd_limit.rlim_cur, INT_MAX) : 65536;
        for (int fd = 0; fd < static_cast<int>(maxfd); ++fd)
            if (fd != control && fd != result && fd != pidfd) ::close(fd);
    }
    const auto report = [&](char code) {
        (void)write_pipe(result, &code, 1);
        std::_Exit(0);
    };
    if (pidfd < 0) report('E');
    for (;;) {
        pollfd fds[2]{{control, POLLIN | POLLHUP, 0}, {pidfd, POLLIN, 0}};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        const int timeout = remaining.count() <= 0 ? 0 :
            static_cast<int>(std::min<std::int64_t>(remaining.count(), INT_MAX));
        const int ready = ::poll(fds, 2, timeout);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) report('E');
        if (fds[0].revents & (POLLIN | POLLHUP)) {
            char command{};
            if (::read(control, &command, 1) == 1 && command == 'X') report('A');
            report('E');
        }
        if (fds[1].revents & POLLIN) report('C');
        if (Clock::now() >= deadline) {
            signal_group(worker, SIGTERM);
            ::usleep(50000);
            signal_group(worker, SIGKILL);
            report('T');
        }
    }
}
} // namespace

std::string encode_request_payload(const WorkerRequest& r) {
    std::string out;
    field(out, "run_id", r.run_id); field(out, "attempt_id", r.attempt_id);
    field(out, "node_id", r.node_id); field(out, "adapter_id", r.adapter_id);
    field(out, "adapter_version", r.adapter_version); field(out, "request_digest", r.request_digest);
    number(out, "input_count", r.input_artifact_refs.size());
    for (const auto& ref : r.input_artifact_refs) {
        field(out, "input_ref", ref.reference); number(out, "input_size", ref.size_bytes);
        field(out, "input_digest", ref.sha256);
    }
    number(out, "max_wall_ms", r.budget.max_wall_ms);
    number(out, "max_input_bytes", r.budget.max_input_bytes);
    number(out, "max_output_bytes", r.budget.max_output_bytes);
    number(out, "max_operations", r.budget.max_operations);
    number(out, "max_memory_bytes", r.budget.max_memory_bytes);
    return out;
}
WorkerRequest decode_request_payload(std::string_view payload) {
    Reader reader(payload); WorkerRequest r;
    r.run_id = reader.field("run_id"); r.attempt_id = reader.field("attempt_id");
    r.node_id = reader.field("node_id"); r.adapter_id = reader.field("adapter_id");
    r.adapter_version = reader.field("adapter_version"); r.request_digest = reader.field("request_digest");
    const auto count = reader.number("input_count");
    if (count > 1024) throw bruce::Error("InvalidWorkerPayload");
    for (std::uint64_t i = 0; i < count; ++i) {
        ArtifactRef ref; ref.reference = reader.field("input_ref");
        ref.size_bytes = reader.number("input_size"); ref.sha256 = reader.field("input_digest");
        r.input_artifact_refs.push_back(std::move(ref));
    }
    r.budget.max_wall_ms = reader.number("max_wall_ms");
    r.budget.max_input_bytes = reader.number("max_input_bytes");
    r.budget.max_output_bytes = reader.number("max_output_bytes");
    r.budget.max_operations = reader.number("max_operations");
    r.budget.max_memory_bytes = reader.number("max_memory_bytes");
    reader.finish();
    if (!token(r.run_id) || !token(r.attempt_id) || !token(r.node_id) ||
        !token(r.adapter_id) || !token(r.adapter_version) || !digest(r.request_digest) ||
        !valid_budget(r.budget)) throw bruce::Error("InvalidWorkerPayload");
    for (const auto& ref : r.input_artifact_refs)
        if (!relative_ref(ref.reference) || !digest(ref.sha256)) throw bruce::Error("InvalidWorkerPayload");
    return r;
}
std::string encode_result_payload(const WorkerResult& r) {
    std::string out;
    field(out, "status", status_name(r.status)); field(out, "result_code", r.result_code);
    number(out, "has_output", r.output_artifact_ref.has_value());
    if (r.output_artifact_ref) {
        field(out, "output_ref", r.output_artifact_ref->reference);
        number(out, "output_size", r.output_artifact_ref->size_bytes);
        field(out, "output_digest", r.output_artifact_ref->sha256);
    }
    number(out, "has_payload_digest", r.payload_digest.has_value());
    if (r.payload_digest) field(out, "payload_digest", *r.payload_digest);
    number(out, "payload_size", r.payload_size);
    number(out, "wall_ms", r.usage.wall_ms); number(out, "input_bytes", r.usage.input_bytes);
    number(out, "output_bytes", r.usage.output_bytes); number(out, "operations", r.usage.operations);
    number(out, "peak_memory_bytes", r.usage.peak_memory_bytes);
    field(out, "failure_code", code_name(r.failure_code));
    return out;
}
WorkerResult decode_result_payload(std::string_view payload) {
    Reader reader(payload); WorkerResult r;
    r.status = parse_status(reader.field("status")); r.result_code = reader.field("result_code");
    const auto has_output = reader.number("has_output");
    if (has_output > 1) throw bruce::Error("InvalidWorkerPayload");
    if (has_output) {
        ArtifactRef ref; ref.reference = reader.field("output_ref");
        ref.size_bytes = reader.number("output_size"); ref.sha256 = reader.field("output_digest");
        r.output_artifact_ref = std::move(ref);
    }
    const auto has_digest = reader.number("has_payload_digest");
    if (has_digest > 1) throw bruce::Error("InvalidWorkerPayload");
    if (has_digest) r.payload_digest = reader.field("payload_digest");
    r.payload_size = reader.number("payload_size");
    r.usage.wall_ms = reader.number("wall_ms"); r.usage.input_bytes = reader.number("input_bytes");
    r.usage.output_bytes = reader.number("output_bytes"); r.usage.operations = reader.number("operations");
    r.usage.peak_memory_bytes = reader.number("peak_memory_bytes");
    r.failure_code = parse_code(reader.field("failure_code")); reader.finish();
    if (!token(r.result_code) || (r.payload_digest && !digest(*r.payload_digest)) ||
        (r.output_artifact_ref && (!token(r.output_artifact_ref->reference) ||
                                   !digest(r.output_artifact_ref->sha256))) ||
        (r.status != WorkerStatus::READY && r.output_artifact_ref) ||
        (r.status == WorkerStatus::READY && r.failure_code != FailureCode::READY) ||
        (r.status == WorkerStatus::BLOCKED && r.failure_code != FailureCode::BLOCKED) ||
        (r.status == WorkerStatus::CANCELLED && r.failure_code != FailureCode::CANCELLED) ||
        (r.status == WorkerStatus::FAILED &&
         (r.failure_code == FailureCode::READY || r.failure_code == FailureCode::BLOCKED ||
          r.failure_code == FailureCode::CANCELLED || r.failure_code == FailureCode::NONE)))
        throw bruce::Error("InvalidWorkerPayload");
    return r;
}

struct WorkerAttemptHandle::State {
    pid_t pid{-1};
    int to_child{-1}, from_child{-1}, start_error{-1};
    pid_t watchdog_pid{-1};
    int watch_control{-1}, watch_result{-1};
    char watch_status{};
    std::string pending, received;
    std::optional<Frame> result_frame;
    std::size_t sent{};
    std::filesystem::path run_root;
    std::string attempt_id;
    Budget budget;
    Clock::time_point started{Clock::now()}, cancel_requested_at{}, cancel_at{}, term_at{}, last_query{};
    bool cancel_requested{}, cancel_delivered{}, terminating{}, killed{}, observed_exit{}, exited{}, stdout_closed{}, group_cleaned{};
    int exit_status{};
    std::uint64_t peak_bytes{}, input_bytes{};
    std::size_t control_frames{}, control_bytes{};
    std::optional<WorkerAttemptOutcome> outcome;
    FailureCode forced_code{FailureCode::NONE};
    void flush_input(std::size_t max_write = 32768) {
        const bool had_pending = !pending.empty();
        std::size_t written{};
        while (to_child >= 0 && sent < pending.size() && written < max_write) {
            const auto count = std::min<std::size_t>(pending.size() - sent, max_write - written);
            const auto n = write_pipe(to_child, pending.data() + sent, count);
            if (n > 0) { sent += static_cast<std::size_t>(n); written += static_cast<std::size_t>(n); }
            else if (n < 0 && (errno == EAGAIN || errno == EINTR)) break;
            else { close_fd(to_child); break; }
        }
        if (had_pending && sent == pending.size()) {
            if (cancel_requested && !cancel_delivered) {
                cancel_delivered = true;
                cancel_at = Clock::now();
            }
            pending.clear(); sent = 0;
        }
    }
    void collect_watch_status() {
        if (watch_status || watch_result < 0) return;
        const auto n = ::read(watch_result, &watch_status, 1);
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) watch_status = 'E';
        if (watch_status) {
            close_fd(watch_result);
            close_fd(watch_control);
            if (watchdog_pid > 0) {
                int status{};
                while (::waitpid(watchdog_pid, &status, 0) < 0 && errno == EINTR) {}
                watchdog_pid = -1;
            }
        }
    }
    ~State() {
        close_fd(watch_control);
        if (watchdog_pid > 0) {
            (void)::kill(watchdog_pid, SIGKILL);
            int status{};
            while (::waitpid(watchdog_pid, &status, 0) < 0 && errno == EINTR) {}
        }
        close_fd(watch_result);
        if (pid > 0) {
            if (!group_cleaned) (void)cleanup_group(pid);
            if (!exited) {
                int status; while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            }
        }
        close_fd(to_child); close_fd(from_child); close_fd(start_error);
    }
};
WorkerAttemptHandle::WorkerAttemptHandle() = default;
WorkerAttemptHandle::WorkerAttemptHandle(std::unique_ptr<State> state) : state_(std::move(state)) {}
WorkerAttemptHandle::~WorkerAttemptHandle() = default;
WorkerAttemptHandle::WorkerAttemptHandle(WorkerAttemptHandle&&) noexcept = default;
WorkerAttemptHandle& WorkerAttemptHandle::operator=(WorkerAttemptHandle&&) noexcept = default;

WorkerAttemptHandle start_attempt(const WorkerRequest& request, const SupervisorOptions& options) {
    auto s = std::make_unique<WorkerAttemptHandle::State>();
    s->budget = options.budgets; s->attempt_id = request.attempt_id;
    if (!valid_budget(options.budgets) || !same_budget(request.budget, options.budgets) ||
        options.grace_ms != 1000 || !token(request.run_id) || !token(request.attempt_id) ||
        !token(request.node_id) || !token(request.adapter_id) ||
        !token(request.adapter_version) || !digest(request.request_digest) ||
        request.input_artifact_refs.size() > 1024) {
        s->outcome = failure(FailureCode::INVALID_REQUEST); return WorkerAttemptHandle(std::move(s));
    }
    try {
        if (!options.executable.is_absolute() || !std::filesystem::is_regular_file(options.executable))
            throw bruce::Error("WorkerExecutableUnavailable");
        struct stat root_stat{};
        const bool root_exists = ::lstat(options.run_root.c_str(), &root_stat) == 0;
        if (root_exists && (!S_ISDIR(root_stat.st_mode) || root_stat.st_uid != ::geteuid() ||
                            (root_stat.st_mode & 0777) != 0700))
            throw bruce::Error("WorkerRunRootUnavailable");
        std::filesystem::create_directories(options.run_root);
        s->run_root = std::filesystem::canonical(options.run_root);
        if (!root_exists && ::chmod(s->run_root.c_str(), 0700) != 0)
            throw bruce::Error("WorkerRunRootUnavailable");
        if (!make_attempt_dirs(s->run_root, request.attempt_id))
            throw bruce::Error("WorkerAttemptDirectoryUnavailable");
        for (const auto& ref : request.input_artifact_refs) {
            if (!relative_ref(ref.reference) || !digest(ref.sha256)) {
                s->outcome = failure(FailureCode::INVALID_REQUEST); return WorkerAttemptHandle(std::move(s));
            }
            const auto path = s->run_root / ref.reference;
            const auto resolved = std::filesystem::weakly_canonical(path);
            const auto root_str = s->run_root.native() + "/";
            if (!resolved.native().starts_with(root_str)) {
                s->outcome = failure(FailureCode::INVALID_REQUEST); return WorkerAttemptHandle(std::move(s));
            }
            const auto remaining = s->budget.max_input_bytes - std::min(s->input_bytes, s->budget.max_input_bytes);
            if (ref.size_bytes > remaining) {
                s->outcome = failure(FailureCode::INPUT_LIMIT_EXCEEDED); return WorkerAttemptHandle(std::move(s));
            }
            struct stat input_stat{};
            if (::lstat(path.c_str(), &input_stat) != 0 || !S_ISREG(input_stat.st_mode) ||
                input_stat.st_size < 0) {
                s->outcome = failure(FailureCode::INVALID_REQUEST); return WorkerAttemptHandle(std::move(s));
            }
            if (static_cast<std::uint64_t>(input_stat.st_size) > remaining) {
                s->outcome = failure(FailureCode::INPUT_LIMIT_EXCEEDED); return WorkerAttemptHandle(std::move(s));
            }
            std::string bytes;
            if (!read_artifact(path, remaining, bytes)) {
                s->outcome = failure(bytes.size() > remaining ? FailureCode::INPUT_LIMIT_EXCEEDED
                                                              : FailureCode::INVALID_REQUEST);
                return WorkerAttemptHandle(std::move(s));
            }
            s->input_bytes += bytes.size();
            if (bytes.size() != ref.size_bytes || bruce::sha256(bytes) != ref.sha256) {
                s->outcome = failure(FailureCode::DIGEST_MISMATCH); return WorkerAttemptHandle(std::move(s));
            }
        }
        s->pending = encode_frame(FrameType::REQUEST, encode_request_payload(request));
    } catch (const std::exception&) {
        s->outcome = failure(FailureCode::PROCESS_START_FAILED); return WorkerAttemptHandle(std::move(s));
    }
    int input[2]{-1,-1}, output[2]{-1,-1}, error_pipe[2]{-1,-1};
    if (::pipe(input) || ::pipe(output) || ::pipe(error_pipe)) {
        for (int& fd : input) close_fd(fd);
        for (int& fd : output) close_fd(fd);
        for (int& fd : error_pipe) close_fd(fd);
        s->outcome = failure(FailureCode::PROCESS_START_FAILED); return WorkerAttemptHandle(std::move(s));
    }
    cloexec(error_pipe[1]);
    const auto executable = options.executable.string();
    const auto working_directory = s->run_root.string();
    const auto parent_pid = ::getpid();
    s->started = Clock::now();
    const auto pid = ::fork();
    if (pid == 0) {
        ::close(input[1]); ::close(output[0]); ::close(error_pipe[0]);
        auto fail_child = [&] { const int err = errno; (void)::write(error_pipe[1], &err, sizeof(err)); std::_Exit(127); };
        if (::setpgid(0, 0) != 0 || ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
            ::getppid() != parent_pid ||
            ::dup2(input[0], STDIN_FILENO) < 0 || ::dup2(output[1], STDOUT_FILENO) < 0)
            fail_child();
        const int nullfd = ::open("/dev/null", O_WRONLY);
        if (nullfd < 0 || ::dup2(nullfd, STDERR_FILENO) < 0) fail_child();
        struct rlimit limit{static_cast<rlim_t>(options.budgets.max_memory_bytes),
                            static_cast<rlim_t>(options.budgets.max_memory_bytes)};
        if (::setrlimit(RLIMIT_AS, &limit) != 0 || ::chdir(working_directory.c_str()) != 0) fail_child();
        bool closed = true;
        if (error_pipe[1] > 3)
            closed = ::syscall(SYS_close_range, 3U, static_cast<unsigned>(error_pipe[1] - 1), 0U) == 0;
        if (closed)
            closed = ::syscall(SYS_close_range, static_cast<unsigned>(error_pipe[1] + 1),
                               UINT_MAX, 0U) == 0;
        if (!closed) {
            struct rlimit fd_limit{};
            const auto maxfd = ::getrlimit(RLIMIT_NOFILE, &fd_limit) == 0 ?
                std::min<rlim_t>(fd_limit.rlim_cur, INT_MAX) : 65536;
            for (int fd = 3; fd < static_cast<int>(maxfd); ++fd)
                if (fd != error_pipe[1]) ::close(fd);
        }
        const char* argv[] = {executable.c_str(), "worker-v1", "child", nullptr};
        ::execv(executable.c_str(), const_cast<char* const*>(argv));
        fail_child();
    }
    close_fd(input[0]); close_fd(output[1]); close_fd(error_pipe[1]);
    if (pid < 0) {
        close_fd(input[1]); close_fd(output[0]); close_fd(error_pipe[0]);
        s->outcome = failure(FailureCode::PROCESS_START_FAILED); return WorkerAttemptHandle(std::move(s));
    }
    (void)::setpgid(pid, pid);
    s->pid = pid; s->to_child = input[1]; s->from_child = output[0]; s->start_error = error_pipe[0];
    nonblock(s->to_child); nonblock(s->from_child); nonblock(s->start_error);
    int watch_control[2]{-1,-1}, watch_result[2]{-1,-1};
    if (::pipe(watch_control) != 0 || ::pipe(watch_result) != 0) {
        for (int& fd : watch_control) close_fd(fd);
        for (int& fd : watch_result) close_fd(fd);
        s->outcome = failure(FailureCode::PROCESS_START_FAILED);
        return WorkerAttemptHandle(std::move(s));
    }
    const auto watcher = ::fork();
    if (watcher == 0) {
        ::close(watch_control[1]); ::close(watch_result[0]);
        run_watchdog(pid, parent_pid,
                     s->started + std::chrono::milliseconds(options.budgets.max_wall_ms),
                     watch_control[0], watch_result[1]);
    }
    close_fd(watch_control[0]); close_fd(watch_result[1]);
    if (watcher < 0) {
        close_fd(watch_control[1]); close_fd(watch_result[0]);
        s->outcome = failure(FailureCode::PROCESS_START_FAILED);
        return WorkerAttemptHandle(std::move(s));
    }
    s->watchdog_pid = watcher;
    s->watch_control = watch_control[1]; s->watch_result = watch_result[0];
    nonblock(s->watch_control); nonblock(s->watch_result);
    s->last_query = s->started;
    s->flush_input();
    return WorkerAttemptHandle(std::move(s));
}

std::optional<WorkerAttemptOutcome> poll_attempt(WorkerAttemptHandle& handle) {
    if (!handle.state_) throw bruce::Error("InvalidWorkerAttemptHandle");
    auto& s = *handle.state_;
    if (s.outcome) return s.outcome;
    std::size_t frames_this_poll{};
    const auto consume_frames = [&] {
        while (!s.received.empty() && frames_this_poll < max_poll_control_frames) {
            const auto separator = s.received.find("\n\n");
            if (separator == std::string::npos) {
                if (s.received.size() > 256) throw bruce::Error("InvalidWorkerFrameHeader");
                return;
            }
            if (separator > 256) throw bruce::Error("InvalidWorkerFrameHeader");
            const auto marker = s.received.find("\npayload-bytes=");
            if (marker == std::string::npos || marker >= separator) throw bruce::Error("InvalidWorkerFrameHeader");
            const auto begin = marker + std::string_view("\npayload-bytes=").size();
            const auto end = s.received.find('\n', begin);
            if (end == std::string::npos || end > separator || begin == end ||
                end - begin > 8) throw bruce::Error("InvalidWorkerFrameSize");
            std::size_t payload_size{};
            const auto [last, ec] = std::from_chars(s.received.data() + begin,
                                                    s.received.data() + end, payload_size);
            if (ec != std::errc{} || last != s.received.data() + end ||
                payload_size > max_result_frame - separator - 2)
                throw bruce::Error("InvalidWorkerFrameSize");
            const auto total = separator + 2 + payload_size;
            if (s.received.size() < total) return;
            const auto frame = decode_frame(std::string_view(s.received).substr(0, total), max_result_frame);
            if (frame.type == FrameType::RESULT) {
                if (s.result_frame) throw bruce::Error("DuplicateWorkerResult");
                s.result_frame = frame;
            } else if ((frame.type != FrameType::STATUS && frame.type != FrameType::ACK) ||
                       s.result_frame) throw bruce::Error("UnexpectedWorkerFrame");
            else {
                ++s.control_frames;
                s.control_bytes += total;
                if (s.control_frames > max_attempt_control_frames ||
                    s.control_bytes > max_attempt_control_bytes)
                    throw bruce::Error("WorkerControlFrameLimitExceeded");
            }
            ++frames_this_poll;
            s.received.erase(0, total);
        }
    };
    const auto now = Clock::now();
    s.flush_input();
    if (!s.cancel_requested && s.pending.empty() && now - s.last_query >= std::chrono::milliseconds(250) &&
        s.to_child >= 0) {
        s.pending = encode_frame(FrameType::STATUS_QUERY, ""); s.last_query = now;
    }
    if (s.from_child >= 0) {
        char chunk[4096];
        std::size_t read_this_poll{};
        try { consume_frames(); }
        catch (const std::exception&) {
            if (s.forced_code == FailureCode::NONE) s.forced_code = FailureCode::PROTOCOL_ERROR;
            s.received.clear(); close_fd(s.from_child);
        }
        while (s.from_child >= 0 && read_this_poll < max_poll_read_bytes &&
               frames_this_poll < max_poll_control_frames) {
            const auto n = ::read(s.from_child, chunk, sizeof(chunk));
            if (n > 0) {
                read_this_poll += static_cast<std::size_t>(n);
                if (s.received.size() + static_cast<std::size_t>(n) > max_result_frame) {
                    if (s.forced_code == FailureCode::NONE) s.forced_code = FailureCode::PROTOCOL_ERROR;
                    s.received.clear();
                    close_fd(s.from_child); break;
                }
                s.received.append(chunk, static_cast<std::size_t>(n));
                try { consume_frames(); }
                catch (const std::exception&) {
                    if (s.forced_code == FailureCode::NONE) s.forced_code = FailureCode::PROTOCOL_ERROR;
                    s.received.clear(); close_fd(s.from_child); break;
                }
            } else if (n == 0) { s.stdout_closed = true; close_fd(s.from_child); break; }
            else { if (errno != EAGAIN && errno != EINTR) { s.stdout_closed = true; close_fd(s.from_child); } break; }
        }
    }
    if (s.start_error >= 0) {
        int child_errno{};
        const auto n = ::read(s.start_error, &child_errno, sizeof(child_errno));
        if (n > 0) s.forced_code = FailureCode::PROCESS_START_FAILED;
        if (n != -1 || errno != EAGAIN) close_fd(s.start_error);
    }
    s.collect_watch_status();
    if (s.watch_status == 'T' && !s.cancel_requested)
        s.forced_code = FailureCode::TIMEOUT;
    else if (s.watch_status == 'E' && s.forced_code == FailureCode::NONE)
        s.forced_code = FailureCode::PROCESS_CRASHED;
    if (s.forced_code == FailureCode::NONE) {
        const int fd = open_output(s.run_root, s.attempt_id);
        if (fd >= 0) {
            struct stat st{};
            if (::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0 &&
                static_cast<std::uint64_t>(st.st_size) > s.budget.max_output_bytes)
                s.forced_code = FailureCode::OUTPUT_LIMIT_EXCEEDED;
            ::close(fd);
        }
    }
    if (!s.observed_exit) {
        s.peak_bytes = std::max(s.peak_bytes, proc_peak_bytes(s.pid));
        siginfo_t info{};
        if (::waitid(P_PID, static_cast<id_t>(s.pid), &info, WEXITED | WNOHANG | WNOWAIT) == 0) {
            if (info.si_pid == s.pid) s.observed_exit = true;
        } else if (errno == ECHILD) {
            s.observed_exit = true; s.exited = true;
            s.forced_code = FailureCode::PROCESS_CRASHED;
        }
    }
    const auto observed = Clock::now();
    if (!s.observed_exit && !s.cancel_requested && s.forced_code == FailureCode::NONE &&
        observed - s.started >= std::chrono::milliseconds(s.budget.max_wall_ms))
        s.forced_code = FailureCode::TIMEOUT;
    const auto cancel_origin = s.cancel_delivered ? s.cancel_at : s.cancel_requested_at;
    if (!s.observed_exit && s.cancel_requested && observed >= cancel_origin + fixed_grace &&
        !s.terminating && s.forced_code == FailureCode::NONE)
        s.forced_code = FailureCode::CANCELLED;
    if (s.forced_code != FailureCode::NONE && !s.terminating) {
        signal_group(s.pid, SIGTERM); s.terminating = true; s.term_at = observed;
    }
    if (s.terminating && !s.killed && observed - s.term_at >= term_grace) {
        signal_group(s.pid, SIGKILL); s.killed = true;
    }
    if (!s.observed_exit) return std::nullopt;
    // Keep the main worker as an unreaped zombie until the watchdog has
    // observed its pidfd and finished any timeout escalation.  This keeps
    // both the PID and process-group identity reserved through its SIGKILL.
    s.collect_watch_status();
    if (!s.watch_status) return std::nullopt;
    if (s.watch_status == 'T' && !s.cancel_requested) s.forced_code = FailureCode::TIMEOUT;
    if (s.watch_status == 'E' && s.forced_code == FailureCode::NONE)
        s.forced_code = FailureCode::PROCESS_CRASHED;
    // Kill descendants before waiting for EOF; they may have inherited stdout.
    if (!s.group_cleaned) s.group_cleaned = cleanup_group(s.pid);
    if (!s.exited) {
        struct rusage usage{};
        pid_t w;
        do { w = ::wait4(s.pid, &s.exit_status, 0, &usage); }
        while (w < 0 && errno == EINTR);
        s.exited = true;
        if (w == s.pid && usage.ru_maxrss > 0)
            s.peak_bytes = std::max(s.peak_bytes, static_cast<std::uint64_t>(usage.ru_maxrss) * 1024);
        else if (w != s.pid && s.forced_code == FailureCode::NONE)
            s.forced_code = FailureCode::PROCESS_CRASHED;
    }
    if (!s.group_cleaned) {
        if (s.forced_code == FailureCode::NONE) s.forced_code = FailureCode::PROCESS_CRASHED;
        close_fd(s.from_child);
    }
    close_fd(s.to_child);
    if (s.from_child >= 0 || (frames_this_poll == max_poll_control_frames && !s.received.empty()))
        return std::nullopt;
    close_fd(s.start_error);
    WorkerAttemptOutcome out;
    if (s.forced_code != FailureCode::NONE) out = failure(s.forced_code);
    else if (s.cancel_requested) out = failure(FailureCode::CANCELLED);
    else if (!WIFEXITED(s.exit_status) || WEXITSTATUS(s.exit_status) != 0)
        out = failure(FailureCode::PROCESS_CRASHED);
    else {
        try {
            if (!s.received.empty() || !s.result_frame) throw bruce::Error("InvalidWorkerResultFrame");
            out.result = decode_result_payload(s.result_frame->payload);
            if (out.result.usage.operations > s.budget.max_operations)
                out = failure(FailureCode::OPERATION_BUDGET_EXCEEDED);
            else if (out.result.usage.input_bytes > s.budget.max_input_bytes ||
                     s.input_bytes > s.budget.max_input_bytes)
                out = failure(FailureCode::INPUT_LIMIT_EXCEEDED);
            else if (out.result.usage.output_bytes > s.budget.max_output_bytes ||
                     out.result.payload_size > s.budget.max_output_bytes ||
                     (out.result.output_artifact_ref &&
                      out.result.output_artifact_ref->size_bytes > s.budget.max_output_bytes))
                out = failure(FailureCode::OUTPUT_LIMIT_EXCEEDED);
            else if (out.result.status == WorkerStatus::READY) {
                if (out.result.failure_code != FailureCode::READY || !out.result.output_artifact_ref ||
                    !out.result.payload_digest || out.result.output_artifact_ref->reference != "result")
                    out = failure(FailureCode::PROTOCOL_ERROR);
                else {
                    std::string bytes;
                    const int fd = open_output(s.run_root, s.attempt_id);
                    struct stat output_stat{};
                    if (fd < 0) out = failure(FailureCode::PROTOCOL_ERROR);
                    else if (::fstat(fd, &output_stat) != 0 || !S_ISREG(output_stat.st_mode)) {
                        ::close(fd); out = failure(FailureCode::PROTOCOL_ERROR);
                    } else if (output_stat.st_size < 0 ||
                               static_cast<std::uint64_t>(output_stat.st_size) > s.budget.max_output_bytes) {
                        ::close(fd); out = failure(FailureCode::OUTPUT_LIMIT_EXCEEDED);
                    } else if (!read_regular_fd(fd, s.budget.max_output_bytes, bytes))
                        out = failure(FailureCode::PROTOCOL_ERROR);
                    else if (bytes.size() != out.result.payload_size ||
                             bytes.size() != out.result.output_artifact_ref->size_bytes ||
                             bruce::sha256(bytes) != *out.result.payload_digest ||
                             bruce::sha256(bytes) != out.result.output_artifact_ref->sha256)
                        out = failure(FailureCode::DIGEST_MISMATCH);
                }
            }
        } catch (const std::exception&) { out = failure(FailureCode::PROTOCOL_ERROR); }
    }
    out.result.usage.wall_ms = elapsed_ms(s.started);
    out.result.usage.input_bytes = s.input_bytes;
    out.result.usage.peak_memory_bytes = std::max(out.result.usage.peak_memory_bytes, s.peak_bytes);
    s.outcome = out;
    return s.outcome;
}

WorkerAttemptOutcome finish_attempt(WorkerAttemptHandle& attempt) {
    for (;;) {
        if (auto outcome = poll_attempt(attempt)) return *outcome;
        ::usleep(1000);
    }
}
void request_cancel(WorkerAttemptHandle& attempt) {
    if (!attempt.state_) throw bruce::Error("InvalidWorkerAttemptHandle");
    auto& s = *attempt.state_;
    if (s.outcome || s.cancel_requested) return;
    s.collect_watch_status();
    if (s.watch_status == 'T') { (void)finish_attempt(attempt); return; }
    siginfo_t info{};
    if (s.watch_status == 'C' || s.observed_exit ||
        (::waitid(P_PID, static_cast<id_t>(s.pid), &info, WEXITED | WNOHANG | WNOWAIT) == 0 &&
         info.si_pid == s.pid)) {
        (void)finish_attempt(attempt);
        return;
    }
    if (s.watch_control >= 0) {
        const char disarm = 'X';
        const auto n = write_pipe(s.watch_control, &disarm, 1);
        if (n == 1) {
            pollfd acknowledgement{s.watch_result, POLLIN | POLLHUP, 0};
            (void)::poll(&acknowledgement, 1, 100);
            s.collect_watch_status();
        }
        if (!s.watch_status && s.watchdog_pid > 0) {
            (void)::kill(s.watchdog_pid, SIGKILL);
            int status{};
            while (::waitpid(s.watchdog_pid, &status, 0) < 0 && errno == EINTR) {}
            s.watchdog_pid = -1;
            s.collect_watch_status();
            if (!s.watch_status || s.watch_status == 'E') s.watch_status = 'A';
            close_fd(s.watch_control); close_fd(s.watch_result);
        }
    }
    if (s.watch_status == 'T' || s.watch_status == 'C') {
        (void)finish_attempt(attempt);
        return;
    }
    s.cancel_requested = true;
    s.cancel_requested_at = Clock::now();
    if (s.to_child >= 0) {
        s.pending.erase(0, s.sent);
        s.pending += encode_frame(FrameType::CANCEL, "");
        s.sent = 0;
        s.flush_input(max_metadata + 1024);
    }
}

} // namespace bruce::worker_v1
