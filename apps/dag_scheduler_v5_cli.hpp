#pragma once

#include "dag_scheduler_v3_cli.hpp"
#include "bruce/artifact/artifact_payload_v1.hpp"
#include "bruce/artifact/artifact_payload_external_v1.hpp"
#include "bruce/artifact/artifact_payload_stream_v1.hpp"
#include "bruce/artifact/payload_run_inspector_v1.hpp"
#include "bruce/artifact/payload_flow_inspector_v2.hpp"
#include "bruce/artifact/payload_flow_v2.hpp"
#include "bruce/artifact/execute_handoff_lineage_v1.hpp"
#include "bruce/artifact/execute_handoff_inspector_v1.hpp"
#include "bruce/artifact/v5_manifest_id_v1.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/worker/worker_run_inspector_v1.hpp"
#include "bruce/worker/worker_supervisor_v1.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include "bruce/application/common_runtime_host_v2.hpp"
#include "bruce/application/common_runtime_host_v2_batch.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <memory>
#include <openssl/evp.h>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fs.h>
#include <unistd.h>

namespace bruce {
namespace dag_scheduler_v5_cli_detail {

inline constexpr std::string_view usage =
    "Usage: bruce dag-scheduler-v5 execute SPEC --output BUNDLE [--retry N] [--max-wall-ms N] [--max-input-bytes N] [--max-output-bytes N] [--max-operations N] [--max-memory-bytes N] [--workers N] [--handoff-v1]";
inline constexpr std::string_view inspect_usage =
    "Usage: bruce dag-scheduler-v5 inspect BUNDLE [--handoff-v1]";
inline constexpr std::string_view publish_usage =
    "Usage: bruce dag-scheduler-v5 publish BUNDLE --store STORE";
inline constexpr std::string_view payload_usage =
    "Usage: bruce dag-scheduler-v5 payload SPEC --output BUNDLE --store STORE [--external-inputs INPUTS] [--flow-v2] [--stream-v15] [--retry N] [--max-wall-ms N] [--max-input-bytes N] [--max-output-bytes N] [--max-operations N] [--max-memory-bytes N] [--workers N]";

struct Options {
    std::uint64_t retry{0};
    worker_v1::Budget budget{30000, 65536, 65536, 1000000, 268435456};
    std::uint64_t workers{1};
};

struct PublishRequest {
    std::filesystem::path bundle;
    std::filesystem::path store;
};

struct CancelRequest {
    std::filesystem::path bundle;
};

struct PayloadRequest {
    std::filesystem::path spec;
    std::filesystem::path bundle;
    std::filesystem::path store;
    std::optional<std::filesystem::path> external_inputs;
    Options options;
    bool flow_v2{false};
    bool stream_v15{false};
};

struct PayloadInspectRequestV1 {
    std::filesystem::path bundle;
    bool stream_v15{false};
};

struct PayloadInspectRequestV2 {
    std::filesystem::path bundle;
};

inline PayloadInspectRequestV2 parse_payload_inspect_v2(std::vector<std::string_view> args) {
    if (args.size() != 3 || args[0] != "payload-inspect" || args[2] != "--flow-v2" ||
        args[1].empty() || args[1].starts_with('-')) throw Error("PROTOCOL_ERROR");
    const std::filesystem::path bundle(args[1]);
    if (bundle.filename() == "." || bundle.filename() == ".." || bundle.string().ends_with(".run"))
        throw Error("PROTOCOL_ERROR");
    return {bundle};
}

inline std::string render_payload_inspection_v2(const artifact_v1::PayloadFlowInspectionV2& value) {
    std::ostringstream out;
    out << "{\"contract\":\"dag-scheduler-v5-payload-flow-inspect-v2\",\"action\":\"payload-inspect\""
        << ",\"status\":" << json_string(value.status)
        << ",\"run_status\":" << json_string(value.run_status)
        << ",\"run_id\":" << json_string(value.run_id)
        << ",\"node_count\":" << value.node_count
        << ",\"attempt_count\":" << value.attempt_count
        << ",\"binding_count\":" << value.binding_count
        << ",\"verified_input_bytes\":" << value.verified_input_bytes
        << ",\"stream_v15\":" << (value.stream_v15 ? "true" : "false")
        << ",\"input_completeness\":" << json_string(value.input_completeness)
        << ",\"provenance_scope\":\"LOCAL_CONSISTENCY\"}\n";
    return out.str();
}

inline int payload_inspect_v2(const PayloadInspectRequestV2& request) {
    try {
        const auto inspection = artifact_v1::inspect_payload_flow_v2(request.bundle);
        std::cout << render_payload_inspection_v2(inspection);
        return inspection.status == "VALID" ? 0 : 1;
    } catch (...) {
        std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}

inline PayloadInspectRequestV1 parse_payload_inspect_v1(std::vector<std::string_view> args) {
    if ((args.size() != 2 && args.size() != 3) || args[0] != "payload-inspect" ||
        args[1].empty() || args[1].starts_with('-') ||
        (args.size() == 3 && args[2] != "--stream-v15"))
        throw Error("PROTOCOL_ERROR");
    const std::filesystem::path bundle(args[1]);
    if (bundle.filename() == "." || bundle.filename() == ".." ||
        bundle.string().ends_with(".run"))
        throw Error("PROTOCOL_ERROR");
    return {bundle, args.size() == 3};
}

inline std::string render_payload_inspection_v1(const artifact_v1::PayloadRunInspectionV1& value) {
    std::ostringstream out;
    out << "{\"contract\":\"dag-scheduler-v5-payload-inspect-v1\",\"action\":\"payload-inspect\""
        << ",\"status\":" << json_string(value.run_finished ? "VALID" : "INCOMPLETE")
        << ",\"run_finished\":" << (value.run_finished ? "true" : "false")
        << ",\"run_status\":" << json_string(value.run_finished ? value.run_status : "INCOMPLETE")
        << ",\"source_contract\":" << json_string(value.source_contract)
        << ",\"run_id\":" << json_string(value.run_id)
        << ",\"last_seq\":" << value.last_seq
        << ",\"node_count\":" << value.node_count
        << ",\"attempt_count\":" << value.attempt_count
        << ",\"accepted_artifact_count\":" << value.accepted_artifact_count
        << ",\"stream_check\":" << json_string(value.streams_requested ? "VERIFIED_PRESENT" : "NOT_REQUESTED")
        << ",\"verified_stream_count\":" << value.verified_stream_count
        << ",\"verified_stream_bytes\":" << value.verified_stream_bytes
        << ",\"input_completeness\":\"NOT_PROVABLE\",\"external_provenance\":\"NOT_CHECKED\"}\n";
    return out.str();
}

inline int payload_inspect_v1(const PayloadInspectRequestV1& request) {
    try {
        const auto inspection = artifact_v1::inspect_payload_run_v1(request.bundle, {request.stream_v15});
        const auto report = render_payload_inspection_v1(inspection);
        std::cout << report;
        return inspection.run_finished ? 0 : 1;
    } catch (...) {
        std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}

inline bool publish_path_token(std::string_view value) {
    return !value.empty() && !value.starts_with('-');
}

inline PublishRequest parse_publish(std::vector<std::string_view> args) {
    if (args.size() != 4 || args[0] != "publish" || args[2] != "--store" ||
        !publish_path_token(args[1]) || !publish_path_token(args[3]))
        throw Error("PROTOCOL_ERROR");
    const std::filesystem::path bundle(args[1]);
    if (bundle.filename() == "." || bundle.filename() == ".." ||
        bundle.string().ends_with(".run"))
        throw Error("PROTOCOL_ERROR");
    return {bundle, std::filesystem::path(args[3])};
}

inline PublishRequest parse_publish(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0);
    for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
    return parse_publish(std::move(args));
}

inline CancelRequest parse_cancel(std::vector<std::string_view> args) {
    if (args.size() != 2 || args[0] != "cancel" || !publish_path_token(args[1]))
        throw Error("PROTOCOL_ERROR");
    std::filesystem::path bundle(args[1]);
    // A trailing separator makes path::filename() empty. Strip only those
    // lexical separators for the syntax check so BUNDLE.run/ cannot be
    // mistaken for a bundle target and reach the availability path.
    while (!bundle.empty() && bundle.filename().empty()) {
        const auto parent = bundle.parent_path();
        if (parent == bundle) break;
        bundle = parent;
    }
    if (bundle.filename() == "." || bundle.filename() == ".." ||
        bundle.string().ends_with(".run"))
        throw Error("PROTOCOL_ERROR");
    return {std::filesystem::path(args[1])};
}

inline CancelRequest parse_cancel(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0);
    for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
    return parse_cancel(std::move(args));
}

inline std::uint64_t decimal(std::string_view text, std::uint64_t maximum, bool zero_allowed) {
    if (text.empty() || (text.size() > 1 && text.front() == '0')) throw Error("InvalidDagSchedulerV5Option");
    std::uint64_t value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() ||
        value > maximum || (!zero_allowed && value == 0)) throw Error("InvalidDagSchedulerV5Option");
    return value;
}

inline Options parse_options(const std::vector<std::string>& args) {
    if (args.size() % 2) throw Error("InvalidDagSchedulerV5Option");
    Options result;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < args.size(); i += 2) {
        const auto& key = args[i];
        if (!seen.insert(key).second) throw Error("InvalidDagSchedulerV5Option");
        const auto& value = args[i + 1];
        if (key == "--retry") result.retry = decimal(value, 3, true);
        else if (key == "--max-wall-ms") result.budget.max_wall_ms = decimal(value, 600000, false);
        else if (key == "--max-input-bytes") result.budget.max_input_bytes = decimal(value, 1048576, false);
        else if (key == "--max-output-bytes") result.budget.max_output_bytes = decimal(value, 5242880, false);
        else if (key == "--max-operations") result.budget.max_operations = decimal(value, 10000000, false);
        else if (key == "--max-memory-bytes") result.budget.max_memory_bytes = decimal(value, 2147483648ULL, false);
        else if (key == "--workers") result.workers = decimal(value, 4, false);
        else throw Error("InvalidDagSchedulerV5Option");
    }
    return result;
}

inline PayloadRequest parse_payload(std::vector<std::string_view> args) {
    if (args.size() < 6 || args[0] != "payload" ||
        !publish_path_token(args[1]) || args[2] != "--output" ||
        !publish_path_token(args[3]) || args[4] != "--store" ||
        !publish_path_token(args[5]))
        throw Error("PROTOCOL_ERROR");
    const std::filesystem::path spec(args[1]);
    const std::filesystem::path bundle(args[3]);
    const std::filesystem::path store(args[5]);
    if (bundle.filename() == "." || bundle.filename() == ".." ||
        bundle.string().ends_with(".run") || store == bundle)
        throw Error("PROTOCOL_ERROR");
    std::vector<std::string> option_args;
    option_args.reserve(args.size() - 6);
    std::optional<std::filesystem::path> external_inputs;
    bool flow_v2 = false;
    bool stream_v15 = false;
    for (std::size_t index = 6; index < args.size();) {
        if (args[index] == "--handoff-v1") throw Error("PROTOCOL_ERROR");
        if (args[index] == "--stream-v15") {
            if (stream_v15) throw Error("InvalidDagSchedulerV5Option");
            stream_v15 = true;
            ++index;
            continue;
        }
        if (args[index] == "--flow-v2") {
            if (flow_v2) throw Error("InvalidDagSchedulerV5Option");
            flow_v2 = true;
            ++index;
            continue;
        }
        if (args[index] == "--external-inputs") {
            if (external_inputs || index + 1 >= args.size() ||
                !publish_path_token(args[index + 1]))
                throw Error("InvalidDagSchedulerV5Option");
            external_inputs = std::filesystem::path(args[index + 1]);
            index += 2;
            continue;
        }
        option_args.emplace_back(args[index]);
        ++index;
    }
    return {spec, bundle, store, external_inputs, parse_options(option_args), flow_v2, stream_v15};
}

inline PayloadRequest parse_payload(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 2 ? static_cast<std::size_t>(argc - 2) : 0);
    for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
    return parse_payload(std::move(args));
}

inline std::filesystem::path normalized_inspect_bundle_path(const std::filesystem::path& bundle) {
    if (bundle.empty()) throw Error("PROTOCOL_ERROR");
    const auto source = bundle.is_absolute() ? bundle.relative_path() : bundle;
    std::size_t depth = 0;
    for (const auto& component : source) {
        if (component.empty() || component == ".") continue;
        if (component == "..") {
            if (depth == 0) throw Error("PROTOCOL_ERROR");
            --depth;
        } else {
            ++depth;
        }
    }
    const auto absolute = std::filesystem::absolute(bundle).lexically_normal();
    if (absolute.filename().empty() || absolute.filename() == "." ||
        absolute.filename() == ".." || absolute.filename().string().ends_with(".run"))
        throw Error("PROTOCOL_ERROR");
    return absolute;
}

inline std::string inspect_bundle_file(const std::filesystem::path& bundle) {
    const auto absolute = normalized_inspect_bundle_path(bundle);
    int directory = ::open(absolute.root_path().c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory < 0) throw Error("PROTOCOL_ERROR");
    const auto parent = absolute.parent_path();
    for (const auto& component : parent.relative_path()) {
        if (component.empty() || component == "." || component == "..") {
            ::close(directory); throw Error("PROTOCOL_ERROR");
        }
        const int next = ::openat(directory, component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(directory);
        if (next < 0) throw Error("PROTOCOL_ERROR");
        directory = next;
    }
    struct stat st{};
    const int fd = ::openat(directory, absolute.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    ::close(directory);
    if (fd < 0 || ::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
        st.st_uid != ::geteuid() || (st.st_mode & 0077) != 0 || st.st_size <= 0 ||
        static_cast<std::uint64_t>(st.st_size) > bruce::max_file_bytes) {
        if (fd >= 0) ::close(fd);
        throw Error("PROTOCOL_ERROR");
    }
    std::string bytes(static_cast<std::size_t>(st.st_size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ::close(fd); throw Error("PROTOCOL_ERROR"); }
        offset += static_cast<std::size_t>(n);
    }
    ::close(fd);
    return bytes;
}

inline std::string render_inspection_report(const worker_v1::WorkerRunInspectionV1& inspection) {
    std::string report = "{\"contract\":\"dag-scheduler-v5-inspect-v1\",\"action\":\"inspect\"";
    report += ",\"status\":" + json_string(inspection.run_finished ? "VALID" : "INCOMPLETE");
    report += ",\"run_finished\":" + std::string(inspection.run_finished ? "true" : "false");
    report += ",\"run_status\":" + json_string(inspection.status);
    report += ",\"run_id\":" + json_string(inspection.run_id);
    report += ",\"last_seq\":" + std::to_string(inspection.last_seq);
    report += ",\"attempts\":[";
    for (std::size_t i = 0; i < inspection.attempts.size(); ++i) {
        if (i) report += ',';
        const auto& attempt = inspection.attempts[i];
        report += "{\"attempt_id\":" + json_string(attempt.attempt_id) + ",\"statuses\":[";
        for (std::size_t j = 0; j < attempt.statuses.size(); ++j) {
            if (j) report += ',';
            report += json_string(attempt.statuses[j]);
        }
        report += "],\"accepted\":" + std::string(attempt.accepted ? "true" : "false");
        report += ",\"event_count\":" + std::to_string(attempt.event_count);
        report += ",\"digest\":" + (attempt.digest ? json_string(*attempt.digest) : "null");
        report += ",\"size_bytes\":" + (attempt.size_bytes ? std::to_string(*attempt.size_bytes) : "null");
        if (attempt.usage) {
            const auto& u = *attempt.usage;
            report += ",\"usage\":{\"wall_ms\":" + std::to_string(u.wall_ms) +
                ",\"input_bytes\":" + std::to_string(u.input_bytes) +
                ",\"output_bytes\":" + std::to_string(u.output_bytes) +
                ",\"operations\":" + std::to_string(u.operations) +
                ",\"peak_memory_bytes\":" + std::to_string(u.peak_memory_bytes) + "}";
        } else {
            report += ",\"usage\":null";
        }
        report += ",\"artifact_verified\":" + std::string(attempt.artifact_verified ? "true" : "false") + '}';
    }
    report += "]}\n";
    return report;
}

struct InspectJsonValue {
    enum class Kind { string, number, boolean, null_value, object, array } kind{};
    std::string text;
    std::uint64_t number{};
    bool boolean{};
    std::vector<std::pair<std::string, InspectJsonValue>> object;
    std::vector<InspectJsonValue> array;
};

class InspectJsonParser {
public:
    explicit InspectJsonParser(std::string_view source) : source_(source) {}
    InspectJsonValue parse() {
        auto value = value_at();
        if (cursor_ != source_.size()) throw Error("PROTOCOL_ERROR");
        return value;
    }
private:
    [[noreturn]] static void reject() { throw Error("PROTOCOL_ERROR"); }
    std::string string_at() {
        if (cursor_ >= source_.size() || source_[cursor_++] != '"') reject();
        std::string value;
        while (cursor_ < source_.size()) {
            const char c = source_[cursor_++];
            if (c == '"') return value;
            if (static_cast<unsigned char>(c) < 0x20 || c == '\\') reject();
            value.push_back(c);
        }
        reject();
    }
    std::uint64_t number_at() {
        const auto begin = cursor_;
        while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
        if (begin == cursor_ || (cursor_ - begin > 1 && source_[begin] == '0')) reject();
        try { return bruce::integer<std::uint64_t>(source_.substr(begin, cursor_ - begin)); }
        catch (...) { reject(); }
    }
    InspectJsonValue value_at() {
        if (cursor_ >= source_.size()) reject();
        if (source_[cursor_] == '"') {
            InspectJsonValue value;
            value.kind = InspectJsonValue::Kind::string;
            value.text = string_at();
            return value;
        }
        if (source_[cursor_] == '{') {
            if (++depth_ > max_depth) reject();
            ++cursor_;
            InspectJsonValue value; value.kind = InspectJsonValue::Kind::object;
            if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
            while (true) {
                if (cursor_ >= source_.size() || source_[cursor_] != '"') reject();
                auto key = string_at();
                if (std::any_of(value.object.begin(), value.object.end(),
                                [&](const auto& item) { return item.first == key; })) reject();
                if (cursor_ >= source_.size() || source_[cursor_++] != ':') reject();
                value.object.emplace_back(std::move(key), value_at());
                if (cursor_ >= source_.size()) reject();
                if (source_[cursor_] == '}') { ++cursor_; --depth_; return value; }
                if (source_[cursor_++] != ',') reject();
            }
        }
        if (source_[cursor_] == '[') {
            if (++depth_ > max_depth) reject();
            ++cursor_;
            InspectJsonValue value; value.kind = InspectJsonValue::Kind::array;
            if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
            while (true) {
                value.array.push_back(value_at());
                if (cursor_ >= source_.size()) reject();
                if (source_[cursor_] == ']') { ++cursor_; --depth_; return value; }
                if (source_[cursor_++] != ',') reject();
            }
        }
        if (source_.substr(cursor_, 4) == "true") {
            cursor_ += 4; InspectJsonValue value; value.kind = InspectJsonValue::Kind::boolean;
            value.boolean = true; return value;
        }
        if (source_.substr(cursor_, 5) == "false") {
            cursor_ += 5; InspectJsonValue value; value.kind = InspectJsonValue::Kind::boolean;
            value.boolean = false; return value;
        }
        if (source_.substr(cursor_, 4) == "null") {
            cursor_ += 4; InspectJsonValue value; value.kind = InspectJsonValue::Kind::null_value;
            return value;
        }
        InspectJsonValue value; value.kind = InspectJsonValue::Kind::number;
        value.number = number_at(); return value;
    }
    std::string_view source_;
    std::size_t cursor_{};
    static constexpr std::size_t max_depth = 64;
    std::size_t depth_{};
};

inline const InspectJsonValue& inspect_field(const InspectJsonValue& object, std::string_view key) {
    if (object.kind != InspectJsonValue::Kind::object) throw Error("PROTOCOL_ERROR");
    for (const auto& item : object.object) if (item.first == key) return item.second;
    throw Error("PROTOCOL_ERROR");
}

inline void inspect_exact_keys(const InspectJsonValue& object,
                               std::initializer_list<std::string_view> keys) {
    if (object.kind != InspectJsonValue::Kind::object || object.object.size() != keys.size())
        throw Error("PROTOCOL_ERROR");
    std::size_t index = 0;
    for (const auto key : keys) {
        if (object.object[index].first != key) throw Error("PROTOCOL_ERROR");
        ++index;
    }
}

inline std::string inspect_string(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::string) throw Error("PROTOCOL_ERROR");
    return value.text;
}
inline std::uint64_t inspect_number(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::number) throw Error("PROTOCOL_ERROR");
    return value.number;
}
inline bool inspect_boolean(const InspectJsonValue& value) {
    if (value.kind != InspectJsonValue::Kind::boolean) throw Error("PROTOCOL_ERROR");
    return value.boolean;
}
inline std::optional<std::string> inspect_optional_string(const InspectJsonValue& value) {
    if (value.kind == InspectJsonValue::Kind::null_value) return std::nullopt;
    return inspect_string(value);
}

