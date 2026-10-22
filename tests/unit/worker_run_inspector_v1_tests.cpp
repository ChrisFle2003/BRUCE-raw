#include "bruce/worker/worker_run_inspector_v1.hpp"
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
    try { f(); } catch (const bruce::Error& error) {
        require(std::string(error.what()) == "PROTOCOL_ERROR", message);
        return;
    }
    std::cerr << message << '\n'; std::exit(1);
}
WorkerLogEvent event(std::uint64_t seq, std::string name, std::string status,
                     std::string attempt = {}) {
    WorkerLogEvent e; e.run_id = "run-1"; e.seq = seq; e.ts_unix_ms = 17;
    e.event = std::move(name); e.status = std::move(status);
    if (!attempt.empty()) e.attempt_id = std::move(attempt);
    return e;
}
void make_layout(const std::filesystem::path& root, std::string_view attempt = "a-1") {
    std::filesystem::create_directories(root / "inputs");
    std::filesystem::create_directories(root / "attempts" / std::string(attempt) / "output");
    std::error_code ec;
    std::filesystem::permissions(root, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    std::filesystem::permissions(root / "inputs", std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    std::filesystem::permissions(root / "attempts", std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    std::filesystem::permissions(root / "attempts" / std::string(attempt),
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    std::filesystem::permissions(root / "attempts" / std::string(attempt) / "output",
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
}
void write_artifact(const std::filesystem::path& root, std::string_view attempt, std::string_view bytes) {
    std::ofstream out(root / "attempts" / std::string(attempt) / "output/result.bin",
                      std::ios::binary);
    out << bytes;
    std::error_code ec;
    std::filesystem::permissions(root / "attempts" / std::string(attempt) / "output/result.bin",
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void append_success(WorkerRunLog& log, std::string_view attempt, std::uint64_t& seq,
                    std::string_view digest, std::uint64_t size) {
    log.append(event(++seq, "ATTEMPT_STARTED", "READY", std::string(attempt)));
    log.append(event(++seq, "STATUS", "RUNNING", std::string(attempt)));
    log.append(event(++seq, "PROCESS_EXITED", "READY", std::string(attempt)));
    auto result = event(++seq, "RESULT_RECEIVED", "READY", std::string(attempt));
    result.digest = std::string(digest); result.size_bytes = size;
    result.reference = "attempts/" + std::string(attempt) + "/output/result.bin";
    log.append(result);
    auto accepted = event(++seq, "ARTIFACT_ACCEPTED", "READY", std::string(attempt));
    accepted.digest = std::string(digest); accepted.size_bytes = size;
    accepted.reference = "attempts/" + std::string(attempt) + "/output/result.bin";
    accepted.usage = WorkerUsage{3, 4, size, 1, 8};
    log.append(accepted);
}

void stage3d_contracts(const std::filesystem::path& base) {
    // Removing a typed reason, retaining metadata, or allowing a second
    // decision must make these real JSONL inspection checks fail.
    for (const auto* reason : {"OUTPUT_LIMIT_EXCEEDED", "TIMEOUT", "ARTIFACT_REFERENCE_INVALID",
                               "ARTIFACT_MANIFEST_INVALID", "ARTIFACT_DIGEST_MISMATCH",
                               "ARTIFACT_STORE_VIOLATION"}) {
        const auto root = base / (std::string("typed-rejection-") + reason);
        make_layout(root);
        write_artifact(root, "a-1", "data");
        auto log = WorkerRunLog::create(root, "run-1");
        log.append(event(1, "RUN_CREATED", "READY"));
        log.append(event(2, "ATTEMPT_STARTED", "RUNNING", "a-1"));
        log.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
        auto result = event(4, "RESULT_RECEIVED", "READY", "a-1");
        result.reference = "attempts/a-1/output/result.bin";
        result.digest = bruce::sha256("data"); result.size_bytes = 4;
        result.usage = WorkerUsage{9, 7, 4, 3, 100};
        log.append(result);
        log.append(event(5, "RESULT_REJECTED", reason, "a-1"));
        const auto pending = read_file(root / "control/run-log.jsonl");
        log.append(event(6, "RUN_FINISHED", "FAILED"));
        const auto inspection = inspect_worker_run_v1(root);
        const auto& attempt = inspection.attempts.front();
        require(inspection.status == "FAILED" && attempt.statuses.back() == "FAILED" &&
                !attempt.accepted && !attempt.artifact_verified && !attempt.digest &&
                !attempt.size_bytes && !attempt.artifact_reference && attempt.usage &&
                attempt.usage->wall_ms == 9 && attempt.usage->input_bytes == 7 &&
                attempt.usage->output_bytes == 4 && attempt.usage->operations == 3 &&
                attempt.usage->peak_memory_bytes == 100,
                "typed rejection projection lost usage or retained artifact metadata");
        const auto original = read_file(root / "control/run-log.jsonl");
        const auto rewrite = [&](std::string text) {
            std::ofstream out(root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc);
            out << text;
        };
        for (const auto* mutation : {"unknown", "metadata", "usage", "duplicate", "accept", "cancel", "restart", "cancel-restart"}) {
            std::string text = pending;
            if (std::string_view(mutation) == "unknown") {
                const auto at = text.find(std::string("\"status\":\"") + reason, text.find("RESULT_REJECTED"));
                text.replace(at + 10, std::string(reason).size(), "UNKNOWN_REASON");
            } else if (std::string_view(mutation) == "metadata") {
                text.insert(text.size() - 2, ",\"digest\":\"" + bruce::sha256("data") +
                    "\",\"reference\":\"attempts/a-1/output/result.bin\",\"size_bytes\":4");
            } else if (std::string_view(mutation) == "usage") {
                text.insert(text.size() - 2,
                    ",\"usage\":{\"wall_ms\":9,\"input_bytes\":7,\"output_bytes\":4,\"operations\":3,\"peak_memory_bytes\":100}");
            } else {
                rewrite(text);
                auto extra = WorkerRunLog::create(root, "run-1");
                if (std::string_view(mutation) == "duplicate") extra.append(event(6, "RESULT_REJECTED", reason, "a-1"));
                if (std::string_view(mutation) == "accept") {
                    auto accepted = result; accepted.seq = 6; accepted.event = "ARTIFACT_ACCEPTED";
                    extra.append(accepted);
                }
                if (std::string_view(mutation) == "cancel") {
                    extra.append(event(6, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
                    extra.append(event(7, "RUN_FINISHED", "CANCELLED"));
                }
                if (std::string_view(mutation) == "cancel-restart")
                    extra.append(event(6, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
                if (std::string_view(mutation) == "restart" || std::string_view(mutation) == "cancel-restart") {
                    make_layout(root, "a-1-2"); write_artifact(root, "a-1-2", "next");
                    std::uint64_t seq = std::string_view(mutation) == "cancel-restart" ? 6 : 5;
                    append_success(extra, "a-1-2", seq, bruce::sha256("next"), 4);
                    extra.append(event(++seq, "RUN_FINISHED", "READY"));
                }
                text = read_file(root / "control/run-log.jsonl");
            }
            rewrite(text);
            const bool timeout = std::string_view(reason) == "TIMEOUT";
            if (timeout && (std::string_view(mutation) == "cancel" || std::string_view(mutation) == "restart")) {
                const auto retried = inspect_worker_run_v1(root);
                require(retried.attempts.front().statuses.back() ==
                        (std::string_view(mutation) == "cancel" ? "CANCELLED" : "FAILED"),
                        "TIMEOUT pending retry/cancel projection invalid");
                if (std::string_view(mutation) == "restart")
                    require(retried.status == "READY" && retried.attempts.size() == 2 && retried.attempts.back().accepted,
                            "TIMEOUT history prevented later successful retry");
            } else {
                rejects([&] { (void)inspect_worker_run_v1(root); }, "invalid post-result rejection decision accepted");
            }
            rewrite(original);
        }
    }
    for (const auto* scenario : {"before-result", "failed-result", "duplicate-cancel", "cancel-then-reject"}) {
        const auto root = base / (std::string("invalid-decision-") + scenario);
        make_layout(root); write_artifact(root, "a-1", "data");
        auto log = WorkerRunLog::create(root, "run-1");
        log.append(event(1, "RUN_CREATED", "READY"));
        log.append(event(2, "ATTEMPT_STARTED", "RUNNING", "a-1"));
        std::uint64_t seq = 2;
        if (std::string_view(scenario) != "before-result") {
            const auto* status = std::string_view(scenario) == "failed-result" ? "FAILED" : "READY";
            log.append(event(++seq, "PROCESS_EXITED", status, "a-1"));
            auto result = event(++seq, "RESULT_RECEIVED", status, "a-1");
            if (std::string_view(status) == "READY") {
                result.reference = "attempts/a-1/output/result.bin";
                result.digest = bruce::sha256("data"); result.size_bytes = 4;
            }
            log.append(result);
        }
        if (std::string_view(scenario) == "duplicate-cancel" || std::string_view(scenario) == "cancel-then-reject")
            log.append(event(++seq, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
        log.append(event(++seq, std::string_view(scenario) == "duplicate-cancel" ? "CANCEL_REQUESTED" : "RESULT_REJECTED",
                         std::string_view(scenario) == "duplicate-cancel" ? "CANCELLED" : "TIMEOUT", "a-1"));
        rejects([&] { (void)inspect_worker_run_v1(root); }, "pre-result/failed-result/cancel decision accepted");
    }
    // Stage3A single-worker hosts include measured usage in RUN_FINISHED.
    // Preserve that established record shape when real attempts exist.
    for (const auto* status : {"READY", "FAILED", "CANCELLED"}) {
        const auto root = base / (std::string("legacy-finish-usage-") + status);
        make_layout(root); write_artifact(root, "a-1", "data");
        auto log = WorkerRunLog::create(root, "run-1");
        std::uint64_t seq = 1;
        log.append(event(seq, "RUN_CREATED", "READY"));
        if (std::string_view(status) == "READY") {
            append_success(log, "a-1", seq, bruce::sha256("data"), 4);
        } else {
            log.append(event(++seq, "ATTEMPT_STARTED", "RUNNING", "a-1"));
            log.append(event(++seq, "PROCESS_EXITED", status, "a-1"));
            auto result = event(++seq, "RESULT_RECEIVED", status, "a-1");
            result.usage = WorkerUsage{3, 4, 4, 1, 8};
            log.append(result);
        }
        auto finished = event(++seq, "RUN_FINISHED", status);
        finished.usage = WorkerUsage{3, 4, 4, 1, 8};
        log.append(finished);
        const auto inspected = inspect_worker_run_v1(root);
        require(inspected.run_finished && inspected.status == status && inspected.attempts.size() == 1 &&
                inspected.attempts.front().statuses.back() == status,
                "legacy RUN_FINISHED measured usage was rejected");
    }
    for (const auto* status : {"FAILED", "CANCELLED", "READY", "RUNNING", "TIMEOUT"}) {
        const auto root = base / (std::string("zero-attempt-") + status);
        make_layout(root);
        auto log = WorkerRunLog::create(root, "run-1");
        log.append(event(1, "RUN_CREATED", "READY"));
        log.append(event(2, "RUN_FINISHED", status));
        if (std::string_view(status) == "FAILED" || std::string_view(status) == "CANCELLED") {
            const auto inspected = inspect_worker_run_v1(root);
            require(inspected.run_finished && inspected.status == status && inspected.attempts.empty(),
                    "scheduler failure/cancel invented an attempt");
            auto text = read_file(root / "control/run-log.jsonl");
            text.insert(text.size() - 2, ",\"usage\":{\"wall_ms\":0,\"input_bytes\":0,\"output_bytes\":0,\"operations\":0,\"peak_memory_bytes\":0}");
            { std::ofstream out(root / "control/run-log.jsonl"); out << text; }
            rejects([&] { (void)inspect_worker_run_v1(root); }, "metadata-bearing zero-attempt completion accepted");
        } else {
            rejects([&] { (void)inspect_worker_run_v1(root); }, "malformed zero-attempt completion accepted");
        }
    }
}
}

int main() {
    const auto base = std::filesystem::temp_directory_path() / "bruce-worker-run-inspector-v1-tests";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    stage3d_contracts(base);

    const auto root = base / "complete.run";
    make_layout(root);
    write_artifact(root, "a-1", "data");
    auto log = WorkerRunLog::create(root, "run-1");
    std::uint64_t seq{};
    log.append(event(++seq, "RUN_CREATED", "READY"));
    append_success(log, "a-1", seq, bruce::sha256("data"), 4);
    log.append(event(++seq, "RUN_FINISHED", "READY"));
    const auto before = std::filesystem::file_size(root / "control/run-log.jsonl");
    const auto inspected = inspect_worker_run_v1(root);
    require(inspected.run_id == "run-1" && inspected.status == "READY", "complete run metadata mismatch");
    require(inspected.run_finished && inspected.last_seq == seq, "complete run state mismatch");
    require(inspected.attempts.size() == 1 && inspected.attempts.front().accepted,
            "accepted attempt missing");
    require(inspected.attempts.front().artifact_verified && inspected.attempts.front().digest.has_value(),
            "artifact verification metadata missing");
    require(std::filesystem::file_size(root / "control/run-log.jsonl") == before,
            "inspection modified the log");

    for (const auto* result_status : {"READY", "FAILED"}) {
        const auto cancelled_root = base / (std::string("post-result-cancel-") + result_status);
        make_layout(cancelled_root);
        write_artifact(cancelled_root, "a-1", "data");
        auto cancelled_log = WorkerRunLog::create(cancelled_root, "run-1");
        cancelled_log.append(event(1, "RUN_CREATED", "READY"));
        cancelled_log.append(event(2, "ATTEMPT_STARTED", "RUNNING", "a-1"));
        cancelled_log.append(event(3, "PROCESS_EXITED", result_status, "a-1"));
        auto pending_result = event(4, "RESULT_RECEIVED", result_status, "a-1");
        if (std::string_view(result_status) == "READY") {
            pending_result.reference = "attempts/a-1/output/result.bin";
            pending_result.size_bytes = 4;
            pending_result.digest = bruce::sha256("data");
        }
        cancelled_log.append(pending_result);
        cancelled_log.append(event(5, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
        cancelled_log.append(event(6, "RUN_FINISHED", "CANCELLED"));
        const auto cancelled = inspect_worker_run_v1(cancelled_root);
        require(cancelled.status == "CANCELLED" && cancelled.attempts.size() == 1 &&
                cancelled.attempts.front().statuses.back() == "CANCELLED" &&
                !cancelled.attempts.front().accepted && !cancelled.attempts.front().artifact_verified,
                "post-result cancellation did not decide the pending attempt");
        auto text = read_file(cancelled_root / "control/run-log.jsonl");
        const auto original = text;
        const auto cancel_at = text.find("\"event\":\"CANCEL_REQUESTED\",\"status\":\"CANCELLED\"");
        require(cancel_at != std::string::npos, "post-result cancel missing");
        text.replace(cancel_at, std::string("\"event\":\"CANCEL_REQUESTED\",\"status\":\"CANCELLED\"").size(),
                     "\"event\":\"CANCEL_REQUESTED\",\"status\":\"FAILED\"");
        { std::ofstream out(cancelled_root / "control/run-log.jsonl"); out << text; }
        rejects([&] { (void)inspect_worker_run_v1(cancelled_root); }, "non-CANCELLED post-result cancel accepted");
        { std::ofstream out(cancelled_root / "control/run-log.jsonl"); out << original; }
    }

    const auto accepted_cancel_root = base / "accepted-cancel.run";
    make_layout(accepted_cancel_root);
    write_artifact(accepted_cancel_root, "a-1", "data");
    auto accepted_cancel = WorkerRunLog::create(accepted_cancel_root, "run-1");
    std::uint64_t accepted_cancel_seq{};
    accepted_cancel.append(event(++accepted_cancel_seq, "RUN_CREATED", "READY"));
    append_success(accepted_cancel, "a-1", accepted_cancel_seq, bruce::sha256("data"), 4);
    accepted_cancel.append(event(++accepted_cancel_seq, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
    accepted_cancel.append(event(++accepted_cancel_seq, "RUN_FINISHED", "CANCELLED"));
    rejects([&] { (void)inspect_worker_run_v1(accepted_cancel_root); },
            "post-acceptance cancel changed accepted attempt");

    const auto cancelled_accept_root = base / "cancelled-accept.run";
    make_layout(cancelled_accept_root);
    write_artifact(cancelled_accept_root, "a-1", "data");
    auto cancelled_accept = WorkerRunLog::create(cancelled_accept_root, "run-1");
    cancelled_accept.append(event(1, "RUN_CREATED", "READY"));
    cancelled_accept.append(event(2, "ATTEMPT_STARTED", "RUNNING", "a-1"));
    cancelled_accept.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    auto cancelled_ready = event(4, "RESULT_RECEIVED", "READY", "a-1");
    cancelled_ready.reference = "attempts/a-1/output/result.bin";
    cancelled_ready.digest = bruce::sha256("data");
    cancelled_ready.size_bytes = 4;
    cancelled_accept.append(cancelled_ready);
    cancelled_accept.append(event(5, "CANCEL_REQUESTED", "CANCELLED", "a-1"));
    auto late_accept = cancelled_ready;
    late_accept.seq = 6;
    late_accept.event = "ARTIFACT_ACCEPTED";
    cancelled_accept.append(late_accept);
    rejects([&] { (void)inspect_worker_run_v1(cancelled_accept_root); },
            "artifact accepted after post-result cancel");

    const auto rejected_root = base / "rejected.run";
    make_layout(rejected_root);
    write_artifact(rejected_root, "a-1", "data");
    auto rejected_log = WorkerRunLog::create(rejected_root, "run-1");
    rejected_log.append(event(1, "RUN_CREATED", "READY"));
    rejected_log.append(event(2, "ATTEMPT_STARTED", "RUNNING", "a-1"));
    rejected_log.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    rejected_log.append(cancelled_ready);
    rejected_log.append(event(5, "RESULT_REJECTED", "OUTPUT_LIMIT_EXCEEDED", "a-1"));
    rejected_log.append(event(6, "RUN_FINISHED", "FAILED"));
    const auto rejected = inspect_worker_run_v1(rejected_root);
    require(rejected.status == "FAILED" && rejected.attempts.front().statuses.back() == "FAILED" &&
            !rejected.attempts.front().accepted && !rejected.attempts.front().artifact_verified &&
            !rejected.attempts.front().digest,
            "explicit receipt-budget rejection did not decide READY as FAILED");
    const auto rejected_original = read_file(rejected_root / "control/run-log.jsonl");
    auto wrong_rejection = rejected_original;
    const auto output_limit_at = wrong_rejection.find("OUTPUT_LIMIT_EXCEEDED");
    require(output_limit_at != std::string::npos, "rejection test event missing");
    wrong_rejection.replace(output_limit_at, std::string("OUTPUT_LIMIT_EXCEEDED").size(), "FAILED");
    { std::ofstream out(rejected_root / "control/run-log.jsonl"); out << wrong_rejection; }
    rejects([&] { (void)inspect_worker_run_v1(rejected_root); }, "generic result rejection accepted");

    const auto accepted_rejection_root = base / "accepted-rejection.run";
    make_layout(accepted_rejection_root);
    write_artifact(accepted_rejection_root, "a-1", "data");
    auto accepted_rejection = WorkerRunLog::create(accepted_rejection_root, "run-1");
    std::uint64_t accepted_rejection_seq{};
    accepted_rejection.append(event(++accepted_rejection_seq, "RUN_CREATED", "READY"));
    append_success(accepted_rejection, "a-1", accepted_rejection_seq, bruce::sha256("data"), 4);
    accepted_rejection.append(event(++accepted_rejection_seq, "RESULT_REJECTED", "OUTPUT_LIMIT_EXCEEDED", "a-1"));
    rejects([&] { (void)inspect_worker_run_v1(accepted_rejection_root); },
            "result rejection after artifact acceptance was accepted");

    const auto accepted_restart_root = base / "accepted-restart.run";
    make_layout(accepted_restart_root, "a-1");
    make_layout(accepted_restart_root, "a-2");
    write_artifact(accepted_restart_root, "a-1", "data");
    auto accepted_restart = WorkerRunLog::create(accepted_restart_root, "run-1");
    std::uint64_t accepted_restart_seq{};
    accepted_restart.append(event(++accepted_restart_seq, "RUN_CREATED", "READY"));
    append_success(accepted_restart, "a-1", accepted_restart_seq, bruce::sha256("data"), 4);
    accepted_restart.append(event(++accepted_restart_seq, "ATTEMPT_STARTED", "READY", "a-2"));
    rejects([&] { (void)inspect_worker_run_v1(accepted_restart_root); },
            "attempt restart after accepted artifact was accepted");

    const auto original_log = read_file(root / "control/run-log.jsonl");
    auto noncanonical = original_log;
    const auto reference = std::string("attempts/a-1/output/result.bin");
    const auto replacement = std::string("attempts/a-1/output/bad space");
    const auto reference_pos = noncanonical.find(reference);
    require(reference_pos != std::string::npos, "test reference missing");
    noncanonical.replace(reference_pos, reference.size(), replacement);
    { std::ofstream out(root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << noncanonical; }
    rejects([&] { (void)inspect_worker_run_v1(root); }, "noncanonical artifact reference accepted");
    { std::ofstream out(root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << original_log; }

    std::filesystem::create_symlink(root, base / "run-root-alias");
    rejects([&] { (void)inspect_worker_run_v1(base / "run-root-alias"); },
            "symlinked run root accepted");
    std::filesystem::create_symlink(base, base / "parent-alias");
    rejects([&] { (void)inspect_worker_run_v1(base / "parent-alias" / "complete.run"); },
            "symlinked intermediate run-root component accepted");

    const auto incomplete_root = base / "incomplete.run";
    make_layout(incomplete_root);
    auto incomplete = WorkerRunLog::create(incomplete_root, "run-1");
    incomplete.append(event(1, "RUN_CREATED", "READY"));
    incomplete.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    const auto incomplete_result = inspect_worker_run_v1(incomplete_root);
    require(!incomplete_result.run_finished && incomplete_result.status == "INCOMPLETE",
            "missing RUN_FINISHED was not incomplete");

    const auto interleaved_root = base / "interleaved.run";
    make_layout(interleaved_root, "a-1"); make_layout(interleaved_root, "a-2");
    write_artifact(interleaved_root, "a-1", "one"); write_artifact(interleaved_root, "a-2", "two");
    auto interleaved = WorkerRunLog::create(interleaved_root, "run-1");
    std::uint64_t iseq{};
    interleaved.append(event(++iseq, "RUN_CREATED", "READY"));
    interleaved.append(event(++iseq, "ATTEMPT_STARTED", "READY", "a-1"));
    interleaved.append(event(++iseq, "ATTEMPT_STARTED", "READY", "a-2"));
    interleaved.append(event(++iseq, "PROCESS_EXITED", "READY", "a-1"));
    auto interleaved_result = event(++iseq, "RESULT_RECEIVED", "READY", "a-1");
    interleaved_result.reference = "attempts/a-1/output/result.bin";
    interleaved_result.size_bytes = 3; interleaved_result.digest = bruce::sha256("one");
    interleaved.append(interleaved_result);
    auto a1 = event(++iseq, "ARTIFACT_ACCEPTED", "READY", "a-1"); a1.reference = "attempts/a-1/output/result.bin"; a1.size_bytes = 3; a1.digest = bruce::sha256("one"); interleaved.append(a1);
    interleaved.append(event(++iseq, "PROCESS_EXITED", "FAILED", "a-2"));
    interleaved.append(event(++iseq, "RESULT_RECEIVED", "FAILED", "a-2"));
    interleaved.append(event(++iseq, "RUN_FINISHED", "FAILED"));
    const auto inter = inspect_worker_run_v1(interleaved_root);
    require(inter.attempts.size() == 2 && inter.status == "FAILED", "interleaved attempts rejected");

    const auto cross_attempt_reference_root = base / "cross-attempt-reference.run";
    make_layout(cross_attempt_reference_root, "a-1");
    make_layout(cross_attempt_reference_root, "a-2");
    write_artifact(cross_attempt_reference_root, "a-1", "one");
    write_artifact(cross_attempt_reference_root, "a-2", "two");
    auto cross_attempt_reference = WorkerRunLog::create(cross_attempt_reference_root, "run-1");
    cross_attempt_reference.append(event(1, "RUN_CREATED", "READY"));
    cross_attempt_reference.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    cross_attempt_reference.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    auto cross_attempt_result = event(4, "RESULT_RECEIVED", "READY", "a-1");
    cross_attempt_result.reference = "attempts/a-2/output/result.bin";
    cross_attempt_result.size_bytes = 3;
    cross_attempt_result.digest = bruce::sha256("two");
    cross_attempt_reference.append(cross_attempt_result);
    rejects([&] { (void)inspect_worker_run_v1(cross_attempt_reference_root); },
            "result reference from another attempt was accepted");

    const auto bad_root = base / "bad.run";
    make_layout(bad_root); write_artifact(bad_root, "a-1", "data");
    auto bad = WorkerRunLog::create(bad_root, "run-1");
    bad.append(event(1, "RUN_CREATED", "READY"));
    bad.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    auto accepted = event(3, "ARTIFACT_ACCEPTED", "READY", "a-1"); accepted.reference = "attempts/a-1/output/result.bin"; accepted.size_bytes = 4; accepted.digest = bruce::sha256("data");
    bad.append(accepted);
    // Corrupt the log after a valid append and ensure the inspector fails closed.
    auto text = std::string{}; { std::ifstream in(bad_root / "control/run-log.jsonl"); text.assign(std::istreambuf_iterator<char>(in), {}); }
    text += "{broken";
    { std::ofstream out(bad_root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << text; }
    rejects([&] { (void)inspect_worker_run_v1(bad_root); }, "partial JSONL was accepted");

    const auto missing_result_ref_root = base / "missing-result-ref.run";
    make_layout(missing_result_ref_root); write_artifact(missing_result_ref_root, "a-1", "data");
    auto missing_result_ref = WorkerRunLog::create(missing_result_ref_root, "run-1");
    missing_result_ref.append(event(1, "RUN_CREATED", "READY"));
    missing_result_ref.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    missing_result_ref.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    missing_result_ref.append(event(4, "RESULT_RECEIVED", "READY", "a-1"));
    auto accepted_without_result_artifact = event(5, "ARTIFACT_ACCEPTED", "READY", "a-1");
    accepted_without_result_artifact.reference = "attempts/a-1/output/result.bin";
    accepted_without_result_artifact.size_bytes = 4;
    accepted_without_result_artifact.digest = bruce::sha256("data");
    missing_result_ref.append(accepted_without_result_artifact);
    rejects([&] { (void)inspect_worker_run_v1(missing_result_ref_root); },
            "artifact accepted without matching result metadata");

    const auto mismatch_root = base / "mismatch.run";
    make_layout(mismatch_root); write_artifact(mismatch_root, "a-1", "data");
    auto mismatch_log = WorkerRunLog::create(mismatch_root, "run-1");
    mismatch_log.append(event(1, "RUN_CREATED", "READY"));
    mismatch_log.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    mismatch_log.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    auto mismatch_result = event(4, "RESULT_RECEIVED", "READY", "a-1");
    mismatch_result.reference = "attempts/a-1/output/result.bin";
    mismatch_result.size_bytes = 4; mismatch_result.digest = bruce::sha256("data");
    mismatch_log.append(mismatch_result);
    auto mismatch_accept = event(5, "ARTIFACT_ACCEPTED", "READY", "a-1");
    mismatch_accept.reference = "attempts/a-1/output/result.bin";
    mismatch_accept.size_bytes = 4; mismatch_accept.digest = bruce::sha256("data");
    mismatch_log.append(mismatch_accept);
    auto mismatch_text = read_file(mismatch_root / "control/run-log.jsonl");
    const auto accepted_line = mismatch_text.find("\"event\":\"ARTIFACT_ACCEPTED\"");
    const auto accepted_digest = mismatch_text.find("\"digest\":\"", accepted_line);
    require(accepted_line != std::string::npos && accepted_digest != std::string::npos,
            "mismatch test record missing");
    mismatch_text.replace(accepted_digest + 10, 64, bruce::sha256("bad"));
    { std::ofstream out(mismatch_root / "control/run-log.jsonl", std::ios::binary | std::ios::trunc); out << mismatch_text; }
    rejects([&] { (void)inspect_worker_run_v1(mismatch_root); },
            "artifact digest mismatch accepted");

    const auto ready_without_artifact_root = base / "ready-without-artifact.run";
    make_layout(ready_without_artifact_root);
    auto ready_without_artifact = WorkerRunLog::create(ready_without_artifact_root, "run-1");
    ready_without_artifact.append(event(1, "RUN_CREATED", "READY"));
    ready_without_artifact.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    ready_without_artifact.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    ready_without_artifact.append(event(4, "RESULT_RECEIVED", "READY", "a-1"));
    ready_without_artifact.append(event(5, "RUN_FINISHED", "READY"));
    rejects([&] { (void)inspect_worker_run_v1(ready_without_artifact_root); },
            "READY run without accepted artifact was accepted");

    const auto failed_with_ready_result_root = base / "failed-with-ready-result.run";
    make_layout(failed_with_ready_result_root);
    auto failed_with_ready_result = WorkerRunLog::create(failed_with_ready_result_root, "run-1");
    failed_with_ready_result.append(event(1, "RUN_CREATED", "READY"));
    failed_with_ready_result.append(event(2, "ATTEMPT_STARTED", "READY", "a-1"));
    failed_with_ready_result.append(event(3, "PROCESS_EXITED", "READY", "a-1"));
    failed_with_ready_result.append(event(4, "RESULT_RECEIVED", "READY", "a-1"));
    failed_with_ready_result.append(event(5, "RUN_FINISHED", "FAILED"));
    rejects([&] { (void)inspect_worker_run_v1(failed_with_ready_result_root); },
            "FAILED run with unaccepted READY result was accepted");

    std::filesystem::remove_all(base);
    std::cout << "worker run inspector v1 contracts passed\n";
}
