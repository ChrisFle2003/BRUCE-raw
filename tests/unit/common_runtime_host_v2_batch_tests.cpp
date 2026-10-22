#include "bruce/application/common_runtime_host_v2_batch.hpp"

#include "bruce/core/types.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/worker/worker_supervisor_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

using namespace bruce;
using namespace bruce::worker_v1;

namespace {

void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

worker_v1::Budget budget() { return {3000, 65536, 65536, 100, 268435456}; }

CommonRuntimeHostV2BatchContext context(const std::filesystem::path& executable,
                                        const std::filesystem::path& root,
                                        std::uint64_t workers = 2) {
    return {root, executable, budget(), workers, 1000};
}

WorkerRequest request(std::string node, std::string attempt,
                      worker_v1::Budget limits = budget()) {
    WorkerRequest result;
    result.run_id = "batch-run";
    result.attempt_id = std::move(attempt);
    result.node_id = std::move(node);
    result.adapter_id = "literal-v1";
    result.adapter_version = "1";
    result.request_digest = sha256("request");
    result.budget = limits;
    return result;
}

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_all(std::string_view data) {
    while (!data.empty()) {
        const auto written = ::write(STDOUT_FILENO, data.data(), data.size());
        if (written <= 0) std::_Exit(2);
        data.remove_prefix(static_cast<std::size_t>(written));
    }
}

std::string read_request() {
    std::string bytes;
    char value{};
    while (bytes.find("\n\n") == std::string::npos && bytes.size() < 256) {
        if (::read(STDIN_FILENO, &value, 1) != 1) std::_Exit(3);
        bytes.push_back(value);
    }
    const auto at = bytes.find("payload-bytes=");
    if (at == std::string::npos) std::_Exit(4);
    const auto end = bytes.find('\n', at);
    const auto count = std::stoul(bytes.substr(at + 14, end - at - 14));
    for (std::size_t index = 0; index < count; ++index) {
        if (::read(STDIN_FILENO, &value, 1) != 1) std::_Exit(5);
        bytes.push_back(value);
    }
    return bytes;
}

int child() {
    const auto request_data = decode_request_payload(
        decode_frame(read_request(), 65536).payload);
    if (request_data.node_id == "slow") ::usleep(90000);
    if ((request_data.node_id == "retryable" && request_data.attempt_id == "first") ||
        request_data.node_id == "always_crash")
        std::_Exit(9);
    if (request_data.node_id == "ready_after_cancel" ||
        (request_data.node_id == "deadline_retry" && request_data.attempt_id == "timeout")) {
        std::string bytes;
        char buffer[256];
        while (bytes.find("type=CANCEL\n") == std::string::npos) {
            const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count <= 0) std::_Exit(6);
            bytes.append(buffer, static_cast<std::size_t>(count));
        }
    }
    WorkerResult result;
    if (request_data.node_id == "rejected") {
        result.status = WorkerStatus::FAILED;
        result.result_code = "FAILED";
        result.failure_code = FailureCode::FAILED;
        write_all(encode_frame(FrameType::RESULT, encode_result_payload(result)));
        return 0;
    }
    if (request_data.node_id == "cancelled") {
        result.status = WorkerStatus::CANCELLED;
        result.result_code = "CANCELLED";
        result.failure_code = FailureCode::CANCELLED;
        write_all(encode_frame(FrameType::RESULT, encode_result_payload(result)));
        return 0;
    }
    result.status = WorkerStatus::READY;
    result.result_code = "READY";
    result.failure_code = FailureCode::READY;
    const std::string payload = "payload-" + request_data.attempt_id;
    const auto relative = std::filesystem::path("attempts") / request_data.attempt_id /
        "output/result.bin";
    std::filesystem::create_directories(relative.parent_path());
    std::ofstream out(relative, std::ios::binary);
    out << payload;
    out.close();
    result.output_artifact_ref = ArtifactRef{"result", payload.size(), sha256(payload)};
    result.payload_digest = sha256(payload);
    result.payload_size = payload.size();
    result.usage.output_bytes = payload.size();
    write_all(encode_frame(FrameType::RESULT, encode_result_payload(result)));
    return 0;
}

