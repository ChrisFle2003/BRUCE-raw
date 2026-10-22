#include "bruce/runtime/runtime_orchestrator_v1.hpp"
#include "bruce/mod/logic_mod_artifact_dag_v6_plan.hpp"
#include "bruce/mod/logic_mod_supervision_log_v6.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <type_traits>
#include <filesystem>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }
template<class F> void rejects(F&& fn, std::string_view expected) {
    try { fn(); } catch (const bruce::Error& error) {
        check(error.what() == expected, "unexpected rejection"); return;
    }
    throw bruce::Error("missing rejection");
}

bruce::RuntimeOrchestratorLimitsV1 limits() { return {4, 2, 8, 16, 4096}; }
bruce::RuntimeOrchestratorReservationV1 reservation() { return {2, 1000, 4, 512}; }

void valid_and_invalid_state_transitions_are_closed() {
    bruce::RuntimeOrchestratorJobV1 job{"run-001", 1, reservation()};
    check(job.status().state == bruce::RuntimeOrchestratorStateV1::QUEUED, "initial queue state");
    job.transition(bruce::RuntimeOrchestratorStateV1::ADMITTED, {}, 2);
    job.transition(bruce::RuntimeOrchestratorStateV1::STARTING, {}, 3);
    job.transition(bruce::RuntimeOrchestratorStateV1::RUNNING, {}, 4);
    job.transition(bruce::RuntimeOrchestratorStateV1::DRAINING, bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, 5);
    job.transition(bruce::RuntimeOrchestratorStateV1::CANCELLED, bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, 6);
    check(job.status().finish_sequence == 6, "finish sequence missing");
    rejects([&] { job.transition(bruce::RuntimeOrchestratorStateV1::RUNNING, {}, 7); }, "InvalidRuntimeOrchestratorTransition");
    bruce::RuntimeOrchestratorJobV1 direct{"run-002", 2, reservation()};
    rejects([&] { direct.transition(bruce::RuntimeOrchestratorStateV1::RUNNING, {}, 3); }, "InvalidRuntimeOrchestratorTransition");
    bruce::RuntimeOrchestratorJobV1 reason{"run-003", 3, reservation()};
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::CANCELLED, bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, 4); }, "InvalidRuntimeOrchestratorTransition");
    reason.transition(bruce::RuntimeOrchestratorStateV1::ADMITTED, {}, 4);
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::STARTING, bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, 5); }, "InvalidRuntimeOrchestratorTransition");
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::FAILED, bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, 5); }, "InvalidRuntimeOrchestratorTransition");
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::STARTING, {}, 4); }, "InvalidRuntimeOrchestratorTransition");
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::DRAINING, bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, 5); }, "InvalidRuntimeOrchestratorTransition");
    reason.transition(bruce::RuntimeOrchestratorStateV1::DRAINING, bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, 5);
    rejects([&] { reason.transition(bruce::RuntimeOrchestratorStateV1::CANCELLED, bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, 6); }, "InvalidRuntimeOrchestratorTransition");
    reason.transition(bruce::RuntimeOrchestratorStateV1::CANCELLED, bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, 6);
}

std::size_t descriptor_count() {
    DIR* directory = opendir("/proc/self/fd");
    check(directory != nullptr, "fd directory unavailable");
    std::size_t count{};
    while (const dirent* entry = readdir(directory)) if (std::string_view(entry->d_name) != "." && std::string_view(entry->d_name) != "..") ++count;
    closedir(directory); return count;
}

