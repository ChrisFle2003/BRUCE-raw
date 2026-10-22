#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"
#include "bruce/mod/logic_mod_payload_v1.hpp"
#include "bruce/mod/logic_mod_result_artifact_v1.hpp"
#include "bruce/mod/logic_mod_supervision_log_v6.hpp"
#include "bruce/artifact/artifact_store_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include "logic_mod_worker_v1_internal.hpp"
#include "logic_mod_artifact_dag_execution.hpp"
#include "bruce/core/types.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <thread>

namespace bruce {
namespace {
using Clock = std::chrono::steady_clock;
std::string field(std::string_view value) { return std::to_string(value.size()) + ':' + std::string(value); }
[[noreturn]] void invalid() { throw Error("LogicModDagInvalid"); }
const LogicModDagNodeV2& node_for(const std::vector<LogicModDagNodeV2>& nodes, std::string_view id) {
    const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.id == id; });
    if (found == nodes.end()) invalid();
    return *found;
}
LogicModPublishedOutputV1 output_of(const LogicModRunV1& run) { return {run.output_manifest_id, run.output_object_index, run.output_sha256, run.output_size_bytes, run.output_codec}; }
void prepare_predecessor(const std::filesystem::path& store, const LogicModPublishedOutputV1& expected,
    const LogicModArtifactDagControlV6& control, const artifact_v4::HttpsTransportV4& transport) {
    auto local = artifact_v1::ArtifactStoreV1::open_existing(store);
    if (!local.manifest_entry_exists(expected.manifest_id)) {
        if (!control.remote_restore) throw Error("ArtifactManifestMissing");
        try { (void)local.inspect(expected.sha256); }
        catch (const Error& error) { if (std::string_view(error.what()) != "ArtifactObjectMissing") throw; }
        (void)artifact_v4::restore_from_remote_v4(expected.manifest_id, store, *control.remote_restore, transport);
    }
}
mod::LogicModPeakMemoryAvailabilityV6 availability(logic_mod_worker_v1_internal::LogicModPeakMemoryAvailabilityV6 value) {
    using From = logic_mod_worker_v1_internal::LogicModPeakMemoryAvailabilityV6;
    if (value == From::OBSERVED) return mod::LogicModPeakMemoryAvailabilityV6::OBSERVED;
    if (value == From::UNAVAILABLE) return mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE;
    return mod::LogicModPeakMemoryAvailabilityV6::NOT_APPLICABLE;
}
mod::SupervisionEventV6 event(std::uint64_t sequence, std::string event_name, std::string status,
    std::string attempt_id, std::string node_id) {
    mod::SupervisionEventV6 value; value.seq = sequence; value.event = std::move(event_name); value.status = std::move(status);
    value.attempt_id = std::move(attempt_id); value.node_id = std::move(node_id); return value;
}
bool host_cancelled(const LogicModArtifactDagControlV6& control) {
    return control.host_cancel_latch && control.host_cancel_latch->load(std::memory_order_acquire);
}
}

