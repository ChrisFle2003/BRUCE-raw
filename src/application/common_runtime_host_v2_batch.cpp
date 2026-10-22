#include "bruce/application/common_runtime_host_v2_batch.hpp"

#include "bruce/core/types.hpp"
#include "bruce/worker/worker_abort_control_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include "bruce/worker/worker_supervisor_v1.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace bruce {
namespace {
using namespace worker_v1;

bool same_budget(const worker_v1::Budget& left, const worker_v1::Budget& right) {
    return left.max_wall_ms == right.max_wall_ms &&
        left.max_input_bytes == right.max_input_bytes &&
        left.max_output_bytes == right.max_output_bytes &&
        left.max_operations == right.max_operations &&
        left.max_memory_bytes == right.max_memory_bytes;
}

bool valid_budget(const worker_v1::Budget& budget) {
    return budget.max_wall_ms > 0 && budget.max_wall_ms <= 600000 &&
        budget.max_input_bytes > 0 && budget.max_input_bytes <= 1048576 &&
        budget.max_output_bytes > 0 && budget.max_output_bytes <= 5242880 &&
        budget.max_operations > 0 && budget.max_operations <= 10000000 &&
        budget.max_memory_bytes > 0 && budget.max_memory_bytes <= 2147483648ULL;
}

bool token(std::string_view value) {
    return !value.empty() && value.size() <= 128 &&
        std::all_of(value.begin(), value.end(), [](const char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_';
        });
}

bool digest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool relative_ref(const std::filesystem::path& reference) {
    if (reference.empty() || reference.is_absolute() || reference.has_root_name()) return false;
    for (const auto& part : reference) if (part == "." || part == "..") return false;
    return true;
}

bool valid_request(const WorkerRequest& request, const worker_v1::Budget& budget,
                   std::string_view run_id) {
    if (request.run_id != run_id || !same_budget(request.budget, budget) ||
        !token(request.run_id) || !token(request.attempt_id) || !token(request.node_id) ||
        !token(request.adapter_id) || !token(request.adapter_version) ||
        !digest(request.request_digest) || request.input_artifact_refs.size() > 1024) return false;
    for (const auto& reference : request.input_artifact_refs) {
        if (!relative_ref(reference.reference) || !digest(reference.sha256)) return false;
    }
    return true;
}

std::optional<std::filesystem::path> canonical_host_executable(
    const std::filesystem::path& executable) {
    if (!executable.is_absolute()) return std::nullopt;
    std::error_code error;
    const auto self = std::filesystem::canonical("/proc/self/exe", error);
    if (error) return std::nullopt;
    const auto candidate = std::filesystem::canonical(executable, error);
    if (error || candidate != self) return std::nullopt;
    return candidate;
}

bool private_root(const std::filesystem::path& root) {
    if (root.empty() || !root.is_absolute()) return false;
    struct stat status{};
    if (::lstat(root.c_str(), &status) == 0)
        return S_ISDIR(status.st_mode) && status.st_uid == ::geteuid() &&
            (status.st_mode & 07777) == 0700;
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error || ::chmod(root.c_str(), 0700) != 0 || ::lstat(root.c_str(), &status) != 0)
        return false;
    return S_ISDIR(status.st_mode) && status.st_uid == ::geteuid() &&
        (status.st_mode & 07777) == 0700;
}

std::string_view status_name(WorkerStatus status) {
    switch (status) {
    case WorkerStatus::READY: return "READY";
    case WorkerStatus::FAILED: return "FAILED";
    case WorkerStatus::BLOCKED: return "BLOCKED";
    case WorkerStatus::CANCELLED: return "CANCELLED";
    }
    throw Error("InvalidWorkerStatus");
}

std::uint64_t unix_ms() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

WorkerResult cancelled_result(const WorkerResult& original) {
    WorkerResult result;
    result.status = WorkerStatus::CANCELLED;
    result.result_code = "CANCELLED";
    result.failure_code = FailureCode::CANCELLED;
    result.usage = original.usage;
    return result;
}

WorkerResult protocol_failure(const WorkerResult& original) {
    WorkerResult result;
    result.status = WorkerStatus::FAILED;
    result.result_code = "PROTOCOL_ERROR";
    result.failure_code = FailureCode::PROTOCOL_ERROR;
    result.usage = original.usage;
    return result;
}

