#include "bruce/application/application_task.hpp"
#include "bruce/application/research_gates.hpp"
#include "bruce/experience/store.hpp"
#include <type_traits>
namespace bruce {
static ApplicationGateReport evaluate_gates_impl(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    const auto run=replay_application(bundle,checkpoint);
    const auto assessment=assess_application(run);
    ApplicationGateReport report;
    report.kind=assessment.kind;report.replay_checked=true;
    report.applicable=std::holds_alternative<Run>(run) ||
        std::holds_alternative<ExactResearchRun>(run) || std::holds_alternative<GridResearchRun>(run);
    if(report.applicable) {
        const auto gate=research_gates({assessment.status,assessment.stop_reason});
        report.selection.allowed=gate.selection;
        report.selection.reason=research_gate_reason_name(gate.selection_reason);
        try {
            audit_experience_source(bundle,checkpoint);
            report.experience_source={true,"Allowed"};
        } catch(const Error& error) {
            report.experience_source.reason=error.what();
        }
    }
    return report;
}
ApplicationGateReport evaluate_application_gates(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    ReplayCheckpoint protected_checkpoint;
    if(checkpoint) protected_checkpoint=[&]{(void)detail::observe_replay_checkpoint(checkpoint);};
    try {return evaluate_gates_impl(bundle,protected_checkpoint);}
    catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
}
std::string render_application_gates(const ApplicationGateReport& report) {
    auto decision=[](bool allowed,std::string_view reason) {
        return std::string("{\"allowed\":")+(allowed ? "true" : "false")+",\"reason\":"+json_string(reason)+"}";
    };
    return "{\"contract\":\"application-gates-v1\",\"policy\":\"research-gate-policy-v1\",\"kind\":"+json_string(report.kind)+
        ",\"replay_checked\":"+std::string(report.replay_checked ? "true" : "false")+",\"applicable\":"+(report.applicable ? "true" : "false")+
        ",\"selection\":"+decision(report.selection.allowed,report.selection.reason)+",\"experience_source\":"+decision(report.experience_source.allowed,report.experience_source.reason)+
        ",\"scope\":\"Research selection gate and existing experience source audit; no projection or destination-store admission\",\"learn_promotion\":"+(report.learn_promotion ? "true" : "false")+",\"publish\":"+(report.publish ? "true" : "false")+",\"general_proof\":"+(report.general_proof ? "true" : "false")+"}";
}
std::string inspect_application_gates(std::string_view bundle) {
    return render_application_gates(evaluate_application_gates(bundle));
}
}
