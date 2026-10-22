#include "bruce/worker/worker_run_inspector_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::worker_v1 {
namespace {

[[noreturn]] void protocol_error() { throw bruce::Error("PROTOCOL_ERROR"); }

bool rejection_reason(std::string_view value) {
    constexpr std::array<std::string_view, 6> reasons{
        "OUTPUT_LIMIT_EXCEEDED", "TIMEOUT", "ARTIFACT_REFERENCE_INVALID",
        "ARTIFACT_MANIFEST_INVALID", "ARTIFACT_DIGEST_MISMATCH", "ARTIFACT_STORE_VIOLATION"};
    for (const auto reason : reasons) if (value == reason) return true;
    return false;
}

void identifier(std::string_view value) {
    if (value.empty() || value.size() > 128 ||
        !((value.front() >= 'A' && value.front() <= 'Z') ||
          (value.front() >= 'a' && value.front() <= 'z') ||
          (value.front() >= '0' && value.front() <= '9'))) protocol_error();
    for (const char c : value.substr(1))
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-'))
            protocol_error();
}

void status_value(std::string_view value) {
    if (value.empty() || value.size() > 64 || value.front() < 'A' || value.front() > 'Z') protocol_error();
    for (const char c : value.substr(1))
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) protocol_error();
}

void digest_value(std::string_view value) {
    if (value.size() != 64) protocol_error();
    for (const char c : value)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) protocol_error();
}

class RecordParser {
public:
    explicit RecordParser(std::string_view line) : line_(line) {}
    void literal(std::string_view value) {
        if (!line_.substr(pos_).starts_with(value)) {
            protocol_error();
        }
        pos_ += value.size();
    }
    std::string string_value() {
        const auto begin = pos_;
        while (pos_ < line_.size() && line_[pos_] != '"') {
            const auto c = static_cast<unsigned char>(line_[pos_++]);
            if (c < 0x20 || c == '\\') protocol_error();
        }
        if (pos_ == line_.size()) protocol_error();
        const std::string value(line_.substr(begin, pos_ - begin));
        ++pos_;
        return value;
    }
    std::uint64_t number() {
        const auto begin = pos_;
        while (pos_ < line_.size() && line_[pos_] >= '0' && line_[pos_] <= '9') ++pos_;
        if (begin == pos_ || (pos_ - begin > 1 && line_[begin] == '0')) protocol_error();
        try { return bruce::integer<std::uint64_t>(line_.substr(begin, pos_ - begin)); }
        catch (const bruce::Error&) { protocol_error(); }
    }
    bool starts(std::string_view text) const { return line_.substr(pos_).starts_with(text); }
    bool done() const { return pos_ == line_.size(); }
private:
    std::string_view line_;
    std::size_t pos_{};
};

struct ParsedRecord { WorkerLogEvent event; };

void reference_component(std::string_view value) {
    if (value.empty() || value.size() > 128 ||
        !((value.front() >= 'A' && value.front() <= 'Z') ||
          (value.front() >= 'a' && value.front() <= 'z') ||
          (value.front() >= '0' && value.front() <= '9') ||
          value.front() == '_' || value.front() == '-')) protocol_error();
    for (const char c : value.substr(1))
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            protocol_error();
}

std::string attempt_series(std::string_view attempt_id) {
    const auto first = attempt_id.find('-');
    if (first == std::string_view::npos) return std::string(attempt_id);
    const auto second = attempt_id.find('-', first + 1);
    return std::string(attempt_id.substr(0, second == std::string_view::npos ? attempt_id.size() : second));
}

std::optional<std::string> attempt_single_family(std::string_view attempt_id) {
    const auto first = attempt_id.find('-');
    if (first == std::string_view::npos || attempt_id.find('-', first + 1) != std::string_view::npos)
        return std::nullopt;
    return std::string("@") + std::string(attempt_id.substr(0, first));
}