void report_root_preflight_does_not_leak_failed_existing_run_descriptors() {
    char pattern[] = "/tmp/bruce-orchestrator-report-fd-XXXXXX";
    check(::mkdtemp(pattern) != nullptr, "mkdtemp failed");
    const std::filesystem::path report(pattern);
    const auto run = std::filesystem::path(report.string() + ".run");
    std::filesystem::create_directory(run);
    check(::chmod(run.c_str(), 0755) == 0, "run chmod failed");
    const auto before = descriptor_count();
    for (unsigned index{}; index != 64; ++index)
        rejects([&] { bruce::validate_logic_mod_artifact_dag_v6_plan_report_root(report); }, "InvalidLogicModArtifactDagV6Plan");
    check(descriptor_count() == before, "report preflight leaked descriptors");
    check(::chmod(run.c_str(), 0700) == 0, "restore run chmod failed");
    std::filesystem::remove_all(report);
    std::filesystem::remove_all(run);
}

void queue_cancel_is_idempotent_and_terminal() {
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host-test");
    const auto first = orchestrator.submit(reservation());
    const auto before = orchestrator.status(first);
    check(orchestrator.cancel(first), "initial cancel rejected");
    check(orchestrator.cancel(first), "idempotent cancel rejected");
    const auto after = orchestrator.status(first);
    check(after.state == bruce::RuntimeOrchestratorStateV1::CANCELLED, "queue cancel state");
    check(after.reason == bruce::RuntimeOrchestratorReasonV1::CANCELLED_QUEUED, "queue cancel reason");
    check(after.finish_sequence > before.submit_sequence, "finish sequence missing");
}

void submission_is_deterministic_and_validates_before_sequence() {
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host-test");
    auto bad = reservation(); bad.max_parallel_nodes = 0;
    rejects([&] { (void)orchestrator.submit(bad); }, "InvalidRuntimeOrchestratorReservation");
    const auto first = orchestrator.submit(reservation());
    const auto second = orchestrator.submit(reservation());
    const auto first_status = orchestrator.status(first);
    const auto second_status = orchestrator.status(second);
    check(first_status.enqueue_sequence == 1 && second_status.enqueue_sequence == 2, "enqueue order not deterministic");
    check(first != second, "opaque ids duplicated");
    check(first_status.run_id.find("host-test") == std::string::npos, "host identity leaked in run id");
}

void duplicate_or_malformed_generated_ids_do_not_consume_enqueue_sequence() {
    bruce::RuntimeOrchestratorV1 duplicate(limits(), "host", [](std::uint64_t) { return std::string("run-fixed"); });
    const auto first = duplicate.submit(reservation());
    rejects([&] { (void)duplicate.submit(reservation()); }, "DuplicateRuntimeOrchestratorRunId");
    check(duplicate.status(first).enqueue_sequence == 1, "duplicate changed first sequence");
    bruce::RuntimeOrchestratorV1 malformed(limits(), "host", [](std::uint64_t) { return std::string("unsafe/path"); });
    rejects([&] { (void)malformed.submit(reservation()); }, "InvalidRuntimeOrchestratorRunId");
    check(malformed.statuses().empty(), "malformed id allocated job");
}

void limits_and_reservations_reject_bad_values_and_overflow() {
    auto bad_limits = limits(); bad_limits.max_active_runs = 0;
    rejects([&] { bruce::RuntimeOrchestratorV1 ignored(bad_limits, "host"); }, "InvalidRuntimeOrchestratorLimits");
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host");
    auto bad = reservation(); bad.max_total_wall_ms = 0;
    rejects([&] { (void)orchestrator.submit(bad); }, "InvalidRuntimeOrchestratorReservation");
    bad = reservation(); bad.max_total_payload_bytes = std::numeric_limits<std::uint64_t>::max();
    rejects([&] { (void)orchestrator.submit(bad); }, "InvalidRuntimeOrchestratorReservation");
}

void zero_payload_reservation_is_valid_but_zero_wall_is_not() {
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host");
    auto no_payload = reservation(); no_payload.max_total_payload_bytes = 0;
    check(!orchestrator.submit(no_payload).empty(), "zero payload reservation rejected");
    auto no_wall = reservation(); no_wall.max_total_wall_ms = 0;
    rejects([&] { (void)orchestrator.submit(no_wall); }, "InvalidRuntimeOrchestratorReservation");
}

