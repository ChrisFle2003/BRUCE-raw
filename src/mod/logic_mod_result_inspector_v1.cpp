#include "bruce/mod/logic_mod_result_inspector_v1.hpp"
#include "bruce/core/json.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/mod/logic_mod_worker_v1.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bruce {
namespace {
[[noreturn]] void invalid_record_file() { throw Error("LogicModResultRecordInvalid"); }
struct RecordDescriptor {
    int fd;
    explicit RecordDescriptor(int value) : fd(value) { if (fd < 0) invalid_record_file(); }
    ~RecordDescriptor() { ::close(fd); }
    RecordDescriptor(const RecordDescriptor&) = delete;
    RecordDescriptor& operator=(const RecordDescriptor&) = delete;
};
std::string store_status(const Error& error) {
    const std::string_view code(error.what());
    if (code == "ArtifactManifestMissing" || code == "ArtifactObjectMissing") return "MISSING";
    if (code == "LogicModOutputReplayMismatch" || code == "ArtifactObjectCorrupt") return "IDENTITY_MISMATCH";
    // ArtifactStoreOpenFailed also covers permission/type/open failures. Do not
    // infer absence from errno or probe paths outside the existing Store APIs.
    return "STORE_ERROR";
}
std::string optional_json(const std::optional<std::string>& value) {
    return value ? json_string(*value) : "null";
}
}

std::string read_logic_mod_result_record_file_v1(const std::filesystem::path& path) {
    constexpr std::size_t limit = 2 * 65536;
    if (path.empty() || path.filename().empty()) invalid_record_file();
    std::vector<std::string> names;
    for (const auto& part : path) {
        const auto name = part.string();
        if (name == "/" || name == ".") continue;
        if (name.empty() || name == ".." || name.find('\0') != std::string::npos) invalid_record_file();
        names.push_back(name);
    }
    if (names.empty()) invalid_record_file();
    RecordDescriptor parent(::open(path.is_absolute() ? "/" : ".",
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    for (std::size_t i = 0; i + 1 < names.size(); ++i) {
        RecordDescriptor next(::openat(parent.fd, names[i].c_str(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        std::swap(parent.fd, next.fd);
    }
    RecordDescriptor file(::openat(parent.fd, names.back().c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY));
    struct stat info{};
    if (::fstat(file.fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_uid != ::geteuid() || (info.st_mode & 0077) != 0 || info.st_nlink != 1 ||
        info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > limit) invalid_record_file();
    // The opened descriptor pins the checked inode. Never reopen the path after
    // fstat, and never issue read() for a FIFO, device, directory or symlink.
    std::string bytes;
    bytes.reserve(static_cast<std::size_t>(info.st_size));
    std::array<char, 8192> buffer{};
    while (true) {
        const auto count = ::read(file.fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) invalid_record_file();
        if (count == 0) return bytes;
        const auto size = static_cast<std::size_t>(count);
        if (size > limit - bytes.size()) invalid_record_file();
        bytes.append(buffer.data(), size);
    }
}

LogicModResultInspectionV1 inspect_logic_mod_result_v1(
    std::string_view bytes, const std::filesystem::path& local_store) {
    LogicModRunV1 record;
    try { record = read_logic_mod_run_record_v1(bytes); }
    catch (const Error&) { return {}; }

    LogicModResultInspectionV1 report;
    report.accepted = record.accepted;
    report.input_present = record.input_present;
    report.output_present = record.output_present;
    report.binding_identity = sha256(record.binding_identity);
    if (!record.output_present) { report.status = "NO_OUTPUT"; return report; }
    report.output_manifest_id = record.output_manifest_id;
    report.output_sha256 = record.output_sha256;
    report.output_size_bytes = record.output_size_bytes;
    report.output_codec = record.output_codec;
    try {
        // The result verifier deliberately collapses non-READY metadata and
        // identity mismatch. Existing read-only APIs permit this one bounded
        // distinction without changing the verifier's replay error contract.
        {
            const auto store = artifact_v1::ArtifactStoreV1::open_existing(local_store);
            if (store.read_manifest(record.output_manifest_id).status != "READY") {
                report.status = "NOT_READY";
                return report;
            }
        }
        verify_logic_mod_result_artifact_v1({local_store, record.output_manifest_id},
            {record.output_manifest_id, record.output_object_index, record.output_sha256,
                record.output_size_bytes, record.output_codec}, record.result);
        report.status = "READY";
    } catch (const Error& error) { report.status = store_status(error); }
    catch (...) { report.status = "STORE_ERROR"; }
    return report;
}

std::string render_logic_mod_result_inspection_v1(const LogicModResultInspectionV1& report) {
    const auto boolean = [](bool value) { return value ? "true" : "false"; };
    const std::string_view readiness = report.status == "READY" ? "READY" :
        report.status == "NO_OUTPUT" ? "NO_OUTPUT" : "NOT_READY";
    return "{\"contract\":\"logic-mod-result-inspect-v1\",\"action\":\"inspect\",\"status\":" + json_string(report.status) +
        ",\"accepted\":" + boolean(report.accepted) + ",\"input_present\":" + boolean(report.input_present) +
        ",\"output_present\":" + boolean(report.output_present) + ",\"binding_identity\":" + optional_json(report.binding_identity) +
        ",\"output_manifest_id\":" + (report.output_present ? optional_json(report.output_manifest_id) : "null") +
        ",\"output_sha256\":" + (report.output_present ? optional_json(report.output_sha256) : "null") +
        ",\"output_size_bytes\":" + (report.output_present && report.output_size_bytes ? std::to_string(*report.output_size_bytes) : "null") +
        ",\"output_codec\":" + (report.output_present ? optional_json(report.output_codec) : "null") +
        ",\"replay_readiness\":" + json_string(readiness) + "}\n";
}
}
