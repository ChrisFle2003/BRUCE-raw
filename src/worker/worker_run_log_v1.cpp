#include "bruce/worker/worker_run_log_v1.hpp"

#include "bruce/core/json.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <array>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <regex>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace bruce::worker_v1 {
namespace {
constexpr std::array<std::string_view, 9> allowed_events{
    "RUN_CREATED", "ATTEMPT_STARTED", "STATUS", "CANCEL_REQUESTED",
    "PROCESS_EXITED", "RESULT_RECEIVED", "RESULT_REJECTED", "ARTIFACT_ACCEPTED", "RUN_FINISHED"};

[[noreturn]] void protocol_error() { throw bruce::Error("PROTOCOL_ERROR"); }

bool rejection_reason(std::string_view value) {
    constexpr std::array<std::string_view, 6> reasons{
        "OUTPUT_LIMIT_EXCEEDED", "TIMEOUT", "ARTIFACT_REFERENCE_INVALID",
        "ARTIFACT_MANIFEST_INVALID", "ARTIFACT_DIGEST_MISMATCH", "ARTIFACT_STORE_VIOLATION"};
    for (const auto reason : reasons) if (value == reason) return true;
    return false;
}

bool allowed(std::string_view value) {
    for (const auto candidate : allowed_events) if (candidate == value) return true;
    return false;
}

class JsonValidator {
public:
    explicit JsonValidator(std::string_view input) : input_(input) {}
    bool valid() { skip(); value(); skip(); return cursor_ == input_.size(); }
private:
    void skip() { while (cursor_ < input_.size() && (input_[cursor_] == ' ' || input_[cursor_] == '\t' || input_[cursor_] == '\r' || input_[cursor_] == '\n')) ++cursor_; }
    bool take(char c) { skip(); if (cursor_ == input_.size() || input_[cursor_] != c) return false; ++cursor_; return true; }
    bool string() {
        if (!take('"')) return false;
        while (cursor_ < input_.size()) {
            const unsigned char c = static_cast<unsigned char>(input_[cursor_++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (cursor_ == input_.size()) return false;
                const char esc = input_[cursor_++];
                if (esc == '"' || esc == '\\' || esc == '/' || esc == 'b' || esc == 'f' || esc == 'n' || esc == 'r' || esc == 't') continue;
                if (esc != 'u' || input_.size() - cursor_ < 4) return false;
                for (int i = 0; i < 4; ++i) {
                    const char h = input_[cursor_++];
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'))) return false;
                }
            }
        }
        return false;
    }
    bool number() {
        skip(); const auto begin = cursor_;
        if (cursor_ < input_.size() && input_[cursor_] == '-') ++cursor_;
        if (cursor_ == input_.size()) return false;
        if (input_[cursor_] == '0') ++cursor_;
        else { if (input_[cursor_] < '1' || input_[cursor_] > '9') return false; while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_; }
        if (cursor_ < input_.size() && input_[cursor_] == '.') { ++cursor_; const auto digits = cursor_; while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_; if (digits == cursor_) return false; }
        if (cursor_ < input_.size() && (input_[cursor_] == 'e' || input_[cursor_] == 'E')) { ++cursor_; if (cursor_ < input_.size() && (input_[cursor_] == '+' || input_[cursor_] == '-')) ++cursor_; const auto digits = cursor_; while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') ++cursor_; if (digits == cursor_) return false; }
        return cursor_ > begin;
    }
    bool value() {
        skip(); if (cursor_ == input_.size()) return false;
        if (input_[cursor_] == '"') return string();
        if (input_[cursor_] == '{') { ++cursor_; skip(); if (take('}')) return true; do { if (!string() || !take(':') || !value()) return false; } while (take(',')); return take('}'); }
        if (input_[cursor_] == '[') { ++cursor_; skip(); if (take(']')) return true; do { if (!value()) return false; } while (take(',')); return take(']'); }
        if (input_.substr(cursor_, 4) == "true") { cursor_ += 4; return true; }
        if (input_.substr(cursor_, 5) == "false") { cursor_ += 5; return true; }
        if (input_.substr(cursor_, 4) == "null") { cursor_ += 4; return true; }
        return number();
    }
    std::string_view input_;
    std::size_t cursor_{};
};

void identifier(std::string_view text) {
    static const std::regex grammar("[A-Za-z0-9][A-Za-z0-9._:-]{0,127}");
    if (!std::regex_match(text.begin(), text.end(), grammar)) protocol_error();
}

void status_value(std::string_view text) {
    static const std::regex grammar("[A-Z][A-Z0-9_]{0,63}");
    if (!std::regex_match(text.begin(), text.end(), grammar)) protocol_error();
}

void reference_value(std::string_view text) {
    if (text.empty() || text.front() == '/' || text.find('\\') != std::string_view::npos || text.find(':') != std::string_view::npos) protocol_error();
    std::size_t begin = 0;
    while (begin < text.size()) {
        const auto end = text.find('/', begin);
        const auto component = text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin);
        static const std::regex grammar("[A-Za-z0-9_-][A-Za-z0-9._-]{0,127}");
        if (component == "." || component == ".." || !std::regex_match(component.begin(), component.end(), grammar)) protocol_error();
        if (end == std::string_view::npos) break;
        begin = end + 1;
        if (begin == text.size()) protocol_error();
    }
}

std::string serialize(const WorkerLogEvent& e, const std::string& run_id) {
    if (e.schema != "worker-run-log-v1" || !allowed(e.event)) protocol_error();
    identifier(run_id); status_value(e.status);
    if (e.event == "RESULT_REJECTED" &&
        (!rejection_reason(e.status) || !e.attempt_id || e.reference || e.digest ||
         e.size_bytes || e.usage)) protocol_error();
    if (e.attempt_id) identifier(*e.attempt_id);
    if (e.run_id && *e.run_id != run_id) protocol_error();
    if (e.digest) {
        if (e.digest->size() != 64) protocol_error();
        for (const char c : *e.digest) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) protocol_error();
    }
    if (e.reference) {
        reference_value(*e.reference);
        const std::filesystem::path ref(*e.reference);
        if (ref.empty() || ref.is_absolute() || ref.has_root_name()) protocol_error();
        for (const auto& component : ref) if (component == "..") protocol_error();
        if (!e.size_bytes || !e.digest) protocol_error();
    } else if (e.size_bytes || e.digest) {
        protocol_error();
    }
    std::string out = "{\"schema\":" + bruce::json_string(e.schema) +
        ",\"run_id\":" + bruce::json_string(run_id);
    if (e.attempt_id) out += ",\"attempt_id\":" + bruce::json_string(*e.attempt_id);
    out += ",\"seq\":" + std::to_string(e.seq) + ",\"ts_unix_ms\":" + std::to_string(e.ts_unix_ms) +
        ",\"event\":" + bruce::json_string(e.event) + ",\"status\":" + bruce::json_string(e.status);
    if (e.digest) out += ",\"digest\":" + bruce::json_string(*e.digest);
    if (e.reference) out += ",\"reference\":" + bruce::json_string(*e.reference);
    if (e.size_bytes) out += ",\"size_bytes\":" + std::to_string(*e.size_bytes);
    if (e.usage) out += ",\"usage\":{\"wall_ms\":" + std::to_string(e.usage->wall_ms) +
        ",\"input_bytes\":" + std::to_string(e.usage->input_bytes) +
        ",\"output_bytes\":" + std::to_string(e.usage->output_bytes) +
        ",\"operations\":" + std::to_string(e.usage->operations) +
        ",\"peak_memory_bytes\":" + std::to_string(e.usage->peak_memory_bytes) + "}";
    out += '}';
    return out;
}

std::string_view string_field(std::string_view line, std::string_view key, bool required) {
    const std::string needle = "\"" + std::string(key) + "\":\"";
    const auto at = line.find(needle);
    if (at == std::string_view::npos) { if (required) protocol_error(); return {}; }
    if (line.find(needle, at + needle.size()) != std::string_view::npos) protocol_error();
    const auto begin = at + needle.size();
    const auto end = line.find('"', begin);
    if (end == std::string_view::npos || line.substr(begin, end - begin).find('\\') != std::string_view::npos) protocol_error();
    return line.substr(begin, end - begin);
}

std::optional<std::uint64_t> number_field(std::string_view line, std::string_view key, bool required) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const auto at = line.find(needle);
    if (at == std::string_view::npos) { if (required) protocol_error(); return {}; }
    if (line.find(needle, at + needle.size()) != std::string_view::npos) protocol_error();
    const auto begin = at + needle.size(); auto end = begin;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') ++end;
    if (begin == end || (end - begin > 1 && line[begin] == '0')) protocol_error();
    try { return bruce::integer<std::uint64_t>(line.substr(begin, end - begin)); }
    catch (const bruce::Error&) { protocol_error(); }
}