void queue_capacity_counts_only_queued_jobs() {
    auto limited = limits(); limited.max_queued_runs = 1;
    bruce::RuntimeOrchestratorV1 orchestrator(limited, "host");
    const auto first = orchestrator.submit(reservation());
    rejects([&] { (void)orchestrator.submit(reservation()); }, "RuntimeOrchestratorQueueFull");
    check(orchestrator.cancel(first), "queue cancel rejected");
    check(!orchestrator.submit(reservation()).empty(), "terminal job still consumed queue capacity");
}

void fifo_admission_reserves_all_declared_resources() {
    auto constrained = limits();
    constrained.max_active_runs = 2;
    constrained.max_reserved_parallel_nodes = 4;
    constrained.max_reserved_started_workers = 8;
    constrained.max_reserved_payload_bytes = 1024;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    const auto first = orchestrator.submit(reservation());
    const auto second = orchestrator.submit(reservation());
    const auto third = orchestrator.submit(reservation());
    orchestrator.poll();
    check(orchestrator.status(first).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "first run was not admitted");
    check(orchestrator.status(second).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "second run was not admitted");
    check(orchestrator.status(third).state == bruce::RuntimeOrchestratorStateV1::QUEUED, "active reservation exceeded host limit");
    check(!orchestrator.status(first).start_sequence, "admission falsely claimed a start");
}

void a_nonfitting_fifo_head_blocks_a_smaller_younger_run() {
    auto constrained = limits();
    constrained.max_active_runs = 1;
    constrained.max_reserved_parallel_nodes = 2;
    constrained.max_reserved_started_workers = 4;
    constrained.max_reserved_payload_bytes = 512;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    const auto active = orchestrator.submit(reservation());
    orchestrator.poll();
    check(orchestrator.status(active).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "fixture active run missing");
    const auto head = orchestrator.submit(reservation());
    auto small = reservation();
    small.max_parallel_nodes = 1;
    small.max_started_workers = 1;
    small.max_total_payload_bytes = 1;
    const auto younger = orchestrator.submit(small);
    orchestrator.poll();
    check(orchestrator.status(head).state == bruce::RuntimeOrchestratorStateV1::QUEUED, "non-fitting head changed state");
    check(orchestrator.status(younger).state == bruce::RuntimeOrchestratorStateV1::QUEUED, "younger run bypassed FIFO head");
}

void a_nonfitting_resource_head_never_backfills_with_active_capacity() {
    struct Case {
        std::string_view name;
        void (*adjust)(bruce::RuntimeOrchestratorLimitsV1&, bruce::RuntimeOrchestratorReservationV1&, bruce::RuntimeOrchestratorReservationV1&);
    };
    const std::array cases{
        Case{"parallel nodes", [](auto& value, auto&, auto& head) { value.max_reserved_parallel_nodes = 3; head.max_parallel_nodes = 3; }},
        Case{"started workers", [](auto& value, auto& active, auto& head) { value.max_reserved_started_workers = 3; active.max_started_workers = 1; head.max_started_workers = 3; }},
        Case{"payload bytes", [](auto& value, auto& active, auto& head) { value.max_reserved_payload_bytes = 300; active.max_total_payload_bytes = 100; head.max_total_payload_bytes = 300; }},
    };
    for (const auto& scenario : cases) {
        auto constrained = limits();
        constrained.max_active_runs = 3; // The active-run slot is deliberately not the blocker.
        constrained.max_reserved_parallel_nodes = 8;
        constrained.max_reserved_started_workers = 16;
        constrained.max_reserved_payload_bytes = 4096;
        auto active_reservation = reservation();
        auto head = reservation();
        scenario.adjust(constrained, active_reservation, head);
        bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
        const auto active = orchestrator.submit(active_reservation);
        orchestrator.poll();
        check(orchestrator.status(active).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "resource fixture active run missing");
        const auto blocked_head = orchestrator.submit(head);
        auto younger = reservation();
        younger.max_parallel_nodes = 1;
        younger.max_started_workers = 1;
        younger.max_total_payload_bytes = 1;
        const auto smaller = orchestrator.submit(younger);
        orchestrator.poll();
        check(orchestrator.status(blocked_head).state == bruce::RuntimeOrchestratorStateV1::QUEUED, scenario.name.data());
        check(orchestrator.status(smaller).state == bruce::RuntimeOrchestratorStateV1::QUEUED, "younger run backfilled despite free active slot");
    }
}

