#include "bruce/application/application_task.hpp"
#include <type_traits>
namespace bruce {
ApplicationRun replay_application(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.starts_with("BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1\n")) return replay_grid_stage_verify(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1\n")) return replay_grid_stage_sweep(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V1\n") || bundle.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V2\n") || (bundle.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V3\n") || bundle.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V4\n"))) return replay_grid_research(bundle,10000000,{},checkpoint);
    if(bundle.starts_with("BRUCE_GRID_RESUME_BUNDLE_V1\n") || bundle.starts_with("BRUCE_GRID_RESUME_BUNDLE_V2\n")) return replay_grid_resume(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_GRID_BUNDLE_V1\n") || bundle.starts_with("BRUCE_GRID_BUNDLE_V2\n")) return replay_grid(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V1\n") || bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V2\n") ||
       bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V3\n") || bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V4\n")) return replay_grid_batch(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_FRACTION_COMPARISON_V1\n") || bundle.starts_with("BRUCE_FRACTION_COMPARISON_V2\n") || bundle.starts_with("BRUCE_FRACTION_COMPARISON_V3\n")) return replay_fraction_comparison(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_COMPATIBILITY_BUNDLE_V1\n")) return replay_compatibility(bundle,checkpoint);
    if(bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V32\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V33\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V34\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V10\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V29\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V30\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V31\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V9\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V26\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V27\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V28\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V8\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V23\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V24\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V25\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V7\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V20\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V21\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V22\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V17\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V18\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V19\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V14\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V15\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V16\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V11\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V12\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V13\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V6\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V5\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V4\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V3\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V10\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V9\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V8\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V1\n") || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V2\n") || bundle.starts_with("BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1\n") || bundle.starts_with("BRUCE_RATIONAL_LINEAR_BUNDLE_V1\n") || bundle.starts_with("BRUCE_EXACT_BUNDLE_V1\n") || bundle.starts_with("BRUCE_POWER_BUNDLE_V1\n") ||
       bundle.starts_with("BRUCE_ADAPTIVE_BUNDLE_V1\n") || bundle.starts_with("BRUCE_POLYNOMIAL_BUNDLE_V1\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V1\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V2\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V3\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V4\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V5\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V6\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V7\n"))
        return replay_exact_research(bundle,10000000,{},checkpoint);
    return replay(bundle,checkpoint);
}
ApplicationAssessment assess_application(const ApplicationRun& run){
    auto fields=std::visit([](const auto& r)->std::pair<std::string,std::string>{
        using T=std::decay_t<decltype(r)>;
        if constexpr(std::is_same_v<T,GridRun>) return {r.stopped ? "STOPPED" : "EVALUATED",r.stopped ? "GridSentinel" : "Completed"};
        else if constexpr(std::is_same_v<T,GridStageVerifyRun>) return {r.status,"Completed"};
        else if constexpr(std::is_same_v<T,GridResumeRun>) return {r.batch.status,r.batch.stop_reason};
        else if constexpr(std::is_same_v<T,GridResearchRun>) return {r.research.status,r.research.stop_reason};
        else if constexpr(std::is_same_v<T,CompatibilityRun>) return {r.result.status,r.result.stop_reason};
        else return {r.status,r.stop_reason};
    },run);
    const auto scope=std::visit([](const auto& r){
        using T=std::decay_t<decltype(r)>;
        if constexpr(std::is_same_v<T,GridRun> || std::is_same_v<T,GridBatchRun> || std::is_same_v<T,GridResumeRun> || std::is_same_v<T,GridStageSweepRun>)return ClaimScope::grid_evaluation;
        else if constexpr(std::is_same_v<T,GridStageVerifyRun>)return ClaimScope::finite_grid_stage_domain;
        else if constexpr(std::is_same_v<T,FractionComparisonRun>)return ClaimScope::rational_fraction_comparison;
        else if constexpr(std::is_same_v<T,CompatibilityRun>)return ClaimScope::numeric_selection;
        else return ClaimScope::sample_evidence;
    },run);
    const bool complete=fields.first!="INCOMPLETE";
    const bool proof=scope==ClaimScope::rational_fraction_comparison && complete && fields.second=="Completed" &&
        (fields.first=="PROVEN_EQUIVALENT" || fields.first=="PROVEN_DIFFERENT");
    return {application_kind(run),std::move(fields.first),std::move(fields.second),complete,scope,proof};
}
std::pair<std::string,std::string> application_status(const ApplicationRun& run){
    const auto assessment=assess_application(run);
    return {assessment.status,assessment.stop_reason};
}
std::string assess_application_bundle(std::string_view bundle){
    const auto a=assess_application(replay_application(bundle));
    std::string_view scope;
    switch(a.claim_scope){
        case ClaimScope::sample_evidence:scope="declared-family-sample-evidence";break;
        case ClaimScope::grid_evaluation:scope="grid-evaluation";break;
        case ClaimScope::rational_fraction_comparison:scope="supplied-polynomial-fractions-over-Q";break;
        case ClaimScope::numeric_selection:scope="numeric-rule-selection";break;
        case ClaimScope::finite_grid_stage_domain:scope="finite-uint64-scaled-exponential-stage-domain-0..57";break;
    }
    return "{\"contract\":\"application-assessment-v1\",\"kind\":"+json_string(a.kind)+
        ",\"replay_checked\":true,\"status\":"+json_string(a.status)+",\"stop_reason\":"+json_string(a.stop_reason)+
        ",\"result_complete\":"+(a.complete ? "true" : "false")+",\"claim_scope\":"+json_string(scope)+
        ",\"proof_established\":"+(a.proof_established ? "true" : "false")+"}";
}
std::string inspect_application(std::string_view bundle){
    const auto run=replay_application(bundle);
    const auto assessment=assess_application(run);
    const auto claim=assessment.claim_scope==ClaimScope::finite_grid_stage_domain ?
        ",\"claim_scope\":\"finite-uint64-scaled-exponential-stage-domain-0..57\"" : "";
    return "{\"contract\":\"application-result-v1\",\"kind\":"+json_string(application_kind(run))+
        ",\"replay_checked\":true,\"lifecycle\":"+json_string(assessment.complete ? "COMPLETED" : "INCOMPLETE")+
        ",\"status\":"+json_string(assessment.status)+",\"stop_reason\":"+json_string(assessment.stop_reason)+
        claim+",\"scope\":\"Execution summary only; native result retains mathematical claim scope\"}";
}
}