inline bool ascii_identifier_char(unsigned char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9');
}
inline void inspect_identifier(std::string_view value) {
    if (value.empty() || value.size() > 128 ||
        !ascii_identifier_char(static_cast<unsigned char>(value.front()))) throw Error("PROTOCOL_ERROR");
    for (const char c : value.substr(1))
        if (!ascii_identifier_char(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != ':' && c != '-')
            throw Error("PROTOCOL_ERROR");
}
inline void inspect_digest(std::string_view value) {
    if (value.size() != 64 || !std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) throw Error("PROTOCOL_ERROR");
}
inline void inspect_public_reference(std::string_view value) {
    if (!value.starts_with("artifact/") || value.size() <= 9 || value.find('/') != 8 ||
        value.find('\\') != std::string_view::npos || value.find("..") != std::string_view::npos)
        throw Error("PROTOCOL_ERROR");
    inspect_identifier(value.substr(9));
}
inline worker_v1::WorkerUsage inspect_usage_value(const InspectJsonValue& value) {
    inspect_exact_keys(value, {"wall_ms", "input_bytes", "output_bytes", "operations", "peak_memory_bytes"});
    worker_v1::WorkerUsage usage;
    usage.wall_ms = inspect_number(value.object[0].second);
    usage.input_bytes = inspect_number(value.object[1].second);
    usage.output_bytes = inspect_number(value.object[2].second);
    usage.operations = inspect_number(value.object[3].second);
    usage.peak_memory_bytes = inspect_number(value.object[4].second);
    return usage;
}
struct InspectBudget {
    std::uint64_t max_wall_ms{};
    std::uint64_t max_input_bytes{};
    std::uint64_t max_output_bytes{};
    std::uint64_t max_operations{};
    std::uint64_t max_memory_bytes{};
};
inline InspectBudget inspect_budget_value(const InspectJsonValue& value) {
    inspect_exact_keys(value, {"max_wall_ms", "max_input_bytes", "max_output_bytes", "max_operations", "max_memory_bytes"});
    InspectBudget budget;
    budget.max_wall_ms = inspect_number(value.object[0].second);
    budget.max_input_bytes = inspect_number(value.object[1].second);
    budget.max_output_bytes = inspect_number(value.object[2].second);
    budget.max_operations = inspect_number(value.object[3].second);
    budget.max_memory_bytes = inspect_number(value.object[4].second);
    if (!budget.max_wall_ms || budget.max_wall_ms > 600000 ||
        !budget.max_input_bytes || budget.max_input_bytes > 1048576 ||
        !budget.max_output_bytes || budget.max_output_bytes > 5242880 ||
        !budget.max_operations || budget.max_operations > 10000000 ||
        !budget.max_memory_bytes || budget.max_memory_bytes > 2147483648ULL)
        throw Error("PROTOCOL_ERROR");
    return budget;
}
inline void inspect_usage_bounds(const worker_v1::WorkerUsage& usage, const InspectBudget& budget) {
    if (usage.wall_ms > budget.max_wall_ms + 1000 || usage.input_bytes > budget.max_input_bytes ||
        usage.output_bytes > budget.max_output_bytes || usage.operations > budget.max_operations ||
        usage.peak_memory_bytes > budget.max_memory_bytes)
        throw Error("PROTOCOL_ERROR");
}
inline void inspect_status(std::string_view value) {
    if (value != "READY" && value != "FAILED" && value != "BLOCKED" && value != "CANCELLED")
        throw Error("PROTOCOL_ERROR");
}
inline void inspect_failure(std::string_view value) {
    static constexpr std::array<std::string_view, 15> values{
        "NONE", "READY", "FAILED", "BLOCKED", "INVALID_REQUEST", "PROTOCOL_ERROR",
        "PROCESS_START_FAILED", "PROCESS_CRASHED", "TIMEOUT", "CANCELLED",
        "INPUT_LIMIT_EXCEEDED", "OUTPUT_LIMIT_EXCEEDED", "OPERATION_BUDGET_EXCEEDED",
        "MEMORY_LIMIT_EXCEEDED", "DIGEST_MISMATCH"};
    if (std::find(values.begin(), values.end(), value) == values.end()) throw Error("PROTOCOL_ERROR");
}
inline void inspect_result_code(std::string_view value) {
    inspect_identifier(value);
}
inline std::string_view expected_result_code(std::string_view adapter_id) {
    if (adapter_id == "literal-v1") return "LITERAL_READY";
    if (adapter_id == "append-v1") return "APPENDED_READY";
    if (adapter_id == "join-v1") return "JOINED_READY";
    throw Error("PROTOCOL_ERROR");
}
inline bool inspect_multi_attempt_for_node(std::string_view attempt_id, std::size_t node_index) {
    // V5 assigns the canonical receipt order to the attempt-series ordinal;
    // the inspector also requires the matching ID to exist in the run log.
    const auto prefix = std::string("a-") + std::to_string(node_index + 1) + "-";
    return attempt_id.starts_with(prefix);
}

inline void inspect_request_digest(const std::filesystem::path& run_root,
                                  std::string_view node_id, std::string_view expected) {
    const auto path = run_root / "inputs" / (std::string(node_id) + ".bin");
    struct stat link_stat{};
    if (::lstat(path.c_str(), &link_stat) != 0 || S_ISLNK(link_stat.st_mode))
        throw Error("PROTOCOL_ERROR");
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) throw Error("PROTOCOL_ERROR");
    struct stat stat_value{};
    if (::fstat(fd, &stat_value) != 0 || !S_ISREG(stat_value.st_mode) || stat_value.st_nlink != 1 ||
        stat_value.st_uid != ::geteuid() || (stat_value.st_mode & 0077) != 0 || stat_value.st_size < 0 ||
        static_cast<std::uint64_t>(stat_value.st_size) > bruce::max_file_bytes) {
        ::close(fd); throw Error("PROTOCOL_ERROR");
    }
    std::string bytes(static_cast<std::size_t>(stat_value.st_size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ::close(fd); throw Error("PROTOCOL_ERROR"); }
        offset += static_cast<std::size_t>(count);
    }
    ::close(fd);
    if (sha256(bytes) != expected) throw Error("PROTOCOL_ERROR");
}

struct ValidatedV5ArtifactImportV1 {
    std::string node_id;
    std::string attempt_id;
    std::string public_reference;
    std::string source_reference;
    std::string digest;
    std::uint64_t size_bytes{};
    std::string status;
    std::string result_code;
};

struct ValidatedV5RunV1 {
    std::string run_id;
    std::string status;
    std::filesystem::path run_root;
    std::size_t expected_node_count{};
    std::vector<ValidatedV5ArtifactImportV1> artifacts;
};

inline ValidatedV5RunV1 inspect_execute_report(const std::string& bytes,
                                               const std::filesystem::path& run_root,
                                               const worker_v1::WorkerRunInspectionV1& inspection,
                                               const std::function<void(std::string_view,
                                                   std::string_view)>& check_request = {}) {
    if (bytes.empty() || bytes.back() != '\n' || (bytes.size() > 1 && bytes[bytes.size() - 2] == '\n'))
        throw Error("PROTOCOL_ERROR");
    const auto root = InspectJsonParser(std::string_view(bytes).substr(0, bytes.size() - 1)).parse();
    ValidatedV5RunV1 validated;
    validated.run_id = inspection.run_id;
    validated.status = inspection.status;
    validated.run_root = run_root;
    if (root.kind != InspectJsonValue::Kind::object || root.object.size() < 2 ||
        inspect_string(inspect_field(root, "contract")) != "dag-scheduler-v5-cli-v1" ||
        inspect_string(inspect_field(root, "action")) != "execute") throw Error("PROTOCOL_ERROR");
    const auto& status_value = inspect_field(root, "status");
    const auto report_status = inspect_string(status_value);
    inspect_status(report_status);
    if (inspection.run_finished && report_status != inspection.status) throw Error("PROTOCOL_ERROR");
    const auto& contract = inspect_field(root, "spec_digest"); inspect_digest(inspect_string(contract));
    inspect_identifier(inspect_string(inspect_field(root, "dag_id")));
    if (root.object.size() < 5) throw Error("PROTOCOL_ERROR");
    const bool single = root.object[4].first == "node_id";
    validated.expected_node_count = single ? 1 : 0;
    if (single) {
        inspect_exact_keys(root, {"contract", "action", "spec_digest", "dag_id", "node_id", "adapter_id",
            "request_digest", "retry", "workers", "budget", "status", "failure_code", "result_code",
            "reference", "payload_digest", "payload_size", "usage", "attempts"});
        const auto node_id = inspect_string(inspect_field(root, "node_id"));
        inspect_identifier(node_id);
        const auto adapter_id = inspect_string(inspect_field(root, "adapter_id"));
        if (adapter_id != "literal-v1" && adapter_id != "append-v1" && adapter_id != "join-v1")
            throw Error("PROTOCOL_ERROR");
        const auto request_digest = inspect_string(inspect_field(root, "request_digest"));
        inspect_digest(request_digest);
        if (check_request) check_request(node_id, request_digest);
        else inspect_request_digest(run_root, node_id, request_digest);
        if (inspect_number(inspect_field(root, "retry")) > 3 ||
            inspect_number(inspect_field(root, "workers")) < 1 || inspect_number(inspect_field(root, "workers")) > 4)
            throw Error("PROTOCOL_ERROR");
        const auto budget = inspect_budget_value(inspect_field(root, "budget"));
        const auto root_failure = inspect_string(inspect_field(root, "failure_code"));
        inspect_failure(root_failure);
        const auto root_result_code = inspect_string(inspect_field(root, "result_code"));
        inspect_result_code(root_result_code);
        const auto report_reference = inspect_optional_string(inspect_field(root, "reference"));
        if (report_reference) inspect_public_reference(*report_reference);
        const auto report_digest = inspect_optional_string(inspect_field(root, "payload_digest"));
        if (report_digest) inspect_digest(*report_digest);
        const auto report_size = inspect_number(inspect_field(root, "payload_size"));
        const auto report_usage = inspect_usage_value(inspect_field(root, "usage"));
        inspect_usage_bounds(report_usage, budget);
        if (!inspection.attempts.empty() && inspection.attempts.back().usage &&
            (report_usage.wall_ms != inspection.attempts.back().usage->wall_ms ||
             report_usage.input_bytes != inspection.attempts.back().usage->input_bytes ||
             report_usage.output_bytes != inspection.attempts.back().usage->output_bytes ||
             report_usage.operations != inspection.attempts.back().usage->operations ||
             report_usage.peak_memory_bytes != inspection.attempts.back().usage->peak_memory_bytes))
            throw Error("PROTOCOL_ERROR");
        const auto& attempts = inspect_field(root, "attempts");
        if (attempts.kind != InspectJsonValue::Kind::array || attempts.array.size() != inspection.attempts.size())
            throw Error("PROTOCOL_ERROR");
        std::optional<std::string> accepted_digest;
        std::optional<std::uint64_t> accepted_size;
        std::optional<std::string> accepted_reference;
        for (std::size_t i = 0; i < attempts.array.size(); ++i) {
            const auto& item = attempts.array[i];
            inspect_exact_keys(item, {"attempt_id", "status", "failure_code", "result_code", "accepted",
                "reference", "payload_digest", "payload_size", "usage"});
            const auto& actual = inspection.attempts[i];
            if (inspect_string(inspect_field(item, "attempt_id")) != actual.attempt_id ||
                inspect_boolean(inspect_field(item, "accepted")) != actual.accepted)
                throw Error("PROTOCOL_ERROR");
            if (inspect_string(inspect_field(item, "status")) != actual.statuses.back())
                throw Error("PROTOCOL_ERROR");
            const auto item_status = inspect_string(inspect_field(item, "status"));
            inspect_status(item_status);
            const auto item_failure = inspect_string(inspect_field(item, "failure_code"));
            inspect_failure(item_failure);
            const auto item_result_code = inspect_string(inspect_field(item, "result_code"));
            inspect_result_code(item_result_code);
            const auto item_reference = inspect_optional_string(inspect_field(item, "reference"));
            const auto item_digest = inspect_optional_string(inspect_field(item, "payload_digest"));
            const auto item_size = inspect_number(inspect_field(item, "payload_size"));
            const auto item_usage = inspect_usage_value(inspect_field(item, "usage"));
            inspect_usage_bounds(item_usage, budget);
            if (actual.usage && (item_usage.wall_ms != actual.usage->wall_ms ||
                                 item_usage.input_bytes != actual.usage->input_bytes ||
                                 item_usage.output_bytes != actual.usage->output_bytes ||
                                 item_usage.operations != actual.usage->operations ||
                                 item_usage.peak_memory_bytes != actual.usage->peak_memory_bytes))
                throw Error("PROTOCOL_ERROR");
            if (actual.accepted) {
                if (item_status != "READY" || item_failure != "READY" ||
                    item_result_code != expected_result_code(adapter_id)) throw Error("PROTOCOL_ERROR");
                if (!item_reference || !item_digest || !actual.digest || !actual.size_bytes ||
                    *item_digest != *actual.digest || item_size != *actual.size_bytes)
                    throw Error("PROTOCOL_ERROR");
                inspect_public_reference(*item_reference);
                if (*item_reference != "artifact/" + node_id) throw Error("PROTOCOL_ERROR");
                accepted_reference = *item_reference; accepted_digest = *item_digest; accepted_size = item_size;
                if (!actual.artifact_reference) throw Error("PROTOCOL_ERROR");
                validated.artifacts.push_back({node_id, actual.attempt_id, *item_reference,
                    *actual.artifact_reference, *item_digest, item_size, item_status,
                    item_result_code});
            } else if (item_status == "READY" || item_reference || item_digest || item_size != 0) {
                throw Error("PROTOCOL_ERROR");
            }
        }
        if (accepted_digest) {
            if (report_status != "READY" || root_failure != "READY" ||
                root_result_code != expected_result_code(adapter_id)) throw Error("PROTOCOL_ERROR");
            if (!report_reference || !report_digest || *report_digest != *accepted_digest ||
                report_size != *accepted_size || *report_reference != *accepted_reference)
                throw Error("PROTOCOL_ERROR");
        } else if (report_status == "READY" || report_reference || report_digest || report_size != 0) {
            throw Error("PROTOCOL_ERROR");
        }
    } else {
        inspect_exact_keys(root, {"contract", "action", "spec_digest", "dag_id", "retry", "workers", "budget",
            "status", "receipts"});
        if (inspect_number(inspect_field(root, "retry")) > 3 ||
            inspect_number(inspect_field(root, "workers")) < 1 || inspect_number(inspect_field(root, "workers")) > 4)
            throw Error("PROTOCOL_ERROR");
        const auto budget = inspect_budget_value(inspect_field(root, "budget"));
        const auto& receipts = inspect_field(root, "receipts");
        if (receipts.kind != InspectJsonValue::Kind::array || receipts.array.empty()) throw Error("PROTOCOL_ERROR");
        validated.expected_node_count = receipts.array.size();
        std::set<std::string> seen;
        std::set<std::string> node_ids;
        for (std::size_t receipt_index = 0; receipt_index < receipts.array.size(); ++receipt_index) {
            const auto& receipt = receipts.array[receipt_index];
            inspect_exact_keys(receipt, {"node_id", "callback_id", "request_digest", "attempt_id", "status",
                "failure_code", "result_code", "reference", "payload_digest", "payload_size", "usage", "attempts"});
            const auto node_id = inspect_string(inspect_field(receipt, "node_id"));
            inspect_identifier(node_id);
            if (!node_ids.insert(node_id).second) throw Error("PROTOCOL_ERROR");
            const auto callback_id = inspect_string(inspect_field(receipt, "callback_id"));
            if (callback_id != "literal-v1" && callback_id != "append-v1" && callback_id != "join-v1")
                throw Error("PROTOCOL_ERROR");
            const auto request_digest = inspect_string(inspect_field(receipt, "request_digest"));
            inspect_digest(request_digest);
            if (check_request) check_request(node_id, request_digest);
            else inspect_request_digest(run_root, node_id, request_digest);
            const auto attempt_id = inspect_optional_string(inspect_field(receipt, "attempt_id"));
            if (attempt_id && !inspect_multi_attempt_for_node(*attempt_id, receipt_index))
                throw Error("PROTOCOL_ERROR");
            const auto receipt_status = inspect_string(inspect_field(receipt, "status"));
            inspect_status(receipt_status);
            const auto receipt_failure = inspect_string(inspect_field(receipt, "failure_code"));
            inspect_failure(receipt_failure);
            const auto receipt_result_code = inspect_string(inspect_field(receipt, "result_code"));
            inspect_result_code(receipt_result_code);
            const auto reference = inspect_optional_string(inspect_field(receipt, "reference"));
            if (reference) {
                inspect_public_reference(*reference);
                if (*reference != "artifact/" + node_id) throw Error("PROTOCOL_ERROR");
            }
            const auto digest = inspect_optional_string(inspect_field(receipt, "payload_digest"));
            if (digest) inspect_digest(*digest);
            const auto receipt_size = inspect_number(inspect_field(receipt, "payload_size"));
            if (receipt_size > budget.max_output_bytes) throw Error("PROTOCOL_ERROR");
            if (receipt_status == "READY") {
                if (!attempt_id || !reference || !digest || receipt_failure != "READY" ||
                    receipt_result_code != expected_result_code(callback_id)) throw Error("PROTOCOL_ERROR");
            } else if (reference || digest || receipt_size != 0) {
                throw Error("PROTOCOL_ERROR");
            }
            const auto receipt_usage = inspect_usage_value(inspect_field(receipt, "usage"));
            inspect_usage_bounds(receipt_usage, budget);
            if (attempt_id) {
                const auto actual = std::find_if(inspection.attempts.begin(), inspection.attempts.end(),
                    [&](const auto& item) { return item.attempt_id == *attempt_id; });
                if (actual == inspection.attempts.end() || inspect_string(inspect_field(receipt, "status")) != actual->statuses.back())
                    throw Error("PROTOCOL_ERROR");
                if (actual->accepted && (!digest || !actual->digest || *digest != *actual->digest ||
                                         !actual->size_bytes || receipt_size != *actual->size_bytes))
                    throw Error("PROTOCOL_ERROR");
                if (actual->usage && (receipt_usage.wall_ms != actual->usage->wall_ms ||
                                      receipt_usage.input_bytes != actual->usage->input_bytes ||
                                      receipt_usage.output_bytes != actual->usage->output_bytes ||
                                      receipt_usage.operations != actual->usage->operations ||
                                      receipt_usage.peak_memory_bytes != actual->usage->peak_memory_bytes))
                    throw Error("PROTOCOL_ERROR");
            }
            const auto& history = inspect_field(receipt, "attempts");
            if (history.kind != InspectJsonValue::Kind::array) throw Error("PROTOCOL_ERROR");
            for (const auto& attempt : history.array) {
                inspect_exact_keys(attempt, {"attempt_id", "status", "failure_code", "result_code", "accepted",
                    "payload_digest", "payload_size", "usage"});
                const auto id = inspect_string(inspect_field(attempt, "attempt_id"));
                if (!inspect_multi_attempt_for_node(id, receipt_index)) throw Error("PROTOCOL_ERROR");
                if (!seen.insert(id).second) throw Error("PROTOCOL_ERROR");
                const auto actual = std::find_if(inspection.attempts.begin(), inspection.attempts.end(),
                    [&](const auto& item) { return item.attempt_id == id; });
                if (actual == inspection.attempts.end()) throw Error("PROTOCOL_ERROR");
                if (inspect_boolean(inspect_field(attempt, "accepted")) != actual->accepted)
                    throw Error("PROTOCOL_ERROR");
                inspect_status(inspect_string(inspect_field(attempt, "status")));
                const auto attempt_failure = inspect_string(inspect_field(attempt, "failure_code"));
                inspect_failure(attempt_failure);
                const auto attempt_result_code = inspect_string(inspect_field(attempt, "result_code"));
                inspect_result_code(attempt_result_code);
                const auto attempt_digest = inspect_optional_string(inspect_field(attempt, "payload_digest"));
                const auto attempt_size = inspect_number(inspect_field(attempt, "payload_size"));
                if (actual->accepted && (attempt_failure != "READY" ||
                                         attempt_result_code != expected_result_code(callback_id) ||
                                         !attempt_digest || !actual->digest || *attempt_digest != *actual->digest ||
                                          !actual->size_bytes || attempt_size != *actual->size_bytes))
                    throw Error("PROTOCOL_ERROR");
                if (inspect_string(inspect_field(attempt, "status")) != actual->statuses.back())
                    throw Error("PROTOCOL_ERROR");
                if (!actual->accepted && (attempt_digest || attempt_size != 0)) throw Error("PROTOCOL_ERROR");
                const auto attempt_usage = inspect_usage_value(inspect_field(attempt, "usage"));
                inspect_usage_bounds(attempt_usage, budget);
                if (actual->usage && (attempt_usage.wall_ms != actual->usage->wall_ms ||
                                      attempt_usage.input_bytes != actual->usage->input_bytes ||
                                      attempt_usage.output_bytes != actual->usage->output_bytes ||
                                      attempt_usage.operations != actual->usage->operations ||
                                      attempt_usage.peak_memory_bytes != actual->usage->peak_memory_bytes))
                    throw Error("PROTOCOL_ERROR");
            }
            if (attempt_id) {
                if (history.array.empty() || !seen.contains(*attempt_id) ||
                    inspect_string(inspect_field(history.array.back(), "attempt_id")) != *attempt_id)
                    throw Error("PROTOCOL_ERROR");
            } else if (!history.array.empty()) {
                throw Error("PROTOCOL_ERROR");
            }
            if (!attempt_id && std::any_of(inspection.attempts.begin(), inspection.attempts.end(),
                    [&](const auto& item) { return inspect_multi_attempt_for_node(item.attempt_id, receipt_index); }))
                throw Error("PROTOCOL_ERROR");
            if (receipt_status == "READY") {
                if (!attempt_id || !reference || !digest) throw Error("PROTOCOL_ERROR");
                const auto actual = std::find_if(inspection.attempts.begin(), inspection.attempts.end(),
                    [&](const auto& item) { return item.attempt_id == *attempt_id; });
                if (actual == inspection.attempts.end() || !actual->accepted ||
                    !actual->artifact_reference || !actual->size_bytes || !actual->digest)
                    throw Error("PROTOCOL_ERROR");
                validated.artifacts.push_back({node_id, *attempt_id, *reference,
                    *actual->artifact_reference, *digest, receipt_size, receipt_status,
                    receipt_result_code});
            }
        }
        if (seen.size() != inspection.attempts.size()) throw Error("PROTOCOL_ERROR");
    }
    return validated;
}