WorkerResult timeout_result(const WorkerResult& original) {
    WorkerResult result;
    result.status = WorkerStatus::FAILED;
    result.result_code = "TIMEOUT";
    result.failure_code = FailureCode::TIMEOUT;
    result.usage = original.usage;
    return result;
}

std::string_view rejection_name(CommonRuntimeHostV2RejectionReason reason) {
    switch (reason) {
    case CommonRuntimeHostV2RejectionReason::OUTPUT_LIMIT_EXCEEDED: return "OUTPUT_LIMIT_EXCEEDED";
    case CommonRuntimeHostV2RejectionReason::TIMEOUT: return "TIMEOUT";
    case CommonRuntimeHostV2RejectionReason::ARTIFACT_REFERENCE_INVALID: return "ARTIFACT_REFERENCE_INVALID";
    case CommonRuntimeHostV2RejectionReason::ARTIFACT_MANIFEST_INVALID: return "ARTIFACT_MANIFEST_INVALID";
    case CommonRuntimeHostV2RejectionReason::ARTIFACT_DIGEST_MISMATCH: return "ARTIFACT_DIGEST_MISMATCH";
    case CommonRuntimeHostV2RejectionReason::ARTIFACT_STORE_VIOLATION: return "ARTIFACT_STORE_VIOLATION";
    }
    throw Error("INVALID_REJECTION_REASON");
}

bool canonical_ready(const WorkerResult& result) {
    return result.status == WorkerStatus::READY && result.failure_code == FailureCode::READY &&
        result.output_artifact_ref && result.payload_digest &&
        result.output_artifact_ref->reference == "result" &&
        result.output_artifact_ref->size_bytes == result.payload_size &&
        result.output_artifact_ref->sha256 == *result.payload_digest;
}

} // namespace

struct CommonRuntimeHostV2Batch::State {
    struct Attempt {
        std::string id;
        std::string node_id;
        std::optional<worker_v1::WorkerAttemptHandle> handle;
        std::optional<worker_v1::WorkerResult> result;
        std::optional<std::chrono::steady_clock::time_point> deadline;
        bool deadline_expired{};
        bool rejection_logged{};
        bool accepted{};
        bool decided{};
        bool retry_decided{};
        bool cancel_logged{};
    };

    std::filesystem::path root;
    std::filesystem::path executable;
    worker_v1::Budget budget;
    std::uint64_t max_workers{};
    std::uint64_t grace_ms{};
    std::string run_id;
    std::optional<worker_v1::WorkerRunLog> log;
    std::map<std::string, Attempt> active;
    std::map<std::string, Attempt> completed;
    std::map<std::string, std::string> latest_attempt_by_node;
    std::uint64_t seq{};
    bool cancel_logged{};
    bool abort_invalid{};
    bool invalid_context{};
    bool scheduler_failed{};
    bool finished{};
    std::optional<CommonRuntimeHostV2BatchOutcome> outcome;

    void append(std::string event, std::string status, const std::string* attempt_id = nullptr,
                const std::optional<ArtifactRef>& artifact = {},
                const std::optional<WorkerUsage>& usage = {}) {
        if (!log) return;
        WorkerLogEvent record;
        record.seq = ++seq;
        record.ts_unix_ms = unix_ms();
        record.event = std::move(event);
        record.status = std::move(status);
        if (attempt_id) record.attempt_id = *attempt_id;
        if (artifact) {
            record.reference = "attempts/" + *attempt_id + "/output/result.bin";
            record.digest = artifact->sha256;
            record.size_bytes = artifact->size_bytes;
        }
        record.usage = usage;
        log->append(record);
    }

    void append_run_finished(WorkerStatus status) {
        if (!finished) {
            append("RUN_FINISHED", std::string(status_name(status)));
            finished = true;
        }
    }

    void project_completed(Attempt& attempt, WorkerResult result) {
        if (attempt.result) return;
        if (abort_invalid) {
            result = protocol_failure(result);
        } else if (cancel_logged || attempt.cancel_logged) {
            result = cancelled_result(result);
        } else if (attempt.deadline_expired) {
            result = timeout_result(result);
        } else if (result.status == WorkerStatus::READY && !canonical_ready(result)) {
            result = protocol_failure(result);
        }
        if (!canonical_ready(result)) {
            result.output_artifact_ref.reset();
            result.payload_digest.reset();
            result.payload_size = 0;
        }
        const auto status = std::string(status_name(result.status));
        const auto artifact = canonical_ready(result) ? result.output_artifact_ref : std::nullopt;
        append("PROCESS_EXITED", status, &attempt.id, {}, result.usage);
        append("RESULT_RECEIVED", status, &attempt.id, artifact, result.usage);
        attempt.result = std::move(result);
        attempt.handle.reset();
    }
};