void impossible_head_is_blocked_and_does_not_stop_following_admission() {
    auto constrained = limits();
    constrained.max_active_runs = 1;
    constrained.max_reserved_parallel_nodes = 1;
    constrained.max_reserved_started_workers = 4;
    constrained.max_reserved_payload_bytes = 512;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    auto impossible = reservation();
    impossible.max_parallel_nodes = 2;
    const auto first = orchestrator.submit(impossible);
    auto fitting = reservation();
    fitting.max_parallel_nodes = 1;
    const auto second = orchestrator.submit(fitting);
    orchestrator.poll();
    const auto first_status = orchestrator.status(first);
    check(first_status.state == bruce::RuntimeOrchestratorStateV1::BLOCKED, "impossible head not blocked");
    check(first_status.reason == bruce::RuntimeOrchestratorReasonV1::RESERVATION_EXCEEDS_HOST, "wrong blocked reason");
    check(orchestrator.status(second).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "following fitting run did not progress");
}

void concurrent_submit_and_poll_never_admit_a_run_twice() {
    auto constrained = limits();
    constrained.max_queued_runs = 64;
    constrained.max_active_runs = 2;
    constrained.max_reserved_parallel_nodes = 4;
    constrained.max_reserved_started_workers = 8;
    constrained.max_reserved_payload_bytes = 1024;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    std::thread submitter([&] { for (unsigned index{}; index != 32; ++index) (void)orchestrator.submit(reservation()); });
    std::thread poller([&] { for (unsigned index{}; index != 128; ++index) orchestrator.poll(); });
    submitter.join();
    poller.join();
    orchestrator.poll();
    std::uint64_t admitted{};
    for (const auto& status : orchestrator.statuses()) if (status.state == bruce::RuntimeOrchestratorStateV1::ADMITTED) ++admitted;
    check(admitted == 2, "concurrent admission exceeded or duplicated active slots");
}

