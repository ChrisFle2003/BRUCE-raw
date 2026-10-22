#include "bruce/artifact/execute_handoff_inspector_v1.hpp"
#include "bruce/artifact/execute_handoff_lineage_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/worker/worker_run_inspector_v1.hpp"
#include "../../apps/dag_scheduler_v5_cli.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <set>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce::artifact_v1 {
namespace {
namespace detail = bruce::dag_scheduler_v5_cli_detail;

[[noreturn]] void fail() { throw Error("PROTOCOL_ERROR"); }
void ensure(bool condition) { if (!condition) fail(); }

struct Fd {
    int value{-1};
    explicit Fd(int source) : value(source) { ensure(value >= 0); }
    ~Fd() { if (value >= 0) ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

struct DirectoryFds {
    std::vector<int> values;
    ~DirectoryFds() { for (const int fd : values) ::close(fd); }
    DirectoryFds(const DirectoryFds&) = delete;
    DirectoryFds& operator=(const DirectoryFds&) = delete;
    DirectoryFds() = default;
};

bool same_stat(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
        a.st_mode == b.st_mode && a.st_uid == b.st_uid &&
        a.st_nlink == b.st_nlink && a.st_size == b.st_size &&
        a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

// All components are opened relative to an already-open directory. The run
// subtree is private; ancestors may be ordinary user-selected directories.
std::string read_file(const std::filesystem::path& path,
                      const std::filesystem::path& run_root,
                      bool exact_private_mode = false) {
    ensure(path.is_absolute());
    Fd root(::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    int parent = root.value;
    DirectoryFds directories;
    bool private_subtree = false;
    std::filesystem::path opened_path = "/";
    const auto components = path.relative_path();
    for (auto it = components.begin(); it != components.end(); ++it) {
        const auto name = it->string();
        ensure(!name.empty() && name != "." && name != "..");
        auto next = it; ++next;
        if (next == components.end()) {
            const int fd = ::openat(parent, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
            Fd file(fd);
            struct stat before{}, after{}, entry{};
            ensure(::fstat(fd, &before) == 0 && S_ISREG(before.st_mode) &&
                before.st_uid == ::geteuid() && before.st_nlink == 1 &&
                (before.st_mode & 0077) == 0 && before.st_size >= 0 &&
                static_cast<std::uint64_t>(before.st_size) <= max_file_bytes);
            if (exact_private_mode) ensure((before.st_mode & 0777) == 0600);
            std::string bytes(static_cast<std::size_t>(before.st_size), '\0');
            std::size_t offset{};
            while (offset < bytes.size()) {
                const auto n = ::read(fd, bytes.data() + offset, bytes.size() - offset);
                if (n < 0 && errno == EINTR) continue;
                ensure(n > 0);
                offset += static_cast<std::size_t>(n);
            }
            ensure(::fstat(fd, &after) == 0 && same_stat(before, after) &&
                ::fstatat(parent, name.c_str(), &entry, AT_SYMLINK_NOFOLLOW) == 0 &&
                same_stat(before, entry));
            return bytes;
        }
        const int directory = ::openat(parent, name.c_str(),
            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ensure(directory >= 0);
        directories.values.push_back(directory);
        struct stat st{};
        ensure(::fstat(directory, &st) == 0 && S_ISDIR(st.st_mode));
        opened_path /= name;
        if (opened_path == run_root) private_subtree = true;
        if (private_subtree) ensure(st.st_uid == ::geteuid() && (st.st_mode & 0777) == 0700);
        parent = directory;
    }
    fail();
}

using Json = detail::InspectJsonValue;
const Json& field(const Json& object, std::string_view key) { return detail::inspect_field(object, key); }
std::string string_field(const Json& object, std::string_view key) {
    return detail::inspect_string(field(object, key));
}

struct ReportNode {
    std::string callback;
    std::string request_digest;
};

std::map<std::string, ReportNode> report_nodes(const Json& report) {
    std::map<std::string, ReportNode> nodes;
    const auto& receipts = field(report, "receipts");
    ensure(receipts.kind == Json::Kind::array);
    for (const auto& receipt : receipts.array) {
        const auto id = string_field(receipt, "node_id");
        ensure(nodes.emplace(id, ReportNode{string_field(receipt, "callback_id"),
            string_field(receipt, "request_digest")}).second);
    }
    return nodes;
}

std::map<std::string, ReportNode> single_report_node(const Json& report) {
    const auto id = string_field(report, "node_id");
    return {{id, {string_field(report, "adapter_id"), string_field(report, "request_digest")}}};
}

std::map<std::string, std::string> attempt_nodes(const Json& report, bool single) {
    std::map<std::string, std::string> result;
    if (single) {
        const auto node = string_field(report, "node_id");
        const auto& attempts = field(report, "attempts");
        ensure(attempts.kind == Json::Kind::array);
        for (const auto& attempt : attempts.array)
            ensure(result.emplace(string_field(attempt, "attempt_id"), node).second);
    } else {
        const auto& receipts = field(report, "receipts");
        ensure(receipts.kind == Json::Kind::array);
        for (const auto& receipt : receipts.array) {
            const auto node = string_field(receipt, "node_id");
            const auto& attempts = field(receipt, "attempts");
            ensure(attempts.kind == Json::Kind::array);
            for (const auto& attempt : attempts.array)
                ensure(result.emplace(string_field(attempt, "attempt_id"), node).second);
        }
    }
    return result;
}

struct LogEvidence {
    std::map<std::string, std::set<std::string>> accepted_before_start;
    bool cancel_requested{};
};

LogEvidence inspect_log_evidence(std::string_view bytes) {
    LogEvidence result;
    std::set<std::string> accepted;
    std::size_t begin{};
    while (begin < bytes.size()) {
        const auto end = bytes.find('\n', begin);
        ensure(end != std::string_view::npos && end > begin);
        const auto event = detail::InspectJsonParser(bytes.substr(begin, end - begin)).parse();
        const auto kind = string_field(event, "event");
        if (kind == "ATTEMPT_STARTED") {
            const auto id = string_field(event, "attempt_id");
            ensure(result.accepted_before_start.emplace(id, accepted).second);
        } else if (kind == "ARTIFACT_ACCEPTED") {
            accepted.insert(string_field(event, "attempt_id"));
        } else if (kind == "CANCEL_REQUESTED") {
            result.cancel_requested = true;
        }
        begin = end + 1;
    }
    return result;
}
} // namespace

ExecuteHandoffInspectionV1 inspect_execute_handoff_v1(const std::filesystem::path& bundle) {
    const auto absolute = detail::normalized_inspect_bundle_path(bundle);
    const auto run_root = std::filesystem::path(absolute.string() + ".run");
    const auto report_bytes = read_file(absolute, run_root);
    const auto sidecar_bytes = read_file(
        std::filesystem::path(absolute.string() + ".handoff-v1.json"), run_root, true);
    const auto log_path = run_root / "control/run-log.jsonl";
    const auto log_bytes = read_file(log_path, run_root);
    const auto lineage = decode_execute_handoff_lineage_v1(sidecar_bytes);
    const auto run = worker_v1::inspect_worker_run_v1(run_root);
    const auto validated = detail::inspect_execute_report(report_bytes, run_root, run,
        [&](std::string_view node_id, std::string_view digest) {
            ensure(sha256(read_file(run_root / "inputs" / (std::string(node_id) + ".bin"), run_root)) == digest);
        });
    ensure(log_bytes == read_file(log_path, run_root));
    ensure(lineage.report_digest == sha256(report_bytes) && lineage.run_id == run.run_id &&
        validated.run_id == run.run_id && lineage.attempts.size() == run.attempts.size() &&
        lineage.nodes.size() == validated.expected_node_count);

    ensure(!report_bytes.empty() && report_bytes.back() == '\n');
    const auto report = detail::InspectJsonParser(std::string_view(report_bytes).substr(0, report_bytes.size() - 1)).parse();
    ensure(lineage.spec_digest == string_field(report, "spec_digest"));
    const bool single = lineage.nodes.size() == 1;
    const auto nodes = single ? single_report_node(report) : report_nodes(report);
    const auto owners = attempt_nodes(report, single);
    const auto evidence = inspect_log_evidence(log_bytes);
    ensure(nodes.size() == lineage.nodes.size() && owners.size() == lineage.attempts.size() &&
        evidence.accepted_before_start.size() == lineage.attempts.size());

    std::map<std::string, std::vector<std::string>> dependencies;
    for (const auto& node : lineage.nodes) {
        const auto found = nodes.find(node.node_id);
        ensure(found != nodes.end() && found->second.callback == node.callback_id &&
            found->second.request_digest == node.request_digest &&
            dependencies.emplace(node.node_id, node.dependencies).second);
        ensure(sha256(read_file(run_root / "inputs" / (node.node_id + ".bin"), run_root)) == node.request_digest);
    }
    for (const auto& node : lineage.nodes) {
        std::set<std::string> seen_dependencies;
        for (const auto& dependency : node.dependencies)
            ensure(dependencies.contains(dependency) && dependency != node.node_id &&
                seen_dependencies.insert(dependency).second);
    }

    std::map<std::string, detail::ValidatedV5ArtifactImportV1> accepted;
    for (const auto& artifact : validated.artifacts)
        ensure(accepted.emplace(artifact.node_id, artifact).second);
    if (run.run_finished) {
        const auto abort = worker_v1::probe_abort(run_root);
        ensure(abort != worker_v1::AbortProbeResult::INVALID);
        const bool host_cancelled = abort == worker_v1::AbortProbeResult::REQUESTED;
        ensure((run.status == "CANCELLED") == host_cancelled);
        if (evidence.cancel_requested) ensure(host_cancelled);
        std::vector<std::string> receipt_statuses;
        if (single) {
            receipt_statuses.push_back(string_field(report, "status"));
        } else {
            const auto& receipts = field(report, "receipts");
            ensure(receipts.kind == Json::Kind::array && receipts.array.size() == lineage.nodes.size());
            for (const auto& receipt : receipts.array)
                receipt_statuses.push_back(string_field(receipt, "status"));
        }
        const auto has_status = [&](std::string_view status) {
            return std::find(receipt_statuses.begin(), receipt_statuses.end(), status) != receipt_statuses.end();
        };
        const bool all_ready = std::all_of(receipt_statuses.begin(), receipt_statuses.end(),
            [](const auto& status) { return status == "READY"; });
        if (run.status == "READY") {
            ensure(all_ready && accepted.size() == lineage.nodes.size());
        } else if (run.status == "FAILED") {
            ensure(!all_ready && (has_status("FAILED") || has_status("BLOCKED")) &&
                !has_status("CANCELLED"));
        } else if (run.status == "BLOCKED") {
            ensure(accepted.empty() && std::all_of(receipt_statuses.begin(), receipt_statuses.end(),
                [](const auto& status) { return status == "BLOCKED"; }));
        } else if (run.status == "CANCELLED") {
            ensure((all_ready || has_status("CANCELLED")) &&
                (!evidence.cancel_requested || has_status("CANCELLED")));
        } else {
            fail();
        }
    }
    ExecuteHandoffInspectionV1 result;
    result.status = run.run_finished ? "VALID" : "INCOMPLETE";
    result.run_status = run.run_finished ? run.status : "INCOMPLETE";
    result.run_id = run.run_id;
    result.node_count = lineage.nodes.size();
    result.attempt_count = lineage.attempts.size();
    for (std::size_t index{}; index < lineage.attempts.size(); ++index) {
        const auto& attempt = lineage.attempts[index];
        ensure(attempt.consumer_attempt_id == run.attempts[index].attempt_id &&
            owners.at(attempt.consumer_attempt_id) == attempt.consumer_node_id);
        const auto dep = dependencies.find(attempt.consumer_node_id);
        ensure(dep != dependencies.end() && attempt.inputs.size() == dep->second.size());
        for (std::size_t input_index{}; input_index < attempt.inputs.size(); ++input_index) {
            const auto& input = attempt.inputs[input_index];
            ensure(input.input_index == input_index &&
                input.producer_node_id == dep->second[input_index]);
            const auto producer = accepted.find(input.producer_node_id);
            ensure(producer != accepted.end() &&
                evidence.accepted_before_start.at(attempt.consumer_attempt_id).contains(input.producer_attempt_id) &&
                producer->second.attempt_id == input.producer_attempt_id &&
                producer->second.source_reference == input.reference &&
                producer->second.digest == input.sha256 &&
                producer->second.size_bytes == input.size_bytes);
            const auto bytes = read_file(run_root / input.reference, run_root);
            ensure(bytes.size() == input.size_bytes && sha256(bytes) == input.sha256 &&
                result.verified_input_bytes <= UINT64_MAX - input.size_bytes);
            result.verified_input_bytes += input.size_bytes;
            ++result.binding_count;
        }
    }
    return result;
}
} // namespace bruce::artifact_v1
