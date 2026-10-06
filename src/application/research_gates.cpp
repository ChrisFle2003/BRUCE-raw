#include "bruce/application/research_gates.hpp"
#include <algorithm>
#include <iterator>
namespace bruce {
namespace {
constexpr std::string_view completed_statuses[]={"CANDIDATE","NO_CANDIDATE","REFUTED","AMBIGUOUS","VERIFIED_ON_SAMPLES"};
}
std::span<const std::string_view> research_gate_completed_statuses(){return completed_statuses;}
std::string_view research_gate_reason_name(ResearchGateReason reason) {
    switch(reason) {
        case ResearchGateReason::allowed:return "Allowed";
        case ResearchGateReason::status_not_completed:return "StatusNotCompleted";
        case ResearchGateReason::stop_not_completed:return "StopNotCompleted";
        case ResearchGateReason::holdout_missing:return "HoldoutMissing";
        case ResearchGateReason::dependent_source:return "DependentSource";
        case ResearchGateReason::portfolio_incomplete:return "PortfolioIncomplete";
        case ResearchGateReason::verified_hypothesis_missing:return "VerifiedHypothesisMissing";
    }
    throw Error("UnknownResearchGateReason");
}
ResearchGateDecision research_gates(const ResearchGateFacts& f) {
    using R=ResearchGateReason;
    ResearchGateDecision out;
    out.selection_reason=std::find(std::begin(completed_statuses),std::end(completed_statuses),f.status)==std::end(completed_statuses)
        ? R::status_not_completed : f.stop_reason!="Completed" ? R::stop_not_completed : R::allowed;
    out.experience_base_reason=out.selection_reason!=R::allowed ? out.selection_reason :
        !f.has_holdout ? R::holdout_missing : !f.independent ? R::dependent_source :
        !f.complete_portfolio ? R::portfolio_incomplete : R::allowed;
    out.experience_reason=out.experience_base_reason!=R::allowed ? out.experience_base_reason :
        !f.verified_hypothesis ? R::verified_hypothesis_missing : R::allowed;
    out.selection=out.selection_reason==R::allowed;
    out.experience_base=out.experience_base_reason==R::allowed;
    out.experience=out.experience_reason==R::allowed;
    return out;
}
}
