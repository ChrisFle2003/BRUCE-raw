#include "bruce/mod/logic_mod_artifact_dag_v6_plan.hpp"
#include "logic_mod_artifact_dag_execution.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bruce {
namespace {
[[noreturn]] void invalid_plan() { throw Error("InvalidLogicModArtifactDagV6Plan"); }
void check_directory(int fd, bool final) {
    struct stat info{};
    if (fstat(fd, &info) != 0 || !S_ISDIR(info.st_mode)) invalid_plan();
    if (final && (info.st_uid != geteuid() || (info.st_mode & 0077))) invalid_plan();
}
int open_no_symlink_directory(const std::filesystem::path& path) {
    int fd = open(path.is_absolute() ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) invalid_plan();
    for (const auto& part : path) {
        const auto name = part.string();
        if (name.empty() || name == "/" || name == ".") continue;
        const int next = openat(fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (next < 0) { close(fd); invalid_plan(); }
        close(fd); fd = next;
    }
    return fd;
}
}

void validate_logic_mod_artifact_dag_v6_plan_report_root(const std::filesystem::path& report_root) {
    if (report_root.empty()) invalid_plan();
    int fd = open_no_symlink_directory(report_root);
    try {
        check_directory(fd, true);
        close(fd);
        fd = -1;
        const auto run = std::filesystem::path(report_root.string() + ".run");
        int parent = open_no_symlink_directory(run.parent_path().empty() ? std::filesystem::path(".") : run.parent_path());
        const auto name = run.filename().string();
        const int run_fd = openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        close(parent);
        if (run_fd < 0) { if (errno == ENOENT) return; invalid_plan(); }
        try { check_directory(run_fd, true); }
        catch (...) { close(run_fd); throw; }
        const int control = openat(run_fd, "control", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        close(run_fd);
        if (control < 0) { if (errno == ENOENT) return; invalid_plan(); }
        try { check_directory(control, true); }
        catch (...) { close(control); throw; }
        close(control);
    } catch (...) { if (fd >= 0) close(fd); throw; }
}

bool logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(
    const LogicModArtifactDagControlV6& control, std::uint64_t parallel_nodes,
    std::uint64_t total_wall_ms, std::uint64_t started_workers,
    std::uint64_t payload_bytes) noexcept {
    return control.max_parallel_nodes == parallel_nodes &&
        control.max_started_workers == started_workers &&
        control.max_total_wall_ms && *control.max_total_wall_ms == total_wall_ms &&
        control.max_total_payload_bytes && *control.max_total_payload_bytes == payload_bytes;
}

LogicModArtifactDagV6Plan prepare_logic_mod_artifact_dag_v6_plan(LogicModArtifactDagV6PlanRequest request) {
    if (!request.session || request.session->nodes.empty() || request.session->nodes.size() > 16 || request.store.empty() || request.spec_digest.size() != 64 ||
        request.control.report_root.empty() || request.control.max_parallel_nodes == 0 || request.control.max_started_workers == 0 ||
        !request.control.max_total_wall_ms || !request.control.max_total_payload_bytes)
        throw Error("InvalidLogicModArtifactDagV6Plan");
    validate_logic_mod_artifact_dag_v6_plan_report_root(request.control.report_root);
    LogicModArtifactDagV6Plan plan(std::move(request));
    plan.owned_registries_ = plan.request_.session->registries;
    plan.nodes_.reserve(plan.request_.session->nodes.size());
    for (const auto& node : plan.request_.session->nodes) {
        if (node.registry_index >= plan.owned_registries_.size() || !plan.owned_registries_[node.registry_index]) throw Error("InvalidLogicModArtifactDagV6Plan");
        plan.nodes_.push_back({node.id, *plan.owned_registries_[node.registry_index], node.mod_id, node.version, node.template_id,
            node.input, node.predecessors, node.input_artifact, node.output_artifact, node.budgets});
    }
    if (plan.request_.control.max_parallel_nodes > 4 || plan.request_.control.max_started_workers > 16 ||
        !*plan.request_.control.max_total_wall_ms || *plan.request_.control.max_total_wall_ms > 60000 ||
        *plan.request_.control.max_total_payload_bytes > 8388608 ||
        (plan.request_.control.cancel_after_ms && (!*plan.request_.control.cancel_after_ms || *plan.request_.control.cancel_after_ms > 60000)))
        throw Error("InvalidLogicModArtifactDagV6Plan");
    (void)detail::canonical_logic_mod_artifact_dag_order_v4(plan.nodes_, plan.request_.store, plan.request_.spec_digest);
    for (const auto& node : plan.nodes_) detail::preflight_logic_mod_artifact_dag_node_v4(node);
    detail::validate_logic_mod_artifact_dag_targets_v4(plan.nodes_, plan.request_.store);
    return plan;
}

LogicModArtifactDagRunV6 LogicModArtifactDagV6Plan::run(std::shared_ptr<const std::atomic_bool> latch) && {
    if (consumed_) throw Error("LogicModArtifactDagV6PlanAlreadyRun");
    consumed_ = true;
    request_.control.host_cancel_latch = std::move(latch);
    auto result = run_logic_mod_artifact_dag_v6(nodes_, request_.store, request_.spec_digest, std::move(request_.control));
    if (result.accepted && request_.publish_record) request_.publish_record(logic_mod_artifact_dag_record_v6(result));
    return result;
}

bool LogicModArtifactDagV6Plan::has_exact_orchestrator_reservation(
    std::uint64_t parallel_nodes, std::uint64_t total_wall_ms,
    std::uint64_t started_workers, std::uint64_t payload_bytes) const noexcept {
    return logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(
        request_.control, parallel_nodes, total_wall_ms, started_workers, payload_bytes);
}
}
