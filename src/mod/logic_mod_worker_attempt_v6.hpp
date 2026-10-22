#pragma once

// Private V6 attempt seam.  Nothing below src/mod may include this header.
#include "bruce/mod/logic_mod_worker_v1.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>

namespace bruce::logic_mod_worker_v1_internal {

enum class LogicModPeakMemoryAvailabilityV6 { OBSERVED, UNAVAILABLE, NOT_APPLICABLE };

struct LogicModAttemptOutcomeV6 {
    std::string response_frame;
    std::string response_digest;
    std::string worker_status;
    std::string terminal_reason;
    std::uint64_t elapsed_ms{};
    std::optional<std::uint64_t> peak_memory_bytes;
    LogicModPeakMemoryAvailabilityV6 peak_memory_availability{LogicModPeakMemoryAvailabilityV6::NOT_APPLICABLE};
    bool reaped{};
    // Host-observed actions only; used by the V6 diagnostic sidecar.  These
    // are not worker protocol fields and never enter a canonical run record.
    bool cancel_requested{};
    bool term_sent{};
    bool kill_sent{};
};

class LogicModWorkerAttemptV6;

class LogicModPreparedWorkerV6 {
public:
    struct State;
    LogicModPreparedWorkerV6();
    ~LogicModPreparedWorkerV6();
    LogicModPreparedWorkerV6(LogicModPreparedWorkerV6&&) noexcept;
    LogicModPreparedWorkerV6& operator=(LogicModPreparedWorkerV6&&) noexcept;
    LogicModPreparedWorkerV6(const LogicModPreparedWorkerV6&) = delete;
    LogicModPreparedWorkerV6& operator=(const LogicModPreparedWorkerV6&) = delete;
private:
    explicit LogicModPreparedWorkerV6(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend LogicModPreparedWorkerV6 prepare_logic_mod_worker_v6(std::string_view, std::string_view, LogicModBudgetsV1, bool, int);
    friend class LogicModWorkerAttemptV6;
    friend LogicModWorkerAttemptV6 start_logic_mod_worker_attempt_v6(LogicModPreparedWorkerV6&&);
};

class LogicModWorkerAttemptV6 {
public:
    struct State;
    LogicModWorkerAttemptV6();
    ~LogicModWorkerAttemptV6();
    LogicModWorkerAttemptV6(LogicModWorkerAttemptV6&&) noexcept;
    LogicModWorkerAttemptV6& operator=(LogicModWorkerAttemptV6&&) noexcept;
    LogicModWorkerAttemptV6(const LogicModWorkerAttemptV6&) = delete;
    LogicModWorkerAttemptV6& operator=(const LogicModWorkerAttemptV6&) = delete;
private:
    explicit LogicModWorkerAttemptV6(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
    friend LogicModWorkerAttemptV6 start_logic_mod_worker_attempt_v6(LogicModPreparedWorkerV6&&);
    friend std::optional<LogicModAttemptOutcomeV6> poll_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6&);
    friend LogicModAttemptOutcomeV6 finish_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6&);
    friend void cancel_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6&);
    friend bool logic_mod_attempt_leader_exit_pending_v6_for_test(const LogicModWorkerAttemptV6&);
    friend bool logic_mod_attempt_wait_for_output_ready_v6_for_test(LogicModWorkerAttemptV6&, int);
};

[[nodiscard]] LogicModPreparedWorkerV6 prepare_logic_mod_worker_v6(
    std::string_view worker_bytes, std::string_view request_frame, LogicModBudgetsV1 budgets,
    bool has_payload, int payload_fd);
[[nodiscard]] LogicModWorkerAttemptV6 start_logic_mod_worker_attempt_v6(LogicModPreparedWorkerV6&& prepared);
[[nodiscard]] std::optional<LogicModAttemptOutcomeV6> poll_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt);
[[nodiscard]] LogicModAttemptOutcomeV6 finish_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt);
void cancel_logic_mod_worker_attempt_v6(LogicModWorkerAttemptV6& attempt);

// Private deterministic seam for the Linux rusage conversion; the caller
// still combines this with a procfs observation in the attempt state.
[[nodiscard]] std::optional<std::uint64_t> logic_mod_peak_from_rusage_kib_v6(std::uint64_t kib);
[[nodiscard]] bool cleanup_logic_mod_process_group_v6_for_test(pid_t pgid);
using LogicModAttemptSignalHookV6 = int (*)(pid_t, int);
void set_logic_mod_attempt_signal_hook_v6_for_test(LogicModAttemptSignalHookV6 hook);
void set_logic_mod_attempt_force_pdeath_failure_v6_for_test(bool enabled);
void set_logic_mod_attempt_force_preexec_exit_with_error_descendant_v6_for_test(bool enabled);
void set_logic_mod_attempt_force_preexec_descendant_output_v6_for_test(bool enabled);
void set_logic_mod_attempt_force_reader_eagain_once_v6_for_test(bool enabled);
void set_logic_mod_attempt_force_start_error_eagain_once_v6_for_test(bool enabled);
[[nodiscard]] bool logic_mod_attempt_leader_exit_pending_v6_for_test(const LogicModWorkerAttemptV6& attempt);
[[nodiscard]] bool logic_mod_attempt_wait_for_output_ready_v6_for_test(LogicModWorkerAttemptV6& attempt, int timeout_ms);

} // namespace bruce::logic_mod_worker_v1_internal