bool wait_until(const std::function<bool()>& predicate) {
    for (unsigned count{}; count != 2000; ++count) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

bruce::LogicModArtifactDagRunV6 cancelled_v6_run() {
    bruce::LogicModArtifactDagRunV6 run;
    run.status = "DAG_HOST_CANCELLED";
    return run;
}

bruce::LogicModArtifactDagRunV6 accepted_v6_run() {
    bruce::LogicModArtifactDagRunV6 run;
    run.accepted = true;
    run.status = "ACCEPTED";
    run.dag_id = "opaque-dag-id";
    return run;
}

void active_cancel_latches_once_drains_before_releasing_reservation() {
    auto constrained = limits();
    constrained.max_active_runs = 1;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    auto entered = std::make_shared<std::atomic_bool>(false);
    auto release = std::make_shared<std::atomic_bool>(false);
    struct ReleaseOnExit {
        std::shared_ptr<std::atomic_bool> release;
        ~ReleaseOnExit() { release->store(true, std::memory_order_release); }
    } release_on_exit{release};
    auto saw_latch = std::make_shared<std::atomic_bool>(false);
    const auto active = orchestrator.submit(reservation(), [entered, release, saw_latch](std::shared_ptr<const std::atomic_bool> latch) {
        entered->store(true, std::memory_order_release);
        while (!release->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        saw_latch->store(latch && latch->load(std::memory_order_acquire), std::memory_order_release);
        return cancelled_v6_run();
    });
    const auto waiting = orchestrator.submit(reservation());
    orchestrator.poll();
    check(wait_until([&] { return entered->load(std::memory_order_acquire); }), "runner never started");
    check(orchestrator.cancel(active), "active cancel rejected");
    check(orchestrator.cancel(active), "active cancel was not idempotent");
    check(orchestrator.status(active).state == bruce::RuntimeOrchestratorStateV1::DRAINING, "active cancel did not drain");
    orchestrator.poll();
    check(orchestrator.status(waiting).state == bruce::RuntimeOrchestratorStateV1::QUEUED, "reservation released before V6 returned");
    release->store(true, std::memory_order_release);
    check(wait_until([&] { orchestrator.poll(); return orchestrator.status(active).state == bruce::RuntimeOrchestratorStateV1::CANCELLED; }), "cancelled V6 run was not joined");
    const auto finished = orchestrator.status(active);
    check(finished.reason == bruce::RuntimeOrchestratorReasonV1::CANCELLED_ACTIVE, "active cancel reason");
    check(saw_latch->load(std::memory_order_acquire), "runner did not receive host latch");
    check(orchestrator.status(waiting).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "reservation was not released after join");
}

void external_v6_cancel_is_failed_not_relabelled_as_host_cancel() {
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host");
    const auto run_id = orchestrator.submit(reservation(), [](std::shared_ptr<const std::atomic_bool>) { return cancelled_v6_run(); });
    orchestrator.poll();
    check(wait_until([&] { orchestrator.poll(); return orchestrator.status(run_id).finish_sequence.has_value(); }), "external completion did not join");
    const auto status = orchestrator.status(run_id);
    check(status.state == bruce::RuntimeOrchestratorStateV1::FAILED, "external V6 cancel relabelled as host cancel");
    check(status.reason == bruce::RuntimeOrchestratorReasonV1::V6_CANCELLED_EXTERNALLY, "external V6 cancel reason");
    check(status.v6_status == "DAG_HOST_CANCELLED" && status.v6_accepted == false, "external V6 completion missing from status");
}

void late_cancel_cannot_relabel_an_already_returned_external_v6_cancel() {
    auto completion_observed = std::make_shared<std::atomic_bool>(false);
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host", {}, [completion_observed](std::string_view) {
        completion_observed->store(true, std::memory_order_release);
    });
    const auto run_id = orchestrator.submit(reservation(), [](std::shared_ptr<const std::atomic_bool>) {
        return cancelled_v6_run();
    });
    orchestrator.poll();
    check(wait_until([&] { return completion_observed->load(std::memory_order_acquire); }), "controller did not record the V6 return");
    check(!orchestrator.cancel(run_id), "late cancel changed a returned V6 result");
    check(wait_until([&] { orchestrator.poll(); return orchestrator.status(run_id).finish_sequence.has_value(); }), "returned V6 cancel was not joined");
    const auto status = orchestrator.status(run_id);
    check(status.state == bruce::RuntimeOrchestratorStateV1::FAILED, "late cancel relabelled external V6 cancellation");
    check(status.reason == bruce::RuntimeOrchestratorReasonV1::V6_CANCELLED_EXTERNALLY, "late cancel reason");
}

void completed_success_wins_a_cancel_after_v6_return_before_join() {
    auto completion_observed = std::make_shared<std::atomic_bool>(false);
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host", {}, [completion_observed](std::string_view) {
        completion_observed->store(true, std::memory_order_release);
    });
    const auto run_id = orchestrator.submit(reservation(), [](std::shared_ptr<const std::atomic_bool>) {
        return accepted_v6_run();
    });
    orchestrator.poll();
    check(wait_until([&] { return completion_observed->load(std::memory_order_acquire); }), "controller did not record the V6 return");
    check(!orchestrator.cancel(run_id), "cancel raced after V6 had already returned");
    check(wait_until([&] { orchestrator.poll(); return orchestrator.status(run_id).finish_sequence.has_value(); }), "completed V6 call not joined");
    const auto status = orchestrator.status(run_id);
    check(status.state == bruce::RuntimeOrchestratorStateV1::SUCCEEDED, "completed success was overwritten by later cancel");
    check(status.reason == bruce::RuntimeOrchestratorReasonV1::NONE, "successful completion has cancel reason");
    check(status.v6_accepted == true && status.dag_id == "opaque-dag-id", "accepted V6 completion missing from status");
}

