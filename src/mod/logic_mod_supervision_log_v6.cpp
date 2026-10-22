#include "bruce/mod/logic_mod_supervision_log_v6.hpp"

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <regex>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::mod {
namespace {
constexpr std::size_t kMaxLineBytes = 4096;
constexpr std::size_t kMaxLogBytes = 1024 * 1024;
constexpr std::array<std::string_view, 6> kEvents{
    "ATTEMPT_STARTED", "CANCEL_SENT", "TERM_SENT", "KILL_SENT", "REAPED", "ATTEMPT_FINISHED"};
constexpr std::array<std::string_view, 6> kStatuses{"RUNNING", "READY", "FAILED", "BLOCKED", "CANCELLED", "SUPERVISION_FAILED"};
constexpr std::array<std::string_view, 13> kReasons{
    "READY_ACCEPTED", "WORKER_FAILED", "WORKER_BLOCKED", "HOST_CANCELLED", "WALL_TIME_LIMIT", "MEMORY_LIMIT",
    "INPUT_LIMIT", "OUTPUT_LIMIT", "PROCESS_START_FAILED", "PROCESS_CRASHED", "PROTOCOL_ERROR", "DIGEST_MISMATCH", "SUPERVISION_UNAVAILABLE"};

[[noreturn]] void protocol_error() { throw Error("PROTOCOL_ERROR"); }
class ScopedFd {
public:
    explicit ScopedFd(int value = -1) : value_(value) {}
    ~ScopedFd() { if (value_ >= 0) close(value_); }
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
    [[nodiscard]] int get() const { return value_; }
    [[nodiscard]] int release() { const int value = value_; value_ = -1; return value; }
private:
    int value_;
};
bool contains(const auto& values, std::string_view value) {
    for (const auto candidate : values) if (candidate == value) return true;
    return false;
}
void checked_identifier(std::string_view value) {
    static const std::regex grammar("[A-Za-z0-9][A-Za-z0-9._:-]{0,127}");
    if (!std::regex_match(value.begin(), value.end(), grammar)) protocol_error();
}
void check_owner_only(const struct stat& info, bool directory) {
    if ((directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)) || info.st_uid != geteuid() ||
        (info.st_mode & 0077) != 0 || (!directory && info.st_nlink != 1)) protocol_error();
}
int checked_open_dir(int parent, std::string_view name, bool create) {
    if (name.empty() || name == "." || name == "..") protocol_error();
    if (create && mkdirat(parent, std::string(name).c_str(), 0700) != 0 && errno != EEXIST) protocol_error();
    const int fd = openat(parent, std::string(name).c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) protocol_error();
    ScopedFd owned(fd);
    struct stat info{};
    if (fstat(fd, &info) != 0) protocol_error();
    check_owner_only(info, true);
    return owned.release();
}
int open_parent_chain(const std::filesystem::path& parent) {
    const bool absolute = parent.is_absolute();
    int fd = open(absolute ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) protocol_error();
    for (const auto& component : parent) {
        const auto name = component.string();
        if (name.empty() || name == "/") continue;
        const int next = openat(fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (next < 0) { close(fd); protocol_error(); }
        close(fd); fd = next;
    }
    return fd;
}
int open_control(const std::filesystem::path& report_root, bool create) {
    const std::filesystem::path run(std::filesystem::path(report_root.string() + ".run"));
    if (run.filename().empty() || run.filename() == "." || run.filename() == "..") protocol_error();
    int parent = open_parent_chain(run.parent_path().empty() ? std::filesystem::path(".") : run.parent_path());
    int root = -1;
    int control = -1;
    try {
        root = checked_open_dir(parent, run.filename().string(), create);
        control = checked_open_dir(root, "control", create);
    } catch (...) { if (root >= 0) close(root); close(parent); throw; }
    close(root); close(parent);
    return control;
}
enum class LogAccess { READ_ONLY, READ_WRITE, READ_WRITE_CREATE };
int open_log_checked(int control_fd, LogAccess access) {
    int flags = O_CLOEXEC | O_NOFOLLOW;
    if (access == LogAccess::READ_ONLY) flags |= O_RDONLY | O_NONBLOCK;
    else flags |= O_RDWR;
    if (access == LogAccess::READ_WRITE_CREATE) flags |= O_CREAT;
    const int fd = openat(control_fd, "supervision-v6.jsonl", flags, 0600);
    if (fd < 0) protocol_error();
    ScopedFd owned(fd);
    struct stat info{};
    if (fstat(fd, &info) != 0) protocol_error();
    check_owner_only(info, false);
    return owned.release();
}

std::string availability_name(LogicModPeakMemoryAvailabilityV6 value) {
    switch (value) {
    case LogicModPeakMemoryAvailabilityV6::OBSERVED: return "OBSERVED";
    case LogicModPeakMemoryAvailabilityV6::UNAVAILABLE: return "UNAVAILABLE";
    case LogicModPeakMemoryAvailabilityV6::NOT_APPLICABLE: return "NOT_APPLICABLE";
    }
    protocol_error();
}
LogicModPeakMemoryAvailabilityV6 parse_availability(std::string_view value) {
    if (value == "OBSERVED") return LogicModPeakMemoryAvailabilityV6::OBSERVED;
    if (value == "UNAVAILABLE") return LogicModPeakMemoryAvailabilityV6::UNAVAILABLE;
    if (value == "NOT_APPLICABLE") return LogicModPeakMemoryAvailabilityV6::NOT_APPLICABLE;
    protocol_error();
}
void validate_event_shape(const SupervisionEventV6& event, std::string_view run_id) {
    if (event.schema != "logic-mod-supervision-v6" || !event.forbidden_data.empty() || !contains(kEvents, event.event) ||
        !contains(kStatuses, event.status) || event.seq == 0) protocol_error();
    checked_identifier(run_id); checked_identifier(event.attempt_id); checked_identifier(event.node_id);
    if (event.terminal_reason && !contains(kReasons, *event.terminal_reason)) protocol_error();
    if (event.peak_memory_availability == LogicModPeakMemoryAvailabilityV6::OBSERVED) {
        if (!event.peak_memory_bytes) protocol_error();
    } else if (event.peak_memory_bytes) {
        protocol_error();
    }
    if (event.event == "ATTEMPT_STARTED") {
        if (event.status != "RUNNING" || event.terminal_reason) protocol_error();
    } else if (event.event == "CANCEL_SENT" || event.event == "TERM_SENT" || event.event == "KILL_SENT") {
        if (event.status != "CANCELLED" || event.terminal_reason ||
            event.wall_time_limit_ms || event.address_space_limit_bytes || event.input_limit_bytes || event.output_limit_bytes) protocol_error();
    } else {
        if (!event.terminal_reason || event.status == "RUNNING" ||
            event.wall_time_limit_ms || event.address_space_limit_bytes || event.input_limit_bytes || event.output_limit_bytes) protocol_error();
    }
    if (event.terminal_reason) {
        const auto reason = *event.terminal_reason;
        const bool compatible =
            (event.status == "READY" && reason == "READY_ACCEPTED") ||
            (event.status == "BLOCKED" && reason == "WORKER_BLOCKED") ||
            (event.status == "CANCELLED" && reason == "HOST_CANCELLED") ||
            (event.status == "SUPERVISION_FAILED" && reason == "SUPERVISION_UNAVAILABLE") ||
            (event.status == "FAILED" && reason != "READY_ACCEPTED" && reason != "WORKER_BLOCKED" && reason != "HOST_CANCELLED");
        if (!compatible) protocol_error();
    }
}
std::string serialize(const SupervisionEventV6& event, std::string_view run_id) {
    validate_event_shape(event, run_id);
    std::string output = "{\"schema\":\"logic-mod-supervision-v6\",\"run_id\":" + json_string(run_id) +
        ",\"attempt_id\":" + json_string(event.attempt_id) + ",\"node_id\":" + json_string(event.node_id) +
        ",\"seq\":" + std::to_string(event.seq) + ",\"event\":" + json_string(event.event) +
        ",\"status\":" + json_string(event.status);
    const auto string_opt = [&](std::string_view name, const std::optional<std::string>& value) {
        if (value) output += ",\"" + std::string(name) + "\":" + json_string(*value);
    };
    const auto number_opt = [&](std::string_view name, const std::optional<std::uint64_t>& value) {
        if (value) output += ",\"" + std::string(name) + "\":" + std::to_string(*value);
    };
    string_opt("terminal_reason", event.terminal_reason);
    number_opt("elapsed_ms", event.elapsed_ms);
    number_opt("wall_time_limit_ms", event.wall_time_limit_ms);
    number_opt("address_space_limit_bytes", event.address_space_limit_bytes);
    number_opt("input_limit_bytes", event.input_limit_bytes);
    number_opt("output_limit_bytes", event.output_limit_bytes);
    output += ",\"peak_memory_availability\":" + json_string(availability_name(event.peak_memory_availability));
    number_opt("peak_memory_bytes", event.peak_memory_bytes);
    output += '}';
    if (output.size() + 1 > kMaxLineBytes) protocol_error();
    return output;
}

struct Field { std::string key; std::string value; bool string{}; };
std::vector<Field> parse_object(std::string_view text) {
    if (text.empty() || text.front() != '{' || text.back() != '}') protocol_error();
    std::vector<Field> fields; std::size_t pos = 1;
    const auto quoted = [&](std::size_t& at) {
        if (at >= text.size() || text[at++] != '\"') protocol_error();
        const auto begin = at;
        while (at < text.size() && text[at] != '\"') { if (text[at] == '\\' || static_cast<unsigned char>(text[at]) < 0x20) protocol_error(); ++at; }
        if (at >= text.size()) protocol_error();
        return std::string(text.substr(begin, at++ - begin));
    };
    while (pos < text.size() - 1) {
        Field field; field.key = quoted(pos);
        if (pos >= text.size() || text[pos++] != ':') protocol_error();
        if (pos < text.size() && text[pos] == '\"') { field.string = true; field.value = quoted(pos); }
        else { const auto begin = pos; while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos; if (begin == pos) protocol_error(); field.value = std::string(text.substr(begin, pos - begin)); }
        for (const auto& previous : fields) if (previous.key == field.key) protocol_error();
        fields.push_back(std::move(field));
        if (pos == text.size() - 1) break;
        if (text[pos++] != ',') protocol_error();
    }
    return fields;
}
const Field* field(const std::vector<Field>& fields, std::string_view name) {
    for (const auto& candidate : fields) if (candidate.key == name) return &candidate;
    return nullptr;
}
std::string required_string(const std::vector<Field>& fields, std::string_view name) {
    const auto* value = field(fields, name); if (!value || !value->string) protocol_error(); return value->value;
}
std::optional<std::uint64_t> optional_number(const std::vector<Field>& fields, std::string_view name) {
    const auto* value = field(fields, name); if (!value) return std::nullopt; if (value->string || (value->value.size() > 1 && value->value.front() == '0')) protocol_error();
    std::uint64_t result{}; const auto [where, error] = std::from_chars(value->value.data(), value->value.data() + value->value.size(), result);
    if (error != std::errc{} || where != value->value.data() + value->value.size()) protocol_error();
    return result;
}
SupervisionEventV6 parse_line(std::string_view line, std::string_view run_id) {
    const auto fields = parse_object(line);
    constexpr std::array<std::string_view, 15> allowed{"schema","run_id","attempt_id","node_id","seq","event","status","terminal_reason","elapsed_ms","wall_time_limit_ms","address_space_limit_bytes","input_limit_bytes","output_limit_bytes","peak_memory_availability","peak_memory_bytes"};
    for (const auto& value : fields) if (!contains(allowed, value.key)) protocol_error();
    SupervisionEventV6 event;
    event.schema = required_string(fields, "schema");
    if (required_string(fields, "run_id") != run_id) protocol_error();
    event.attempt_id = required_string(fields, "attempt_id"); event.node_id = required_string(fields, "node_id");
    const auto number = optional_number(fields, "seq"); if (!number) protocol_error(); event.seq = *number;
    event.event = required_string(fields, "event"); event.status = required_string(fields, "status");
    if (const auto* reason = field(fields, "terminal_reason")) { if (!reason->string) protocol_error(); event.terminal_reason = reason->value; }
    event.elapsed_ms = optional_number(fields, "elapsed_ms");
    event.wall_time_limit_ms = optional_number(fields, "wall_time_limit_ms");
    event.address_space_limit_bytes = optional_number(fields, "address_space_limit_bytes");
    event.input_limit_bytes = optional_number(fields, "input_limit_bytes");
    event.output_limit_bytes = optional_number(fields, "output_limit_bytes");
    event.peak_memory_availability = parse_availability(required_string(fields, "peak_memory_availability"));
    event.peak_memory_bytes = optional_number(fields, "peak_memory_bytes");
    if (serialize(event, run_id) != line) protocol_error();
    return event;
}
void validate_lifecycle(const std::vector<SupervisionEventV6>& events, std::string_view run_id) {
    struct State {
        bool started{}; bool cancelled{}; bool termed{}; bool killed{}; bool reaped{}; bool finished{};
        std::string node_id; std::string status; std::string reason;
    };
    std::vector<std::pair<std::string, State>> states;
    std::uint64_t previous{};
    for (const auto& event : events) {
        validate_event_shape(event, run_id);
        if (event.seq <= previous) protocol_error();
        previous = event.seq;
        auto found = states.end(); for (auto it = states.begin(); it != states.end(); ++it) if (it->first == event.attempt_id) { found = it; break; }
        if (found == states.end()) found = states.emplace(states.end(), event.attempt_id, State{});
        auto& state = found->second;
        if (event.event == "ATTEMPT_STARTED") { if (state.started || state.reaped || state.finished) protocol_error(); state.started = true; state.node_id = event.node_id; }
        else { if (!state.started || state.finished || state.node_id != event.node_id) protocol_error();
            if (event.event == "CANCEL_SENT") { if (state.cancelled || state.reaped) protocol_error(); state.cancelled = true; }
            else if (event.event == "TERM_SENT") { if (!state.cancelled || state.termed || state.reaped) protocol_error(); state.termed = true; }
            else if (event.event == "KILL_SENT") { if (!state.termed || state.killed || state.reaped) protocol_error(); state.killed = true; }
            else if (event.event == "REAPED") { if (state.reaped) protocol_error(); state.reaped = true; state.status = event.status; state.reason = *event.terminal_reason; }
            else if (event.event == "ATTEMPT_FINISHED") { if (!state.reaped || state.status != event.status || state.reason != *event.terminal_reason) protocol_error(); state.finished = true; }
        }
    }
}
std::vector<SupervisionEventV6> read_checked_fd(int fd, std::string_view run_id) {
    struct stat info{}; if (fstat(fd, &info) != 0 || static_cast<std::uintmax_t>(info.st_size) > kMaxLogBytes) protocol_error();
    check_owner_only(info, false);
    std::string bytes(static_cast<std::size_t>(info.st_size), '\0');
    if (!bytes.empty() && (pread(fd, bytes.data(), bytes.size(), 0) != static_cast<ssize_t>(bytes.size()))) protocol_error();
    std::vector<SupervisionEventV6> events; std::size_t begin{};
    while (begin < bytes.size()) {
        const auto newline = bytes.find('\n', begin); if (newline == std::string::npos || newline == begin || newline - begin >= kMaxLineBytes) protocol_error();
        events.push_back(parse_line(std::string_view(bytes).substr(begin, newline - begin), run_id)); begin = newline + 1;
    }
    validate_lifecycle(events, run_id);
    return events;
}
} // namespace

LogicModSupervisionLogV6::LogicModSupervisionLogV6(int control_fd, std::string run_id, std::uint64_t last_seq,
                                                   std::vector<SupervisionEventV6> events)
    : control_fd_(control_fd), run_id_(std::move(run_id)), last_seq_(last_seq), events_(std::move(events)) {}
LogicModSupervisionLogV6::~LogicModSupervisionLogV6() { if (control_fd_ >= 0) close(control_fd_); }
LogicModSupervisionLogV6::LogicModSupervisionLogV6(LogicModSupervisionLogV6&& other) noexcept
    : control_fd_(other.control_fd_), run_id_(std::move(other.run_id_)), last_seq_(other.last_seq_), events_(std::move(other.events_)) { other.control_fd_ = -1; }
LogicModSupervisionLogV6& LogicModSupervisionLogV6::operator=(LogicModSupervisionLogV6&& other) noexcept {
    if (this != &other) { if (control_fd_ >= 0) close(control_fd_); control_fd_ = other.control_fd_; other.control_fd_ = -1; run_id_ = std::move(other.run_id_); last_seq_ = other.last_seq_; events_ = std::move(other.events_); }
    return *this;
}
LogicModSupervisionLogV6 LogicModSupervisionLogV6::create(const std::filesystem::path& report_root, std::string run_id) {
    checked_identifier(run_id);
    const int control = open_control(report_root, true);
    try {
        ScopedFd log(open_log_checked(control, LogAccess::READ_WRITE_CREATE));
        if (flock(log.get(), LOCK_EX) != 0) protocol_error();
        auto events = read_checked_fd(log.get(), run_id);
        const auto last = events.empty() ? 0 : events.back().seq;
        return LogicModSupervisionLogV6(control, std::move(run_id), last, std::move(events));
    } catch (...) { close(control); throw; }
}
void LogicModSupervisionLogV6::append(const SupervisionEventV6& event) {
    if (event.seq != last_seq_ + 1) protocol_error();
    auto candidate = events_; candidate.push_back(event); validate_lifecycle(candidate, run_id_);
    const std::string line = serialize(event, run_id_) + "\n";
    ScopedFd log(open_log_checked(control_fd_, LogAccess::READ_WRITE));
    if (flock(log.get(), LOCK_EX) != 0) protocol_error();
    auto current = read_checked_fd(log.get(), run_id_);
    if (current.size() != events_.size() || (!current.empty() && current.back().seq != last_seq_)) protocol_error();
    for (std::size_t index = 0; index < current.size(); ++index) {
        if (serialize(current[index], run_id_) != serialize(events_[index], run_id_)) protocol_error();
    }
    struct stat info{}; if (fstat(log.get(), &info) != 0 || static_cast<std::uintmax_t>(info.st_size) + line.size() > kMaxLogBytes ||
        pwrite(log.get(), line.data(), line.size(), info.st_size) != static_cast<ssize_t>(line.size()) || fsync(log.get()) != 0 || fsync(control_fd_) != 0) protocol_error();
    events_ = std::move(candidate); last_seq_ = event.seq;
}
std::vector<SupervisionEventV6> validate_logic_mod_supervision_log_v6(const std::filesystem::path& report_root, std::string_view run_id) {
    checked_identifier(run_id); ScopedFd control(open_control(report_root, false));
    ScopedFd log(open_log_checked(control.get(), LogAccess::READ_ONLY));
    if (flock(log.get(), LOCK_EX) != 0) protocol_error();
    return read_checked_fd(log.get(), run_id);
}
} // namespace bruce::mod
