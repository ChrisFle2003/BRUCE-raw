#include "bruce/observability/controller_observability_cli_v1.hpp"
#include "bruce/observability/controller_observability_crosswalk_v1.hpp"
#include "bruce/worker/worker_run_inspector_v1.hpp"
#include "bruce/worker/worker_run_log_v1.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace {
void check(bool value, const char* message) { if (!value) throw bruce::Error(message); }
std::filesystem::path root(std::string_view suffix) {
    std::string pattern = (std::filesystem::temp_directory_path() / ("bruce-observability-cli-v1-" + std::string(suffix) + "-XXXXXX")).string();
    check(::mkdtemp(pattern.data()) != nullptr, "unique fixture root");
    const std::filesystem::path value(pattern); std::filesystem::create_directories(value / "inputs");
    std::filesystem::create_directories(value / "attempts");
    for (const auto& path : {value, value / "inputs", value / "attempts"})
        std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
    return value;
}
bruce::worker_v1::WorkerLogEvent event(std::uint64_t sequence, std::string name, std::string status) {
    bruce::worker_v1::WorkerLogEvent value; value.run_id = "safe-run"; value.seq = sequence; value.ts_unix_ms = 1;
    value.event = std::move(name); value.status = std::move(status); return value;
}
void v5_fixture_and_incomplete_history_remain_native_evidence() {
    const auto complete = root("complete.run");
    auto log = bruce::worker_v1::WorkerRunLog::create(complete, "safe-run");
    log.append(event(1, "RUN_CREATED", "READY")); log.append(event(2, "RUN_FINISHED", "FAILED"));
    const auto complete_fact = bruce::observe_v5_worker_run_v1(bruce::worker_v1::inspect_worker_run_v1(complete));
    check(complete_fact.terminal == bruce::ControllerObservationTerminalV1::FAILED, "V5 fixture did not bind to failed observation");

    const auto incomplete = root("incomplete.run");
    auto open_log = bruce::worker_v1::WorkerRunLog::create(incomplete, "safe-run");
    open_log.append(event(1, "RUN_CREATED", "READY"));
    const auto incomplete_fact = bruce::observe_v5_worker_run_v1(bruce::worker_v1::inspect_worker_run_v1(incomplete));
    check(incomplete_fact.phase == bruce::ControllerObservationPhaseV1::UNKNOWN, "incomplete V5 history claimed liveness");
    std::filesystem::remove_all(complete); std::filesystem::remove_all(incomplete);
}
void renderer_remains_redacted_for_huge_native_data() {
    bruce::LogicModArtifactDagRunV6 run;
    run.dag_id = "/secret/path?token=" + std::string(1024 * 1024, 'x');
    run.status = "DAG_READY";
    run.accepted = true;
    const auto out = bruce::render_controller_observation_v1(bruce::observe_logic_mod_artifact_dag_v6(run));
    check(out.size() < 512, "renderer retained huge native data");
    check(out.find("secret") == std::string::npos, "renderer leaked native data");
}
void v6_reader_rejects_unsafe_paths() {
    bool rejected = false;
    try { (void)bruce::read_controller_observability_v6_record_v1({}); }
    catch (const bruce::Error&) { rejected = true; }
    check(rejected, "empty V6 path accepted");
    const auto dir = root("unsafe");
    const auto target = dir / "target";
    { std::ofstream out(target); out << "not a canonical record"; }
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
    const auto rejects = [](const std::filesystem::path& path) {
        try { (void)bruce::read_controller_observability_v6_record_v1(path); return false; }
        catch (const bruce::Error&) { return true; }
    };
    std::filesystem::create_symlink(target, dir / "link");
    check(rejects(dir / "link"), "symlink V6 evidence accepted");
    check(rejects(dir), "directory V6 evidence accepted");
    std::filesystem::permissions(target, std::filesystem::perms::group_read, std::filesystem::perm_options::add);
    check(rejects(target), "group-readable V6 evidence accepted");
    std::filesystem::permissions(target, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
    std::filesystem::create_hard_link(target, dir / "hardlink");
    check(rejects(target), "hard-linked V6 evidence accepted");
    std::filesystem::remove_all(dir);
}
}
int main() {
    try {
        v5_fixture_and_incomplete_history_remain_native_evidence();
        renderer_remains_redacted_for_huge_native_data();
        v6_reader_rejects_unsafe_paths();
        std::cout << "controller observability cli v1 tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