std::string events(const std::filesystem::path& root) {
    const auto data = read_bytes(root / "control/run-log.jsonl");
    std::string result;
    std::size_t position = 0;
    while (position < data.size()) {
        const auto end = data.find('\n', position);
        require(end != std::string::npos, "unterminated run log line");
        const auto at = data.find("\"event\":\"", position);
        require(at < end, "run log event missing");
        const auto begin = at + 9;
        const auto close = data.find('"', begin);
        result += data.substr(begin, close - begin) + ",";
        position = end + 1;
    }
    return result;
}

void accept_all(CommonRuntimeHostV2Batch& batch, const std::vector<CommonRuntimeHostV2BatchResult>& results) {
    for (const auto& result : results)
        if (result.result.status == WorkerStatus::READY)
            record_attempt_acceptance_v2(batch, result.attempt_id);
}

std::vector<CommonRuntimeHostV2BatchResult> finish_ready(CommonRuntimeHostV2Batch& batch) {
    std::vector<CommonRuntimeHostV2BatchResult> results;
    for (int iteration = 0; iteration < 100 && results.size() < 2; ++iteration) {
        auto polled = poll_batch_v2(batch);
        results.insert(results.end(), polled.begin(), polled.end());
        if (results.size() < 2) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    accept_all(batch, results);
    return results;
}

std::vector<CommonRuntimeHostV2BatchResult> wait_for_result(CommonRuntimeHostV2Batch& batch) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        auto results = poll_batch_v2(batch);
        if (!results.empty()) return results;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(false, "attempt did not complete within test deadline");
    return {};
}

