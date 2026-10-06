#pragma once
#include "bruce/core/types.hpp"
#include <span>
namespace bruce {
struct ResearchGateFacts {
    std::string_view status,stop_reason;
    bool has_holdout=false,independent=false,complete_portfolio=false,verified_hypothesis=false;
};
enum class ResearchGateReason {
    allowed, status_not_completed, stop_not_completed, holdout_missing,
    dependent_source, portfolio_incomplete, verified_hypothesis_missing
};
// Read-only built-in policy catalog; no runtime rule mutation.
std::span<const std::string_view> research_gate_completed_statuses();
std::string_view research_gate_reason_name(ResearchGateReason);
// Describes policy only; it does not validate a source or authorize an import.
std::string render_research_gate_contract();
struct ResearchGateDecision {
    bool selection=false,experience_base=false,experience=false;
    bool learn_promotion=false,publish=false,general_proof=false;
    ResearchGateReason selection_reason=ResearchGateReason::status_not_completed;
    ResearchGateReason experience_base_reason=ResearchGateReason::status_not_completed;
    ResearchGateReason experience_reason=ResearchGateReason::status_not_completed;
};
ResearchGateDecision research_gates(const ResearchGateFacts&);
}