void long_v6_dag_id_is_moved_before_reservation_release() {
    auto constrained = limits();
    constrained.max_active_runs = 1;
    bruce::RuntimeOrchestratorV1 orchestrator(constrained, "host");
    const auto first = orchestrator.submit(reservation(), [](std::shared_ptr<const std::atomic_bool>) {
        auto run = accepted_v6_run();
        run.dag_id.assign(65536, 'd');
        return run;
    });
    const auto waiting = orchestrator.submit(reservation());
    orchestrator.poll();
    check(wait_until([&] { orchestrator.poll(); return orchestrator.status(first).finish_sequence.has_value(); }), "long DAG-id completion was not joined");
    const auto finished = orchestrator.status(first);
    check(finished.state == bruce::RuntimeOrchestratorStateV1::SUCCEEDED && finished.dag_id && finished.dag_id->size() == 65536, "long DAG-id completion was not retained");
    check(orchestrator.status(waiting).state == bruce::RuntimeOrchestratorStateV1::ADMITTED, "long DAG-id completion held its reservation");
}

void public_status_is_redacted_and_metadata_is_immutable() {
    bruce::RuntimeOrchestratorV1 orchestrator(limits(), "host-test");
    auto requested = reservation();
    const auto id = orchestrator.submit(requested);
    requested.max_parallel_nodes = 7;
    const auto status = orchestrator.status(id);
    check(status.reservation.max_parallel_nodes == 2, "reservation was not copied");
    const auto rendered = bruce::runtime_orchestrator_status_v1(status);
    // Reservation values are intentionally public; no payload content, path,
    // URL, process identity, credential or signature may appear.
    for (const auto forbidden : {"/tmp", "https://", "pid", "pgid", "secret", "credential=", "ed25519", "frame"})
        check(rendered.find(forbidden) == std::string::npos, "status leaked forbidden data");
    check(rendered.find(status.run_id) != std::string::npos, "status omitted opaque id");
}

void plan_adapter_is_move_only_and_rejects_incomplete_inputs() {
    static_assert(!std::is_copy_constructible_v<bruce::LogicModArtifactDagV6Plan>);
    bruce::LogicModArtifactDagV6PlanRequest request;
    rejects([&] { (void)bruce::prepare_logic_mod_artifact_dag_v6_plan(std::move(request)); }, "InvalidLogicModArtifactDagV6Plan");
}

void plan_reservation_must_match_every_finite_v6_control_value() {
    bruce::LogicModArtifactDagControlV6 control;
    control.max_parallel_nodes = 2;
    control.max_total_wall_ms = 1000;
    control.max_started_workers = 4;
    control.max_total_payload_bytes = 512;
    check(bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 2, 1000, 4, 512), "exact V6 reservation rejected");
    check(!bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 1, 1000, 4, 512), "underdeclared parallel limit accepted");
    check(!bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 2, 999, 4, 512), "underdeclared wall limit accepted");
    check(!bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 2, 1000, 3, 512), "underdeclared worker limit accepted");
    check(!bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 2, 1000, 4, 511), "underdeclared payload limit accepted");
    control.max_total_payload_bytes.reset();
    check(!bruce::logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(control, 2, 1000, 4, 512), "non-finite payload control accepted");
}

void report_root_preflight_rejects_a_symlinked_ancestor() {
    char pattern[] = "/tmp/bruce-orchestrator-report-XXXXXX";
    check(::mkdtemp(pattern) != nullptr, "mkdtemp failed");
    const std::filesystem::path root(pattern);
    const auto target = root / "target";
    const auto link = root / "link";
    std::filesystem::create_directory(target);
    std::filesystem::create_directory_symlink(target, link);
    rejects([&] { bruce::validate_logic_mod_artifact_dag_v6_plan_report_root(link); }, "InvalidLogicModArtifactDagV6Plan");
    std::filesystem::remove_all(root);
}