ParsedRecord parse_record(std::string_view line, const std::optional<std::string>& expected_run) {
    if (line.size() < 2 || line.front() != '{' || line.back() != '}') protocol_error();
    RecordParser p(line);
    p.literal("{\"schema\":\"worker-run-log-v1\",\"run_id\":\"");
    WorkerLogEvent e;
    e.run_id = p.string_value();
    identifier(*e.run_id);
    if (expected_run && *expected_run != *e.run_id) protocol_error();
    if (p.starts(",\"attempt_id\":\"")) {
        p.literal(",\"attempt_id\":\""); e.attempt_id = p.string_value(); identifier(*e.attempt_id);
    }
    p.literal(",\"seq\":"); e.seq = p.number();
    p.literal(",\"ts_unix_ms\":"); e.ts_unix_ms = p.number();
    p.literal(",\"event\":\""); e.event = p.string_value();
    p.literal(",\"status\":\""); e.status = p.string_value(); status_value(e.status);
    if (p.starts(",\"digest\":\"")) { p.literal(",\"digest\":\""); e.digest = p.string_value(); digest_value(*e.digest); }
    if (p.starts(",\"reference\":\"")) { p.literal(",\"reference\":\""); e.reference = p.string_value(); }
    if (p.starts(",\"size_bytes\":")) { p.literal(",\"size_bytes\":"); e.size_bytes = p.number(); }
    if (p.starts(",\"usage\":{\"")) {
        p.literal(",\"usage\":{\""); WorkerUsage u;
        p.literal("wall_ms\":"); u.wall_ms = p.number();
        p.literal(",\"input_bytes\":"); u.input_bytes = p.number();
        p.literal(",\"output_bytes\":"); u.output_bytes = p.number();
        p.literal(",\"operations\":"); u.operations = p.number();
        p.literal(",\"peak_memory_bytes\":"); u.peak_memory_bytes = p.number();
        p.literal("}"); e.usage = u;
    }
    p.literal("}"); if (!p.done()) protocol_error();
    if (e.seq == 0 || e.event.empty()) protocol_error();
    const std::array<std::string_view, 9> allowed{
        "RUN_CREATED", "ATTEMPT_STARTED", "STATUS", "CANCEL_REQUESTED",
        "PROCESS_EXITED", "RESULT_RECEIVED", "RESULT_REJECTED", "ARTIFACT_ACCEPTED", "RUN_FINISHED"};
    if (std::find(allowed.begin(), allowed.end(), e.event) == allowed.end()) protocol_error();
    if (e.reference) {
        if (!e.digest || !e.size_bytes || e.reference->empty() || e.reference->front() == '/' ||
            e.reference->find('\\') != std::string::npos || e.reference->find(':') != std::string::npos) protocol_error();
        std::size_t at = 0;
        while (at < e.reference->size()) {
            const auto slash = e.reference->find('/', at);
            const auto part = e.reference->substr(at, slash == std::string::npos ? e.reference->size() - at : slash - at);
            reference_component(part);
            at = slash == std::string::npos ? e.reference->size() : slash + 1;
        }
    } else if (e.digest || e.size_bytes) protocol_error();
    return {std::move(e)};
}

void check_private_dir(int dirfd, const char* name) {
    const int fd = ::openat(dirfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) protocol_error();
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() ||
        (st.st_mode & 0777) != 0700) { ::close(fd); protocol_error(); }
    ::close(fd);
}

std::string read_log(int rootfd) {
    const int control = ::openat(rootfd, "control", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (control < 0) protocol_error();
    const int fd = ::openat(control, "run-log.jsonl", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    ::close(control);
    if (fd < 0) protocol_error();
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
        st.st_uid != ::geteuid() || (st.st_mode & 0077) != 0 || st.st_size <= 0 ||
        static_cast<std::uint64_t>(st.st_size) > bruce::max_file_bytes) {
        ::close(fd); protocol_error();
    }
    std::string data(static_cast<std::size_t>(st.st_size), '\0');
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto n = ::read(fd, data.data() + offset, data.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ::close(fd); protocol_error(); }
        offset += static_cast<std::size_t>(n);
    }
    ::close(fd);
    if (data.empty() || data.back() != '\n') protocol_error();
    return data;
}

void verify_artifact(int rootfd, std::string_view reference,
                     std::uint64_t expected_size, std::string_view expected_digest) {
    int parent = ::dup(rootfd);
    if (parent < 0) protocol_error();
    std::size_t start = 0;
    while (true) {
        const auto slash = reference.find('/', start);
        const bool last = slash == std::string_view::npos;
        const auto component = reference.substr(start, last ? reference.size() - start : slash - start);
        if (component.empty() || component == "." || component == "..") { ::close(parent); protocol_error(); }
        if (last) {
            const int fd = ::openat(parent, std::string(component).c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
            ::close(parent);
            if (fd < 0) protocol_error();
            struct stat st{};
            if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
                st.st_uid != ::geteuid() || (st.st_mode & 0077) != 0 || st.st_size < 0 ||
                static_cast<std::uint64_t>(st.st_size) != expected_size ||
                expected_size > bruce::max_file_bytes) { ::close(fd); protocol_error(); }
            std::string data(static_cast<std::size_t>(st.st_size), '\0');
            std::size_t offset = 0;
            while (offset < data.size()) {
                const auto n = ::read(fd, data.data() + offset, data.size() - offset);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) { ::close(fd); protocol_error(); }
                offset += static_cast<std::size_t>(n);
            }
            ::close(fd);
            if (bruce::sha256(data) != expected_digest) protocol_error();
            return;
        }
        const int next = ::openat(parent, std::string(component).c_str(),
                                  O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(parent);
        if (next < 0) protocol_error();
        struct stat directory_stat{};
        if (::fstat(next, &directory_stat) != 0 || !S_ISDIR(directory_stat.st_mode) ||
            directory_stat.st_uid != ::geteuid() || (directory_stat.st_mode & 0777) != 0700) {
            ::close(next); protocol_error();
        }
        parent = next; start = slash + 1;
    }
}