template<class Action>
void require_error(Action action, std::string_view expected, const char* message) {
    try { action(); }
    catch (const Error& error) {
        require(error.what() == expected, message);
        return;
    }
    require(false, message);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "worker-v1" &&
        std::string_view(argv[2]) == "child") return child();

    const auto base = std::filesystem::temp_directory_path() /
        ("bruce-common-runtime-host-v2-batch-" + std::to_string(::getpid()));
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const auto executable = std::filesystem::canonical(argv[0]);

    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "two"));
        start_batch_attempt_v2(batch, request("slow", "slow"));
        start_batch_attempt_v2(batch, request("fast", "fast"));
        const auto results = finish_ready(batch);
        require(results.size() == 2, "two workers did not complete");
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::READY && outcome.host_exit_code == 0,
                "successful batch did not finish READY");
        const auto log = read_bytes(base / "two/control/run-log.jsonl");
        require(events(base / "two").find("RUN_CREATED") == 0 &&
                events(base / "two").find("RUN_FINISHED") != std::string::npos,
                "shared batch lifecycle is incomplete");
        for (int sequence = 1; sequence <= 12; ++sequence)
            require(log.find("\"seq\":" + std::to_string(sequence) + ",") != std::string::npos,
                    "shared sequence has a gap");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "limit", 2));
        start_batch_attempt_v2(batch, request("slow", "a"));
        start_batch_attempt_v2(batch, request("slow", "b"));
        bool limited = false;
        try { start_batch_attempt_v2(batch, request("slow", "c")); }
        catch (const Error& error) { limited = std::string(error.what()) == "WORKER_LIMIT_EXCEEDED"; }
        require(limited, "active worker limit was not enforced");
        request_cancel_batch_v2(batch);
        require(finish_batch_v2(batch).status == WorkerStatus::CANCELLED,
                "limited batch did not cancel");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "cancel", 1));
        start_batch_attempt_v2(batch, request("ready_after_cancel", "late"));
        request_cancel_batch_v2(batch);
        request_cancel_batch_v2(batch);
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::CANCELLED, "cancelled batch was not CANCELLED");
        const auto log = events(base / "cancel");
        require(log.find("ARTIFACT_ACCEPTED") == std::string::npos,
                "late READY was accepted after cancellation");
        require(log.find("CANCEL_REQUESTED") != std::string::npos,
                "cancel event was not logged");
    }
    {
        const auto root = base / "external-cancel";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("ready_after_cancel", "external"));
        require(request_abort(root) == AbortRequestResult::REQUESTED,
                "external abort marker was not created");
        for (int iteration = 0; iteration < 100 &&
             poll_batch_v2(batch).empty(); ++iteration)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        require(finish_batch_v2(batch).status == WorkerStatus::CANCELLED,
                "external abort marker did not cancel batch");
    }
    {
        const auto root = base / "invalid-marker";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("ready_after_cancel", "invalid-marker"));
        { std::ofstream marker(root / "control/cancel.request"); marker << "bad marker\n"; }
        std::vector<CommonRuntimeHostV2BatchResult> invalid_results;
        for (int iteration = 0; iteration < 100 && invalid_results.empty(); ++iteration) {
            invalid_results = poll_batch_v2(batch);
            if (invalid_results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(invalid_results.size() == 1 && invalid_results.front().invalid_control &&
                    invalid_results.front().result.failure_code == FailureCode::PROTOCOL_ERROR,
                "invalid control projection did not retain its Host classification");
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "invalid abort marker did not fail batch");
        require(events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "invalid abort marker accepted artifact");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "rejected", 1));
        start_batch_attempt_v2(batch, request("rejected", "rejected"));
        for (int iteration = 0; iteration < 100; ++iteration) {
            if (!poll_batch_v2(batch).empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::FAILED && outcome.host_exit_code == 1,
                "rejected result did not fail batch");
        require(events(base / "rejected").find("ARTIFACT_ACCEPTED") == std::string::npos,
                "rejected result was accepted");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "ready-rejected", 1));
        start_batch_attempt_v2(batch, request("ready-rejected", "ready-rejected"));
        std::vector<CommonRuntimeHostV2BatchResult> results;
        for (int iteration = 0; iteration < 100 && results.empty(); ++iteration) {
            results = poll_batch_v2(batch);
            if (results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(results.size() == 1 && results.front().result.status == WorkerStatus::READY,
                "ready result was not returned for rejection decision");
        record_attempt_rejection_v2(batch, results.front().attempt_id);
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "explicitly rejected READY result did not fail batch");
        require(events(base / "ready-rejected").find("ARTIFACT_ACCEPTED") == std::string::npos,
                "explicitly rejected result was accepted");
    }
    {
        const auto root = base / "retry-success";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("retryable", "first"));
        std::vector<CommonRuntimeHostV2BatchResult> first_results;
        for (int iteration = 0; iteration < 100 && first_results.empty(); ++iteration) {
            first_results = poll_batch_v2(batch);
            if (first_results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(first_results.size() == 1 &&
                    first_results.front().result.failure_code == FailureCode::PROCESS_CRASHED,
                "retry fixture did not produce a retryable failure");
        record_attempt_retry_v2(batch, "first");
        const auto log_after_retry = read_bytes(root / "control/run-log.jsonl");
        require(log_after_retry.find("ARTIFACT_ACCEPTED") == std::string::npos,
                "historical retry failure emitted an artifact acceptance");
        start_batch_attempt_v2(batch, request("retryable", "second"));
        std::vector<CommonRuntimeHostV2BatchResult> second_results;
        for (int iteration = 0; iteration < 100 && second_results.empty(); ++iteration) {
            second_results = poll_batch_v2(batch);
            if (second_results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(second_results.size() == 1 && second_results.front().result.status == WorkerStatus::READY,
                "retry fixture did not produce the final READY result");
        record_attempt_acceptance_v2(batch, "second");
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::READY && outcome.host_exit_code == 0,
                "successful retry did not finish READY");
        const auto log = read_bytes(root / "control/run-log.jsonl");
        require(log.find("ARTIFACT_ACCEPTED") != std::string::npos,
                "successful retry did not emit artifact acceptance");
        require(log.find("\"event\":\"RESULT_RECEIVED\",\"status\":\"FAILED\"") != std::string::npos &&
                log.find("\"attempt_id\":\"first\"") != std::string::npos,
                "historical retry failure was not preserved in the log");
        require(log.find("\"event\":\"ARTIFACT_ACCEPTED\",\"status\":\"READY\"") != std::string::npos &&
                log.find("\"attempt_id\":\"second\"") != std::string::npos,
                "final retry acceptance used the wrong attempt");
        require(log.find("\"event\":\"RUN_FINISHED\",\"status\":\"READY\"") != std::string::npos,
                "successful retry did not emit final READY");
        bool duplicate = false;
        try { record_attempt_retry_v2(batch, "first"); }
        catch (const Error& error) { duplicate = std::string(error.what()) == "RESULT_DECISION_REQUIRED"; }
        require(duplicate, "duplicate retry decision was accepted");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "retry-decisions", 1));
        bool unknown = false;
        try { record_attempt_retry_v2(batch, "missing"); }
        catch (const Error& error) { unknown = std::string(error.what()) == "RESULT_DECISION_REQUIRED"; }
        require(unknown, "unknown retry decision was not rejected");
        start_batch_attempt_v2(batch, request("fast", "final"));
        std::vector<CommonRuntimeHostV2BatchResult> results;
        for (int iteration = 0; iteration < 100 && results.empty(); ++iteration) {
            results = poll_batch_v2(batch);
            if (results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        bool ready_retry = false;
        try { record_attempt_retry_v2(batch, "final"); }
        catch (const Error& error) { ready_retry = std::string(error.what()) == "RESULT_NOT_RETRYABLE"; }
        require(ready_retry, "READY retry decision was not rejected");
        record_attempt_rejection_v2(batch, "final");
        bool duplicate = false;
        try { record_attempt_retry_v2(batch, "final"); }
        catch (const Error& error) { duplicate = std::string(error.what()) == "RESULT_DECISION_REQUIRED"; }
        require(duplicate, "retry after final rejection was accepted");
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "final rejection did not fail the batch");
    }
    {
        const auto root = base / "retry-exhaustion";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("always_crash", "first"));
        std::vector<CommonRuntimeHostV2BatchResult> first_results;
        for (int iteration = 0; iteration < 100 && first_results.empty(); ++iteration) {
            first_results = poll_batch_v2(batch);
            if (first_results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(first_results.size() == 1 &&
                    first_results.front().result.failure_code == FailureCode::PROCESS_CRASHED,
                "retry exhaustion fixture did not produce the first failure");
        record_attempt_retry_v2(batch, "first");
        start_batch_attempt_v2(batch, request("always_crash", "second"));
        std::vector<CommonRuntimeHostV2BatchResult> second_results;
        for (int iteration = 0; iteration < 100 && second_results.empty(); ++iteration) {
            second_results = poll_batch_v2(batch);
            if (second_results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(second_results.size() == 1 &&
                    second_results.front().result.failure_code == FailureCode::PROCESS_CRASHED,
                "retry exhaustion fixture did not produce the final failure");
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::FAILED && outcome.host_exit_code == 1,
                "retry exhaustion did not finish FAILED");
        const auto log = read_bytes(root / "control/run-log.jsonl");
        require(log.find("ARTIFACT_ACCEPTED") == std::string::npos,
                "retry exhaustion emitted an artifact acceptance");
        require(log.find("\"attempt_id\":\"first\"") != std::string::npos &&
                log.find("\"attempt_id\":\"second\"") != std::string::npos,
                "retry exhaustion did not preserve both attempts");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "retry-cancel", 1));
        start_batch_attempt_v2(batch, request("slow", "cancelled"));
        request_cancel_batch_v2(batch);
        const auto outcome = finish_batch_v2(batch);
        require(outcome.status == WorkerStatus::CANCELLED,
                "cancelled retry attempt did not finish CANCELLED");
        bool rejected = false;
        try { record_attempt_retry_v2(batch, "cancelled"); }
        catch (const Error& error) { rejected = std::string(error.what()) == "RESULT_NOT_RETRYABLE"; }
        require(rejected, "cancelled attempt was eligible for retry");
    }
    {
        const auto root = base / "projected-failure-cancel";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("rejected", "projected-failure"));
        std::vector<CommonRuntimeHostV2BatchResult> results;
        for (int iteration = 0; iteration < 100 && results.empty(); ++iteration) {
            results = poll_batch_v2(batch);
            if (results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(results.size() == 1 && results.front().result.status == WorkerStatus::FAILED,
                "projected failure fixture did not complete");
        request_cancel_batch_v2(batch);
        require(finish_batch_v2(batch).status == WorkerStatus::CANCELLED,
                "cancelled projected failure did not finish CANCELLED");
        const auto log = read_bytes(root / "control/run-log.jsonl");
        require(log.find("\"event\":\"CANCEL_REQUESTED\"") != std::string::npos &&
                log.find("\"attempt_id\":\"projected-failure\"") != std::string::npos,
                "cancel did not log projected failure cancellation");
        const auto first_cancel = log.find("\"event\":\"CANCEL_REQUESTED\"");
        require(first_cancel == log.rfind("\"event\":\"CANCEL_REQUESTED\""),
                "projected failure cancellation was logged more than once");
    }
    {
        auto batch = start_batch_v2("batch-run", context(executable, base / "cancelled-result", 1));
        start_batch_attempt_v2(batch, request("cancelled", "cancelled-result"));
        std::vector<CommonRuntimeHostV2BatchResult> results;
        for (int iteration = 0; iteration < 100 && results.empty(); ++iteration) {
            results = poll_batch_v2(batch);
            if (results.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(results.size() == 1 && results.front().result.status == WorkerStatus::CANCELLED,
                "cancelled result fixture did not complete");
        bool rejected = false;
        try { record_attempt_retry_v2(batch, "cancelled-result"); }
        catch (const Error& error) { rejected = std::string(error.what()) == "RESULT_NOT_RETRYABLE"; }
        require(rejected, "CANCELLED result was eligible for retry");
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "undecided cancelled result did not fail the batch");
    }
    {
        auto invalid_workers = context(executable, base / "invalid-workers", 0);
        auto invalid = start_batch_v2("batch-run", invalid_workers);
        require(finish_batch_v2(invalid).status == WorkerStatus::FAILED,
                "invalid max_workers context was accepted");
        auto invalid_grace = context(executable, base / "invalid-grace");
        invalid_grace.grace_ms = 999;
        invalid = start_batch_v2("batch-run", invalid_grace);
        require(finish_batch_v2(invalid).status == WorkerStatus::FAILED,
                "invalid grace context was accepted");
        auto duplicate = start_batch_v2("batch-run", context(executable, base / "duplicate", 2));
        start_batch_attempt_v2(duplicate, request("fast", "same"));
        bool rejected = false;
        try { start_batch_attempt_v2(duplicate, request("fast", "same")); }
        catch (const Error& error) { rejected = std::string(error.what()) == "DUPLICATE_ATTEMPT_ID"; }
        require(rejected, "duplicate attempt ID was accepted");
        request_cancel_batch_v2(duplicate);
        (void)finish_batch_v2(duplicate);
    }
    // An expired preparation deadline must never dispatch a worker.
    {
        const auto root = base / "expired-before-start";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        CommonRuntimeHostV2AttemptOptions options;
        options.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        require_error([&] { start_batch_attempt_v2(batch, request("fast", "expired"), options); },
                      "TIMEOUT", "expired deadline was dispatched");
        record_scheduler_failure_v2(batch);
        record_scheduler_failure_v2(batch);
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "scheduler failure without attempts was not FAILED");
        require(events(root) == "RUN_CREATED,RUN_FINISHED,",
                "preparation failure fabricated attempt events");
        require(!std::filesystem::exists(root / "attempts/expired"),
                "preparation failure created a worker directory");
    }
    // Absolute timeout cancels/reaps a live worker but never becomes user cancel.
    {
        const auto root = base / "active-deadline";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        CommonRuntimeHostV2AttemptOptions options;
        options.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
        start_batch_attempt_v2(batch, request("deadline_retry", "timeout"), options);
        std::this_thread::sleep_until(*options.deadline);
        const auto results = wait_for_result(batch);
        const auto& result = results.front().result;
        require(result.status == WorkerStatus::FAILED && result.failure_code == FailureCode::TIMEOUT &&
                    result.result_code == "TIMEOUT",
                "active deadline was not projected as FAILED/TIMEOUT");
        require(result.usage.peak_memory_bytes > 0 && result.usage.wall_ms > 0 && !result.output_artifact_ref &&
                    !result.payload_digest && result.payload_size == 0,
                "timeout lost measured usage or exposed artifact metadata");
        require_error([&] { record_attempt_acceptance_v2(batch, "timeout"); },
                      "RESULT_NOT_ACCEPTABLE", "timeout artifact was accepted");
        record_attempt_retry_v2(batch, "timeout");
        start_batch_attempt_v2(batch, request("deadline_retry", "after-timeout"));
        accept_all(batch, wait_for_result(batch));
        require(finish_batch_v2(batch).status == WorkerStatus::READY,
                "explicit retry after active deadline did not recover");
        require(events(root).find("CANCEL_REQUESTED") == std::string::npos,
                "deadline fabricated an owner cancellation");
    }
    // Rejection must constrain acceptance and retries, preserving the READY usage history.
    {
        using Reason = CommonRuntimeHostV2RejectionReason;
        const std::pair<Reason, const char*> reasons[] = {
            {Reason::OUTPUT_LIMIT_EXCEEDED, "OUTPUT_LIMIT_EXCEEDED"},
            {Reason::TIMEOUT, "TIMEOUT"},
            {Reason::ARTIFACT_REFERENCE_INVALID, "ARTIFACT_REFERENCE_INVALID"},
            {Reason::ARTIFACT_MANIFEST_INVALID, "ARTIFACT_MANIFEST_INVALID"},
            {Reason::ARTIFACT_DIGEST_MISMATCH, "ARTIFACT_DIGEST_MISMATCH"},
            {Reason::ARTIFACT_STORE_VIOLATION, "ARTIFACT_STORE_VIOLATION"},
        };
        for (const auto& [reason, name] : reasons) {
            const auto root = base / (std::string("typed-") + name);
            auto batch = start_batch_v2("batch-run", context(executable, root, 1));
            CommonRuntimeHostV2AttemptOptions options;
            if (reason == Reason::TIMEOUT)
                options.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            start_batch_attempt_v2(batch, request("fast", "candidate"), options);
            const auto results = wait_for_result(batch);
            require(results.front().result.status == WorkerStatus::READY &&
                        results.front().result.usage.output_bytes > 0,
                    "typed rejection fixture was not READY");
            if (options.deadline) std::this_thread::sleep_until(*options.deadline);
            record_attempt_rejection_v2(batch, "candidate", reason);
            require_error([&] { record_attempt_rejection_v2(batch, "candidate", reason); },
                          "RESULT_DECISION_REQUIRED", "repeated rejection was accepted");
            require_error([&] { record_attempt_acceptance_v2(batch, "candidate"); },
                          reason == Reason::TIMEOUT ? "RESULT_NOT_ACCEPTABLE" : "RESULT_DECISION_REQUIRED",
                          "rejected candidate was accepted");
            const auto log = read_bytes(root / "control/run-log.jsonl");
            require(log.find(std::string("\"event\":\"RESULT_REJECTED\",\"status\":\"") + name + "\"}") != std::string::npos,
                    "typed rejection was not logged without metadata");
            require(log.find("ARTIFACT_ACCEPTED") == std::string::npos,
                    "rejected candidate emitted acceptance");
            if (reason == Reason::TIMEOUT) {
                record_attempt_retry_v2(batch, "candidate");
                start_batch_attempt_v2(batch, request("fast", "recovered"));
                accept_all(batch, wait_for_result(batch));
                require(finish_batch_v2(batch).status == WorkerStatus::READY,
                        "post-READY timeout could not recover via explicit retry");
            } else {
                require_error([&] { record_attempt_retry_v2(batch, "candidate"); },
                              "RESULT_DECISION_REQUIRED", "terminal payload rejection reopened for retry");
                require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                        "terminal payload rejection did not fail batch");
            }
        }
    }
    {
        const auto root = base / "timeout-without-retry";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("fast", "candidate"));
        (void)wait_for_result(batch);
        record_attempt_rejection_v2(batch, "candidate", CommonRuntimeHostV2RejectionReason::TIMEOUT);
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "timeout without explicit retry did not finish FAILED");
        require(events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "timeout without retry accepted an artifact");
    }
    {
        const auto root = base / "rejection-invalid";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        using Reason = CommonRuntimeHostV2RejectionReason;
        require_error([&] { record_attempt_rejection_v2(batch, "missing", Reason::TIMEOUT); },
                      "RESULT_DECISION_REQUIRED", "unknown candidate was rejected");
        start_batch_attempt_v2(batch, request("fast", "candidate"));
        (void)wait_for_result(batch);
        const auto before = read_bytes(root / "control/run-log.jsonl");
        require_error([&] { record_attempt_rejection_v2(batch, "candidate", static_cast<Reason>(999)); },
                      "INVALID_REJECTION_REASON", "unknown rejection reason was accepted");
        require(read_bytes(root / "control/run-log.jsonl") == before,
                "invalid rejection changed lifecycle history");
        record_attempt_acceptance_v2(batch, "candidate");
        require_error([&] { record_attempt_rejection_v2(batch, "candidate", Reason::TIMEOUT); },
                      "RESULT_DECISION_REQUIRED", "accepted candidate was rejected");
        record_scheduler_failure_v2(batch);
        require(finish_batch_v2(batch).status == WorkerStatus::FAILED,
                "scheduler failure after accepted worker was ignored");
    }
    {
        const auto root = base / "rejection-cancelled";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("fast", "candidate"));
        (void)wait_for_result(batch);
        request_cancel_batch_v2(batch);
        require_error([&] { record_attempt_rejection_v2(batch, "candidate", CommonRuntimeHostV2RejectionReason::TIMEOUT); },
                      "RESULT_DECISION_REQUIRED", "cancelled candidate was rejected");
        record_scheduler_failure_v2(batch);
        require(finish_batch_v2(batch).status == WorkerStatus::CANCELLED,
                "scheduler failure overrode owner cancel");
    }
    {
        const auto root = base / "acceptance-observes-cancel";
        auto batch = start_batch_v2("batch-run", context(executable, root, 1));
        start_batch_attempt_v2(batch, request("fast", "candidate"));
        (void)wait_for_result(batch);
        require(request_abort(root) == AbortRequestResult::REQUESTED,
                "acceptance cancel marker was not created");
        require_error([&] { record_attempt_acceptance_v2(batch, "candidate"); },
                      "RESULT_NOT_ACCEPTABLE", "acceptance ignored owner cancel marker");
        require(finish_batch_v2(batch).status == WorkerStatus::CANCELLED &&
                    events(root).find("ARTIFACT_ACCEPTED") == std::string::npos,
                "acceptance after marker was not cancelled");
    }
    std::filesystem::remove_all(base);
    return 0;
}