LogicModArtifactDagRunV6 run_logic_mod_artifact_dag_v6(const std::vector<LogicModDagNodeV2>& nodes,
    const std::filesystem::path& store, std::string_view spec, LogicModArtifactDagControlV6 control,
    const artifact_v4::HttpsTransportV4& transport) {
    if (control.report_root.empty()) throw Error("LogicModDagReportRequired");
    if (!control.max_parallel_nodes || control.max_parallel_nodes > 4) throw Error("LogicModDagParallelLimitInvalid");
    // Match V4: total-wall accounting begins before all-node preflight and
    // remains an admission budget, never a retroactive kill policy.
    const auto started_at = Clock::now();
    if (!control.max_started_workers || control.max_started_workers > 16 ||
        (control.max_total_wall_ms && (!*control.max_total_wall_ms || *control.max_total_wall_ms > 60000)) ||
        (control.cancel_after_ms && (!*control.cancel_after_ms || *control.cancel_after_ms > 60000)) ||
        (control.max_total_payload_bytes && *control.max_total_payload_bytes > 8388608)) throw Error("LogicModDagBudgetLimitInvalid");
    const auto node_order = detail::canonical_logic_mod_artifact_dag_order_v4(nodes, store, spec);
    for (const auto& node : nodes) detail::preflight_logic_mod_artifact_dag_node_v4(node);
    detail::validate_logic_mod_artifact_dag_targets_v4(nodes, store);
    // This validates the explicit URL before any Store mutation or worker fork.
    if (control.remote_restore) (void)artifact_v3::https_request_v3(*control.remote_restore, "/manifests/check.mirror", Clock::now() + std::chrono::seconds(30));
    // A queued controller can cancel a run before V6 owns any sidecar, store
    // mutation, or child.  This is intentionally after normal argument and
    // all-node preflight validation, matching the existing V6 admission edge.
    if (host_cancelled(control)) {
        LogicModArtifactDagRunV6 result;
        result.spec_digest = spec;
        result.node_order = node_order;
        result.status = "DAG_HOST_CANCELLED";
        return result;
    }
    auto log = mod::LogicModSupervisionLogV6::create(control.report_root, "logic-mod-dag-v6");
    LogicModArtifactDagRunV6 result; result.spec_digest = spec; result.node_order = node_order;
    std::set<std::string> completed; std::uint64_t started{}, payload_bytes{}, sequence{};
    while (completed.size() != nodes.size()) {
        std::vector<const LogicModDagNodeV2*> wave;
        for (const auto& id : node_order) { const auto& node = node_for(nodes, id);
            if (!completed.contains(id) && std::all_of(node.predecessors.begin(), node.predecessors.end(), [&](const auto& parent) { return completed.contains(parent); })) {
                wave.push_back(&node); if (wave.size() == control.max_parallel_nodes) break;
            }
        }
        const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_at).count());
        if (host_cancelled(control) || (control.cancel_after_ms && elapsed >= *control.cancel_after_ms) || (control.max_total_wall_ms && elapsed >= *control.max_total_wall_ms) || wave.size() > control.max_started_workers - started) {
            result.status = (host_cancelled(control) || (control.cancel_after_ms && elapsed >= *control.cancel_after_ms)) ? "DAG_HOST_CANCELLED" : "DAG_GLOBAL_BUDGET_EXHAUSTED"; return result;
        }
        // V4 freezes and prepares every entry before launching a member of the
        // wave.  Keep that all-or-nothing admission boundary in V6 as well.
        const auto wave_payload = detail::logic_mod_artifact_dag_wave_payload_bytes_v4(wave, result.nodes);
        if (control.max_total_payload_bytes && wave_payload > *control.max_total_payload_bytes - payload_bytes) { result.status = "DAG_GLOBAL_BUDGET_EXHAUSTED"; return result; }
        std::vector<LogicModDagNodeRunV2> entries; entries.reserve(wave.size());
        for (const auto* node : wave) entries.push_back(detail::prepare_logic_mod_artifact_dag_node_v4(*node, store, result.nodes,
            [&](const LogicModPublishedOutputV1& expected) { prepare_predecessor(store, expected, control, transport); }));
        struct Active { const LogicModDagNodeV2* node{}; LogicModDagNodeRunV2 entry; logic_mod_worker_v1_internal::LogicModPreparedExecutionV6 prepared; logic_mod_worker_v1_internal::LogicModWorkerAttemptV6 attempt; std::string attempt_id; std::optional<logic_mod_worker_v1_internal::LogicModAttemptOutcomeV6> outcome; };
        std::vector<Active> active; active.reserve(wave.size());
        for (std::size_t index{}; index < wave.size(); ++index) {
            const auto* node = wave[index];
            Active current; current.node = node; current.entry = std::move(entries[index]); current.attempt_id = "attempt-" + node->id;
            const auto input = node->input_artifact ? std::optional<LogicModInputV1>{{store, node->input_artifact->manifest_id, 0}} : std::nullopt;
            current.prepared = logic_mod_worker_v1_internal::prepare_logic_mod_execution_v6(node->registry, node->mod_id, node->version, node->template_id, "worker.execute", node->input, node->budgets, input, node->output_artifact);
            active.push_back(std::move(current));
        }
        for (auto& current : active) {
            // Wave admission is all-or-nothing.  If the latch changes after
            // the pre-wave boundary, finish starting this already prepared
            // wave and let the active polling path cancel and drain every
            // attempt; returning here would orphan earlier wave members.
            current.attempt = logic_mod_worker_v1_internal::start_logic_mod_execution_v6(current.prepared);
            auto start = event(++sequence, "ATTEMPT_STARTED", "RUNNING", current.attempt_id, current.node->id); start.wall_time_limit_ms = current.node->budgets.wall_ms; start.address_space_limit_bytes = current.node->budgets.memory_bytes; start.input_limit_bytes = current.node->budgets.input_bytes; start.output_limit_bytes = current.node->budgets.output_bytes; start.peak_memory_availability = mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE; log.append(start);
            ++started;
        }
        payload_bytes += wave_payload; bool cancelled{};
        for (;;) {
            const auto current_elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_at).count());
            if (!cancelled && (host_cancelled(control) || (control.cancel_after_ms && current_elapsed >= *control.cancel_after_ms))) { cancelled = true; for (auto& entry : active) if (!entry.outcome) {
                logic_mod_worker_v1_internal::cancel_logic_mod_worker_attempt_v6(entry.attempt);
                auto action = event(++sequence, "CANCEL_SENT", "CANCELLED", entry.attempt_id, entry.node->id);
                action.peak_memory_availability = mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE;
                log.append(action);
            } }
            bool done = true; for (auto& entry : active) if (!entry.outcome) { entry.outcome = logic_mod_worker_v1_internal::poll_logic_mod_worker_attempt_v6(entry.attempt); if (!entry.outcome) done = false; }
            if (done) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        bool failed{};
        for (auto& entry : active) {
            auto outcome = *entry.outcome; auto run = logic_mod_worker_v1_internal::finalize_logic_mod_execution_v6(std::move(entry.prepared), outcome, !cancelled);
            const bool accepted = !cancelled && run.accepted;
            if (!accepted) { run.accepted = false; if (cancelled) { run.status = "HOST_CANCELLED"; run.result.clear(); } failed = true; }
            entry.entry.run = std::move(run); if (entry.entry.run.accepted) entry.entry.result_artifact = output_of(entry.entry.run);
            std::string status = entry.entry.run.accepted ? "READY" : (cancelled ? "CANCELLED" : "FAILED");
            std::string reason = "WORKER_FAILED";
            if (entry.entry.run.accepted) reason = "READY_ACCEPTED";
            else if (cancelled) reason = "HOST_CANCELLED";
            else if (outcome.terminal_reason == "WALL_TIME_LIMIT" || outcome.terminal_reason == "OUTPUT_LIMIT" ||
                     outcome.terminal_reason == "PROCESS_START_FAILED" || outcome.terminal_reason == "PROCESS_CRASHED" ||
                     outcome.terminal_reason == "SUPERVISION_UNAVAILABLE") reason = outcome.terminal_reason;
            else if (outcome.terminal_reason == "EXEC_DENIED") reason = "PROTOCOL_ERROR";
            if (!entry.entry.run.accepted && outcome.terminal_reason == "READY") {
                if (entry.entry.run.status == "LogicResultDigestInvalid") reason = "DIGEST_MISMATCH";
                else if (entry.entry.run.status.starts_with("Logic")) reason = "PROTOCOL_ERROR";
            }
            if (reason == "SUPERVISION_UNAVAILABLE") status = "SUPERVISION_FAILED";
            if (cancelled && outcome.term_sent) { auto action = event(++sequence, "TERM_SENT", "CANCELLED", entry.attempt_id, entry.node->id); action.peak_memory_availability = mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE; log.append(action); }
            if (cancelled && outcome.term_sent && outcome.kill_sent) { auto action = event(++sequence, "KILL_SENT", "CANCELLED", entry.attempt_id, entry.node->id); action.peak_memory_availability = mod::LogicModPeakMemoryAvailabilityV6::UNAVAILABLE; log.append(action); }
            auto reaped = event(++sequence, "REAPED", status, entry.attempt_id, entry.node->id); reaped.terminal_reason = reason; reaped.elapsed_ms = outcome.elapsed_ms; reaped.peak_memory_availability = availability(outcome.peak_memory_availability); reaped.peak_memory_bytes = outcome.peak_memory_bytes; log.append(reaped);
            auto finished = event(++sequence, "ATTEMPT_FINISHED", status, entry.attempt_id, entry.node->id); finished.terminal_reason = reason; finished.elapsed_ms = outcome.elapsed_ms; finished.peak_memory_availability = availability(outcome.peak_memory_availability); finished.peak_memory_bytes = outcome.peak_memory_bytes; log.append(finished);
            completed.insert(entry.entry.id); result.nodes.push_back(std::move(entry.entry));
        }
        std::sort(result.nodes.begin(), result.nodes.end(), [&](const auto& left, const auto& right) { return std::find(node_order.begin(), node_order.end(), left.id) < std::find(node_order.begin(), node_order.end(), right.id); });
        if (failed) { result.status = cancelled ? "DAG_HOST_CANCELLED" : "DAG_NODE_FAILED"; return result; }
    }
    result.accepted = true; result.status = "ACCEPTED"; result.dag_id = sha256("BRUCE_LOGIC_MOD_ARTIFACT_DAG_ID_V2\n" + [&] { std::string body = field(result.spec_digest) + field(std::to_string(result.nodes.size())); for (const auto& node : result.nodes) { body += field(node.id) + field(std::to_string(node.predecessors.size())); for (const auto& parent : node.predecessors) body += field(parent); body += field(logic_mod_run_record_v1(node.run)) + field(node.result_artifact.canonical_identity()) + field(node.internal_input_artifact ? "1" : "0"); if (node.internal_input_artifact) body += field(node.internal_input_artifact->canonical_identity()); } return body; }());
    (void)logic_mod_artifact_dag_record_v2(result); return result;
}
std::string logic_mod_artifact_dag_record_v6(const LogicModArtifactDagRunV6& run) { return logic_mod_artifact_dag_record_v5(run); }
LogicModArtifactDagRunV6 read_logic_mod_artifact_dag_record_v6(std::string_view bytes) { return read_logic_mod_artifact_dag_record_v5(bytes); }
void verify_logic_mod_artifact_dag_replay_v6(const std::vector<LogicModDagNodeV2>& nodes, const std::filesystem::path& store, std::string_view spec, const LogicModArtifactDagRunV6& run) { verify_logic_mod_artifact_dag_replay_v5(nodes, store, spec, run); }
}