void validate_reference(const std::filesystem::path& root, const WorkerLogEvent& e) {
    if (!e.reference) return;
    const auto candidate = root / *e.reference;
    std::error_code ec;
    const auto resolved_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) protocol_error();
    const auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) protocol_error();
    auto r = resolved_root.begin(), c = resolved.begin();
    for (; r != resolved_root.end() && c != resolved.end(); ++r, ++c) if (*r != *c) protocol_error();
    if (r != resolved_root.end()) protocol_error();
    if (!std::filesystem::is_regular_file(resolved, ec) || ec) protocol_error();
    const auto size = std::filesystem::file_size(resolved, ec);
    if (ec || size != *e.size_bytes || size > bruce::max_file_bytes) protocol_error();
    std::ifstream in(resolved, std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad() || bruce::sha256(bytes) != *e.digest) protocol_error();
}

std::uint64_t validate_existing_record(std::string_view line, const std::filesystem::path& root,
                                       const std::string& expected_run_id) {
    if (line.size() < 2 || line.front() != '{' || line.back() != '}' || !JsonValidator(line).valid()) protocol_error();
    WorkerLogEvent e;
    e.schema = std::string(string_field(line, "schema", true));
    e.run_id = std::string(string_field(line, "run_id", true));
    if (*e.run_id != expected_run_id) protocol_error();
    const auto attempt = string_field(line, "attempt_id", false);
    if (!attempt.empty()) e.attempt_id = std::string(attempt);
    e.seq = *number_field(line, "seq", true);
    e.ts_unix_ms = *number_field(line, "ts_unix_ms", true);
    e.event = std::string(string_field(line, "event", true));
    e.status = std::string(string_field(line, "status", true));
    const auto digest = string_field(line, "digest", false);
    if (!digest.empty()) e.digest = std::string(digest);
    const auto reference = string_field(line, "reference", false);
    if (!reference.empty()) e.reference = std::string(reference);
    e.size_bytes = number_field(line, "size_bytes", false);
    if (line.find("\"usage\":{") != std::string_view::npos) {
        WorkerUsage usage;
        usage.wall_ms = *number_field(line, "wall_ms", true);
        usage.input_bytes = *number_field(line, "input_bytes", true);
        usage.output_bytes = *number_field(line, "output_bytes", true);
        usage.operations = *number_field(line, "operations", true);
        usage.peak_memory_bytes = *number_field(line, "peak_memory_bytes", true);
        e.usage = usage;
    }
    if (serialize(e, expected_run_id) != line) protocol_error();
    validate_reference(root, e);
    return e.seq;
}