inline int inspect(const std::filesystem::path& bundle) {
    try {
        const auto normalized_bundle = normalized_inspect_bundle_path(bundle);
        const auto bundle_bytes = inspect_bundle_file(normalized_bundle);
        const auto inspection = worker_v1::inspect_worker_run_v1(
            std::filesystem::path(normalized_bundle.string() + ".run"));
        inspect_execute_report(bundle_bytes,
                               std::filesystem::path(normalized_bundle.string() + ".run"), inspection);
        std::cout << render_inspection_report(inspection);
        return inspection.run_finished ? 0 : 1;
    } catch (...) {
        std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}

inline std::string render_handoff_inspection_v1(const artifact_v1::ExecuteHandoffInspectionV1& value) {
    return std::string("{\"contract\":\"dag-scheduler-v5-handoff-inspect-v1\",\"action\":\"inspect\"") +
        ",\"status\":" + json_string(value.status) +
        ",\"run_status\":" + json_string(value.run_status) +
        ",\"run_id\":" + json_string(value.run_id) +
        ",\"node_count\":" + std::to_string(value.node_count) +
        ",\"attempt_count\":" + std::to_string(value.attempt_count) +
        ",\"binding_count\":" + std::to_string(value.binding_count) +
        ",\"verified_input_bytes\":" + std::to_string(value.verified_input_bytes) +
        ",\"lineage_scope\":\"LOCAL_CONSISTENCY\"}\n";
}

inline int inspect_handoff_v1(const std::filesystem::path& bundle) {
    try {
        const auto inspection = artifact_v1::inspect_execute_handoff_v1(bundle);
        std::cout << render_handoff_inspection_v1(inspection);
        return inspection.status == "VALID" ? 0 : 1;
    } catch (...) {
        std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
        return 2;
    }
}

inline std::string render_cancel_report(std::string_view status) {
    return std::string("{\"contract\":\"dag-scheduler-v5-cancel-v1\",\"action\":\"cancel\",\"status\":") +
        json_string(status) + "}\n";
}

inline int cancel(const std::filesystem::path& bundle) {
    try {
        const auto normalized_bundle = normalized_inspect_bundle_path(bundle);
        const auto run_root = std::filesystem::path(normalized_bundle.string() + ".run");
        std::error_code exists_error;
        const bool published = std::filesystem::exists(normalized_bundle, exists_error);
        if (exists_error) throw Error("CANCEL_TARGET_UNAVAILABLE");
        if (published) {
            const auto bundle_bytes = inspect_bundle_file(normalized_bundle);
            const auto inspection = worker_v1::inspect_worker_run_v1(run_root);
            if (!inspection.run_finished) throw Error("CANCEL_TARGET_UNAVAILABLE");
            (void)inspect_execute_report(bundle_bytes, run_root, inspection);
            std::cout << render_cancel_report("ALREADY_FINISHED");
            return 0;
        }
        const auto observed = worker_v1::probe_abort(run_root);
        if (observed == worker_v1::AbortProbeResult::INVALID)
            throw Error("CANCEL_TARGET_UNAVAILABLE");
        if (observed == worker_v1::AbortProbeResult::REQUESTED) {
            std::cout << render_cancel_report("ALREADY_REQUESTED");
            return 0;
        }
        const auto requested = worker_v1::request_abort(run_root);
        std::cout << render_cancel_report(
            requested == worker_v1::AbortRequestResult::REQUESTED ?
                "REQUESTED" : "ALREADY_REQUESTED");
        return 0;
    } catch (...) {
        std::cerr << "{\"error\":\"CANCEL_TARGET_UNAVAILABLE\"}\n";
        return 1;
    }
}

inline bool safe_reference(const std::filesystem::path& reference) {
    if (reference.empty() || reference.is_absolute() || reference.has_root_name()) return false;
    for (const auto& part : reference)
        if (part.empty() || part == "." || part == "..") return false;
    return true;
}

inline std::vector<std::string> inspect_v5_input_nodes(const std::filesystem::path& run_root) {
    const int input_fd = ::open((run_root / "inputs").c_str(), O_RDONLY | O_DIRECTORY |
                                O_NOFOLLOW | O_CLOEXEC);
    if (input_fd < 0) throw Error("PROTOCOL_ERROR");
    struct stat directory_stat{};
    if (::fstat(input_fd, &directory_stat) != 0 || !S_ISDIR(directory_stat.st_mode) ||
        directory_stat.st_uid != ::geteuid() || (directory_stat.st_mode & 0777) != 0700) {
        ::close(input_fd);
        throw Error("PROTOCOL_ERROR");
    }
    DIR* directory = ::fdopendir(input_fd);
    if (!directory) { ::close(input_fd); throw Error("PROTOCOL_ERROR"); }
    std::set<std::string> seen_nodes;
    std::vector<std::string> nodes;
    errno = 0;
    while (auto* entry = ::readdir(directory)) {
        const std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (name.size() <= 4 || !name.ends_with(".bin")) {
            ::closedir(directory); throw Error("PROTOCOL_ERROR");
        }
        const std::string node = name.substr(0, name.size() - 4);
        inspect_identifier(node);
        const int fd = ::openat(::dirfd(directory), name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) { ::closedir(directory); throw Error("PROTOCOL_ERROR"); }
        struct stat file_stat{};
        const bool valid = ::fstat(fd, &file_stat) == 0 && S_ISREG(file_stat.st_mode) &&
            file_stat.st_nlink == 1 && file_stat.st_uid == ::geteuid() &&
            (file_stat.st_mode & 0077) == 0 && file_stat.st_size >= 0 &&
            static_cast<std::uint64_t>(file_stat.st_size) <= bruce::max_file_bytes;
        ::close(fd);
        if (!valid || !seen_nodes.insert(node).second) {
            ::closedir(directory); throw Error("PROTOCOL_ERROR");
        }
        nodes.push_back(node);
    }
    if (errno != 0) { ::closedir(directory); throw Error("PROTOCOL_ERROR"); }
    ::closedir(directory);
    std::sort(nodes.begin(), nodes.end());
    return nodes;
}

inline bool path_is_same_or_below(const std::filesystem::path& candidate,
                                  const std::filesystem::path& ancestor) {
    std::error_code candidate_error;
    std::error_code ancestor_error;
    auto candidate_path = std::filesystem::weakly_canonical(candidate, candidate_error);
    auto ancestor_path = std::filesystem::weakly_canonical(ancestor, ancestor_error);
    if (candidate_error) candidate_path = std::filesystem::absolute(candidate).lexically_normal();
    if (ancestor_error) ancestor_path = std::filesystem::absolute(ancestor).lexically_normal();
    auto candidate_it = candidate_path.begin();
    auto ancestor_it = ancestor_path.begin();
    for (; ancestor_it != ancestor_path.end(); ++ancestor_it, ++candidate_it) {
        if (candidate_it == candidate_path.end() || *candidate_it != *ancestor_it) return false;
    }
    return true;
}

inline ValidatedV5RunV1 validate_v5_publish_bundle_v1(const std::filesystem::path& bundle) {
    const auto normalized_bundle = normalized_inspect_bundle_path(bundle);
    const auto run_root = std::filesystem::path(normalized_bundle.string() + ".run");
    const auto bundle_bytes = inspect_bundle_file(normalized_bundle);
    const auto inspection = worker_v1::inspect_worker_run_v1(run_root);
    auto validated = inspect_execute_report(bundle_bytes, run_root, inspection);
    if (!inspection.run_finished || inspection.status != "READY" || validated.status != "READY" ||
        validated.artifacts.empty() || validated.artifacts.size() != validated.expected_node_count)
        throw Error("PROTOCOL_ERROR");
    std::set<std::string> nodes;
    for (const auto& artifact : validated.artifacts) {
        if (!nodes.insert(artifact.node_id).second || artifact.status != "READY" ||
            artifact.public_reference != "artifact/" + artifact.node_id ||
            !safe_reference(std::filesystem::path(artifact.source_reference)) ||
            artifact.source_reference.empty() || artifact.digest.size() != 64)
            throw Error("PROTOCOL_ERROR");
    }
    const auto input_nodes = inspect_v5_input_nodes(run_root);
    if (input_nodes.size() != validated.artifacts.size()) throw Error("PROTOCOL_ERROR");
    for (std::size_t index = 0; index < input_nodes.size(); ++index)
        if (input_nodes[index] != validated.artifacts[index].node_id)
            throw Error("PROTOCOL_ERROR");
    return validated;
}

struct PublishedV5ArtifactV1 {
    std::string node_id;
    std::string attempt_id;
    std::string public_reference;
    std::string manifest_id;
    artifact_v1::ArtifactIdV1 object;
};

struct V5ArtifactPublishResultV1 {
    std::string run_id;
    std::vector<PublishedV5ArtifactV1> artifacts;
};

using artifact_v1::v5_manifest_id_v1;

inline V5ArtifactPublishResultV1 publish_v5_artifacts_v1(
        const std::filesystem::path& bundle, const std::filesystem::path& store_root) {
    // The complete run/report binding is established before opening the store
    // or performing the first immutable object write.
    const auto validated = validate_v5_publish_bundle_v1(bundle);
    const auto normalized_bundle = normalized_inspect_bundle_path(bundle);
    const auto run_root = std::filesystem::path(normalized_bundle.string() + ".run");
    if (path_is_same_or_below(store_root, normalized_bundle) ||
        path_is_same_or_below(store_root, run_root))
        throw Error("PROTOCOL_ERROR");
    auto store = artifact_v1::ArtifactStoreV1::open(store_root);
    V5ArtifactPublishResultV1 result;
    result.run_id = validated.run_id;
    result.artifacts.reserve(validated.artifacts.size());
    std::vector<artifact_v1::ArtifactIdV1> objects;
    objects.reserve(validated.artifacts.size());
    for (const auto& artifact : validated.artifacts) {
        const auto source = validated.run_root / std::filesystem::path(artifact.source_reference);
        objects.push_back(store.put_file(source, artifact.digest, artifact.size_bytes));
    }
    for (std::size_t index = 0; index < validated.artifacts.size(); ++index) {
        const auto& artifact = validated.artifacts[index];
        const auto manifest_id = v5_manifest_id_v1(validated.run_id, artifact.node_id,
                                                    artifact.attempt_id);
        artifact_v1::ArtifactManifestV1 manifest;
        manifest.run_id = validated.run_id;
        manifest.node_id = artifact.node_id;
        manifest.attempt_id = artifact.attempt_id;
        manifest.status = artifact.status;
        manifest.result_code = artifact.result_code;
        manifest.objects.push_back({objects[index], {}, artifact.public_reference});
        store.publish_manifest(manifest_id, manifest);
        result.artifacts.push_back({artifact.node_id, artifact.attempt_id,
                                    artifact.public_reference, manifest_id, objects[index]});
    }
    return result;
}

inline std::string render_publish_report(const V5ArtifactPublishResultV1& result) {
    std::string report = "{\"contract\":\"dag-scheduler-v5-publish-v1\",\"action\":\"publish\",\"status\":\"PUBLISHED\",\"run_id\":" +
        json_string(result.run_id) + ",\"manifests\":[";
    for (std::size_t index = 0; index < result.artifacts.size(); ++index) {
        if (index) report += ',';
        const auto& artifact = result.artifacts[index];
        report += "{\"node_id\":" + json_string(artifact.node_id) +
            ",\"attempt_id\":" + json_string(artifact.attempt_id) +
            ",\"reference\":" + json_string(artifact.public_reference) +
            ",\"manifest_id\":" + json_string(artifact.manifest_id) +
            ",\"sha256\":" + json_string(artifact.object.sha256) +
            ",\"size_bytes\":" + std::to_string(artifact.object.size_bytes) + '}';
    }
    report += "]}\n";
    return report;
}

inline int publish(const std::filesystem::path& bundle, const std::filesystem::path& store) {
    const auto result = publish_v5_artifacts_v1(bundle, store);
    std::cout << render_publish_report(result);
    return 0;
}

inline std::string payload_input_digest(EVP_MD_CTX* context) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> value{};
    unsigned size{};
    if (EVP_DigestFinal_ex(context, value.data(), &size) != 1 || size != 32)
        throw Error("WorkerDigestMismatch");
    std::string actual;
    for (unsigned index = 0; index < size; ++index) {
        actual += "0123456789abcdef"[value[index] >> 4];
        actual += "0123456789abcdef"[value[index] & 15];
    }
    return actual;
}

template<class Consumer>
inline void consume_approved_stream(const worker_v1::ArtifactRef& ref,
                                    const std::filesystem::path& path, Consumer consume,
                                    std::chrono::steady_clock::time_point deadline =
                                        std::chrono::steady_clock::time_point::max()) {
    using namespace artifact_v1;
    const auto check_deadline = [&] {
        if (std::chrono::steady_clock::now() >= deadline)
            throw ArtifactPayloadStreamErrorV15(ArtifactPayloadStreamErrorCodeV15::deadline_exceeded);
    };
    check_deadline();
    const auto name = path.filename().string();
    const auto stream_root = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!name.starts_with("stream-")) throw Error("InvalidWorkerInput");
    const ArtifactPayloadStreamV15 stream{name.substr(7), name, ref.sha256,
        ref.size_bytes, artifact_payload_stream_chunk_bytes_v15,
        ref.size_bytes / artifact_payload_stream_chunk_bytes_v15 +
            (ref.size_bytes % artifact_payload_stream_chunk_bytes_v15 != 0)};
    check_deadline();
    const int directory = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory < 0) throw Error("InvalidWorkerInput");
    try {
        verify_payload_stream_v15(stream, stream_root, deadline);
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (!digest || EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1)
            throw Error("WorkerDigestMismatch");
        std::uint64_t total{};
        std::array<char, 8192> buffer{};
        for (std::uint64_t index = 0; index < stream.chunk_count; ++index) {
            check_deadline();
            const auto number = std::to_string(index);
            const auto chunk = "chunk-" + std::string(20 - number.size(), '0') + number + ".bin";
            const int fd = ::openat(directory, chunk.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
            if (fd < 0) throw Error("InvalidWorkerInput");
            try {
                struct stat st{};
                const auto size = std::min(stream.chunk_bytes, ref.size_bytes - total);
                if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 ||
                    st.st_uid != ::geteuid() || st.st_nlink != 1 || (st.st_mode & 0077) ||
                    static_cast<std::uint64_t>(st.st_size) != size)
                    throw Error("InvalidWorkerInput");
                std::uint64_t offset{};
                while (offset < size) {
                    check_deadline();
                    const auto n = ::read(fd, buffer.data(), std::min<std::uint64_t>(buffer.size(), size - offset));
                    if (n < 0 && errno == EINTR) continue;
                    if (n <= 0 || EVP_DigestUpdate(digest.get(), buffer.data(), static_cast<std::size_t>(n)) != 1)
                        throw Error("WorkerDigestMismatch");
                    check_deadline();
                    consume(std::string_view(buffer.data(), static_cast<std::size_t>(n)));
                    offset += static_cast<std::size_t>(n);
                    total += static_cast<std::size_t>(n);
                }
                char extra;
                ssize_t n;
                do {
                    check_deadline();
                    n = ::read(fd, &extra, 1);
                } while (n < 0 && errno == EINTR);
                if (n != 0) throw Error("WorkerDigestMismatch");
            } catch (...) {
                ::close(fd);
                throw;
            }
            ::close(fd);
        }
        if (total != ref.size_bytes || payload_input_digest(digest.get()) != ref.sha256)
            throw Error("WorkerDigestMismatch");
        verify_payload_stream_v15(stream, stream_root, deadline);
    } catch (...) {
        ::close(directory);
        throw;
    }
    ::close(directory);
}

