#pragma once

#include "bruce/mod/logic_mod_artifact_dag_v6.hpp"
#include <functional>

namespace bruce {
// Owner-only session material.  References passed to V6 are materialized from
// these owned registries, so a prepared plan cannot retain caller-borrowed
// registry references.
struct LogicModArtifactDagV6PlanNode {
    std::size_t registry_index{};
    std::string id, mod_id, version, template_id, input;
    std::vector<std::string> predecessors;
    std::optional<LogicModOutputV1> input_artifact;
    LogicModOutputV1 output_artifact;
    LogicModBudgetsV1 budgets{};
};
struct LogicModArtifactDagV6PlanSession {
    std::vector<std::shared_ptr<LogicModRegistryV1>> registries;
    // Opaque owner-only resources (for example extracted package directories)
    // that must outlive the registry while the prepared plan is queued/running.
    std::vector<std::shared_ptr<void>> keepalive;
    std::vector<LogicModArtifactDagV6PlanNode> nodes;
};
// Private-to-host plan carrier.  It deliberately retains no execution thread,
// worker handle or public path-bearing status field.
struct LogicModArtifactDagV6PlanRequest {
    std::shared_ptr<const LogicModArtifactDagV6PlanSession> session;
    std::filesystem::path store;
    std::filesystem::path record;
    std::string spec_digest;
    // The CLI supplies the existing canonical no-overwrite publisher. Keeping
    // it here avoids exposing a record path from controller status.
    std::function<void(std::string_view)> publish_record;
    LogicModArtifactDagControlV6 control;
};
// Task-2 admission-only descriptor-relative report-root check.  This is
// intentionally separate from existing V6 sidecar create/inspect policy.
void validate_logic_mod_artifact_dag_v6_plan_report_root(const std::filesystem::path&);
bool logic_mod_artifact_dag_v6_plan_has_exact_orchestrator_reservation(
    const LogicModArtifactDagControlV6&, std::uint64_t parallel_nodes,
    std::uint64_t total_wall_ms, std::uint64_t started_workers,
    std::uint64_t payload_bytes) noexcept;

class LogicModArtifactDagV6Plan {
public:
    LogicModArtifactDagV6Plan(LogicModArtifactDagV6Plan&&) noexcept = default;
    LogicModArtifactDagV6Plan& operator=(LogicModArtifactDagV6Plan&&) noexcept = default;
    LogicModArtifactDagV6Plan(const LogicModArtifactDagV6Plan&) = delete;
    LogicModArtifactDagV6Plan& operator=(const LogicModArtifactDagV6Plan&) = delete;
    ~LogicModArtifactDagV6Plan() = default;

    // Private host admission query: every Orchestrator reservation must be
    // identical to the finite V6 limits that this immutable plan will run.
    bool has_exact_orchestrator_reservation(std::uint64_t parallel_nodes,
        std::uint64_t total_wall_ms, std::uint64_t started_workers,
        std::uint64_t payload_bytes) const noexcept;
    LogicModArtifactDagRunV6 run(std::shared_ptr<const std::atomic_bool> latch = {}) &&;

private:
    explicit LogicModArtifactDagV6Plan(LogicModArtifactDagV6PlanRequest request) : request_(std::move(request)) {}
    LogicModArtifactDagV6PlanRequest request_;
    std::vector<std::shared_ptr<LogicModRegistryV1>> owned_registries_;
    std::vector<LogicModDagNodeV2> nodes_;
    bool consumed_ = false;
    friend LogicModArtifactDagV6Plan prepare_logic_mod_artifact_dag_v6_plan(LogicModArtifactDagV6PlanRequest);
};

// Validation is deliberately local and bounded.  Full CLI parsing and package
// assembly remain at the CLI boundary until its later migration task.
LogicModArtifactDagV6Plan prepare_logic_mod_artifact_dag_v6_plan(LogicModArtifactDagV6PlanRequest);
}