std::uint64_t validate_log_data(std::string_view data, const std::filesystem::path& root,
                                const std::string& run_id) {
    if (!data.empty() && data.back() != '\n') protocol_error();
    std::uint64_t last = 0;
    std::size_t start = 0;
    while (start < data.size()) {
        const auto end = data.find('\n', start);
        if (end == std::string_view::npos || end == start) protocol_error();
        const auto seq = validate_existing_record(data.substr(start, end - start), root, run_id);
        if (seq <= last) protocol_error();
        last = seq;
        start = end + 1;
    }
    return last;
}
}

WorkerRunLog::WorkerRunLog(std::filesystem::path root, std::string id,
                           std::filesystem::path log, std::uint64_t last)
    : run_root_(std::move(root)), run_id_(std::move(id)), log_path_(std::move(log)), last_seq_(last) {}

WorkerRunLog WorkerRunLog::create(const std::filesystem::path& run_root, std::string run_id) {
    if (run_id.empty()) protocol_error();
    identifier(run_id);
    std::error_code ec;
    std::filesystem::create_directories(run_root / "control", ec);
    if (ec) protocol_error();
    const auto root = std::filesystem::weakly_canonical(run_root, ec);
    if (ec) protocol_error();
    const auto control = std::filesystem::weakly_canonical(root / "control", ec);
    if (ec || control.parent_path() != root) protocol_error();
    std::filesystem::permissions(control, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) protocol_error();
    const auto log = control / "run-log.jsonl";
    struct stat log_stat{};
    if (::lstat(log.c_str(), &log_stat) == 0) {
        if (!S_ISREG(log_stat.st_mode) || log_stat.st_nlink != 1) protocol_error();
    } else if (errno != ENOENT) protocol_error();
    std::uint64_t last = 0;
    if (std::filesystem::exists(log)) {
        const int fd = ::open(log.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) protocol_error();
        struct stat st{};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) { ::close(fd); protocol_error(); }
        std::string data;
        char buffer[8192];
        for (;;) {
            const auto count = ::read(fd, buffer, sizeof(buffer));
            if (count < 0) { ::close(fd); protocol_error(); }
            if (count == 0) break;
            data.append(buffer, static_cast<std::size_t>(count));
            if (data.size() > bruce::max_file_bytes) { ::close(fd); protocol_error(); }
        }
        ::close(fd);
        last = validate_log_data(data, root, run_id);
    }
    return WorkerRunLog(root, std::move(run_id), log, last);
}