inline std::string read_approved_stream(const worker_v1::ArtifactRef& ref,
                                        const std::filesystem::path& path) {
    std::string bytes;
    try {
        bytes.reserve(ref.size_bytes);
        consume_approved_stream(ref, path, [&](std::string_view chunk) { bytes.append(chunk); });
    } catch (...) { throw Error("WorkerDigestMismatch"); }
    return bytes;
}

inline std::string read_approved(const worker_v1::ArtifactRef& ref, std::uint64_t cap) {
    const std::filesystem::path path(ref.reference);
    if (!safe_reference(path)) throw Error("InvalidWorkerInput");
    if (ref.size_bytes > cap) throw Error("WorkerInputLimitExceeded");
    auto prefix = std::filesystem::path{};
    for (const auto& part : path) {
        prefix /= part;
        struct stat st{};
        if (::lstat(prefix.c_str(), &st) != 0 || S_ISLNK(st.st_mode)) throw Error("InvalidWorkerInput");
    }
    struct stat type{};
    if (::lstat(path.c_str(), &type) == 0 && S_ISDIR(type.st_mode))
        return read_approved_stream(ref, path);
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) throw Error("InvalidWorkerInput");
    struct stat st{};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        ::close(fd); throw Error("InvalidWorkerInput");
    }
    if (static_cast<std::uint64_t>(st.st_size) > cap) {
        ::close(fd); throw Error("WorkerInputLimitExceeded");
    }
    if (static_cast<std::uint64_t>(st.st_size) != ref.size_bytes) {
        ::close(fd); throw Error("WorkerDigestMismatch");
    }
    std::string bytes;
    bytes.reserve(ref.size_bytes);
    char chunk[8192];
    while (bytes.size() <= cap) {
        const auto n = ::read(fd, chunk, sizeof(chunk));
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; ::close(fd); throw Error("InvalidWorkerInput"); }
        bytes.append(chunk, static_cast<std::size_t>(n));
    }
    ::close(fd);
    if (bytes.size() > cap) throw Error("WorkerInputLimitExceeded");
    if (bytes.size() != ref.size_bytes || sha256(bytes) != ref.sha256)
        throw Error("WorkerDigestMismatch");
    return bytes;
}

inline void write_all(int fd, std::string_view bytes) {
    while (!bytes.empty()) {
        const auto n = ::write(fd, bytes.data(), bytes.size());
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw Error("WorkerFrameWriteFailed");
        bytes.remove_prefix(static_cast<std::size_t>(n));
    }
}

inline std::string read_request_frame() {
    std::string frame;
    frame.reserve(1024);
    char c{};
    while (!frame.ends_with("\n\n")) {
        const auto n = ::read(STDIN_FILENO, &c, 1);
        if (n != 1 || frame.size() >= 256) throw Error("InvalidWorkerFrameHeader");
        frame.push_back(c);
    }
    const auto key = frame.find("\npayload-bytes=");
    if (key == std::string::npos) throw Error("InvalidWorkerFrameHeader");
    const auto begin = key + std::string_view("\npayload-bytes=").size();
    const auto end = frame.find('\n', begin);
    const auto size = decimal(std::string_view(frame).substr(begin, end - begin), 1048576, true);
    const auto header_size = frame.size();
    frame.resize(header_size + size);
    std::size_t read_count{};
    while (read_count < size) {
        const auto n = ::read(STDIN_FILENO, frame.data() + header_size + read_count, size - read_count);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw Error("InvalidWorkerFrameLength");
        read_count += static_cast<std::size_t>(n);
    }
    return frame;
}

inline std::optional<worker_v1::Frame> try_read_control_frame(std::string& buffered) {
    char chunk[4096];
    for (;;) {
        const auto n = ::read(STDIN_FILENO, chunk, sizeof(chunk));
        if (n > 0) {
            if (buffered.size() + static_cast<std::size_t>(n) > 65536)
                throw Error("WorkerControlFrameLimitExceeded");
            buffered.append(chunk, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        throw Error("WorkerControlFrameReadFailed");
    }
    const auto separator = buffered.find("\n\n");
    if (separator == std::string::npos) {
        if (buffered.size() > 256) throw Error("InvalidWorkerFrameHeader");
        return std::nullopt;
    }
    if (separator > 256) throw Error("InvalidWorkerFrameHeader");
    const auto marker = buffered.find("\npayload-bytes=");
    if (marker == std::string::npos || marker >= separator)
        throw Error("InvalidWorkerFrameHeader");
    const auto begin = marker + std::string_view("\npayload-bytes=").size();
    const auto end = buffered.find('\n', begin);
    if (end == std::string::npos || end > separator || begin == end)
        throw Error("InvalidWorkerFrameSize");
    const auto size = decimal(std::string_view(buffered).substr(begin, end - begin), 65536, true);
    const auto total = separator + 2 + size;
    if (buffered.size() < total) return std::nullopt;
    const auto frame = worker_v1::decode_frame(std::string_view(buffered).substr(0, total), 65536);
    buffered.erase(0, total);
    return frame;
}

inline bool control_input_ready() {
    pollfd descriptor{STDIN_FILENO, POLLIN | POLLHUP, 0};
    return ::poll(&descriptor, 1, 0) > 0 &&
        (descriptor.revents & (POLLIN | POLLHUP)) != 0;
}

inline worker_v1::WorkerResult execute_child_request(const worker_v1::WorkerRequest& request) {
    using namespace worker_v1;
    if (request.adapter_version != "1" || request.input_artifact_refs.empty() ||
        request.input_artifact_refs.size() > 16 ||
        request.input_artifact_refs[0].sha256 != request.request_digest)
        throw Error("InvalidWorkerRequest");
    const auto entry = std::find_if(dag_scheduler_v3_cli_detail::registry.begin(),
        dag_scheduler_v3_cli_detail::registry.end(), [&](const auto& candidate) {
            return candidate.callback_id == request.adapter_id;
        });
    if (entry == dag_scheduler_v3_cli_detail::registry.end()) throw Error("InvalidWorkerAdapter");
    std::uint64_t remaining = request.budget.max_input_bytes;
    const auto take = [&](const ArtifactRef& ref) {
        if (ref.size_bytes > remaining) throw Error("WorkerInputLimitExceeded");
        auto bytes = read_approved(ref, remaining);
        remaining -= bytes.size();
        return bytes;
    };
    const auto request_bytes = take(request.input_artifact_refs[0]);
    std::vector<DagSchedulerInputV2> inputs;
    for (std::size_t i = 1; i < request.input_artifact_refs.size(); ++i) {
        const auto& ref = request.input_artifact_refs[i];
        inputs.push_back({"input-" + std::to_string(i), "READY", take(ref), ref.sha256});
    }
    if (1 + inputs.size() > request.budget.max_operations)
        throw Error("WorkerOperationLimitExceeded");
    const auto callback = entry->execute(request_bytes, inputs);
    if (callback.status != DagSchedulerCallbackStatusV2::ready) throw Error("WorkerAdapterFailed");
    if (callback.payload_digest != sha256(callback.payload_bytes)) throw Error("WorkerDigestMismatch");
    if (callback.payload_bytes.size() > request.budget.max_output_bytes ||
        callback.payload_bytes.size() > dag_scheduler_v3_cli_detail::byte_limit) throw Error("WorkerOutputLimitExceeded");
    const auto path = std::filesystem::path("attempts") / request.attempt_id / "output/result.bin";
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) throw Error("WorkerOutputUnavailable");
    try { write_all(fd, callback.payload_bytes); if (::fsync(fd) != 0) throw Error("WorkerOutputUnavailable"); }
    catch (...) { ::close(fd); throw; }
    ::close(fd);
    WorkerResult result;
    result.status = WorkerStatus::READY;
    result.result_code = callback.result_code;
    result.output_artifact_ref = ArtifactRef{"result", callback.payload_bytes.size(), callback.payload_digest};
    result.payload_digest = callback.payload_digest;
    result.payload_size = callback.payload_bytes.size();
    result.failure_code = FailureCode::READY;
    result.usage.input_bytes = request.budget.max_input_bytes - remaining;
    result.usage.output_bytes = callback.payload_bytes.size();
    result.usage.operations = 1 + inputs.size();
    return result;
}

inline std::string failure_name(worker_v1::FailureCode code);

inline worker_v1::WorkerResult failed_child_result(worker_v1::FailureCode code) {
    worker_v1::WorkerResult result;
    result.status = code == worker_v1::FailureCode::CANCELLED ?
        worker_v1::WorkerStatus::CANCELLED : worker_v1::WorkerStatus::FAILED;
    result.failure_code = code;
    result.result_code = failure_name(code);
    return result;
}

inline worker_v1::WorkerResult finalize_child_result(worker_v1::WorkerResult result,
                                                     bool cancelled) {
    return cancelled ? failed_child_result(worker_v1::FailureCode::CANCELLED) :
        std::move(result);
}

inline worker_v1::FailureCode child_error_code(std::string_view what) {
    using worker_v1::FailureCode;
    if (what == "WorkerOutputLimitExceeded" || what == "DagSchedulerV3PayloadLimitExceeded")
        return FailureCode::OUTPUT_LIMIT_EXCEEDED;
    if (what == "WorkerInputLimitExceeded") return FailureCode::INPUT_LIMIT_EXCEEDED;
    if (what == "WorkerOperationLimitExceeded") return FailureCode::OPERATION_BUDGET_EXCEEDED;
    if (what == "WorkerDigestMismatch") return FailureCode::DIGEST_MISMATCH;
    if (what == "InvalidWorkerRequest" || what == "InvalidWorkerAdapter" ||
        what == "InvalidWorkerInput" || what == "InvalidDagSchedulerV3SpecCallbackArity")
        return FailureCode::INVALID_REQUEST;
    return FailureCode::FAILED;
}

inline int child() {
    using namespace worker_v1;
    WorkerResult result;
    try {
        const auto frame = decode_frame(read_request_frame(), 1048576);
        if (frame.type != FrameType::REQUEST) throw Error("InvalidWorkerFrameType");
        try {
            const auto request = decode_request_payload(frame.payload);
            (void)::fcntl(STDIN_FILENO, F_SETFL, ::fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
            std::string controls;
            bool cancelled = false;
            const auto service_controls = [&] {
                while (controls.find("\n\n") != std::string::npos || control_input_ready()) {
                    const auto control = try_read_control_frame(controls);
                    if (!control) break;
                    if (control->type == FrameType::STATUS_QUERY) {
                        write_all(STDOUT_FILENO, encode_frame(FrameType::STATUS, "status=RUNNING"));
                    } else if (control->type == FrameType::CANCEL) {
                        cancelled = true;
                        write_all(STDOUT_FILENO, encode_frame(FrameType::ACK, "cancelled"));
                    } else {
                        throw Error("UnexpectedWorkerControlFrame");
                    }
                }
            };
            service_controls();
            try {
                result = cancelled ? failed_child_result(FailureCode::CANCELLED) :
                    execute_child_request(request);
                service_controls();
                result = finalize_child_result(std::move(result), cancelled);
            }
            catch (const std::bad_alloc&) { result = failed_child_result(FailureCode::MEMORY_LIMIT_EXCEEDED); }
            catch (const Error& error) { result = failed_child_result(child_error_code(error.what())); }
            catch (const std::exception&) { result = failed_child_result(FailureCode::FAILED); }
        } catch (const Error&) { result = failed_child_result(FailureCode::INVALID_REQUEST); }
    } catch (const Error&) { result = failed_child_result(FailureCode::PROTOCOL_ERROR); }
    catch (const std::exception&) { result = failed_child_result(FailureCode::PROTOCOL_ERROR); }
    try {
        write_all(STDOUT_FILENO, encode_frame(FrameType::RESULT, encode_result_payload(result)));
        return 0;
    } catch (...) {
        return 1;
    }
}

inline std::uint64_t unix_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
inline std::string status_name(worker_v1::WorkerStatus status) {
    switch (status) {
        case worker_v1::WorkerStatus::READY: return "READY";
        case worker_v1::WorkerStatus::FAILED: return "FAILED";
        case worker_v1::WorkerStatus::BLOCKED: return "BLOCKED";
        case worker_v1::WorkerStatus::CANCELLED: return "CANCELLED";
    }
    throw Error("InvalidWorkerStatus");
}
inline std::string failure_name(worker_v1::FailureCode code) {
    switch (code) {
        case worker_v1::FailureCode::NONE: return "NONE";
        case worker_v1::FailureCode::READY: return "READY";
        case worker_v1::FailureCode::FAILED: return "FAILED";
        case worker_v1::FailureCode::BLOCKED: return "BLOCKED";
        case worker_v1::FailureCode::INVALID_REQUEST: return "INVALID_REQUEST";
        case worker_v1::FailureCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
        case worker_v1::FailureCode::PROCESS_START_FAILED: return "PROCESS_START_FAILED";
        case worker_v1::FailureCode::PROCESS_CRASHED: return "PROCESS_CRASHED";
        case worker_v1::FailureCode::TIMEOUT: return "TIMEOUT";
        case worker_v1::FailureCode::CANCELLED: return "CANCELLED";
        case worker_v1::FailureCode::INPUT_LIMIT_EXCEEDED: return "INPUT_LIMIT_EXCEEDED";
        case worker_v1::FailureCode::OUTPUT_LIMIT_EXCEEDED: return "OUTPUT_LIMIT_EXCEEDED";
        case worker_v1::FailureCode::OPERATION_BUDGET_EXCEEDED: return "OPERATION_BUDGET_EXCEEDED";
        case worker_v1::FailureCode::MEMORY_LIMIT_EXCEEDED: return "MEMORY_LIMIT_EXCEEDED";
        case worker_v1::FailureCode::DIGEST_MISMATCH: return "DIGEST_MISMATCH";
    }
    throw Error("InvalidWorkerFailureCode");
}

struct V5AttemptHistory {
    std::string attempt_id;
    worker_v1::WorkerResult result;
    bool accepted{};
};

inline std::string render_report(const dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3& parsed,
        const Options& options, const worker_v1::WorkerResult& result,
        std::string_view reference, const std::vector<V5AttemptHistory>& history,
        bool payload_route = false, std::string_view external_inputs_digest = {},
        std::uint64_t external_input_count = 0, std::uint64_t external_input_bytes = 0) {
    const auto& node = parsed.spec.nodes.front();
    const auto render_usage = [](const worker_v1::WorkerUsage& usage) {
        return std::string("{\"wall_ms\":") + std::to_string(usage.wall_ms) +
            ",\"input_bytes\":" + std::to_string(usage.input_bytes) +
            ",\"output_bytes\":" + std::to_string(usage.output_bytes) +
            ",\"operations\":" + std::to_string(usage.operations) +
            ",\"peak_memory_bytes\":" + std::to_string(usage.peak_memory_bytes) + '}';
    };
    std::string report = std::string("{\"contract\":\"") +
        (payload_route ? (external_inputs_digest.empty() ?
            "dag-scheduler-v5-payload-v13" : "dag-scheduler-v5-payload-v14") :
            "dag-scheduler-v5-cli-v1") +
        "\",\"action\":\"" + (payload_route ? "payload" : "execute") +
        "\",\"spec_digest\":" +
        json_string(sha256(parsed.raw_bytes)) + ",\"dag_id\":" + json_string(parsed.spec.dag_id) +
        ",\"node_id\":" + json_string(node.node_id) +
        ",\"adapter_id\":" + json_string(node.callback_id) +
        ",\"request_digest\":" + json_string(node.request_digest) +
        ",\"retry\":" + std::to_string(options.retry) +
        ",\"workers\":" + std::to_string(options.workers) +
        ",\"budget\":{\"max_wall_ms\":" + std::to_string(options.budget.max_wall_ms) +
        ",\"max_input_bytes\":" + std::to_string(options.budget.max_input_bytes) +
        ",\"max_output_bytes\":" + std::to_string(options.budget.max_output_bytes) +
        ",\"max_operations\":" + std::to_string(options.budget.max_operations) +
        ",\"max_memory_bytes\":" + std::to_string(options.budget.max_memory_bytes) + "}" +
        (external_inputs_digest.empty() ? std::string{} :
            ",\"external_inputs_digest\":" + json_string(external_inputs_digest)) +
        ",\"status\":" + json_string(status_name(result.status)) +
        ",\"failure_code\":" + json_string(failure_name(result.failure_code)) +
        ",\"result_code\":" + json_string(result.result_code) +
        ",\"reference\":" + (reference.empty() ? "null" : json_string(reference)) +
        ",\"payload_digest\":" + (result.payload_digest ? json_string(*result.payload_digest) : "null") +
        ",\"payload_size\":" + std::to_string(result.payload_size) +
        ",\"usage\":" + render_usage(result.usage) +
        (external_inputs_digest.empty() ? std::string{} :
            ",\"external_input_count\":" + std::to_string(external_input_count) +
            ",\"external_input_bytes\":" + std::to_string(external_input_bytes)) +
        ",\"attempts\":[";
    for (std::size_t i = 0; i < history.size(); ++i) {
        if (i) report += ',';
        const auto& attempt = history[i];
        report += "{\"attempt_id\":" + json_string(attempt.attempt_id) +
            ",\"status\":" + json_string(status_name(attempt.result.status)) +
            ",\"failure_code\":" + json_string(failure_name(attempt.result.failure_code)) +
            ",\"result_code\":" + json_string(attempt.result.result_code) +
            ",\"accepted\":" + (attempt.accepted ? "true" : "false") +
            ",\"reference\":" + (attempt.accepted ? json_string(reference) : "null") +
            ",\"payload_digest\":" +
            (attempt.result.payload_digest ? json_string(*attempt.result.payload_digest) : "null") +
            ",\"payload_size\":" + std::to_string(attempt.result.payload_size) +
            ",\"usage\":" + render_usage(attempt.result.usage) + '}';
    }
    report += "]}";
    return report;
}

inline bool retryable(worker_v1::FailureCode code) {
    using worker_v1::FailureCode;
    return code == FailureCode::PROCESS_CRASHED || code == FailureCode::TIMEOUT ||
           code == FailureCode::PROTOCOL_ERROR;
}

struct V5NodeState {
    const DagSchedulerNodeSpecV2* spec{};
    std::size_t ordinal{};
    std::uint64_t attempts{};
    std::string attempt_id;
    std::string accepted_private_reference;
    std::string accepted_public_reference;
    std::string accepted_manifest_id;
    std::vector<worker_v1::ArtifactRef> payload_inputs;
    std::uint64_t external_input_count{};
    std::uint64_t external_input_bytes{};
    worker_v1::WorkerResult result;
    std::optional<worker_v1::WorkerAttemptHandle> handle;
    std::optional<worker_v1::WorkerResult> pending_result;
    std::vector<V5AttemptHistory> history;
    std::chrono::steady_clock::time_point attempt_deadline{};
    bool deadline_cancelled{};
    bool cancellation_logged{};
    bool accepted{};
    bool terminal{};
    bool started_once{};
};

inline std::string attempt_id_for(const V5NodeState& state, std::size_t node_count) {
    if (node_count == 1 && state.attempts == 0) return "a-1";
    return "a-" + std::to_string(state.ordinal + 1) + "-" +
        std::to_string(state.attempts + 1);
}

inline worker_v1::WorkerRequest make_request(
        const std::string& run_id, V5NodeState& state,
        const std::map<std::string, V5NodeState>& states,
        const std::filesystem::path& root, const Options& options,
        bool payload_route = false) {
    using namespace worker_v1;
    WorkerRequest request;
    request.run_id = run_id;
    request.attempt_id = state.attempt_id;
    request.node_id = state.spec->node_id;
    request.adapter_id = state.spec->callback_id;
    request.adapter_version = "1";
    request.request_digest = state.spec->request_digest;
    const auto request_reference = "inputs/" + state.spec->node_id + ".bin";
    request.input_artifact_refs.push_back({request_reference, state.spec->request_bytes.size(),
                                           state.spec->request_digest});
    if (payload_route) {
        request.input_artifact_refs.insert(request.input_artifact_refs.end(),
            state.payload_inputs.begin(), state.payload_inputs.end());
    } else {
        for (const auto& dependency : state.spec->dependencies) {
            const auto& producer = states.at(dependency);
            if (!producer.accepted || producer.accepted_private_reference.empty())
                throw Error("DagSchedulerV5DependencyNotAccepted");
            request.input_artifact_refs.push_back({producer.accepted_private_reference,
                producer.result.payload_size, *producer.result.payload_digest});
        }
    }
    request.budget = options.budget;
    (void)root;
    return request;
}

inline std::string payload_failure_name(std::string_view error) {
    if (error == "ArtifactPayloadReferenceInvalid" ||
        error == "DagSchedulerV5DependencyNotAccepted")
        return "ARTIFACT_REFERENCE_INVALID";
    if (error == "ArtifactPayloadManifestInvalid" ||
        error == "ArtifactPayloadManifestNotReady" ||
        error == "ArtifactManifestMissing" || error == "ArtifactManifestCorrupt")
        return "ARTIFACT_MANIFEST_INVALID";
    if (error == "ArtifactPayloadDigestMismatch" ||
        error == "ArtifactObjectCorrupt")
        return "ARTIFACT_DIGEST_MISMATCH";
    if (error == "ArtifactPayloadStoreViolation" ||
        error == "ArtifactStorePermissions" || error == "ArtifactStoreSymlink" ||
        error == "ArtifactObjectMissing")
        return "ARTIFACT_STORE_VIOLATION";
    if (error == "ArtifactPayloadInputLimitExceeded" ||
        error == "ArtifactCopyLimitExceeded")
        return "INPUT_LIMIT_EXCEEDED";
    if (error == "ArtifactPayloadTimeout" || error == "ArtifactCopyDeadlineExceeded")
        return "TIMEOUT";
    return "ARTIFACT_REFERENCE_INVALID";
}

inline std::string external_failure_name(artifact_v1::ExternalInputsErrorCodeV14 code) {
    using Code = artifact_v1::ExternalInputsErrorCodeV14;
    switch (code) {
    case Code::file_permissions:
    case Code::file_changed:
        return "EXTERNAL_INPUTS_INVALID";
    case Code::invalid_contract:
    case Code::invalid_json:
    case Code::invalid_field:
    case Code::duplicate_field:
    case Code::invalid_identifier:
    case Code::invalid_digest:
    case Code::invalid_number:
    case Code::binding_invalid:
    case Code::duplicate_binding:
    case Code::duplicate_object:
    case Code::input_limit_exceeded:
    case Code::file_missing:
    case Code::file_too_large:
    case Code::file_not_regular:
    case Code::file_symlink:
        return "EXTERNAL_INPUTS_INVALID";
    }
    return "EXTERNAL_INPUTS_INVALID";
}

inline std::string external_selection_failure_name(artifact_v1::ArtifactPayloadErrorCodeV13 code) {
    using Code = artifact_v1::ArtifactPayloadErrorCodeV13;
    switch (code) {
    case Code::manifest_invalid:
    case Code::manifest_not_ready:
    case Code::provenance_mismatch:
        return "EXTERNAL_MANIFEST_INVALID";
    case Code::object_index_out_of_range:
    case Code::reference_mismatch:
    case Code::object_verification_failed:
        return "EXTERNAL_OBJECT_INVALID";
    case Code::object_limit_exceeded:
        return "EXTERNAL_INPUT_LIMIT_EXCEEDED";
    case Code::invalid_reference:
        return "EXTERNAL_MANIFEST_INVALID";
    }
    return "EXTERNAL_MANIFEST_INVALID";
}

inline std::filesystem::path payload_input_path(const std::filesystem::path& root,
                                                const V5NodeState& state,
                                                std::size_t index) {
    return root / "payload-inputs" /
        ("n" + std::to_string(state.ordinal) + "-" + state.attempt_id + "-" +
         std::to_string(index) + ".bin");
}

inline void ensure_payload_input_directory(const std::filesystem::path& root) {
    const auto directory = root / "payload-inputs";
    if (std::filesystem::exists(directory)) {
        struct stat status{};
        if (::stat(directory.c_str(), &status) != 0 || !S_ISDIR(status.st_mode) ||
            status.st_uid != ::geteuid() || (status.st_mode & 0777) != 0700)
            throw Error("ArtifactPayloadStoreViolation");
        return;
    }
    if (::mkdir(directory.c_str(), 0700) != 0 || ::chmod(directory.c_str(), 0700) != 0)
        throw Error("ArtifactPayloadStoreViolation");
}

inline void verify_payload_compatibility_file(
        const std::filesystem::path& path, const artifact_v1::ArtifactIdV1& expected,
        std::chrono::steady_clock::time_point deadline) {
    if (std::chrono::steady_clock::now() >= deadline) throw Error("ArtifactPayloadTimeout");
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) throw Error("ArtifactPayloadStoreViolation");
    try {
        struct stat st{};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
            st.st_nlink != 1 || (st.st_mode & 0777) != 0600 || st.st_size < 0 ||
            static_cast<std::uint64_t>(st.st_size) != expected.size_bytes)
            throw Error("WorkerDigestMismatch");
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(EVP_MD_CTX_new(), EVP_MD_CTX_free);
        if (!digest || EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr) != 1)
            throw Error("WorkerDigestMismatch");
        std::array<char, 8192> bytes{};
        std::uint64_t total{};
        while (total < expected.size_bytes) {
            if (std::chrono::steady_clock::now() >= deadline) throw Error("ArtifactPayloadTimeout");
            const auto n = ::read(fd, bytes.data(),
                std::min<std::uint64_t>(bytes.size(), expected.size_bytes - total));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0 || EVP_DigestUpdate(digest.get(), bytes.data(), static_cast<std::size_t>(n)) != 1)
                throw Error("WorkerDigestMismatch");
            total += static_cast<std::size_t>(n);
        }
        char extra;
        ssize_t n;
        do {
            if (std::chrono::steady_clock::now() >= deadline) throw Error("ArtifactPayloadTimeout");
            n = ::read(fd, &extra, 1);
        } while (n < 0 && errno == EINTR);
        if (n != 0 || payload_input_digest(digest.get()) != expected.sha256)
            throw Error("WorkerDigestMismatch");
    } catch (...) {
        ::close(fd);
        throw;
    }
    ::close(fd);
}