CommonRuntimeHostV2Batch::CommonRuntimeHostV2Batch() = default;
CommonRuntimeHostV2Batch::CommonRuntimeHostV2Batch(std::unique_ptr<State> state)
    : state_(std::move(state)) {}
CommonRuntimeHostV2Batch::~CommonRuntimeHostV2Batch() = default;
CommonRuntimeHostV2Batch::CommonRuntimeHostV2Batch(CommonRuntimeHostV2Batch&&) noexcept = default;
CommonRuntimeHostV2Batch& CommonRuntimeHostV2Batch::operator=(CommonRuntimeHostV2Batch&&) noexcept = default;

CommonRuntimeHostV2Batch start_batch_v2(std::string run_id,
                                        const CommonRuntimeHostV2BatchContext& context) {
    auto state = std::make_unique<CommonRuntimeHostV2Batch::State>();
    state->run_id = std::move(run_id);
    state->root = context.run_root;
    state->executable = context.worker_executable;
    state->budget = context.budget;
    state->max_workers = context.max_workers;
    state->grace_ms = context.grace_ms;
    const auto executable = canonical_host_executable(context.worker_executable);
    if (!token(state->run_id) || !valid_budget(context.budget) || context.max_workers < 1 ||
        context.max_workers > 4 || context.grace_ms != 1000 || !executable ||
        !private_root(context.run_root)) {
        state->invalid_context = true;
        return CommonRuntimeHostV2Batch(std::move(state));
    }
    state->root = std::filesystem::canonical(context.run_root);
    state->executable = *executable;
    state->log.emplace(worker_v1::WorkerRunLog::create(state->root, state->run_id));
    state->append("RUN_CREATED", "READY");
    return CommonRuntimeHostV2Batch(std::move(state));
}

void start_batch_attempt_v2(CommonRuntimeHostV2Batch& batch,
                            const worker_v1::WorkerRequest& request) {
    start_batch_attempt_v2(batch, request, {});
}

void start_batch_attempt_v2(CommonRuntimeHostV2Batch& batch,
                            const worker_v1::WorkerRequest& request,
                            const CommonRuntimeHostV2AttemptOptions& options) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    if (state.invalid_context || state.finished || state.cancel_logged || !state.log)
        throw Error("INVALID_REQUEST");
    if (!valid_request(request, state.budget, state.run_id)) throw Error("INVALID_REQUEST");
    if (state.active.contains(request.attempt_id) || state.completed.contains(request.attempt_id))
        throw Error("DUPLICATE_ATTEMPT_ID");
    if (state.active.size() >= state.max_workers) throw Error("WORKER_LIMIT_EXCEEDED");
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline)
        throw Error("TIMEOUT");
    CommonRuntimeHostV2Batch::State::Attempt attempt;
    attempt.deadline = options.deadline;
    attempt.id = request.attempt_id;
    attempt.node_id = request.node_id;
    state.latest_attempt_by_node[attempt.node_id] = attempt.id;
    state.append("ATTEMPT_STARTED", "RUNNING", &attempt.id);
    state.append("STATUS", "RUNNING", &attempt.id);
    try {
        attempt.handle.emplace(worker_v1::start_attempt(request, {
            state.executable, state.root, state.budget, state.grace_ms}));
    } catch (...) {
        state.append("PROCESS_EXITED", "FAILED", &attempt.id);
        state.append("RESULT_RECEIVED", "FAILED", &attempt.id);
        throw;
    }
    state.active.emplace(attempt.id, std::move(attempt));
}

