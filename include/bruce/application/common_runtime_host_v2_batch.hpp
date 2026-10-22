#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bruce {

struct CommonRuntimeHostV2BatchResult;
struct CommonRuntimeHostV2BatchOutcome;

struct CommonRuntimeHostV2BatchContext {
    std::filesystem::path run_root;
    std::filesystem::path worker_executable;
    worker_v1::Budget budget;
    std::uint64_t max_workers{};
    std::uint64_t grace_ms{1000};
};

struct CommonRuntimeHostV2AttemptOptions {
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

enum class CommonRuntimeHostV2RejectionReason {
    OUTPUT_LIMIT_EXCEEDED,
    TIMEOUT,
    ARTIFACT_REFERENCE_INVALID,
    ARTIFACT_MANIFEST_INVALID,
    ARTIFACT_DIGEST_MISMATCH,
    ARTIFACT_STORE_VIOLATION,
};

class CommonRuntimeHostV2Batch {
public:
    CommonRuntimeHostV2Batch();
    ~CommonRuntimeHostV2Batch();
    CommonRuntimeHostV2Batch(CommonRuntimeHostV2Batch&&) noexcept;
    CommonRuntimeHostV2Batch& operator=(CommonRuntimeHostV2Batch&&) noexcept;
    CommonRuntimeHostV2Batch(const CommonRuntimeHostV2Batch&) = delete;
    CommonRuntimeHostV2Batch& operator=(const CommonRuntimeHostV2Batch&) = delete;

private:
    struct State;
    explicit CommonRuntimeHostV2Batch(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend CommonRuntimeHostV2Batch start_batch_v2(std::string, const CommonRuntimeHostV2BatchContext&);
    friend void start_batch_attempt_v2(CommonRuntimeHostV2Batch&, const worker_v1::WorkerRequest&);
    friend void start_batch_attempt_v2(CommonRuntimeHostV2Batch&, const worker_v1::WorkerRequest&,
                                       const CommonRuntimeHostV2AttemptOptions&);
    friend void record_attempt_rejection_v2(CommonRuntimeHostV2Batch&, std::string_view,
                                            CommonRuntimeHostV2RejectionReason);
    friend void record_scheduler_failure_v2(CommonRuntimeHostV2Batch&);
    friend std::vector<CommonRuntimeHostV2BatchResult> poll_batch_v2(CommonRuntimeHostV2Batch&);
    friend void record_attempt_acceptance_v2(CommonRuntimeHostV2Batch&, std::string_view);
    friend void record_attempt_rejection_v2(CommonRuntimeHostV2Batch&, std::string_view);
    friend void record_attempt_retry_v2(CommonRuntimeHostV2Batch&, std::string_view);
    friend void request_cancel_batch_v2(CommonRuntimeHostV2Batch&);
    friend struct CommonRuntimeHostV2BatchOutcome;
    friend CommonRuntimeHostV2BatchOutcome finish_batch_v2(CommonRuntimeHostV2Batch&);
};

struct CommonRuntimeHostV2BatchResult {
    std::string attempt_id;
    worker_v1::WorkerResult result;
    // True when the Host projected an active attempt after observing an
    // invalid owner-control marker, rather than cancelling a pending result.
    bool invalid_control{};
};

struct CommonRuntimeHostV2BatchOutcome {
    worker_v1::WorkerStatus status{worker_v1::WorkerStatus::FAILED};
    int host_exit_code{1};
};

[[nodiscard]] CommonRuntimeHostV2Batch start_batch_v2(
    std::string run_id, const CommonRuntimeHostV2BatchContext& context);
void start_batch_attempt_v2(CommonRuntimeHostV2Batch&, const worker_v1::WorkerRequest&);
void start_batch_attempt_v2(CommonRuntimeHostV2Batch&, const worker_v1::WorkerRequest&,
                            const CommonRuntimeHostV2AttemptOptions&);
void record_attempt_rejection_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id,
                                 CommonRuntimeHostV2RejectionReason);
void record_scheduler_failure_v2(CommonRuntimeHostV2Batch&);
[[nodiscard]] std::vector<CommonRuntimeHostV2BatchResult> poll_batch_v2(CommonRuntimeHostV2Batch&);
void record_attempt_acceptance_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id);
void record_attempt_rejection_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id);
void record_attempt_retry_v2(CommonRuntimeHostV2Batch&, std::string_view attempt_id);
void request_cancel_batch_v2(CommonRuntimeHostV2Batch&);
[[nodiscard]] CommonRuntimeHostV2BatchOutcome finish_batch_v2(CommonRuntimeHostV2Batch&);

} // namespace bruce
