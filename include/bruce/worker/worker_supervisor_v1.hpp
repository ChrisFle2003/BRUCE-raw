#pragma once

#include "bruce/worker/worker_boundary_v1.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace bruce::worker_v1 {

struct SupervisorOptions {
    std::filesystem::path executable;
    std::filesystem::path run_root;
    Budget budgets;
    std::uint64_t grace_ms{1000};
};

struct WorkerAttemptOutcome {
    WorkerResult result;
};

class WorkerAttemptHandle {
public:
    WorkerAttemptHandle();
    ~WorkerAttemptHandle();
    WorkerAttemptHandle(WorkerAttemptHandle&&) noexcept;
    WorkerAttemptHandle& operator=(WorkerAttemptHandle&&) noexcept;
    WorkerAttemptHandle(const WorkerAttemptHandle&) = delete;
    WorkerAttemptHandle& operator=(const WorkerAttemptHandle&) = delete;

private:
    struct State;
    explicit WorkerAttemptHandle(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend WorkerAttemptHandle start_attempt(const WorkerRequest&, const SupervisorOptions&);
    friend std::optional<WorkerAttemptOutcome> poll_attempt(WorkerAttemptHandle&);
    friend WorkerAttemptOutcome finish_attempt(WorkerAttemptHandle&);
    friend void request_cancel(WorkerAttemptHandle&);
};

[[nodiscard]] std::string encode_request_payload(const WorkerRequest& request);
[[nodiscard]] WorkerRequest decode_request_payload(std::string_view payload);
[[nodiscard]] std::string encode_result_payload(const WorkerResult& result);
[[nodiscard]] WorkerResult decode_result_payload(std::string_view payload);

[[nodiscard]] WorkerAttemptHandle start_attempt(const WorkerRequest& request,
                                                 const SupervisorOptions& options);
[[nodiscard]] std::optional<WorkerAttemptOutcome> poll_attempt(WorkerAttemptHandle& attempt);
[[nodiscard]] WorkerAttemptOutcome finish_attempt(WorkerAttemptHandle& attempt);
void request_cancel(WorkerAttemptHandle& attempt);

} // namespace bruce::worker_v1