std::vector<CommonRuntimeHostV2BatchResult> poll_batch_v2(CommonRuntimeHostV2Batch& batch) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    std::vector<CommonRuntimeHostV2BatchResult> results;
    if (!state.cancel_logged) {
        const auto abort = worker_v1::probe_abort(state.root);
        if (abort == worker_v1::AbortProbeResult::REQUESTED) {
            request_cancel_batch_v2(batch);
        } else if (abort == worker_v1::AbortProbeResult::INVALID) {
            state.abort_invalid = true;
            request_cancel_batch_v2(batch);
        }
    }
    for (auto iterator = state.active.begin(); iterator != state.active.end();) {
        auto& attempt = iterator->second;
        if (!attempt.handle) { ++iterator; continue; }
        if (!state.cancel_logged && !attempt.deadline_expired && attempt.deadline &&
            std::chrono::steady_clock::now() >= *attempt.deadline) {
            attempt.deadline_expired = true;
            worker_v1::request_cancel(*attempt.handle);
        }
        auto completed = worker_v1::poll_attempt(*attempt.handle);
        if (!completed) { ++iterator; continue; }
        state.project_completed(attempt, std::move(completed->result));
        auto node = std::move(attempt);
        iterator = state.active.erase(iterator);
        const auto& result = *node.result;
        results.push_back({node.id, result, state.abort_invalid});
        state.completed.emplace(node.id, std::move(node));
    }
    return results;
}

void record_attempt_acceptance_v2(CommonRuntimeHostV2Batch& batch, std::string_view attempt_id) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    const auto iterator = state.completed.find(std::string(attempt_id));
    if (iterator == state.completed.end() || !iterator->second.result || iterator->second.decided)
        throw Error("RESULT_DECISION_REQUIRED");
    auto& attempt = iterator->second;
    if (!state.cancel_logged) {
        const auto abort = worker_v1::probe_abort(state.root);
        if (abort == worker_v1::AbortProbeResult::REQUESTED) {
            request_cancel_batch_v2(batch);
        } else if (abort == worker_v1::AbortProbeResult::INVALID) {
            state.abort_invalid = true;
            request_cancel_batch_v2(batch);
        }
    }
    if (state.cancel_logged || attempt.cancel_logged || !canonical_ready(*attempt.result))
        throw Error("RESULT_NOT_ACCEPTABLE");
    state.append("ARTIFACT_ACCEPTED", "READY", &attempt.id,
                 attempt.result->output_artifact_ref, attempt.result->usage);
    attempt.accepted = true;
    attempt.decided = true;
}

void record_attempt_rejection_v2(CommonRuntimeHostV2Batch& batch, std::string_view attempt_id) {
    record_attempt_rejection_v2(batch, attempt_id,
                                CommonRuntimeHostV2RejectionReason::OUTPUT_LIMIT_EXCEEDED);
}

void record_attempt_rejection_v2(CommonRuntimeHostV2Batch& batch, std::string_view attempt_id,
                                 CommonRuntimeHostV2RejectionReason reason) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    const auto name = rejection_name(reason);
    auto& state = *batch.state_;
    const auto iterator = state.completed.find(std::string(attempt_id));
    if (iterator == state.completed.end() || !iterator->second.result || iterator->second.decided ||
        iterator->second.rejection_logged)
        throw Error("RESULT_DECISION_REQUIRED");
    auto& attempt = iterator->second;
    if (state.finished || state.cancel_logged || attempt.cancel_logged || !canonical_ready(*attempt.result))
        throw Error("RESULT_NOT_REJECTABLE");
    state.append("RESULT_REJECTED", std::string(name), &attempt.id);
    auto rejected = WorkerResult{};
    rejected.status = WorkerStatus::FAILED;
    rejected.result_code = std::string(name);
    rejected.failure_code = reason == CommonRuntimeHostV2RejectionReason::TIMEOUT
        ? FailureCode::TIMEOUT : (reason == CommonRuntimeHostV2RejectionReason::OUTPUT_LIMIT_EXCEEDED
            ? FailureCode::OUTPUT_LIMIT_EXCEEDED : FailureCode::FAILED);
    rejected.usage = attempt.result->usage;
    attempt.result = std::move(rejected);
    attempt.rejection_logged = true;
    // TIMEOUT still requires the scheduler's explicit retry decision. Other
    // rejection reasons are final and cannot be reopened by that decision.
    attempt.decided = reason != CommonRuntimeHostV2RejectionReason::TIMEOUT;
}

void record_scheduler_failure_v2(CommonRuntimeHostV2Batch& batch) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    if (!batch.state_->finished) batch.state_->scheduler_failed = true;
}

