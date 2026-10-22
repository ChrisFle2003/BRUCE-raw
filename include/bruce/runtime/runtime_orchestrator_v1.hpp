#pragma once

#include "bruce/core/types.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace bruce {
class LogicModArtifactDagV6Plan;
struct RuntimeOrchestratorLimitsV1 {
    std::uint64_t max_queued_runs{};
    std::uint64_t max_active_runs{};
    std::uint64_t max_reserved_parallel_nodes{};
    std::uint64_t max_reserved_started_workers{};
    std::uint64_t max_reserved_payload_bytes{};
};

struct RuntimeOrchestratorReservationV1 {
    std::uint64_t max_parallel_nodes{};
    std::uint64_t max_total_wall_ms{};
    std::uint64_t max_started_workers{};
    std::uint64_t max_total_payload_bytes{};
    bool operator==(const RuntimeOrchestratorReservationV1&) const = default;
};

enum class RuntimeOrchestratorStateV1 { QUEUED, ADMITTED, STARTING, RUNNING, DRAINING, SUCCEEDED, FAILED, BLOCKED, CANCELLED };
enum class RuntimeOrchestratorReasonV1 { NONE, CANCELLED_QUEUED, CANCELLED_ACTIVE, RESERVATION_EXCEEDS_HOST, V6_CANCELLED_EXTERNALLY, V6_FAILED, START_FAILED };

struct RuntimeOrchestratorStatusV1 {
    std::string run_id;
    std::uint64_t enqueue_sequence{};
    RuntimeOrchestratorStateV1 state{};
    RuntimeOrchestratorReasonV1 reason{};
    std::uint64_t submit_sequence{};
    std::optional<std::uint64_t> start_sequence;
    std::optional<std::uint64_t> finish_sequence;
    RuntimeOrchestratorReservationV1 reservation;
    std::optional<bool> v6_accepted;
    std::optional<std::string> v6_status;
    std::optional<std::string> dag_id;
    std::optional<std::string> sidecar_reference;
};
using RuntimeOrchestratorRunIdGeneratorV1 = std::function<std::string(std::uint64_t)>;
// Test seam for the controller lifecycle.  Production submits an owned V6
// plan below; both forms receive only the private V6 cancellation latch.
using RuntimeOrchestratorRunnerV1 = std::function<LogicModArtifactDagRunV6(std::shared_ptr<const std::atomic_bool>)>;
// Test seam: observes the point at which the controller has recorded that a
// runner returned. It is not exposed through the runtime protocol.
using RuntimeOrchestratorCompletionObserverV1 = std::function<void(std::string_view)>;

class RuntimeOrchestratorJobV1 {
public:
    RuntimeOrchestratorJobV1(std::string run_id, std::uint64_t enqueue_sequence, RuntimeOrchestratorReservationV1,
        std::uint64_t submit_sequence = 0);
    const RuntimeOrchestratorStatusV1& status() const noexcept { return status_; }
    void transition(RuntimeOrchestratorStateV1, RuntimeOrchestratorReasonV1, std::uint64_t sequence);
    void record_v6_completion(LogicModArtifactDagRunV6&&) noexcept;
private:
    RuntimeOrchestratorStatusV1 status_;
    std::uint64_t last_transition_sequence_{};
};

class RuntimeOrchestratorV1 {
public:
    explicit RuntimeOrchestratorV1(RuntimeOrchestratorLimitsV1, std::string host_epoch = {}, RuntimeOrchestratorRunIdGeneratorV1 = {}, RuntimeOrchestratorCompletionObserverV1 = {});
    ~RuntimeOrchestratorV1();
    RuntimeOrchestratorV1(const RuntimeOrchestratorV1&) = delete;
    RuntimeOrchestratorV1& operator=(const RuntimeOrchestratorV1&) = delete;
    std::string submit(RuntimeOrchestratorReservationV1);
    std::string submit(RuntimeOrchestratorReservationV1, RuntimeOrchestratorRunnerV1);
    std::string submit(LogicModArtifactDagV6Plan, RuntimeOrchestratorReservationV1);
    bool cancel(std::string_view run_id);
    // Admission-only query for the foreground host.  It deliberately reveals
    // neither queued plans nor their metadata.
    bool has_queue_capacity() const;
    void poll();
    RuntimeOrchestratorStatusV1 status(std::string_view run_id) const;
    std::vector<RuntimeOrchestratorStatusV1> statuses() const;
private:
    struct Entry;
    RuntimeOrchestratorLimitsV1 limits_;
    // All queue inspection, sequence allocation and admission reservations
    // share this lock. A run is either still QUEUED or owns every declared
    // resource slot.
    mutable std::mutex mutex_;
    // Entries have stable addresses for controller-owned threads.  The mutex
    // precedes them so it outlives their joining destruction.
    std::vector<std::unique_ptr<Entry>> jobs_;
    std::uint64_t active_runs_ = 0;
    std::uint64_t reserved_parallel_nodes_ = 0;
    std::uint64_t reserved_started_workers_ = 0;
    std::uint64_t reserved_payload_bytes_ = 0;
    std::uint64_t enqueue_sequence_ = 0;
    std::uint64_t event_sequence_ = 0;
    std::uint64_t next_run_id_ = 0;
    RuntimeOrchestratorRunIdGeneratorV1 make_run_id_;
    RuntimeOrchestratorCompletionObserverV1 completion_observer_;
};

// This renderer is the only Task-2 public projection.  It intentionally has
// no plan, path, URL, process, payload, signature or telemetry parameter.
std::string runtime_orchestrator_status_v1(const RuntimeOrchestratorStatusV1&);
}