void WorkerRunLog::append(const WorkerLogEvent& e) {
    const auto line = serialize(e, run_id_);
    validate_reference(run_root_, e);
    const int dirfd = ::open((run_root_ / "control").c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dirfd < 0) protocol_error();
    const int fd = ::openat(dirfd, "run-log.jsonl", O_RDWR | O_APPEND | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) { ::close(dirfd); protocol_error(); }
    if (::flock(fd, LOCK_EX) != 0) { ::close(fd); ::close(dirfd); protocol_error(); }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
        ::close(fd); ::close(dirfd); protocol_error();
    }
    std::string data;
    char buffer[8192];
    off_t offset = 0;
    for (;;) {
        const auto count = ::pread(fd, buffer, sizeof(buffer), offset);
        if (count < 0) { ::close(fd); ::close(dirfd); protocol_error(); }
        if (count == 0) break;
        data.append(buffer, static_cast<std::size_t>(count));
        offset += count;
        if (data.size() > bruce::max_file_bytes) { ::close(fd); ::close(dirfd); protocol_error(); }
    }
    const auto last = validate_log_data(data, run_root_, run_id_);
    if (e.seq <= last) { ::close(fd); ::close(dirfd); protocol_error(); }
    const std::string record = line + '\n';
    std::size_t written = 0;
    while (written < record.size()) {
        const auto count = ::write(fd, record.data() + written, record.size() - written);
        if (count <= 0) { ::close(fd); ::close(dirfd); protocol_error(); }
        written += static_cast<std::size_t>(count);
    }
    if (::fsync(fd) != 0) { ::close(fd); ::close(dirfd); protocol_error(); }
    ::close(fd);
    ::close(dirfd);
    last_seq_ = e.seq;
}

} // namespace bruce::worker_v1