inline void prepare_payload_stream_inputs(
        artifact_v1::ArtifactStoreV1& store, V5NodeState& state,
        const std::vector<artifact_v1::ArtifactPayloadSelectionV13>& selections,
        const std::filesystem::path& root, const Options& options,
        std::chrono::steady_clock::time_point deadline) {
    using namespace artifact_v1;
    // The supervisor exclusively creates attempts/<id> at launch. Preflight
    // inputs therefore have their own Bruce-owned attempt staging boundary.
    if (std::chrono::steady_clock::now() >= deadline) throw Error("ArtifactPayloadTimeout");
    const auto attempt_root = root / "payload-inputs" / state.attempt_id;
    struct stat parent{};
    if (::lstat(attempt_root.parent_path().c_str(), &parent) != 0 ||
        !S_ISDIR(parent.st_mode) || parent.st_uid != ::geteuid() ||
        (parent.st_mode & 0777) != 0700 || ::mkdir(attempt_root.c_str(), 0700) != 0)
        throw Error("ArtifactPayloadStoreViolation");
    try {
        if (::chmod(attempt_root.c_str(), 0700) != 0)
            throw Error("ArtifactPayloadStoreViolation");
        for (std::size_t index = 0; index < selections.size(); ++index) {
            const bool external = index >= state.spec->dependencies.size();
            const auto check_deadline = [&] {
                if (std::chrono::steady_clock::now() >= deadline)
                    throw Error(external ? "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
            };
            check_deadline();
            const auto input_name = "input-" + std::to_string(index);
            const auto input_root = attempt_root / input_name;
            if (::mkdir(input_root.c_str(), 0700) != 0 || ::chmod(input_root.c_str(), 0700) != 0)
                throw Error("ArtifactPayloadStoreViolation");
            const auto source = input_root / "source.tmp";
            check_deadline();
            const int fd = ::open(source.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (fd < 0) throw Error("ArtifactPayloadStoreViolation");
            try {
                (void)materialize_artifact_inputs_v13(store, {selections[index]}, fd,
                    options.budget.max_input_bytes, deadline);
                check_deadline();
                if (::lseek(fd, 0, SEEK_SET) != 0) throw Error("ArtifactPayloadStoreViolation");
                const auto stream = write_payload_stream_v15(fd, input_root, "payload",
                    selections[index].object, options.budget.max_input_bytes, deadline);
                verify_payload_stream_v15(stream, input_root, deadline);
                // The unchanged V1 supervisor accepts regular files only.
                // Consume authenticated canonical chunks into a private,
                // bounded compatibility file without a payload-sized buffer.
                const auto compatibility = input_root / "input.bin";
                check_deadline();
                const int compatibility_fd = ::open(compatibility.c_str(),
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
                if (compatibility_fd < 0) throw Error("ArtifactPayloadStoreViolation");
                try {
                    consume_approved_stream({stream.reference, stream.size_bytes, stream.sha256},
                        input_root / stream.reference, [&](std::string_view chunk) {
                            while (!chunk.empty()) {
                                check_deadline();
                                const auto n = ::write(compatibility_fd, chunk.data(), chunk.size());
                                if (n < 0 && errno == EINTR) continue;
                                if (n <= 0) throw Error("ArtifactPayloadStoreViolation");
                                chunk.remove_prefix(static_cast<std::size_t>(n));
                            }
                        }, deadline);
                    check_deadline();
                    if (::fsync(compatibility_fd) != 0) throw Error("ArtifactPayloadStoreViolation");
                } catch (...) {
                    ::close(compatibility_fd);
                    throw;
                }
                if (::close(compatibility_fd) != 0) throw Error("ArtifactPayloadStoreViolation");
                verify_payload_compatibility_file(compatibility, selections[index].object, deadline);
                if (::unlink(source.c_str()) != 0) throw Error("ArtifactPayloadStoreViolation");
                if (std::chrono::steady_clock::now() >= deadline)
                    throw Error(external ? "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
                state.payload_inputs.push_back({
                    (std::filesystem::path("payload-inputs") / state.attempt_id /
                        input_name / "input.bin").generic_string(),
                    stream.size_bytes, stream.sha256});
            } catch (const ArtifactPayloadStreamErrorV15& error) {
                ::close(fd);
                switch (error.code()) {
                case ArtifactPayloadStreamErrorCodeV15::limit_exceeded:
                    throw Error(external ? "EXTERNAL_INPUT_LIMIT_EXCEEDED" : "ArtifactPayloadInputLimitExceeded");
                case ArtifactPayloadStreamErrorCodeV15::deadline_exceeded:
                    throw Error(external ? "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
                case ArtifactPayloadStreamErrorCodeV15::invalid_destination:
                case ArtifactPayloadStreamErrorCodeV15::invalid_source:
                    throw Error("ArtifactPayloadStoreViolation");
                default:
                    throw Error(external ? "EXTERNAL_OBJECT_INVALID" : "ArtifactPayloadDigestMismatch");
                }
            } catch (const ArtifactPayloadMaterializationExceptionV13& error) {
                ::close(fd);
                switch (error.code()) {
                case ArtifactPayloadMaterializationErrorV13::input_limit_exceeded:
                case ArtifactPayloadMaterializationErrorV13::object_limit_exceeded:
                    throw Error(external ? "EXTERNAL_INPUT_LIMIT_EXCEEDED" : "ArtifactPayloadInputLimitExceeded");
                case ArtifactPayloadMaterializationErrorV13::deadline_exceeded:
                    throw Error(external ? "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
                default:
                    throw Error(external ? "EXTERNAL_OBJECT_INVALID" : "ArtifactPayloadDigestMismatch");
                }
            } catch (const Error& error) {
                ::close(fd);
                const std::string_view message(error.what());
                if (message == "WorkerDigestMismatch" || message == "InvalidWorkerInput")
                    throw Error(external ? "EXTERNAL_OBJECT_INVALID" : "ArtifactPayloadDigestMismatch");
                if (message == "ArtifactPayloadTimeout" && external)
                    throw Error("EXTERNAL_INPUT_TIMEOUT");
                throw;
            } catch (...) {
                ::close(fd);
                throw;
            }
            if (::close(fd) != 0) throw Error("ArtifactPayloadStoreViolation");
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(attempt_root, ignored);
        state.payload_inputs.clear();
        throw;
    }
}

inline void prepare_payload_inputs(
        artifact_v1::ArtifactStoreV1& store,
        const std::string& run_id, V5NodeState& state,
        const std::map<std::string, V5NodeState>& states,
        const std::filesystem::path& root, const Options& options,
        std::chrono::steady_clock::time_point deadline,
        const artifact_v1::ExternalInputsSidecarV14* external_sidecar = nullptr,
        bool stream_v15 = false) {
    using namespace artifact_v1;
    state.payload_inputs.clear();
    state.external_input_count = 0;
    state.external_input_bytes = 0;
    const ExternalInputBindingV14* external_binding = nullptr;
    if (external_sidecar) {
        for (const auto& binding : external_sidecar->bindings) {
            if (binding.node_id == state.spec->node_id) {
                external_binding = &binding;
                break;
            }
        }
    }
    const std::size_t external_count = external_binding ? external_binding->inputs.size() : 0;
    if (state.spec->dependencies.size() + external_count >= artifact_payload_input_object_limit_v13)
        throw Error("ArtifactPayloadInputLimitExceeded");
    std::uint64_t total = state.spec->request_bytes.size();
    std::vector<ArtifactPayloadSelectionV13> selections;
    selections.reserve(state.spec->dependencies.size() + external_count);
    for (const auto& dependency : state.spec->dependencies) {
        const auto& producer = states.at(dependency);
        if (!producer.accepted || producer.accepted_manifest_id.empty() ||
            !producer.result.payload_digest)
            throw Error("ArtifactPayloadManifestInvalid");
        if (total > options.budget.max_input_bytes ||
            producer.result.payload_size > options.budget.max_input_bytes - total)
            throw Error("ArtifactPayloadInputLimitExceeded");
        if (std::chrono::steady_clock::now() >= deadline)
            throw Error("ArtifactPayloadTimeout");
        ArtifactInputRefV13 reference{
            producer.accepted_manifest_id, 0, *producer.result.payload_digest,
            producer.result.payload_size};
        ArtifactPayloadProvenanceV13 provenance{run_id, dependency,
                                                 producer.attempt_id};
        try {
            selections.push_back(select_artifact_input_v13(store, reference, provenance));
        } catch (const ArtifactPayloadErrorV13& error) {
            switch (error.code()) {
                case ArtifactPayloadErrorCodeV13::manifest_not_ready:
                    throw Error("ArtifactPayloadManifestNotReady");
                case ArtifactPayloadErrorCodeV13::object_verification_failed:
                    throw Error("ArtifactPayloadDigestMismatch");
                case ArtifactPayloadErrorCodeV13::manifest_invalid:
                    throw Error("ArtifactPayloadManifestInvalid");
                default:
                    throw Error("ArtifactPayloadReferenceInvalid");
            }
        }
        total += producer.result.payload_size;
    }
    if (external_binding) {
        for (const auto& input : external_binding->inputs) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw Error("EXTERNAL_INPUT_TIMEOUT");
            if (total > options.budget.max_input_bytes ||
                input.size_bytes > options.budget.max_input_bytes - total)
                throw Error("EXTERNAL_INPUT_LIMIT_EXCEEDED");
            ArtifactInputRefV13 reference{
                input.manifest_id, input.object_index, input.sha256, input.size_bytes};
            ArtifactPayloadProvenanceV13 provenance{
                input.source_run_id, input.source_node_id, input.source_attempt_id};
            try {
                selections.push_back(select_artifact_input_v13(store, reference, provenance));
            } catch (const ArtifactPayloadErrorV13& error) {
                throw Error(external_selection_failure_name(error.code()));
            }
            total += input.size_bytes;
            ++state.external_input_count;
            state.external_input_bytes += input.size_bytes;
        }
    }
    ensure_payload_input_directory(root);
    if (stream_v15) {
        prepare_payload_stream_inputs(store, state, selections, root, options, deadline);
        return;
    }
    std::vector<std::filesystem::path> created_paths;
    try {
        for (std::size_t index = 0; index < selections.size(); ++index) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw Error(index >= state.spec->dependencies.size() ?
                    "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
            const auto path = payload_input_path(root, state, index);
            const int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (fd < 0) throw Error("ArtifactPayloadStoreViolation");
            created_paths.push_back(path);
            try {
                (void)materialize_artifact_inputs_v13(store, {selections[index]}, fd,
                    options.budget.max_input_bytes, deadline);
                if (::close(fd) != 0) throw Error("ArtifactPayloadStoreViolation");
            } catch (const ArtifactPayloadMaterializationExceptionV13& error) {
                ::close(fd);
                switch (error.code()) {
                    case ArtifactPayloadMaterializationErrorV13::input_limit_exceeded:
                    case ArtifactPayloadMaterializationErrorV13::object_limit_exceeded:
                        throw Error(index >= state.spec->dependencies.size() ?
                            "EXTERNAL_INPUT_LIMIT_EXCEEDED" : "ArtifactPayloadInputLimitExceeded");
                    case ArtifactPayloadMaterializationErrorV13::deadline_exceeded:
                        throw Error(index >= state.spec->dependencies.size() ?
                            "EXTERNAL_INPUT_TIMEOUT" : "ArtifactPayloadTimeout");
                    default:
                        throw Error(index >= state.spec->dependencies.size() ?
                            "EXTERNAL_OBJECT_INVALID" : "ArtifactPayloadDigestMismatch");
                }
            } catch (...) {
                ::close(fd);
                throw;
            }
            state.payload_inputs.push_back({
                std::string("payload-inputs/") + path.filename().string(),
                selections[index].object.size_bytes, selections[index].object.sha256});
        }
    } catch (...) {
        for (const auto& path : created_paths) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
        state.payload_inputs.clear();
        throw;
    }
}

inline std::string publish_payload_output(
        artifact_v1::ArtifactStoreV1& store, const std::string& run_id,
        V5NodeState& state, const std::filesystem::path& root) {
    if (!state.result.payload_digest || !state.result.output_artifact_ref ||
        state.result.output_artifact_ref->reference != "result" ||
        state.result.payload_size != state.result.output_artifact_ref->size_bytes)
        throw Error("ArtifactPayloadDigestMismatch");
    const auto source = root / "attempts" / state.attempt_id / "output" / "result.bin";
    try {
        artifact_v1::ArtifactIdV1 object = store.put_file(
            source, *state.result.payload_digest, state.result.payload_size);
        const auto manifest_id = v5_manifest_id_v1(run_id, state.spec->node_id, state.attempt_id);
        artifact_v1::ArtifactManifestV1 manifest;
        manifest.run_id = run_id;
        manifest.node_id = state.spec->node_id;
        manifest.attempt_id = state.attempt_id;
        manifest.status = "READY";
        manifest.result_code = state.result.result_code;
        manifest.objects.push_back({object, {}, "artifact/" + state.spec->node_id});
        store.publish_manifest(manifest_id, manifest);
        return manifest_id;
    } catch (const Error& error) {
        if (std::string_view(error.what()) == "ArtifactExpectedDigestMismatch" ||
            std::string_view(error.what()) == "ArtifactExpectedSizeMismatch")
            throw Error("ArtifactPayloadDigestMismatch");
        throw Error("ArtifactPayloadStoreViolation");
    }
}

inline std::string render_multi_report(
        const dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3& parsed,
        const Options& options, const std::map<std::string, V5NodeState>& states,
        bool success, bool payload_route = false, std::string_view external_inputs_digest = {},
        bool cancelled = false) {
    std::string report = std::string("{\"contract\":\"") +
        (payload_route ? (external_inputs_digest.empty() ?
            "dag-scheduler-v5-payload-v13" : "dag-scheduler-v5-payload-v14") :
            "dag-scheduler-v5-cli-v1") +
        "\",\"action\":\"" + (payload_route ? "payload" : "execute") + "\"";
    report += ",\"spec_digest\":" + json_string(sha256(parsed.raw_bytes));
    report += ",\"dag_id\":" + json_string(parsed.spec.dag_id);
    report += ",\"retry\":" + std::to_string(options.retry);
    report += ",\"workers\":" + std::to_string(options.workers);
    report += ",\"budget\":{\"max_wall_ms\":" + std::to_string(options.budget.max_wall_ms) +
        ",\"max_input_bytes\":" + std::to_string(options.budget.max_input_bytes) +
        ",\"max_output_bytes\":" + std::to_string(options.budget.max_output_bytes) +
        ",\"max_operations\":" + std::to_string(options.budget.max_operations) +
        ",\"max_memory_bytes\":" + std::to_string(options.budget.max_memory_bytes) + "}";
    if (!external_inputs_digest.empty())
        report += ",\"external_inputs_digest\":" + json_string(external_inputs_digest);
    report += ",\"status\":" + json_string(cancelled ? "CANCELLED" : (success ? "READY" : "FAILED")) + ",\"receipts\":[";
    for (std::size_t i = 0; i < parsed.spec.nodes.size(); ++i) {
        const auto& node = parsed.spec.nodes[i];
        const auto& state = states.at(node.node_id);
        if (i) report += ',';
        const auto render_usage = [](const worker_v1::WorkerUsage& usage) {
            return std::string("{\"wall_ms\":") + std::to_string(usage.wall_ms) +
                ",\"input_bytes\":" + std::to_string(usage.input_bytes) +
                ",\"output_bytes\":" + std::to_string(usage.output_bytes) +
                ",\"operations\":" + std::to_string(usage.operations) +
                ",\"peak_memory_bytes\":" + std::to_string(usage.peak_memory_bytes) + '}';
        };
        report += "{\"node_id\":" + json_string(node.node_id) +
            ",\"callback_id\":" + json_string(node.callback_id) +
            ",\"request_digest\":" + json_string(node.request_digest) +
            ",\"attempt_id\":" + (state.attempt_id.empty() ? "null" : json_string(state.attempt_id)) +
            ",\"status\":" + json_string(status_name(state.result.status)) +
            ",\"failure_code\":" + json_string(failure_name(state.result.failure_code)) +
            ",\"result_code\":" + json_string(state.result.result_code) +
            ",\"reference\":" + (state.accepted_public_reference.empty() ? "null" : json_string(state.accepted_public_reference)) +
            ",\"payload_digest\":" + (state.result.payload_digest ? json_string(*state.result.payload_digest) : "null") +
            ",\"payload_size\":" + std::to_string(state.result.payload_size) +
            ",\"usage\":" + render_usage(state.result.usage);
        if (!external_inputs_digest.empty()) {
            report += ",\"external_input_count\":" + std::to_string(state.external_input_count) +
                ",\"external_input_bytes\":" + std::to_string(state.external_input_bytes);
        }
        report += ",\"attempts\":[";
        for (std::size_t attempt = 0; attempt < state.history.size(); ++attempt) {
            if (attempt) report += ',';
            const auto& history = state.history[attempt];
            report += "{\"attempt_id\":" + json_string(history.attempt_id) +
                ",\"status\":" + json_string(status_name(history.result.status)) +
                ",\"failure_code\":" + json_string(failure_name(history.result.failure_code)) +
                ",\"result_code\":" + json_string(history.result.result_code) +
                ",\"accepted\":" + (history.accepted ? "true" : "false") +
                ",\"payload_digest\":" +
                (history.result.payload_digest ? json_string(*history.result.payload_digest) : "null") +
                ",\"payload_size\":" + std::to_string(history.result.payload_size) +
                ",\"usage\":" + render_usage(history.result.usage) + '}';
        }
        report += "]}";
    }
    report += "]}\n";
    return report;
}

inline void mark_blocked(V5NodeState& state) {
    state.result = {};
    state.result.status = worker_v1::WorkerStatus::BLOCKED;
    state.result.failure_code = worker_v1::FailureCode::BLOCKED;
    state.result.result_code = "BLOCKED";
    state.terminal = true;
}

inline void mark_cancelled(V5NodeState& state) {
    state.result = {};
    state.result.status = worker_v1::WorkerStatus::CANCELLED;
    state.result.failure_code = worker_v1::FailureCode::CANCELLED;
    state.result.result_code = "CANCELLED";
    state.attempt_id.clear();
    state.accepted_private_reference.clear();
    state.accepted_public_reference.clear();
    state.accepted_manifest_id.clear();
    state.payload_inputs.clear();
    state.pending_result.reset();
    state.accepted = false;
    state.terminal = true;
}

// The handoff record is a sibling of the public report. Resolve every parent
// component without following links, then create and adopt an exclusive
// owner-only temporary file through that directory descriptor.
inline void publish_handoff_sidecar_v1(const std::filesystem::path& bundle,
                                       std::string_view bytes) {
    if (bytes.empty() || bytes.size() > bruce::max_file_bytes)
        throw Error("HANDOFF_RECORD_INVALID");
    const auto sidecar = normalized_inspect_bundle_path(
        std::filesystem::path(bundle.string() + ".handoff-v1.json"));
    int directory = ::open(sidecar.root_path().c_str(),
                           O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory < 0) throw Error("HANDOFF_RECORD_INVALID");
    int file = -1;
    std::string temporary;
    bool temporary_created = false;
    bool adopted = false;
    try {
        for (const auto& component : sidecar.parent_path().relative_path()) {
            if (component.empty() || component == "." || component == "..")
                throw Error("HANDOFF_RECORD_INVALID");
            const int next = ::openat(directory, component.c_str(),
                                      O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (next < 0) throw Error("HANDOFF_RECORD_INVALID");
            ::close(directory);
            directory = next;
        }
        static std::atomic<std::uint64_t> sequence{0};
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            temporary = ".bruce-handoff-v1-" + std::to_string(::getpid()) + "-" +
                std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            file = ::openat(directory, temporary.c_str(),
                O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
            if (file >= 0) {
                temporary_created = true;
                break;
            }
            if (errno != EEXIST) throw Error("HANDOFF_RECORD_INVALID");
        }
        if (file < 0) throw Error("HANDOFF_RECORD_INVALID");
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(file, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw Error("HANDOFF_RECORD_INVALID");
            offset += static_cast<std::size_t>(written);
        }
        struct stat before{};
        struct stat on_path{};
        if (::fstat(file, &before) != 0 || !S_ISREG(before.st_mode) ||
            before.st_uid != ::geteuid() || (before.st_mode & 07777) != 0600 ||
            before.st_nlink != 1 || before.st_size != static_cast<off_t>(bytes.size()) ||
            ::fstatat(directory, temporary.c_str(), &on_path, AT_SYMLINK_NOFOLLOW) != 0 ||
            before.st_dev != on_path.st_dev || before.st_ino != on_path.st_ino ||
            ::fsync(file) != 0) throw Error("HANDOFF_RECORD_INVALID");
        if (::syscall(SYS_renameat2, directory, temporary.c_str(), directory,
                      sidecar.filename().c_str(), RENAME_NOREPLACE) != 0)
            throw Error("HANDOFF_RECORD_INVALID");
        adopted = true;
        struct stat adopted_path{};
        if (::fstatat(directory, sidecar.filename().c_str(), &adopted_path,
                      AT_SYMLINK_NOFOLLOW) != 0 ||
            before.st_dev != adopted_path.st_dev || before.st_ino != adopted_path.st_ino ||
            !S_ISREG(adopted_path.st_mode) || adopted_path.st_uid != ::geteuid() ||
            (adopted_path.st_mode & 07777) != 0600 || adopted_path.st_nlink != 1 ||
            adopted_path.st_size != static_cast<off_t>(bytes.size()))
            throw Error("HANDOFF_RECORD_INVALID");
        if (::fsync(directory) != 0) throw Error("HANDOFF_RECORD_INVALID");
        if (::close(file) != 0) {
            file = -1;
            throw Error("HANDOFF_RECORD_INVALID");
        }
        file = -1;
    } catch (...) {
        if (!adopted && temporary_created && file >= 0) {
            struct stat owned{};
            struct stat named{};
            if (::fstat(file, &owned) == 0 &&
                ::fstatat(directory, temporary.c_str(), &named, AT_SYMLINK_NOFOLLOW) == 0 &&
                owned.st_dev == named.st_dev && owned.st_ino == named.st_ino)
                ::unlinkat(directory, temporary.c_str(), 0);
        }
        if (file >= 0) ::close(file);
        ::close(directory);
        throw;
    }
    ::close(directory);
}

inline int execute_single_node_v2(
        const dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3& parsed,
        const std::filesystem::path& bundle, const Options& options,
        const std::filesystem::path& executable, const std::filesystem::path& root,
        const std::string& run_id) {
    const auto& node = parsed.spec.nodes.front();
    LocalFiles::publish_new((root / "inputs" / (node.node_id + ".bin")).string(),
                            node.request_bytes);
    worker_v1::Budget worker_budget = options.budget;
    worker_budget.max_output_bytes = std::min(worker_budget.max_output_bytes,
                                              parsed.spec.max_total_receipt_bytes);
    worker_v1::WorkerRequest worker;
    worker.run_id = run_id;
    worker.attempt_id = "a-1";
    worker.node_id = node.node_id;
    worker.adapter_id = node.callback_id;
    worker.adapter_version = "1";
    worker.request_digest = node.request_digest;
    worker.input_artifact_refs.push_back({"inputs/" + node.node_id + ".bin",
                                          node.request_bytes.size(), node.request_digest});
    worker.budget = worker_budget;
    auto run = start_single_worker_v2({std::move(worker)},
        {std::filesystem::absolute(root), std::filesystem::absolute(executable),
         worker_budget, 1000});
    CommonRuntimeHostV2Outcome outcome;
    for (;;) {
        if (worker_v1::probe_abort(root) == worker_v1::AbortProbeResult::REQUESTED)
            request_cancel_v2(run);
        if (auto completed = poll_single_worker_v2(run)) {
            outcome = std::move(*completed);
            break;
        }
        ::usleep(1000);
    }
    const bool accepted = outcome.host_exit_code == 0;
    const std::string reference = accepted ? "artifact/" + node.node_id : std::string{};
    const std::vector<V5AttemptHistory> history{{"a-1", outcome.result, accepted}};
    const auto report = render_report(parsed, options, outcome.result, reference, history) + '\n';
    LocalFiles::publish_new_atomically(bundle.string(), report);
    std::cout << report;
    return outcome.host_exit_code;
}

inline int execute_multi_node_v2(
        const dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3& parsed,
        const std::filesystem::path& bundle, const Options& options,
        const std::filesystem::path& executable, const std::filesystem::path& root,
        const std::string& run_id,
        artifact_v1::ArtifactStoreV1* payload_store = nullptr,
        const artifact_v1::ExternalInputsSidecarV14* external_sidecar = nullptr,
        std::string_view external_inputs_digest = {}) {
    const bool payload_route = payload_store != nullptr;
    using namespace worker_v1;
    std::map<std::string, V5NodeState> states;
    std::map<std::string, std::vector<std::string>> successors;
    std::set<std::string> ready_nodes;
    for (std::size_t i = 0; i < parsed.spec.nodes.size(); ++i) {
        const auto& node = parsed.spec.nodes[i];
        V5NodeState state;
        state.spec = &node;
        state.ordinal = i;
        state.result.status = WorkerStatus::FAILED;
        state.result.failure_code = FailureCode::FAILED;
        state.result.result_code = "PENDING";
        states.emplace(node.node_id, std::move(state));
        LocalFiles::publish_new((root / "inputs" / (node.node_id + ".bin")).string(),
                                node.request_bytes);
        if (node.dependencies.empty()) ready_nodes.insert(node.node_id);
        for (const auto& dependency : node.dependencies)
            successors[dependency].push_back(node.node_id);
    }

    CommonRuntimeHostV2BatchContext context;
    context.run_root = std::filesystem::absolute(root);
    context.worker_executable = std::filesystem::absolute(executable);
    context.budget = options.budget;
    context.max_workers = options.workers;
    context.grace_ms = 1000;
    if (payload_route && ::mkdir((root / "attempts").c_str(), 0700) != 0)
        throw Error("DagSchedulerV5RunRootUnavailable");
    auto batch = start_batch_v2(run_id, context);

    std::uint64_t started_nodes{};
    std::size_t live_attempts{};
    std::set<std::string> live_attempt_ids;
    std::uint64_t retained_bytes{};
    bool run_failed = false;
    bool host_cancelled = false;
    bool abort_invalid = false;
    std::set<std::string> invalid_cancelled_attempt_ids;
    std::set<std::string> invalid_active_attempt_ids;
    std::set<std::string> no_progress_blocked_nodes;
    std::set<std::string> cancelled_dependency_blocked_nodes;
    const auto receipt_order = dag_scheduler_v3_cli_detail::planned_start_order(parsed.spec);
    std::map<std::string, std::size_t> canonical_position;
    for (std::size_t i = 0; i < receipt_order.size(); ++i)
        canonical_position.emplace(receipt_order[i], i);
    std::size_t receipt_cursor{};
    const auto is_ready = [](const WorkerResult& result) {
        return result.status == WorkerStatus::READY &&
            result.failure_code == FailureCode::READY &&
            result.output_artifact_ref && result.payload_digest &&
            result.output_artifact_ref->reference == "result" &&
            result.output_artifact_ref->size_bytes == result.payload_size &&
            result.output_artifact_ref->sha256 == *result.payload_digest;
    };
    const auto make_failed = [](FailureCode code) {
        WorkerResult result;
        result.status = code == FailureCode::CANCELLED ? WorkerStatus::CANCELLED : WorkerStatus::FAILED;
        result.failure_code = code;
        result.result_code = failure_name(code);
        return result;
    };
    const auto block_dependents = [&](const auto& self, const std::string& failed_id,
                                      bool due_to_cancel = false) -> void {
        for (const auto& successor : successors[failed_id]) {
            auto& dependent = states.at(successor);
            ready_nodes.erase(successor);
            if (dependent.terminal) continue;
            mark_blocked(dependent);
            if (due_to_cancel) cancelled_dependency_blocked_nodes.insert(successor);
            self(self, successor, due_to_cancel);
        }
    };
    const auto collect_completed = [&](const auto& completed) {
        for (const auto& item : completed) {
            if (item.invalid_control) invalid_active_attempt_ids.insert(item.attempt_id);
            if (live_attempts > 0) --live_attempts;
            live_attempt_ids.erase(item.attempt_id);
            auto state_iterator = std::find_if(states.begin(), states.end(), [&](const auto& entry) {
                return entry.second.attempt_id == item.attempt_id;
            });
            if (state_iterator != states.end())
                state_iterator->second.pending_result = item.result;
        }
    };
    const auto observe_abort = [&] {
        if (abort_invalid) return;
        const auto observed = probe_abort(root);
        if (observed == AbortProbeResult::INVALID) {
            abort_invalid = true;
            run_failed = true;
            ready_nodes.clear();
            // Let the Host classify the invalid marker before it stops workers.
            // Already projected undecided results are cancelled decisions;
            // active attempts are projected PROTOCOL_ERROR by the Host.
            for (auto& item : states) {
                auto& state = item.second;
                if (state.terminal) continue;
                if (state.pending_result) {
                    if (!invalid_active_attempt_ids.contains(state.attempt_id))
                        invalid_cancelled_attempt_ids.insert(state.attempt_id);
                } else if (!live_attempt_ids.contains(state.attempt_id)) {
                    if (state.history.empty()) {
                        // Preparation can reserve an ID before dispatch. A
                        // blocked node must not advertise an invented attempt.
                        state.attempt_id.clear();
                        state.payload_inputs.clear();
                        state.external_input_count = 0;
                        state.external_input_bytes = 0;
                    } else {
                        state.attempt_id = state.history.back().attempt_id;
                        invalid_cancelled_attempt_ids.insert(state.attempt_id);
                        const auto usage = state.result.usage;
                        state.result = make_failed(FailureCode::CANCELLED);
                        state.result.usage = usage;
                        state.history.back().result = state.result;
                        state.terminal = true;
                    }
                }
            }
            collect_completed(poll_batch_v2(batch));
            return;
        }
        if (observed == AbortProbeResult::REQUESTED) {
            host_cancelled = true;
            ready_nodes.clear();
            request_cancel_batch_v2(batch);
        }
        if (!host_cancelled) return;
        for (auto& item : states) {
            auto& state = item.second;
            if (!state.terminal && !live_attempt_ids.contains(state.attempt_id) &&
                !state.pending_result) {
                if (state.history.empty()) {
                    mark_cancelled(state);
                } else {
                    // Preparation may already reserve the next retry ID.
                    // Cancel still owns the last actually dispatched attempt.
                    state.attempt_id = state.history.back().attempt_id;
                    const auto usage = state.result.usage;
                    state.result = make_failed(FailureCode::CANCELLED);
                    state.result.usage = usage;
                    state.history.back().result = state.result;
                    state.history.back().accepted = false;
                    state.terminal = true;
                }
            }
        }
    };

    while (true) {
        observe_abort();
        while (!ready_nodes.empty() && !host_cancelled && !abort_invalid) {
            observe_abort();
            if (host_cancelled || abort_invalid) break;
            const auto id_to_start = *ready_nodes.begin();
            if (live_attempts >= options.workers) break;
            auto& state = states.at(id_to_start);
            ready_nodes.erase(ready_nodes.begin());
            if (state.terminal || state.handle) continue;
            const bool outside_canonical_start_budget =
                canonical_position.at(id_to_start) >= parsed.spec.max_started_nodes;
            if (!state.started_once && (outside_canonical_start_budget ||
                                        started_nodes >= parsed.spec.max_started_nodes)) {
                state.result = make_failed(FailureCode::FAILED);
                state.result.result_code = "START_BUDGET_EXCEEDED";
                state.terminal = true;
                run_failed = true;
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            }
            state.attempt_id = attempt_id_for(state, parsed.spec.nodes.size());
            state.attempt_deadline = payload_route ? std::chrono::steady_clock::now() +
                std::chrono::milliseconds(options.budget.max_wall_ms) :
                std::chrono::steady_clock::time_point::max();
            if (payload_route) {
                try {
                    prepare_payload_inputs(*payload_store, run_id, state, states, root,
                                           options, state.attempt_deadline, external_sidecar);
                    observe_abort();
                    if (host_cancelled || abort_invalid) continue;
                    if (std::chrono::steady_clock::now() >= state.attempt_deadline)
                        throw Error("ArtifactPayloadTimeout");
                } catch (const Error& error) {
                    observe_abort();
                    if (host_cancelled || abort_invalid) continue;
                    state.result = make_failed(FailureCode::FAILED);
                    const std::string_view message = error.what();
                    state.result.result_code = message.starts_with("EXTERNAL_") ?
                        std::string(message) : payload_failure_name(message);
                    state.terminal = true;
                    run_failed = true;
                    record_scheduler_failure_v2(batch);
                    block_dependents(block_dependents, state.spec->node_id);
                    continue;
                } catch (...) {
                    observe_abort();
                    if (host_cancelled || abort_invalid) continue;
                    state.result = make_failed(FailureCode::FAILED);
                    state.result.result_code = "ARTIFACT_REFERENCE_INVALID";
                    state.terminal = true;
                    run_failed = true;
                    record_scheduler_failure_v2(batch);
                    block_dependents(block_dependents, state.spec->node_id);
                    continue;
                }
            }
            observe_abort();
            if (host_cancelled || abort_invalid) continue;
            try {
                auto request = make_request(run_id, state, states, root, options, payload_route);
                CommonRuntimeHostV2AttemptOptions attempt_options;
                if (payload_route) attempt_options.deadline = state.attempt_deadline;
                start_batch_attempt_v2(batch, request, attempt_options);
            } catch (const Error& error) {
                state.result = make_failed(FailureCode::PROCESS_START_FAILED);
                if (payload_route && std::string_view(error.what()) == "TIMEOUT") {
                    state.result = make_failed(FailureCode::FAILED);
                    state.result.result_code = "TIMEOUT";
                }
                state.terminal = true;
                run_failed = true;
                if (payload_route) record_scheduler_failure_v2(batch);
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            } catch (...) {
                state.result = make_failed(FailureCode::PROCESS_START_FAILED);
                state.terminal = true;
                run_failed = true;
                if (payload_route) record_scheduler_failure_v2(batch);
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            }
            ++state.attempts;
            ++live_attempts;
            live_attempt_ids.insert(state.attempt_id);
            if (!state.started_once) {
                state.started_once = true;
                ++started_nodes;
            }
        }

        observe_abort();
        collect_completed(poll_batch_v2(batch));
        observe_abort();

        while (receipt_cursor < receipt_order.size()) {
            auto& state = states.at(receipt_order[receipt_cursor]);
            if (state.terminal) {
                ++receipt_cursor;
                continue;
            }
            if (!state.pending_result) break;
            auto result = std::move(*state.pending_result);
            // Retain the pending marker through synchronous publication: abort
            // observation must not erase this in-flight attempt's identity or
            // overwrite its previous historical retry before it is appended.
            if (host_cancelled || invalid_cancelled_attempt_ids.contains(state.attempt_id)) {
                const auto usage = result.usage;
                result = make_failed(FailureCode::CANCELLED);
                result.usage = usage;
            }
            std::string published_manifest_id;
            const auto reject = [&](CommonRuntimeHostV2RejectionReason reason,
                                    FailureCode code, std::string_view result_code) {
                record_attempt_rejection_v2(batch, state.attempt_id, reason);
                const auto usage = result.usage;
                result = make_failed(code);
                result.result_code = std::string(result_code);
                result.usage = usage;
            };
            observe_abort();
            if (host_cancelled || invalid_cancelled_attempt_ids.contains(state.attempt_id)) {
                const auto usage = result.usage;
                result = make_failed(FailureCode::CANCELLED);
                result.usage = usage;
            }
            if (payload_route && is_ready(result) &&
                std::chrono::steady_clock::now() >= state.attempt_deadline)
                reject(CommonRuntimeHostV2RejectionReason::TIMEOUT,
                       FailureCode::TIMEOUT, "TIMEOUT");
            const bool output_limit = is_ready(result) &&
                (result.payload_size > parsed.spec.max_total_receipt_bytes ||
                 retained_bytes > parsed.spec.max_total_receipt_bytes ||
                 result.payload_size > parsed.spec.max_total_receipt_bytes - retained_bytes);
            if (output_limit) {
                reject(CommonRuntimeHostV2RejectionReason::OUTPUT_LIMIT_EXCEEDED,
                       FailureCode::OUTPUT_LIMIT_EXCEEDED, "OUTPUT_LIMIT_EXCEEDED");
            }
            if (payload_route && is_ready(result)) {
                // Publication alone never makes a DAG result accepted. Keep
                // its manifest private until the Host's final cancel probe.
                state.result = result;
                try {
                    published_manifest_id = publish_payload_output(*payload_store, run_id, state, root);
                } catch (const Error& error) {
                    observe_abort();
                    if (!host_cancelled && !abort_invalid) {
                        const auto code = payload_failure_name(error.what());
                        const auto reason = code == "ARTIFACT_DIGEST_MISMATCH" ?
                            CommonRuntimeHostV2RejectionReason::ARTIFACT_DIGEST_MISMATCH :
                            (code == "ARTIFACT_MANIFEST_INVALID" ?
                                CommonRuntimeHostV2RejectionReason::ARTIFACT_MANIFEST_INVALID :
                                (code == "ARTIFACT_STORE_VIOLATION" ?
                                    CommonRuntimeHostV2RejectionReason::ARTIFACT_STORE_VIOLATION :
                                    CommonRuntimeHostV2RejectionReason::ARTIFACT_REFERENCE_INVALID));
                        reject(reason, FailureCode::FAILED, code);
                    }
                } catch (...) {
                    observe_abort();
                    if (!host_cancelled && !abort_invalid)
                        reject(CommonRuntimeHostV2RejectionReason::ARTIFACT_STORE_VIOLATION,
                               FailureCode::FAILED, "ARTIFACT_STORE_VIOLATION");
                }
                observe_abort();
                if (host_cancelled || invalid_cancelled_attempt_ids.contains(state.attempt_id)) {
                    const auto usage = result.usage;
                    result = make_failed(FailureCode::CANCELLED);
                    result.usage = usage;
                } else if (is_ready(result) &&
                           std::chrono::steady_clock::now() >= state.attempt_deadline) {
                    reject(CommonRuntimeHostV2RejectionReason::TIMEOUT,
                           FailureCode::TIMEOUT, "TIMEOUT");
                }
            }
            state.pending_result.reset();
            state.result = result;
            const bool accepted = is_ready(result) && !host_cancelled && !abort_invalid;
            state.history.push_back({state.attempt_id, result, false});
            if (accepted) {
                try {
                    record_attempt_acceptance_v2(batch, state.attempt_id);
                    state.accepted = true;
                    state.terminal = true;
                    retained_bytes += result.payload_size;
                    state.history.back().accepted = true;
                    state.accepted_manifest_id = std::move(published_manifest_id);
                    state.accepted_private_reference =
                        "attempts/" + state.attempt_id + "/output/result.bin";
                    state.accepted_public_reference = "artifact/" + state.spec->node_id;
                    for (const auto& successor : successors[state.spec->node_id]) {
                        const auto& successor_spec = *states.at(successor).spec;
                        const bool dependencies_ready = std::all_of(
                            successor_spec.dependencies.begin(), successor_spec.dependencies.end(),
                            [&](const auto& dependency) { return states.at(dependency).accepted; });
                        if (dependencies_ready) ready_nodes.insert(successor);
                    }
                } catch (...) {
                    observe_abort();
                    const auto usage = result.usage;
                    if (host_cancelled || invalid_cancelled_attempt_ids.contains(state.attempt_id)) {
                        state.result = make_failed(FailureCode::CANCELLED);
                    } else {
                        if (is_ready(result)) {
                            try { record_attempt_rejection_v2(batch, state.attempt_id); }
                            catch (...) { /* preserve the protocol failure below */ }
                        }
                        state.result = make_failed(FailureCode::PROTOCOL_ERROR);
                        run_failed = true;
                    }
                    state.result.usage = usage;
                    state.history.back().result = state.result;
                    state.terminal = true;
                    if (!host_cancelled) block_dependents(block_dependents, state.spec->node_id);
                }
                ++receipt_cursor;
            } else if (!host_cancelled && !abort_invalid && retryable(result.failure_code) &&
                       state.attempts <= options.retry) {
                try {
                    record_attempt_retry_v2(batch, state.attempt_id);
                    ready_nodes.insert(state.spec->node_id);
                    // Keep this node's canonical receipt position until its
                    // final attempt is decided. Retries consume no node slot.
                    break;
                } catch (...) {
                    state.result = make_failed(FailureCode::PROTOCOL_ERROR);
                    state.history.back().result = state.result;
                    state.terminal = true;
                    run_failed = true;
                    block_dependents(block_dependents, state.spec->node_id);
                    ++receipt_cursor;
                }
            } else {
                state.terminal = true;
                if (!host_cancelled) {
                    run_failed = true;
                    block_dependents(block_dependents, state.spec->node_id,
                                     result.failure_code == FailureCode::CANCELLED);
                }
                ++receipt_cursor;
            }
        }

        const bool all_terminal = std::all_of(states.begin(), states.end(),
            [](const auto& item) { return item.second.terminal; });
        const bool any_pending = std::any_of(states.begin(), states.end(),
            [](const auto& item) { return item.second.pending_result.has_value(); });
        if (all_terminal && live_attempts == 0 && !any_pending) break;
        if (live_attempts == 0 && ready_nodes.empty() && !any_pending) {
            // A cancel marker may become visible after the preceding poll but
            // before this no-progress classification.  Give parent-side
            // cancellation one final linearization point so open nodes do not
            // become BLOCKED merely because their active predecessor was
            // reaped in that interval.
            observe_abort();
            if (host_cancelled) continue;
            for (auto& item : states) if (!item.second.terminal) {
                mark_blocked(item.second);
                no_progress_blocked_nodes.insert(item.first);
                run_failed = true;
            }
            break;
        }
        ::usleep(1000);
    }

    for (auto& item : states) {
        if (!item.second.terminal) {
            if (host_cancelled && item.second.attempt_id.empty()) {
                mark_cancelled(item.second);
            } else if (host_cancelled) {
                item.second.result = make_failed(FailureCode::CANCELLED);
                item.second.accepted_private_reference.clear();
                item.second.accepted_public_reference.clear();
                item.second.accepted_manifest_id.clear();
                item.second.pending_result.reset();
                item.second.accepted = false;
                item.second.terminal = true;
            }
            else mark_blocked(item.second);
        }
    }
    if (payload_route && run_failed) record_scheduler_failure_v2(batch);
    const auto batch_outcome = finish_batch_v2(batch);
    host_cancelled = host_cancelled || batch_outcome.status == WorkerStatus::CANCELLED;
    if (host_cancelled) {
        // finish_batch_v2 can observe a marker in the narrow interval after
        // the scheduler's final probe.  Only undo BLOCKED states created by
        // the no-progress fallback above; established failures and accepted
        // receipts remain historical facts.
        for (const auto& node_id : no_progress_blocked_nodes)
            mark_cancelled(states.at(node_id));
        for (const auto& node_id : cancelled_dependency_blocked_nodes)
            mark_cancelled(states.at(node_id));
    }
    if (payload_route && abort_invalid && parsed.spec.nodes.size() == 1) {
        // Single-node reports share node/run status. The cancelled historical
        // decision stays in history, while invalid control fails the run.
        auto& state = states.begin()->second;
        const auto usage = state.result.usage;
        state.result = make_failed(FailureCode::PROTOCOL_ERROR);
        state.result.usage = usage;
        state.accepted = false;
        state.accepted_private_reference.clear();
        state.accepted_public_reference.clear();
        state.accepted_manifest_id.clear();
    }
    const bool success = !host_cancelled && !run_failed && std::all_of(
        states.begin(), states.end(), [](const auto& item) { return item.second.accepted; });
    std::string report;
    if (parsed.spec.nodes.size() == 1) {
        const auto& state = states.begin()->second;
        report = render_report(parsed, options, state.result, state.accepted_public_reference,
                               state.history, payload_route, external_inputs_digest,
                               state.external_input_count, state.external_input_bytes) + '\n';
    } else {
        report = render_multi_report(parsed, options, states, success, payload_route,
                                     external_inputs_digest, host_cancelled);
    }
    LocalFiles::publish_new_atomically(bundle.string(), report);
    std::cout << report;
    return success ? 0 : 1;
}

inline int execute(const std::filesystem::path& spec_path, const std::filesystem::path& bundle,
                   const Options& options, const std::filesystem::path& executable,
                   bool payload_route = false,
                   artifact_v1::ArtifactStoreV1* payload_store = nullptr,
                   const artifact_v1::ExternalInputsSidecarV14* external_sidecar = nullptr,
                   std::string_view external_inputs_digest = {},
                   const std::map<std::string, std::size_t>* external_input_counts = nullptr,
                   bool stream_v15 = false, bool flow_v2 = false,
                   bool handoff_v1 = false) {
    using namespace worker_v1;
    const auto parsed = dag_scheduler_v3_cli_detail::parse_spec(spec_path, external_input_counts);
    const auto flow_sidecar_path = std::filesystem::path(bundle.string() + ".flow-v2.json");
    if (std::filesystem::exists(bundle) || std::filesystem::exists(bundle.string() + ".run") ||
        (flow_v2 && std::filesystem::exists(flow_sidecar_path)))
        throw Error("DagSchedulerV5OutputExists");
    const auto root = std::filesystem::path(bundle.string() + ".run");
    if (::mkdir(root.c_str(), 0700) != 0 || ::chmod(root.c_str(), 0700) != 0)
        throw Error("DagSchedulerV5RunRootUnavailable");
    if (::mkdir((root / "inputs").c_str(), 0700) != 0 ||
        ::chmod((root / "inputs").c_str(), 0700) != 0)
        throw Error("DagSchedulerV5RunRootUnavailable");
    if (payload_route && payload_store == nullptr)
        throw Error("ArtifactPayloadStoreViolation");
    const auto id = "run-" + std::to_string(::getpid());
    if (!payload_route && !stream_v15 && !flow_v2 && !handoff_v1 &&
        options.retry == 0 && options.workers == 1 && parsed.spec.nodes.size() == 1)
        return execute_single_node_v2(parsed, bundle, options, executable, root, id);
    if (!payload_route && !stream_v15 && !flow_v2 && !handoff_v1 &&
        (parsed.spec.nodes.size() > 1 ||
         (parsed.spec.nodes.size() == 1 && options.workers == 1 && options.retry > 0)))
        return execute_multi_node_v2(parsed, bundle, options, executable, root, id);
    if (payload_route && !stream_v15 && !flow_v2 && !handoff_v1)
        return execute_multi_node_v2(parsed, bundle, options, executable, root, id,
                                     payload_store, external_sidecar, external_inputs_digest);
    std::optional<artifact_v1::PayloadFlowV2> flow;
    std::optional<artifact_v1::ExecuteHandoffLineageV1> handoff;
    if (flow_v2) {
        flow.emplace();
        flow->run_id = id;
        flow->spec_digest = sha256(parsed.raw_bytes);
        flow->stream_v15 = stream_v15;
        flow->nodes.reserve(parsed.spec.nodes.size());
        for (const auto& node : parsed.spec.nodes) {
            flow->nodes.push_back({node.node_id, node.callback_id, node.request_digest,
                                   node.dependencies});
        }
    }
    if (handoff_v1) {
        handoff.emplace();
        handoff->run_id = id;
        handoff->spec_digest = sha256(parsed.raw_bytes);
        handoff->nodes.reserve(parsed.spec.nodes.size());
        handoff->attempts.reserve(artifact_v1::execute_handoff_lineage_v1_max_attempts);
        for (const auto& node : parsed.spec.nodes)
            handoff->nodes.push_back({node.node_id, node.callback_id,
                                      node.request_digest, node.dependencies});
    }
    auto log = WorkerRunLog::create(root, id);
    std::uint64_t seq{};
    const auto append = [&](std::string event, std::string status, std::string attempt_id = {},
                            std::optional<ArtifactRef> artifact = {},
                            std::optional<WorkerUsage> usage_value = {}) {
        WorkerLogEvent record;
        record.seq = ++seq; record.ts_unix_ms = unix_ms();
        record.event = std::move(event); record.status = std::move(status);
        if (!attempt_id.empty()) record.attempt_id = std::move(attempt_id);
        if (artifact) {
            record.reference = "attempts/" + *record.attempt_id + "/output/result.bin";
            record.size_bytes = artifact->size_bytes;
            record.digest = artifact->sha256;
        }
        record.usage = usage_value;
        log.append(record);
    };
    append("RUN_CREATED", "READY");
    std::map<std::string, V5NodeState> states;
    std::map<std::string, std::vector<std::string>> successors;
    std::set<std::string> ready_nodes;
    for (std::size_t i = 0; i < parsed.spec.nodes.size(); ++i) {
        const auto& node = parsed.spec.nodes[i];
        V5NodeState state;
        state.spec = &node; state.ordinal = i;
        state.result.status = WorkerStatus::FAILED;
        state.result.failure_code = FailureCode::FAILED;
        state.result.result_code = "PENDING";
        states.emplace(node.node_id, std::move(state));
        const auto request_path = root / "inputs" / (node.node_id + ".bin");
        LocalFiles::publish_new(request_path.string(), node.request_bytes);
        if (node.dependencies.empty()) ready_nodes.insert(node.node_id);
        for (const auto& dependency : node.dependencies) successors[dependency].push_back(node.node_id);
    }

    std::uint64_t started_nodes{};
    bool run_failed = false;
    std::uint64_t retained_bytes{};
    const auto receipt_order = dag_scheduler_v3_cli_detail::planned_start_order(parsed.spec);
    std::map<std::string, std::size_t> canonical_position;
    for (std::size_t i = 0; i < receipt_order.size(); ++i)
        canonical_position.emplace(receipt_order[i], i);
    std::size_t receipt_cursor{};
    const auto is_ready = [](const WorkerResult& result) {
        return result.status == WorkerStatus::READY &&
            result.failure_code == FailureCode::READY &&
            result.output_artifact_ref && result.payload_digest &&
            result.output_artifact_ref->reference == "result" &&
            result.output_artifact_ref->size_bytes == result.payload_size &&
            result.output_artifact_ref->sha256 == *result.payload_digest;
    };
    const auto make_failed = [](FailureCode code) {
        WorkerResult result;
        result.status = code == FailureCode::CANCELLED ? WorkerStatus::CANCELLED : WorkerStatus::FAILED;
        result.failure_code = code;
        result.result_code = failure_name(code);
        return result;
    };
    const auto block_dependents = [&](const auto& self, const std::string& failed_id) -> void {
        for (const auto& successor : successors[failed_id]) {
            auto& dependent = states.at(successor);
            ready_nodes.erase(successor);
            if (dependent.terminal) continue;
            mark_blocked(dependent);
            self(self, successor);
        }
    };

    bool host_cancelled = false;
    const auto observe_abort = [&] {
        const auto observed = probe_abort(root);
        if (observed == AbortProbeResult::INVALID) throw Error("PROTOCOL_ERROR");
        if (observed == AbortProbeResult::REQUESTED) host_cancelled = true;
        if (!host_cancelled) return;
        ready_nodes.clear();
        // Cancellation is parent-side linearized here. Log every attempt that
        // has started but has not yet been committed as a receipt, then send
        // the existing supervisor cancellation request to every live handle
        // before polling any of them again.
        for (auto& item : states) {
            auto& state = item.second;
            if (state.terminal || state.cancellation_logged || state.attempt_id.empty()) continue;
            if (!state.handle && !state.pending_result) continue;
            append("CANCEL_REQUESTED", "CANCELLED", state.attempt_id);
            state.cancellation_logged = true;
            if (state.handle) request_cancel(*state.handle);
        }
        for (auto& item : states) {
            auto& state = item.second;
            if (!state.terminal && !state.handle && !state.pending_result)
                mark_cancelled(state);
        }
    };

    while (true) {
        observe_abort();
        while (!ready_nodes.empty() && !host_cancelled) {
            // Recheck immediately before dispatch so a marker created while
            // preparing a bounded request cannot cause a new worker start.
            observe_abort();
            if (host_cancelled) break;
            std::string id_to_start = *ready_nodes.begin();
            if (std::count_if(states.begin(), states.end(), [](const auto& item) {
                    return item.second.handle.has_value();
                }) >= static_cast<std::ptrdiff_t>(options.workers))
                break;
            auto& state = states.at(id_to_start);
            ready_nodes.erase(ready_nodes.begin());
            if (state.terminal || state.handle) continue;
            const bool outside_canonical_start_budget =
                canonical_position.at(id_to_start) >= parsed.spec.max_started_nodes;
            if (!state.started_once && (outside_canonical_start_budget ||
                                        started_nodes >= parsed.spec.max_started_nodes)) {
                state.result = make_failed(FailureCode::FAILED);
                state.result.result_code = "START_BUDGET_EXCEEDED";
                state.terminal = true;
                run_failed = true;
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            }
            state.attempt_id = attempt_id_for(state, parsed.spec.nodes.size());
            state.deadline_cancelled = false;
            state.attempt_deadline = payload_route ?
                std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(options.budget.max_wall_ms) :
                std::chrono::steady_clock::time_point::max();
            bool stream_inputs_prepared = false;
            const auto discard_stream_inputs = [&] {
                if (!stream_v15) return;
                if (stream_inputs_prepared) {
                    std::error_code ignored;
                    std::filesystem::remove_all(root / "payload-inputs" / state.attempt_id, ignored);
                }
                state.payload_inputs.clear();
                state.external_input_count = 0;
                state.external_input_bytes = 0;
            };
            try {
                if (payload_route) {
                    prepare_payload_inputs(*payload_store, id, state, states, root,
                                           options, state.attempt_deadline, external_sidecar, stream_v15);
                    stream_inputs_prepared = stream_v15;
                    if (std::chrono::steady_clock::now() >= state.attempt_deadline)
                        throw Error("ArtifactPayloadTimeout");
                }
            } catch (const Error& error) {
                discard_stream_inputs();
                state.result = make_failed(FailureCode::FAILED);
                const std::string_view message = error.what();
                state.result.result_code = message.starts_with("EXTERNAL_") ?
                    std::string(message) : payload_failure_name(message);
                state.terminal = true;
                run_failed = true;
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            } catch (...) {
                discard_stream_inputs();
                state.result = make_failed(FailureCode::FAILED);
                state.result.result_code = "ARTIFACT_REFERENCE_INVALID";
                state.terminal = true;
                run_failed = true;
                block_dependents(block_dependents, state.spec->node_id);
                continue;
            }
            observe_abort();
            if (host_cancelled) {
                discard_stream_inputs();
                mark_cancelled(state);
                continue;
            }
            auto request = make_request(id, state, states, root, options, payload_route);
            std::optional<artifact_v1::ExecuteHandoffAttemptV1> handoff_attempt;
            if (handoff) {
                handoff_attempt.emplace();
                handoff_attempt->consumer_node_id = state.spec->node_id;
                handoff_attempt->consumer_attempt_id = state.attempt_id;
                handoff_attempt->inputs.reserve(state.spec->dependencies.size());
                for (std::size_t input_index = 0;
                     input_index < state.spec->dependencies.size(); ++input_index) {
                    const auto& dependency = state.spec->dependencies[input_index];
                    const auto& producer = states.at(dependency);
                    const auto& input = request.input_artifact_refs.at(input_index + 1);
                    if (!producer.accepted || !producer.result.payload_digest ||
                        input.reference != producer.accepted_private_reference ||
                        input.sha256 != *producer.result.payload_digest ||
                        input.size_bytes != producer.result.payload_size)
                        throw Error("HANDOFF_RECORD_INVALID");
                    handoff_attempt->inputs.push_back({input_index, dependency,
                        producer.attempt_id, input.reference, input.sha256, input.size_bytes});
                }
            }
            std::optional<artifact_v1::PayloadFlowAttemptV2> flow_attempt;
            if (flow) {
                flow_attempt.emplace();
                flow_attempt->consumer_node_id = state.spec->node_id;
                flow_attempt->consumer_attempt_id = state.attempt_id;
                flow_attempt->inputs.reserve(state.spec->dependencies.size());
                for (std::size_t input_index = 0;
                     input_index < state.spec->dependencies.size(); ++input_index) {
                    const auto& dependency = state.spec->dependencies[input_index];
                    const auto& producer = states.at(dependency);
                    if (!producer.accepted || producer.accepted_manifest_id.empty() ||
                        !producer.result.payload_digest)
                        throw Error("ArtifactPayloadManifestInvalid");
                    flow_attempt->inputs.push_back({
                        input_index, dependency, producer.attempt_id,
                        producer.accepted_manifest_id, 0,
                        *producer.result.payload_digest, producer.result.payload_size});
                }
            }
            append("ATTEMPT_STARTED", "READY", state.attempt_id);
            if (flow) flow->attempts.push_back(std::move(*flow_attempt));
            if (handoff) handoff->attempts.push_back(std::move(*handoff_attempt));
            append("STATUS", "RUNNING", state.attempt_id);
            state.handle.emplace(start_attempt(request, {executable, root, options.budget, 1000}));
            ++state.attempts;
            if (!state.started_once) {
                state.started_once = true;
                ++started_nodes;
            }
        }

        observe_abort();
        bool any_live = false;
        for (auto& item : states) if (item.second.handle) {
            any_live = true;
            auto& state = item.second;
            if (payload_route && !state.deadline_cancelled &&
                std::chrono::steady_clock::now() >= state.attempt_deadline) {
                request_cancel(*state.handle);
                state.deadline_cancelled = true;
            }
            std::optional<WorkerAttemptOutcome> outcome;
            try { outcome = poll_attempt(*state.handle); }
            catch (...) {
                outcome = WorkerAttemptOutcome{make_failed(FailureCode::PROTOCOL_ERROR)};
            }
            if (!outcome) continue;
            state.handle.reset();
            state.pending_result = outcome->result;
            if (payload_route && state.deadline_cancelled) {
                state.pending_result = make_failed(FailureCode::TIMEOUT);
            }
        }

        while (receipt_cursor < receipt_order.size()) {
            auto& state = states.at(receipt_order[receipt_cursor]);
            if (state.terminal) {
                ++receipt_cursor;
                continue;
            }
            if (!state.pending_result) break;
            auto result = std::move(*state.pending_result);
            state.pending_result.reset();
            if (host_cancelled) result = make_failed(FailureCode::CANCELLED);
            const bool ready = is_ready(result);
            if (ready && (result.payload_size > parsed.spec.max_total_receipt_bytes ||
                          retained_bytes > parsed.spec.max_total_receipt_bytes ||
                          result.payload_size > parsed.spec.max_total_receipt_bytes - retained_bytes)) {
                result = make_failed(FailureCode::OUTPUT_LIMIT_EXCEEDED);
                run_failed = true;
            }
            state.result = result;
            const bool accepted = is_ready(result);
            const auto artifact = accepted ? result.output_artifact_ref : std::nullopt;
            append("PROCESS_EXITED", status_name(result.status), state.attempt_id, {}, result.usage);
            append("RESULT_RECEIVED", status_name(result.status), state.attempt_id, artifact, result.usage);
            state.history.push_back({state.attempt_id, result, false});
            if (accepted) {
                try {
                    if (payload_route && std::chrono::steady_clock::now() >= state.attempt_deadline)
                        throw Error("ArtifactPayloadTimeout");
                    if (payload_route)
                        state.accepted_manifest_id = publish_payload_output(*payload_store, id, state, root);
                    state.accepted = true;
                    state.terminal = true;
                    retained_bytes += result.payload_size;
                    state.history.back().accepted = true;
                    state.accepted_private_reference = "attempts/" + state.attempt_id + "/output/result.bin";
                    state.accepted_public_reference = "artifact/" + state.spec->node_id;
                    append("ARTIFACT_ACCEPTED", "READY", state.attempt_id, artifact, result.usage);
                    for (const auto& successor : successors[state.spec->node_id]) {
                        const auto& successor_spec = *states.at(successor).spec;
                        const bool dependencies_ready = std::all_of(successor_spec.dependencies.begin(),
                            successor_spec.dependencies.end(), [&](const auto& dependency) {
                                return states.at(dependency).accepted;
                            });
                        if (dependencies_ready) ready_nodes.insert(successor);
                    }
                } catch (const Error& error) {
                    state.result = make_failed(FailureCode::FAILED);
                    state.result.result_code = payload_route ?
                        payload_failure_name(error.what()) : "FAILED";
                    state.terminal = true;
                    run_failed = true;
                    state.history.back().accepted = false;
                    state.history.back().result = state.result;
                    block_dependents(block_dependents, state.spec->node_id);
                }
                ++receipt_cursor;
            } else if (!host_cancelled && retryable(result.failure_code) && state.attempts <= options.retry) {
                ready_nodes.insert(state.spec->node_id);
                break;
            } else {
                state.terminal = true;
                run_failed = true;
                block_dependents(block_dependents, state.spec->node_id);
                ++receipt_cursor;
            }
        }
        bool all_terminal = std::all_of(states.begin(), states.end(), [](const auto& item) {
            return item.second.terminal;
        });
        const bool any_pending = std::any_of(states.begin(), states.end(), [](const auto& item) {
            return item.second.pending_result.has_value();
        });
        if (all_terminal && !any_live && !any_pending) break;
        if (!any_live && ready_nodes.empty() && !any_pending) {
            for (auto& item : states) if (!item.second.terminal) {
                mark_blocked(item.second);
                run_failed = true;
            }
            break;
        }
        ::usleep(1000);
    }

    const bool success = !host_cancelled && !run_failed && std::all_of(states.begin(), states.end(),
        [](const auto& item) { return item.second.accepted; });
    for (auto& item : states) {
        if (!item.second.terminal) {
            if (host_cancelled) mark_cancelled(item.second);
            else mark_blocked(item.second);
        }
    }
    append("RUN_FINISHED", host_cancelled ? "CANCELLED" : (success ? "READY" : "FAILED"));
    std::string report;
    if (parsed.spec.nodes.size() == 1) {
        const auto& state = states.begin()->second;
        report = render_report(parsed, options, state.result, state.accepted_public_reference,
                               state.history, payload_route, external_inputs_digest,
                               state.external_input_count, state.external_input_bytes) + '\n';
    } else {
        report = render_multi_report(parsed, options, states, success, payload_route,
                                     external_inputs_digest, host_cancelled);
    }
    if (handoff) {
        try {
            LocalFiles::publish_new_atomically(bundle.string(), report);
            handoff->report_digest = sha256(report);
            const auto sidecar = artifact_v1::encode_execute_handoff_lineage_v1(*handoff);
            publish_handoff_sidecar_v1(bundle, sidecar);
        } catch (...) {
            throw Error("HANDOFF_RECORD_INVALID");
        }
    } else {
        LocalFiles::publish_new_atomically(bundle.string(), report);
    }
    if (flow) {
        try {
            flow->report_digest = sha256(report);
            const auto sidecar = artifact_v1::encode_payload_flow_v2(*flow);
            LocalFiles::publish_new_atomically(flow_sidecar_path.string(), sidecar);
        } catch (...) {
            throw Error("FLOW_RECORD_INVALID");
        }
    }
    std::cout << report;
    return success ? 0 : 1;
}

inline void validate_external_bindings(
        const dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3& parsed,
        const artifact_v1::ExternalInputsSidecarV14& sidecar) {
    std::set<std::string> object_keys;
    for (const auto& binding : sidecar.bindings) {
        const auto node_it = std::find_if(parsed.spec.nodes.begin(), parsed.spec.nodes.end(),
            [&](const auto& node) { return node.node_id == binding.node_id; });
        if (node_it == parsed.spec.nodes.end())
            throw Error("EXTERNAL_INPUT_BINDING_INVALID");
        if (node_it->dependencies.size() + binding.inputs.size() >
            artifact_v1::artifact_payload_external_input_limit_v14)
            throw Error("EXTERNAL_INPUT_BINDING_INVALID");
        for (const auto& input : binding.inputs) {
            const auto key = input.manifest_id + "#" + std::to_string(input.object_index);
            if (!object_keys.insert(key).second)
                throw Error("EXTERNAL_INPUT_BINDING_INVALID");
        }
    }
}

// V1.3 keeps the Store boundary explicit even while DAG edge publication and
// predecessor selection are completed by the next orchestration task.  The
// route therefore requires an existing, owner-only Store and then uses the
// same Bruce-supervised WorkerRequest path as V5 execute.  No Store path is
// ever handed to the child process.
inline int execute_payload(const PayloadRequest& request,
                           const std::filesystem::path& executable) {
    if (request.flow_v2 && request.external_inputs)
        throw Error("FLOW_V2_EXTERNAL_INPUTS_UNSUPPORTED");
    if (path_is_same_or_below(request.store, request.bundle) ||
        path_is_same_or_below(request.store,
            std::filesystem::path(request.bundle.string() + ".run")))
        throw Error("PROTOCOL_ERROR");
    // Validate the complete DAG and optional sidecar before opening the Store.
    // In particular, unknown predecessors, cycles, and sidecar structure cannot
    // cause any Store read/write.
    std::optional<artifact_v1::ExternalInputsFileV14> external_file;
    std::map<std::string, std::size_t> external_input_counts;
    if (request.external_inputs) {
        try {
            external_file = artifact_v1::read_external_inputs_v14(*request.external_inputs);
        } catch (const artifact_v1::ExternalInputsErrorV14& error) {
            throw Error(external_failure_name(error.code()));
        }
        for (const auto& binding : external_file->sidecar.bindings)
            external_input_counts.emplace(binding.node_id, binding.inputs.size());
    }
    const auto parsed = dag_scheduler_v3_cli_detail::parse_spec(request.spec,
        request.external_inputs ? &external_input_counts : nullptr);
    if (external_file) validate_external_bindings(parsed, external_file->sidecar);
    auto store = artifact_v1::ArtifactStoreV1::open_existing(request.store);
    return execute(request.spec, request.bundle, request.options, executable, true, &store,
                   external_file ? &external_file->sidecar : nullptr,
                   external_file ? std::string_view(external_file->digest) : std::string_view{},
                   request.external_inputs ? &external_input_counts : nullptr,
                   request.stream_v15, request.flow_v2);
}
} // namespace dag_scheduler_v5_cli_detail

inline int dag_scheduler_v5_cli(int argc, char** argv) {
    if (argc >= 3 && std::string_view(argv[2]) == "payload-inspect") {
        try {
            std::vector<std::string_view> args;
            for (int index = 2; index < argc; ++index) args.emplace_back(argv[index]);
            if (args.size() == 3 && args[2] == "--flow-v2")
                return dag_scheduler_v5_cli_detail::payload_inspect_v2(
                    dag_scheduler_v5_cli_detail::parse_payload_inspect_v2(std::move(args)));
            return dag_scheduler_v5_cli_detail::payload_inspect_v1(
                dag_scheduler_v5_cli_detail::parse_payload_inspect_v1(std::move(args)));
        } catch (...) {
            std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
            return 2;
        }
    }
    if (argc >= 3 && std::string_view(argv[2]) == "publish") {
        try {
            const auto request = dag_scheduler_v5_cli_detail::parse_publish(argc, argv);
            return dag_scheduler_v5_cli_detail::publish(request.bundle, request.store);
        } catch (...) {
            std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
            return 2;
        }
    }
    if (argc >= 3 && std::string_view(argv[2]) == "cancel") {
        try {
            const auto request = dag_scheduler_v5_cli_detail::parse_cancel(argc, argv);
            return dag_scheduler_v5_cli_detail::cancel(request.bundle);
        } catch (...) {
            std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
            return 2;
        }
    }
    if (argc >= 3 && std::string_view(argv[2]) == "inspect") {
        const bool mentions_handoff = std::any_of(argv + 3, argv + argc, [](const char* value) {
            return std::string_view(value) == "--handoff-v1";
        });
        if (mentions_handoff) {
            if (argc != 5 || std::string_view(argv[4]) != "--handoff-v1" ||
                std::string_view(argv[3]).empty() || std::string_view(argv[3]).starts_with('-') ||
                std::string_view(argv[3]).ends_with(".run")) {
                std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
                return 2;
            }
            return dag_scheduler_v5_cli_detail::inspect_handoff_v1(argv[3]);
        }
        if (argc != 4 || std::string_view(argv[3]).empty() ||
            std::string_view(argv[3]).starts_with('-') ||
            std::string_view(argv[3]).ends_with(".run")) {
            std::cerr << "{\"error\":" << json_string(dag_scheduler_v5_cli_detail::inspect_usage) << "}\n";
            return 2;
        }
        return dag_scheduler_v5_cli_detail::inspect(argv[3]);
    }
    if (argc >= 3 && std::string_view(argv[2]) == "payload") {
        try {
            const auto request = dag_scheduler_v5_cli_detail::parse_payload(argc, argv);
            return dag_scheduler_v5_cli_detail::execute_payload(request,
                std::filesystem::canonical("/proc/self/exe"));
        } catch (const Error& error) {
            std::cerr << "{\"error\":" << json_string(error.what()) << "}\n";
            return 2;
        } catch (...) {
            std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
            return 2;
        }
    }
    if (argc < 6 || std::string_view(argv[2]) != "execute" ||
        std::string_view(argv[3]).empty() || std::string_view(argv[3]).starts_with('-') ||
        std::string_view(argv[4]) != "--output" ||
        std::string_view(argv[5]).empty() || std::string_view(argv[5]).starts_with('-')) {
        std::cerr << "{\"error\":" << json_string(dag_scheduler_v5_cli_detail::usage) << "}\n";
        return 2;
    }
    std::vector<std::string> option_args;
    bool handoff_v1 = false;
    for (int i = 6; i < argc;) {
        if (std::string_view(argv[i]) == "--handoff-v1") {
            if (handoff_v1) {
                std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
                return 2;
            }
            handoff_v1 = true;
            ++i;
        } else {
            option_args.emplace_back(argv[i]);
            if (i + 1 < argc) {
                if (std::string_view(argv[i + 1]) == "--handoff-v1") {
                    std::cerr << "{\"error\":\"PROTOCOL_ERROR\"}\n";
                    return 2;
                }
                option_args.emplace_back(argv[i + 1]);
            }
            i += 2;
        }
    }
    try {
        if (handoff_v1) (void)dag_scheduler_v5_cli_detail::normalized_inspect_bundle_path(argv[5]);
        dag_scheduler_v5_cli_detail::Options options;
        try {
            options = dag_scheduler_v5_cli_detail::parse_options(option_args);
        } catch (...) {
            if (handoff_v1) throw Error("PROTOCOL_ERROR");
            throw;
        }
        return dag_scheduler_v5_cli_detail::execute(argv[3], argv[5], options,
            std::filesystem::canonical("/proc/self/exe"), false, nullptr, nullptr,
            {}, nullptr, false, false, handoff_v1);
    } catch (const Error& error) {
        std::cerr << "{\"error\":" << json_string(error.what()) << "}\n";
        return 2;
    }
}

} // namespace bruce
