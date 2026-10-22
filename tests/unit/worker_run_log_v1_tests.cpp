#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace bruce::worker_v1;
namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
template<class F> void rejects(F&& f, const char* message) {
    try { f(); } catch (const bruce::Error&) { return; }
    std::cerr << message << '\n'; std::exit(1);
}
std::string read(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
WorkerLogEvent event(std::uint64_t seq, std::string name = "STATUS") {
    WorkerLogEvent e; e.run_id = "run-1"; e.seq = seq; e.ts_unix_ms = 17;
    e.event = std::move(name); e.status = "RUNNING"; return e;
}
}
int main() {
    const auto base = std::filesystem::temp_directory_path() / "bruce-worker-run-log-v1-tests";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    auto log = WorkerRunLog::create(base / "run", "run-1");
    auto first = event(1); first.status = "READY";
    log.append(first);
    const auto path = base / "run/control/run-log.jsonl";
    const std::string expected = "{\"schema\":\"worker-run-log-v1\",\"run_id\":\"run-1\",\"seq\":1,\"ts_unix_ms\":17,\"event\":\"STATUS\",\"status\":\"READY\"}\n";
    require(read(path) == expected, "event rendering is not canonical JSONL");
    auto second = event(2, "RUN_FINISHED"); log.append(second);
    require(read(path).size() > expected.size() && read(path).back() == '\n', "append did not add one LF-terminated record");
    rejects([&] { log.append(event(2)); }, "duplicate sequence accepted");
    rejects([&] { log.append(event(3, "UNKNOWN")); }, "unknown event accepted");
    auto sensitive = event(3); sensitive.status = "secret=abc";
    rejects([&] { log.append(sensitive); }, "free-form secret status accepted");
    auto second_handle = WorkerRunLog::create(base / "run", "run-1");
    rejects([&] { second_handle.append(event(1)); }, "second handle duplicated the log sequence");
    second_handle.append(event(3));
    auto mismatch = event(4, "ARTIFACT_ACCEPTED");
    mismatch.reference = "artifacts/result.bin"; mismatch.size_bytes = 4; mismatch.digest = std::string(64, '0');
    std::filesystem::create_directories(base / "run/artifacts");
    { std::ofstream out(base / "run/artifacts/result.bin", std::ios::binary); out << "data"; }
    rejects([&] { log.append(mismatch); }, "digest/size mismatch accepted");
    auto outside = event(4, "ARTIFACT_ACCEPTED"); outside.reference = "../outside";
    rejects([&] { log.append(outside); }, "reference escaping run root accepted");

    const auto rejection_root = base / "rejection";
    auto rejection_log = WorkerRunLog::create(rejection_root, "run-1");
    std::uint64_t rejection_seq = 0;
    for (const auto* reason : {"OUTPUT_LIMIT_EXCEEDED", "TIMEOUT", "ARTIFACT_REFERENCE_INVALID",
                               "ARTIFACT_MANIFEST_INVALID", "ARTIFACT_DIGEST_MISMATCH",
                               "ARTIFACT_STORE_VIOLATION"}) {
        auto rejected = event(++rejection_seq, "RESULT_REJECTED");
        rejected.attempt_id = "a-1"; rejected.status = reason;
        rejection_log.append(rejected);
    }
    auto invalid_rejection = event(rejection_seq + 1, "RESULT_REJECTED");
    invalid_rejection.attempt_id = "a-1"; invalid_rejection.status = "UNKNOWN_REASON";
    rejects([&] { rejection_log.append(invalid_rejection); }, "unknown rejection reason accepted by writer");
    invalid_rejection.status = "TIMEOUT";
    invalid_rejection.usage = WorkerUsage{1, 2, 3, 4, 5};
    rejects([&] { rejection_log.append(invalid_rejection); }, "usage-bearing rejection accepted by writer");
    invalid_rejection.usage.reset(); invalid_rejection.attempt_id.reset();
    rejects([&] { rejection_log.append(invalid_rejection); }, "run-scoped rejection accepted by writer");
    invalid_rejection.attempt_id = "a-1"; invalid_rejection.reference = "artifacts/result.bin";
    invalid_rejection.digest = bruce::sha256("data"); invalid_rejection.size_bytes = 4;
    rejects([&] { rejection_log.append(invalid_rejection); }, "artifact-bearing rejection accepted by writer");

    const auto semantic_root = base / "semantic";
    auto semantic_log = WorkerRunLog::create(semantic_root, "run-1");
    semantic_log.append(event(1));
    auto semantic_data = read(semantic_root / "control/run-log.jsonl");
    auto run_id_pos = semantic_data.find("\"run_id\":\"run-1\"");
    semantic_data.replace(run_id_pos, std::string("\"run_id\":\"run-1\"").size(), "\"run_id\":\"run-x\"");
    { std::ofstream out(semantic_root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << semantic_data; }
    rejects([&] { (void)WorkerRunLog::create(semantic_root, "run-1"); }, "historical wrong run_id accepted");

    const auto artifact_root = base / "artifact-semantic";
    std::filesystem::create_directories(artifact_root / "artifacts");
    { std::ofstream out(artifact_root / "artifacts/data.bin", std::ios::binary); out << "data"; }
    auto artifact_log = WorkerRunLog::create(artifact_root, "run-1");
    auto artifact_event = event(1, "ARTIFACT_ACCEPTED");
    artifact_event.reference = "artifacts/data.bin"; artifact_event.size_bytes = 4;
    artifact_event.digest = bruce::sha256("data");
    artifact_log.append(artifact_event);
    auto artifact_data = read(artifact_root / "control/run-log.jsonl");
    const auto size_pos = artifact_data.find("\"size_bytes\":4");
    artifact_data.replace(size_pos, std::string("\"size_bytes\":4").size(), "\"size_bytes\":5");
    { std::ofstream out(artifact_root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << artifact_data; }
    rejects([&] { (void)WorkerRunLog::create(artifact_root, "run-1"); }, "historical size/reference contradiction accepted");

    const auto malformed_root = base / "malformed";
    auto malformed_log = WorkerRunLog::create(malformed_root, "run-1");
    malformed_log.append(event(1));
    auto malformed_data = read(malformed_root / "control/run-log.jsonl");
    const auto status_pos = malformed_data.find("\"status\":\"RUNNING\"");
    malformed_data.replace(status_pos, std::string("\"status\":\"RUNNING\"").size(), "\"status\":17");
    { std::ofstream out(malformed_root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << malformed_data; }
    rejects([&] { (void)WorkerRunLog::create(malformed_root, "run-1"); }, "historical wrong status type accepted");

    const auto symlink_root = base / "symlink-run";
    const auto external_log = base / "outside.jsonl";
    std::filesystem::create_directories(symlink_root / "control");
    { std::ofstream out(external_log); out << "outside\n"; }
    std::filesystem::create_symlink(external_log, symlink_root / "control/run-log.jsonl");
    rejects([&] { (void)WorkerRunLog::create(symlink_root, "run-1"); }, "symlink log accepted at create");

    const auto append_symlink_root = base / "append-symlink-run";
    auto append_symlink_log = WorkerRunLog::create(append_symlink_root, "run-1");
    const auto outside_append = base / "outside-append.jsonl";
    { std::ofstream out(outside_append); out << "untouched\n"; }
    std::filesystem::remove(append_symlink_root / "control/run-log.jsonl");
    std::filesystem::create_symlink(outside_append, append_symlink_root / "control/run-log.jsonl");
    rejects([&] { append_symlink_log.append(event(1)); }, "symlink log accepted at append");
    require(read(outside_append) == "untouched\n", "append modified a file outside the run tree");

    const auto malformed_line_root = base / "malformed-line";
    auto malformed_line_log = WorkerRunLog::create(malformed_line_root, "run-1");
    malformed_line_log.append(event(1));
    { std::ofstream out(malformed_line_root / "control/run-log.jsonl", std::ios::binary | std::ios::app); out << "{broken"; }
    rejects([&] { (void)WorkerRunLog::create(malformed_line_root, "run-1"); }, "malformed existing line accepted");
    std::filesystem::remove_all(base);
    std::cout << "worker run log v1 contracts passed\n";
}
