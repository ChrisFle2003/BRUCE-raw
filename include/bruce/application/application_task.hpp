#pragma once
#include "bruce/core/replay_control.hpp"
#include "bruce/application/research.hpp"
#include "bruce/application/exact_research.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/application/grid_research.hpp"
#include "bruce/application/grid_stage_sweep.hpp"
#include "bruce/application/grid_stage_verify.hpp"
#include "bruce/application/fraction_compare.hpp"
#include "bruce/application/compatibility.hpp"
#include <variant>
namespace bruce {
struct GridResumeTask {std::string parent;std::size_t additional_queries=0;};
struct GridResearchTask {
    ExactResearchTask research;
    std::string training,holdout,axis="x";
    std::optional<std::uint64_t> total_operations;
};
using ApplicationTask=std::variant<Task,ExactResearchTask,GridTask,GridBatchTask,
    GridResumeTask,GridResearchTask,FractionComparisonTask,CompatibilityTask,GridStageSweepTask,GridStageVerifyTask>;
using ApplicationRun=std::variant<Run,ExactResearchRun,GridRun,GridBatchRun,
    GridResumeRun,GridResearchRun,FractionComparisonRun,CompatibilityRun,GridStageSweepRun,GridStageVerifyRun>;
enum class ClaimScope { sample_evidence,grid_evaluation,rational_fraction_comparison,numeric_selection,finite_grid_stage_domain };
struct ApplicationAssessment {
    std::string_view kind;
    std::string status,stop_reason;
    bool complete;
    ClaimScope claim_scope;
    bool proof_established;
};
// Classifies an in-memory result; does not independently verify its provenance.
ApplicationAssessment assess_application(const ApplicationRun&);
// Replays untrusted input before exposing a verified assessment.
std::string assess_application_bundle(std::string_view bundle);
// Replays the bundle, then checks research selection and the existing source audit.
struct ApplicationGateDecision {
    bool allowed=false;
    std::string reason="UnsupportedApplicationKind";
};
struct ApplicationGateReport {
    std::string kind;
    bool replay_checked=false,applicable=false;
    ApplicationGateDecision selection,experience_source;
    bool learn_promotion=false,publish=false,general_proof=false;
};
// Callback exceptions propagate unchanged; no partial gate report is returned.
ApplicationGateReport evaluate_application_gates(std::string_view bundle,const ReplayCheckpoint& checkpoint={});
// Formats supplied fields only; it performs no verification or authorization.
std::string render_application_gates(const ApplicationGateReport&);
std::string inspect_application_gates(std::string_view bundle);
// Native cancellation remains executor-local; the optional work observer also visits
// applicable imports. It throws on failure and never requests a native partial result.
struct ExecutionControl {
    std::function<bool()> cancelled;
    std::function<bool()> experience_cancelled={};
    std::function<bool()> grid_source_cancelled={};
    std::function<bool()> compatibility_source_cancelled={};
    std::function<bool()> resume_import_cancelled={};
    ReplayCheckpoint work_checkpoint={};
};
ApplicationRun execute_application(ApplicationTask,const ExecutionControl& control={});
std::string render_application(const ApplicationRun&);
std::string pack_application(const ApplicationRun&,const ReplayCheckpoint& checkpoint={},const ReplayCheckpoint& records={});
std::string render_application_contract();
std::string_view application_kind(const ApplicationRun&);
ApplicationRun replay_application(std::string_view bundle,const ReplayCheckpoint& checkpoint={});
std::string inspect_application(std::string_view bundle);
std::pair<std::string,std::string> application_status(const ApplicationRun&);
}