void record_attempt_retry_v2(CommonRuntimeHostV2Batch& batch, std::string_view attempt_id) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    const auto iterator = state.completed.find(std::string(attempt_id));
    if (iterator == state.completed.end() || !iterator->second.result || iterator->second.decided)
        throw Error("RESULT_DECISION_REQUIRED");
    auto& attempt = iterator->second;
    if (state.finished || state.cancel_logged || attempt.cancel_logged ||
        attempt.result->status == WorkerStatus::READY ||
        attempt.result->status == WorkerStatus::CANCELLED ||
        attempt.result->failure_code == FailureCode::CANCELLED)
        throw Error("RESULT_NOT_RETRYABLE");
    attempt.retry_decided = true;
    attempt.decided = true;
}

void request_cancel_batch_v2(CommonRuntimeHostV2Batch& batch) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    if (state.invalid_context || state.finished || state.cancel_logged) return;
    state.cancel_logged = true;
    for (auto& [id, attempt] : state.active) {
        if (attempt.cancel_logged) continue;
        state.append("CANCEL_REQUESTED", "CANCELLED", &id);
        attempt.cancel_logged = true;
        if (attempt.handle) worker_v1::request_cancel(*attempt.handle);
    }
    // A worker can have been reaped and projected READY just before the host
    // observes the owner cancel marker. Decide that late result here as well;
    // otherwise finish_batch_v2 would correctly reject an unresolved READY
    // decision after the V5 host has stopped polling.
    for (auto& [id, attempt] : state.completed) {
        // A node between retries still owns its last attempt. Cancel that
        // decision while preserving older failures as historical evidence.
        const bool between_retries = attempt.retry_decided &&
            state.latest_attempt_by_node.at(attempt.node_id) == id;
        if (!attempt.result || (attempt.decided && !between_retries) || attempt.cancel_logged)
            continue;
        state.append("CANCEL_REQUESTED", "CANCELLED", &id);
        attempt.cancel_logged = true;
        attempt.result = cancelled_result(*attempt.result);
        attempt.decided = true;
        attempt.retry_decided = false;
        attempt.accepted = false;
    }
}

CommonRuntimeHostV2BatchOutcome finish_batch_v2(CommonRuntimeHostV2Batch& batch) {
    if (!batch.state_) throw Error("InvalidCommonRuntimeHostV2Batch");
    auto& state = *batch.state_;
    if (state.outcome) return *state.outcome;
    if (state.invalid_context) {
        state.finished = true;
        state.outcome = CommonRuntimeHostV2BatchOutcome{WorkerStatus::FAILED, 1};
        return *state.outcome;
    }
    if (!state.active.empty()) request_cancel_batch_v2(batch);
    while (!state.active.empty()) {
        for (auto iterator = state.active.begin(); iterator != state.active.end();) {
            auto& attempt = iterator->second;
            auto completed = worker_v1::finish_attempt(*attempt.handle);
            state.project_completed(attempt, std::move(completed.result));
            auto node = std::move(attempt);
            iterator = state.active.erase(iterator);
            state.completed.emplace(node.id, std::move(node));
        }
    }
    bool failed = state.scheduler_failed;
    bool unresolved = false;
    std::map<std::string, bool> current_by_node;
    for (const auto& [id, attempt] : state.completed)
        if (!attempt.retry_decided) current_by_node[attempt.node_id] = true;
    for (const auto& [id, attempt] : state.completed) {
        if (!attempt.result) { failed = true; continue; }
        if (attempt.retry_decided) {
            if (!current_by_node.contains(attempt.node_id)) failed = true;
            continue;
        }
        if (attempt.result->status == WorkerStatus::READY && !attempt.decided) unresolved = true;
        if (attempt.result->status != WorkerStatus::READY ||
            (attempt.result->status == WorkerStatus::READY && !attempt.accepted)) failed = true;
    }
    if (unresolved) {
        state.append_run_finished(WorkerStatus::FAILED);
        state.outcome = CommonRuntimeHostV2BatchOutcome{WorkerStatus::FAILED, 1};
        throw Error("RESULT_DECISION_REQUIRED");
    }
    const auto status = state.abort_invalid ? WorkerStatus::FAILED :
        (state.cancel_logged ? WorkerStatus::CANCELLED :
            (failed ? WorkerStatus::FAILED : WorkerStatus::READY));
    state.append_run_finished(status);
    state.outcome = CommonRuntimeHostV2BatchOutcome{status, status == WorkerStatus::READY ? 0 : 1};
    return *state.outcome;
}

} // namespace bruce