void report_root_preflight_allows_readonly_intermediate_directory() {
    char pattern[] = "/tmp/bruce-orchestrator-report-mode-XXXXXX";
    check(::mkdtemp(pattern) != nullptr, "mkdtemp failed");
    const std::filesystem::path root(pattern);
    const auto permissive = root / "permissive";
    const auto report = permissive / "report";
    std::filesystem::create_directory(permissive);
    std::filesystem::create_directory(report);
    check(::chmod(permissive.c_str(), 0755) == 0, "chmod failed");
    check(::chmod(report.c_str(), 0700) == 0, "report chmod failed");
    bruce::validate_logic_mod_artifact_dag_v6_plan_report_root(report);
    check(::chmod(permissive.c_str(), 0700) == 0, "restore chmod failed");
    std::filesystem::remove_all(root);
}

void report_root_preflight_is_equivalent_for_absolute_and_relative_paths() {
    char pattern[] = "/tmp/bruce-orchestrator-report-relative-XXXXXX";
    check(::mkdtemp(pattern) != nullptr, "mkdtemp failed");
    const std::filesystem::path root(pattern);
    const auto unsafe = root / "unsafe";
    const auto report = unsafe / "report";
    std::filesystem::create_directory(unsafe);
    std::filesystem::create_directory(report);
    check(::chmod(unsafe.c_str(), 0755) == 0, "chmod failed");
    check(::chmod(report.c_str(), 0700) == 0, "report chmod failed");
    bruce::validate_logic_mod_artifact_dag_v6_plan_report_root(report);
    const auto old = std::filesystem::current_path();
    std::filesystem::current_path(root);
    try {
        bruce::validate_logic_mod_artifact_dag_v6_plan_report_root("unsafe/report");
    } catch (...) { std::filesystem::current_path(old); std::filesystem::remove_all(root); throw; }
    std::filesystem::current_path(old);
    check(::chmod(unsafe.c_str(), 0700) == 0, "restore chmod failed");
    std::filesystem::remove_all(root);
}
}

int main() {
    try {
        valid_and_invalid_state_transitions_are_closed();
        queue_cancel_is_idempotent_and_terminal();
        submission_is_deterministic_and_validates_before_sequence();
        duplicate_or_malformed_generated_ids_do_not_consume_enqueue_sequence();
        limits_and_reservations_reject_bad_values_and_overflow();
        zero_payload_reservation_is_valid_but_zero_wall_is_not();
        queue_capacity_counts_only_queued_jobs();
        fifo_admission_reserves_all_declared_resources();
        a_nonfitting_fifo_head_blocks_a_smaller_younger_run();
        a_nonfitting_resource_head_never_backfills_with_active_capacity();
        impossible_head_is_blocked_and_does_not_stop_following_admission();
        concurrent_submit_and_poll_never_admit_a_run_twice();
        // Task-4 lifecycle regressions follow the Task-3 state contracts.
        active_cancel_latches_once_drains_before_releasing_reservation();
        external_v6_cancel_is_failed_not_relabelled_as_host_cancel();
        late_cancel_cannot_relabel_an_already_returned_external_v6_cancel();
        completed_success_wins_a_cancel_after_v6_return_before_join();
        long_v6_dag_id_is_moved_before_reservation_release();
        public_status_is_redacted_and_metadata_is_immutable();
        plan_adapter_is_move_only_and_rejects_incomplete_inputs();
        plan_reservation_must_match_every_finite_v6_control_value();
        report_root_preflight_rejects_a_symlinked_ancestor();
        report_root_preflight_allows_readonly_intermediate_directory();
        report_root_preflight_is_equivalent_for_absolute_and_relative_paths();
        report_root_preflight_does_not_leak_failed_existing_run_descriptors();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