int open_root_without_symlink_components(const std::filesystem::path& path) {
    if (path.empty()) protocol_error();
    int fd = ::open(path.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) protocol_error();
    for (const auto& part : path) {
        if (part == "/") continue;
        if (part == "." || part == ".." || part.empty()) { ::close(fd); protocol_error(); }
        const auto name = part.string();
        const int next = ::openat(fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(fd);
        if (next < 0) protocol_error();
        fd = next;
    }
    return fd;
}

} // namespace

WorkerRunInspectionV1 inspect_worker_run_v1(const std::filesystem::path& run_root) {
    const int rootfd = open_root_without_symlink_components(run_root);
    struct stat rst{};
    if (::fstat(rootfd, &rst) != 0 || !S_ISDIR(rst.st_mode) || rst.st_uid != ::geteuid() ||
        (rst.st_mode & 0777) != 0700) { ::close(rootfd); protocol_error(); }
    check_private_dir(rootfd, "control");
    check_private_dir(rootfd, "inputs");
    check_private_dir(rootfd, "attempts");
    const auto data = read_log(rootfd);
    WorkerRunInspectionV1 report;
    std::optional<std::string> run_id;
    struct AttemptState {
        std::size_t position{};
        bool exited{};
        bool result{};
        bool result_artifact{};
        bool accepted{};
        std::string result_reference;
        std::string result_status;
        bool post_result_cancelled{};
        bool result_rejected{};
    };
    std::map<std::string, AttemptState> positions;
    std::set<std::string> accepted_series;
    std::set<std::string> terminal_series;
    std::uint64_t previous = 0;
    bool saw_created = false;
    bool saw_finished = false;
    std::size_t begin = 0;
    while (begin < data.size()) {
        const auto end = data.find('\n', begin);
        if (end == std::string::npos || end == begin) { ::close(rootfd); protocol_error(); }
        const auto parsed = parse_record(std::string_view(data).substr(begin, end - begin), run_id);
        const auto& e = parsed.event;
        if (e.seq <= previous) { ::close(rootfd); protocol_error(); }
        previous = e.seq;
        if (!run_id) { run_id = *e.run_id; report.run_id = *run_id; }
        if (!saw_created) {
            if (e.event != "RUN_CREATED" || e.attempt_id || e.status != "READY" || e.reference || e.usage) { ::close(rootfd); protocol_error(); }
            saw_created = true; report.status = e.status;
        } else if (e.event == "RUN_CREATED") { ::close(rootfd); protocol_error(); }
        if (e.event == "RUN_FINISHED") {
            // Existing single-worker logs carry measured run usage here.
            // Scheduler-only completion still cannot invent worker metadata.
            if (saw_finished || e.attempt_id || e.reference || (e.usage && positions.empty()) || end + 1 != data.size() ||
                (e.status != "READY" && e.status != "FAILED" && e.status != "CANCELLED") ||
                (positions.empty() && e.status == "READY")) { ::close(rootfd); protocol_error(); }
            saw_finished = true; report.status = e.status;
        } else if (e.event != "RUN_CREATED") {
            if (!e.attempt_id) { ::close(rootfd); protocol_error(); }
            auto it = positions.find(*e.attempt_id);
            if (e.event == "ATTEMPT_STARTED") {
                const auto series = attempt_series(*e.attempt_id);
                const auto family = attempt_single_family(*e.attempt_id);
                if (it != positions.end() || accepted_series.contains(series) ||
                    (family && accepted_series.contains(*family)) || terminal_series.contains(series) ||
                    (family && terminal_series.contains(*family))) {
                    ::close(rootfd); protocol_error();
                }
                positions.emplace(*e.attempt_id, AttemptState{report.attempts.size(), false, false, false, false, {}, {}, false, false});
                report.attempts.push_back({});
                report.attempts.back().attempt_id = *e.attempt_id;
                it = positions.find(*e.attempt_id);
            } else if (it == positions.end()) { ::close(rootfd); protocol_error(); }
            auto& state = it->second;
            auto& attempt = report.attempts.at(state.position);
            ++attempt.event_count;
            attempt.statuses.push_back(e.status);
            const bool has_result = state.result;
            if (e.event == "CANCEL_REQUESTED" && has_result) {
                // A projected result awaits scheduler acceptance. Owner cancel
                // may still decide that pending READY/FAILED result, but may
                // never retract an accepted artifact or repeat the decision.
                if (state.accepted || state.post_result_cancelled ||
                    (state.result_rejected && state.result_status != "TIMEOUT") || e.status != "CANCELLED" ||
                    e.reference || e.usage) { ::close(rootfd); protocol_error(); }
                state.post_result_cancelled = true;
                // Owner Cancel closes even a retry-pending TIMEOUT. A later
                // attempt in this series must never turn cancellation into READY.
                terminal_series.insert(attempt_series(*e.attempt_id));
                if (const auto family = attempt_single_family(*e.attempt_id))
                    terminal_series.insert(*family);
                state.result_status = "CANCELLED";
                state.result_artifact = false;
                state.result_reference.clear();
                attempt.digest.reset();
                attempt.size_bytes.reset();
                attempt.artifact_reference.reset();
            } else if (e.event == "RESULT_REJECTED") {
                if (!has_result || state.accepted || state.post_result_cancelled || state.result_rejected ||
                    state.result_status != "READY" || !state.result_artifact ||
                    !rejection_reason(e.status) || e.reference || e.usage) {
                    ::close(rootfd); protocol_error();
                }
                state.result_rejected = true;
                // TIMEOUT is a pending retry/cancel decision. Other typed
                // rejections terminally close this attempt series.
                state.result_status = e.status == "TIMEOUT" ? "TIMEOUT" : "FAILED";
                if (e.status != "TIMEOUT") {
                    terminal_series.insert(attempt_series(*e.attempt_id));
                    if (const auto family = attempt_single_family(*e.attempt_id))
                        terminal_series.insert(*family);
                }
                state.result_artifact = false;
                state.result_reference.clear();
                attempt.statuses.back() = "FAILED";
                attempt.digest.reset();
                attempt.size_bytes.reset();
                attempt.artifact_reference.reset();
            } else if (e.event == "STATUS" || e.event == "CANCEL_REQUESTED") {
                if (has_result || state.exited) { ::close(rootfd); protocol_error(); }
            } else if (e.event == "PROCESS_EXITED") {
                if (has_result || state.exited || attempt.event_count == 1) { ::close(rootfd); protocol_error(); }
                state.exited = true;
            } else if (e.event == "RESULT_RECEIVED") {
                if (has_result || attempt.event_count == 1) { ::close(rootfd); protocol_error(); }
                if (!state.exited) { ::close(rootfd); protocol_error(); }
                state.result = true;
                state.result_status = e.status;
                if (e.reference) {
                    const auto expected_reference = "attempts/" + *e.attempt_id + "/output/result.bin";
                    if (*e.reference != expected_reference) { ::close(rootfd); protocol_error(); }
                    verify_artifact(rootfd, *e.reference, *e.size_bytes, *e.digest);
                    state.result_artifact = true;
                    state.result_reference = *e.reference;
                    attempt.digest = e.digest; attempt.size_bytes = e.size_bytes;
                    attempt.artifact_reference = e.reference; attempt.usage = e.usage;
                }
            } else if (e.event == "ARTIFACT_ACCEPTED") {
                if (!has_result || !state.result_artifact || state.accepted || state.post_result_cancelled ||
                    !e.reference || !e.digest || !e.size_bytes ||
                    *e.reference != state.result_reference ||
                    !attempt.digest || !attempt.size_bytes ||
                    *attempt.digest != *e.digest || *attempt.size_bytes != *e.size_bytes) {
                    ::close(rootfd); protocol_error();
                }
                const auto expected_reference = "attempts/" + *e.attempt_id + "/output/result.bin";
                if (*e.reference != expected_reference) { ::close(rootfd); protocol_error(); }
                verify_artifact(rootfd, *e.reference, *e.size_bytes, *e.digest);
                attempt.digest = e.digest; attempt.size_bytes = e.size_bytes; attempt.usage = e.usage;
                attempt.artifact_reference = e.reference;
                attempt.accepted = true; attempt.artifact_verified = true; state.accepted = true;
                accepted_series.insert(attempt_series(*e.attempt_id));
                if (const auto family = attempt_single_family(*e.attempt_id))
                    accepted_series.insert(*family);
            }
        }
        begin = end + 1;
    }
    if (!saw_created || previous == 0) { ::close(rootfd); protocol_error(); }
    for (const auto& item : positions) {
        if (item.second.exited && !item.second.result) { ::close(rootfd); protocol_error(); }
        if (saw_finished && !item.second.result) { ::close(rootfd); protocol_error(); }
        if (saw_finished && item.second.result_status == "READY" && !item.second.accepted) {
            ::close(rootfd); protocol_error();
        }
    }
    report.run_finished = saw_finished;
    if (!saw_finished) report.status = "INCOMPLETE";
    report.last_seq = previous;
    ::close(rootfd);
    return report;
}

} // namespace bruce::worker_v1
