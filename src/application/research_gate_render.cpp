#include "bruce/application/research_gates.hpp"
#include "bruce/core/json.hpp"
namespace bruce {
std::string render_research_gate_contract() {
    std::string out="{\"contract\":\"research-gate-policy-v1\",\"scope\":\"Policy over caller-validated research facts; no source audit or import authorization\",\"allowed_reason\":\"Allowed\",\"completed_statuses\":[";
    bool first=true;
    for(auto status:research_gate_completed_statuses()){if(!first)out+=',';first=false;out+=json_string(status);}
    out+="],\"reason_precedence\":[";
    first=true;
    for(auto reason:{ResearchGateReason::status_not_completed,ResearchGateReason::stop_not_completed,
        ResearchGateReason::holdout_missing,ResearchGateReason::dependent_source,
        ResearchGateReason::portfolio_incomplete,ResearchGateReason::verified_hypothesis_missing}) {
        if(!first)out+=',';
        first=false;out+=json_string(research_gate_reason_name(reason));
    }
    return out+"],\"selection_checks\":2,\"experience_base_checks\":5,\"experience_checks\":6,\"learn_promotion\":false,\"publish\":false,\"general_proof\":false}";
}
}
