#include "bruce/runtime/runtime_orchestrator_v1.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6_plan.hpp"
#include "bruce/core/json.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <thread>
#include <tuple>

namespace bruce {
namespace {
bool terminal(RuntimeOrchestratorStateV1 s) { return s == RuntimeOrchestratorStateV1::SUCCEEDED || s == RuntimeOrchestratorStateV1::FAILED || s == RuntimeOrchestratorStateV1::BLOCKED || s == RuntimeOrchestratorStateV1::CANCELLED; }
bool permitted(RuntimeOrchestratorStateV1 from, RuntimeOrchestratorStateV1 to) {
    switch (from) {
    case RuntimeOrchestratorStateV1::QUEUED: return to == RuntimeOrchestratorStateV1::ADMITTED || to == RuntimeOrchestratorStateV1::BLOCKED || to == RuntimeOrchestratorStateV1::CANCELLED;
    case RuntimeOrchestratorStateV1::ADMITTED: return to == RuntimeOrchestratorStateV1::STARTING || to == RuntimeOrchestratorStateV1::DRAINING || to == RuntimeOrchestratorStateV1::FAILED;
    case RuntimeOrchestratorStateV1::STARTING: return to == RuntimeOrchestratorStateV1::RUNNING || to == RuntimeOrchestratorStateV1::DRAINING || to == RuntimeOrchestratorStateV1::FAILED;
    case RuntimeOrchestratorStateV1::RUNNING: return to == RuntimeOrchestratorStateV1::DRAINING || to == RuntimeOrchestratorStateV1::SUCCEEDED || to == RuntimeOrchestratorStateV1::FAILED;
    // A V6 call may have returned an accepted result just before the host
    // acquires the cancellation latch.  Joining that completed call preserves
    // its success instead of falsely claiming a host cancellation.
    case RuntimeOrchestratorStateV1::DRAINING: return to == RuntimeOrchestratorStateV1::CANCELLED || to == RuntimeOrchestratorStateV1::FAILED || to == RuntimeOrchestratorStateV1::SUCCEEDED;
    default: return false;
    }
}
bool valid_limits(const RuntimeOrchestratorLimitsV1& v) { return v.max_queued_runs && v.max_active_runs && v.max_reserved_parallel_nodes && v.max_reserved_started_workers && v.max_reserved_payload_bytes; }
bool valid_reservation(const RuntimeOrchestratorReservationV1& v) { return v.max_parallel_nodes && v.max_total_wall_ms && v.max_started_workers && v.max_parallel_nodes <= 4 && v.max_total_wall_ms <= 60000 && v.max_started_workers <= 16 && v.max_total_payload_bytes <= 8388608; }
bool opaque_run_id(std::string_view v) { return !v.empty() && v.size() <= 64 && std::all_of(v.begin(), v.end(), [](unsigned char b) { return (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || b == '-'; }); }
bool fits(std::uint64_t used, std::uint64_t requested, std::uint64_t maximum) { return requested <= maximum && used <= maximum - requested; }
std::string text(RuntimeOrchestratorStateV1 v) { constexpr std::array names{"QUEUED", "ADMITTED", "STARTING", "RUNNING", "DRAINING", "SUCCEEDED", "FAILED", "BLOCKED", "CANCELLED"}; return names.at(static_cast<std::size_t>(v)); }
std::string text(RuntimeOrchestratorReasonV1 v) { constexpr std::array names{"NONE", "CANCELLED_QUEUED", "CANCELLED_ACTIVE", "RESERVATION_EXCEEDS_HOST", "V6_CANCELLED_EXTERNALLY", "V6_FAILED", "START_FAILED"}; return names.at(static_cast<std::size_t>(v)); }
}

struct RuntimeOrchestratorV1::Entry {
    RuntimeOrchestratorJobV1 job;
    RuntimeOrchestratorRunnerV1 runner;
    std::shared_ptr<std::atomic_bool> latch = std::make_shared<std::atomic_bool>(false);
    std::optional<LogicModArtifactDagRunV6> result;
    bool call_returned = false, joining = false, cancel_requested = false, reservation_held = false, terminalized = false;
    std::jthread worker;
    Entry(RuntimeOrchestratorJobV1 v, RuntimeOrchestratorRunnerV1 invoke) : job(std::move(v)), runner(std::move(invoke)) {}
};

RuntimeOrchestratorJobV1::RuntimeOrchestratorJobV1(std::string id, std::uint64_t enqueue, RuntimeOrchestratorReservationV1 reservation, std::uint64_t submit) {
    if (id.empty() || !enqueue || !valid_reservation(reservation)) throw Error("InvalidRuntimeOrchestratorJob");
    if (!submit) submit = enqueue;
    status_ = {std::move(id), enqueue, RuntimeOrchestratorStateV1::QUEUED, RuntimeOrchestratorReasonV1::NONE, submit, {}, {}, reservation, {}, {}, {}, {}};
    status_.sidecar_reference = "sidecar-v1-" + status_.run_id;
    last_transition_sequence_ = submit;
}
void RuntimeOrchestratorJobV1::transition(RuntimeOrchestratorStateV1 state, RuntimeOrchestratorReasonV1 reason, std::uint64_t seq) {
    if (!seq || seq <= last_transition_sequence_ || !permitted(status_.state, state)) throw Error("InvalidRuntimeOrchestratorTransition");
    const bool queued_cancel = status_.state == RuntimeOrchestratorStateV1::QUEUED && state == RuntimeOrchestratorStateV1::CANCELLED && reason == RuntimeOrchestratorReasonV1::CANCELLED_QUEUED;
    const bool active_drain = state == RuntimeOrchestratorStateV1::DRAINING && reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE;
    const bool active_cancel = status_.state == RuntimeOrchestratorStateV1::DRAINING && state == RuntimeOrchestratorStateV1::CANCELLED && reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE;
    if ((state == RuntimeOrchestratorStateV1::CANCELLED && !queued_cancel && !active_cancel) ||
        (state == RuntimeOrchestratorStateV1::DRAINING && !active_drain) ||
        (reason == RuntimeOrchestratorReasonV1::CANCELLED_QUEUED && !queued_cancel) ||
        (reason == RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE && !active_drain && !active_cancel) ||
        (state == RuntimeOrchestratorStateV1::BLOCKED && reason != RuntimeOrchestratorReasonV1::RESERVATION_EXCEEDS_HOST) ||
        (state == RuntimeOrchestratorStateV1::SUCCEEDED && reason != RuntimeOrchestratorReasonV1::NONE)) throw Error("InvalidRuntimeOrchestratorTransition");
    status_.state = state; status_.reason = reason; last_transition_sequence_ = seq;
    if (state == RuntimeOrchestratorStateV1::STARTING) status_.start_sequence = seq;
    if (terminal(state)) status_.finish_sequence = seq;
}
void RuntimeOrchestratorJobV1::record_v6_completion(LogicModArtifactDagRunV6&& result) noexcept {
    status_.v6_accepted = result.accepted;
    status_.v6_status = std::move(result.status);
    if (result.dag_id.empty()) status_.dag_id.reset();
    else status_.dag_id = std::move(result.dag_id);
}

RuntimeOrchestratorV1::RuntimeOrchestratorV1(RuntimeOrchestratorLimitsV1 limits, std::string, RuntimeOrchestratorRunIdGeneratorV1 ids, RuntimeOrchestratorCompletionObserverV1 observer) : limits_(limits), make_run_id_(std::move(ids)), completion_observer_(std::move(observer)) {
    if (!valid_limits(limits_)) throw Error("InvalidRuntimeOrchestratorLimits");
    if (!make_run_id_) make_run_id_ = [](std::uint64_t n) { return "orv1-" + std::to_string(n); };
}
RuntimeOrchestratorV1::~RuntimeOrchestratorV1() { for (auto& entry : jobs_) if (entry->worker.joinable()) entry->worker.join(); }

std::string RuntimeOrchestratorV1::submit(RuntimeOrchestratorReservationV1 r) { return submit(r, {}); }
std::string RuntimeOrchestratorV1::submit(LogicModArtifactDagV6Plan plan, RuntimeOrchestratorReservationV1 r) {
    if (!valid_reservation(r) || !plan.has_exact_orchestrator_reservation(r.max_parallel_nodes,
            r.max_total_wall_ms, r.max_started_workers, r.max_total_payload_bytes))
        throw Error("RuntimeOrchestratorReservationPlanMismatch");
    auto owned = std::make_shared<LogicModArtifactDagV6Plan>(std::move(plan));
    return submit(r, [owned](std::shared_ptr<const std::atomic_bool> latch) mutable { return std::move(*owned).run(std::move(latch)); });
}
std::string RuntimeOrchestratorV1::submit(RuntimeOrchestratorReservationV1 r, RuntimeOrchestratorRunnerV1 runner) {
    std::lock_guard lock(mutex_);
    if (!valid_reservation(r)) throw Error("InvalidRuntimeOrchestratorReservation");
    const auto queued = std::count_if(jobs_.begin(), jobs_.end(), [](const auto& e) { return e->job.status().state == RuntimeOrchestratorStateV1::QUEUED; });
    if (static_cast<std::uint64_t>(queued) >= limits_.max_queued_runs) throw Error("RuntimeOrchestratorQueueFull");
    if (enqueue_sequence_ == std::numeric_limits<std::uint64_t>::max() || next_run_id_ == std::numeric_limits<std::uint64_t>::max() || event_sequence_ == std::numeric_limits<std::uint64_t>::max()) throw Error("RuntimeOrchestratorSequenceOverflow");
    const auto id = make_run_id_(next_run_id_ + 1);
    if (!opaque_run_id(id)) throw Error("InvalidRuntimeOrchestratorRunId");
    if (std::any_of(jobs_.begin(), jobs_.end(), [&](const auto& e) { return e->job.status().run_id == id; })) throw Error("DuplicateRuntimeOrchestratorRunId");
    jobs_.push_back(std::make_unique<Entry>(RuntimeOrchestratorJobV1(id, ++enqueue_sequence_, r, ++event_sequence_), std::move(runner)));
    ++next_run_id_;
    return id;
}

bool RuntimeOrchestratorV1::has_queue_capacity() const {
    std::lock_guard lock(mutex_);
    const auto queued = std::count_if(jobs_.begin(), jobs_.end(), [](const auto& e) {
        return e->job.status().state == RuntimeOrchestratorStateV1::QUEUED;
    });
    return static_cast<std::uint64_t>(queued) < limits_.max_queued_runs;
}

bool RuntimeOrchestratorV1::cancel(std::string_view id) {
    std::lock_guard lock(mutex_);
    const auto found = std::find_if(jobs_.begin(), jobs_.end(), [&](const auto& e) { return e->job.status().run_id == id; });
    if (found == jobs_.end()) return false;
    auto& e = **found; const auto state = e.job.status().state;
    if (state == RuntimeOrchestratorStateV1::CANCELLED || state == RuntimeOrchestratorStateV1::DRAINING) return true;
    if (state == RuntimeOrchestratorStateV1::QUEUED) { e.job.transition(RuntimeOrchestratorStateV1::CANCELLED, RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, ++event_sequence_); return true; }
    // Once V6 has returned, its result owns the terminal classification.
    // A late cancel must neither alter the latch nor relabel an external V6
    // cancellation while poll() is waiting to join the controller thread.
    if (!e.runner || !e.reservation_held || e.call_returned || terminal(state)) return false;
    e.cancel_requested = true;
    e.latch->store(true, std::memory_order_release);
    e.job.transition(RuntimeOrchestratorStateV1::DRAINING, RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, ++event_sequence_);
    return true;
}

void RuntimeOrchestratorV1::poll() {
    for (;;) {
        Entry* completed = nullptr;
        { std::lock_guard lock(mutex_); const auto f = std::find_if(jobs_.begin(), jobs_.end(), [](const auto& e) { return e->call_returned && !e->joining && e->worker.joinable(); }); if (f != jobs_.end()) { completed = f->get(); completed->joining = true; } }
        if (!completed) break;
        completed->worker.join();
        std::lock_guard lock(mutex_);
        if (completed->terminalized) continue;
        if (!completed->result) completed->job.transition(RuntimeOrchestratorStateV1::FAILED, RuntimeOrchestratorReasonV1::START_FAILED, ++event_sequence_);
        else {
            const bool accepted = completed->result->accepted;
            const bool v6_cancelled = completed->result->status == "DAG_HOST_CANCELLED";
            // Move-only publication is noexcept: an allocation failure cannot
            // strand an already-joined run with a held reservation.
            completed->job.record_v6_completion(std::move(*completed->result));
            if (accepted) completed->job.transition(RuntimeOrchestratorStateV1::SUCCEEDED, RuntimeOrchestratorReasonV1::NONE, ++event_sequence_);
            else if (v6_cancelled && completed->cancel_requested) completed->job.transition(RuntimeOrchestratorStateV1::CANCELLED, RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, ++event_sequence_);
            else if (v6_cancelled) completed->job.transition(RuntimeOrchestratorStateV1::FAILED, RuntimeOrchestratorReasonV1::V6_CANCELLED_EXTERNALLY, ++event_sequence_);
            else completed->job.transition(RuntimeOrchestratorStateV1::FAILED, RuntimeOrchestratorReasonV1::V6_FAILED, ++event_sequence_);
        }
        const auto& r = completed->job.status().reservation;
        if (completed->reservation_held) { --active_runs_; reserved_parallel_nodes_ -= r.max_parallel_nodes; reserved_started_workers_ -= r.max_started_workers; reserved_payload_bytes_ -= r.max_total_payload_bytes; completed->reservation_held = false; }
        completed->terminalized = true;
    }
    for (;;) {
        Entry* start = nullptr;
        {
            std::lock_guard lock(mutex_);
            const auto f = std::min_element(jobs_.begin(), jobs_.end(), [](const auto& l, const auto& r) { const bool lq = l->job.status().state == RuntimeOrchestratorStateV1::QUEUED, rq = r->job.status().state == RuntimeOrchestratorStateV1::QUEUED; if (lq != rq) return lq; if (!lq) return false; return std::tie(l->job.status().enqueue_sequence, l->job.status().run_id) < std::tie(r->job.status().enqueue_sequence, r->job.status().run_id); });
            if (f == jobs_.end() || (*f)->job.status().state != RuntimeOrchestratorStateV1::QUEUED) return;
            auto& e = **f; const auto& r = e.job.status().reservation;
            if (r.max_parallel_nodes > limits_.max_reserved_parallel_nodes || r.max_started_workers > limits_.max_reserved_started_workers || r.max_total_payload_bytes > limits_.max_reserved_payload_bytes || !limits_.max_active_runs) { e.job.transition(RuntimeOrchestratorStateV1::BLOCKED, RuntimeOrchestratorReasonV1::RESERVATION_EXCEEDS_HOST, ++event_sequence_); continue; }
            if (!fits(active_runs_, 1, limits_.max_active_runs) || !fits(reserved_parallel_nodes_, r.max_parallel_nodes, limits_.max_reserved_parallel_nodes) || !fits(reserved_started_workers_, r.max_started_workers, limits_.max_reserved_started_workers) || !fits(reserved_payload_bytes_, r.max_total_payload_bytes, limits_.max_reserved_payload_bytes)) return;
            ++active_runs_; reserved_parallel_nodes_ += r.max_parallel_nodes; reserved_started_workers_ += r.max_started_workers; reserved_payload_bytes_ += r.max_total_payload_bytes;
            e.reservation_held = true; e.job.transition(RuntimeOrchestratorStateV1::ADMITTED, RuntimeOrchestratorReasonV1::NONE, ++event_sequence_); if (e.runner) start = &e;
        }
        if (!start) continue;
        try {
            std::jthread worker([this, start] {
                { std::lock_guard lock(mutex_); if (start->job.status().state == RuntimeOrchestratorStateV1::ADMITTED) { start->job.transition(RuntimeOrchestratorStateV1::STARTING, RuntimeOrchestratorReasonV1::NONE, ++event_sequence_); start->job.transition(RuntimeOrchestratorStateV1::RUNNING, RuntimeOrchestratorReasonV1::NONE, ++event_sequence_); } }
                std::optional<LogicModArtifactDagRunV6> result; try { result = start->runner(start->latch); } catch (...) {}
                { std::lock_guard lock(mutex_); start->result = std::move(result); start->call_returned = true; if (completion_observer_) try { completion_observer_(start->job.status().run_id); } catch (...) {} }
            });
            // Publication is protected by the same lock used by poll() to
            // inspect joinability.  A very fast runner may report completion
            // before this assignment, but cannot be terminalized until its
            // controller-owned thread is published and joined.
            { std::lock_guard lock(mutex_); start->worker = std::move(worker); }
        } catch (...) {
            std::lock_guard lock(mutex_); const auto& r = start->job.status().reservation; start->job.transition(RuntimeOrchestratorStateV1::FAILED, RuntimeOrchestratorReasonV1::START_FAILED, ++event_sequence_); --active_runs_; reserved_parallel_nodes_ -= r.max_parallel_nodes; reserved_started_workers_ -= r.max_started_workers; reserved_payload_bytes_ -= r.max_total_payload_bytes; start->reservation_held = false; start->terminalized = true;
        }
    }
}

RuntimeOrchestratorStatusV1 RuntimeOrchestratorV1::status(std::string_view id) const { std::lock_guard lock(mutex_); const auto f = std::find_if(jobs_.begin(), jobs_.end(), [&](const auto& e) { return e->job.status().run_id == id; }); if (f == jobs_.end()) throw Error("RuntimeOrchestratorRunNotFound"); return (*f)->job.status(); }
std::vector<RuntimeOrchestratorStatusV1> RuntimeOrchestratorV1::statuses() const { std::lock_guard lock(mutex_); std::vector<RuntimeOrchestratorStatusV1> out; out.reserve(jobs_.size()); for (const auto& e : jobs_) out.push_back(e->job.status()); return out; }
std::string runtime_orchestrator_status_v1(const RuntimeOrchestratorStatusV1& s) {
    const auto optional_number = [](const std::optional<std::uint64_t>& value) { return value ? std::to_string(*value) : "null"; };
    const auto optional_text = [](const std::optional<std::string>& value) { return value ? json_string(*value) : "null"; };
    return "{\"contract\":\"runtime-orchestrator-v1\",\"run_id\":" + json_string(s.run_id) +
        ",\"enqueue_seq\":" + std::to_string(s.enqueue_sequence) + ",\"submit_seq\":" + std::to_string(s.submit_sequence) +
        ",\"start_seq\":" + optional_number(s.start_sequence) + ",\"finish_seq\":" + optional_number(s.finish_sequence) +
        ",\"state\":" + json_string(text(s.state)) + ",\"reason\":" + json_string(text(s.reason)) +
        ",\"reservation\":{\"max_parallel_nodes\":" + std::to_string(s.reservation.max_parallel_nodes) +
        ",\"max_total_wall_ms\":" + std::to_string(s.reservation.max_total_wall_ms) +
        ",\"max_started_workers\":" + std::to_string(s.reservation.max_started_workers) +
        ",\"max_total_payload_bytes\":" + std::to_string(s.reservation.max_total_payload_bytes) +
        "},\"v6_accepted\":" + (s.v6_accepted ? (*s.v6_accepted ? "true" : "false") : "null") +
        ",\"v6_status\":" + optional_text(s.v6_status) + ",\"dag_id\":" + optional_text(s.dag_id) +
        ",\"sidecar_ref\":" + optional_text(s.sidecar_reference) + "}";
}
}
