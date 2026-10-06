#include "bruce/application/experience_source.hpp"
#include "bruce/application/experience_routing.hpp"
#include "bruce/application/research_gates.hpp"
#include "bruce/application/grid_research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
namespace bruce {
namespace {
bool independent_portfolio(std::string_view source) {
    for(unsigned version:{2,3,5,6,8,9,11,12,14,15,17,18,20,21,23,24,26,27,29,30,32,33})
        if(source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V"+std::to_string(version)+"\n")) return true;
    return false;
}
ExperienceRecord verify_source(std::string_view source,const ExactResearchRun* verified=nullptr,const ReplayCheckpoint& checkpoint={}) {
    if(source.size()>experience_byte_limit) throw Error("ExperienceByteLimit");
    const bool grid=source.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V1\n") ||
        source.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V2\n") || (source.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V3\n") || source.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V4\n"));
    if(grid) {
        GridResearchRun origin;
        try {
            origin=replay_grid_research(source,1000000,[](std::string_view nested) {
                if(!independent_portfolio(nested)) throw Error("ExperienceRequiresIndependentPortfolioSource");
            },checkpoint);
        } catch(const Error& error) {
            if(std::string_view(error.what())=="ReplayOperationLimit") throw Error("ExperienceSourceOperationLimit");
            throw;
        }
        auto record=verify_source(pack_exact_research(origin.research),&origin.research,checkpoint);
        record.grid_origin=true;record.source_digest=sha256(source);
        // Logical source-byte charge; grid replay CPU work is not measured here.
        record.audit_operations+=origin.training_source.size()+origin.holdout_source.size();
        return record;
    }
    const bool witnessed=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V32\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V33\n");
    const bool domain_cached=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V29\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V30\n");
    const bool modular=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V26\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V27\n");
    const bool rational_q=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V23\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V24\n");
    const bool cached=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V20\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V21\n");
    const bool powered=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V17\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V18\n");
    const bool selected=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V14\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V15\n");
    const bool indexed=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V11\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V12\n");
    const bool five=witnessed || domain_cached || modular || rational_q || cached || powered || selected || indexed || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V8\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V9\n");
    const bool four=source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V5\n") || source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V6\n");
    if(source.size()>experience_byte_limit ||
       (!five && !four && !source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V2\n") && !source.starts_with("BRUCE_PORTFOLIO_BUNDLE_V3\n")))
        throw Error("ExperienceRequiresIndependentPortfolioSource");
    // The decoder checks the parsed budget before running evidence, avoiding textual preflight bypasses.
    ExactResearchRun run;
    try {run=verified ? *verified : replay_exact_research(source,1000000,{},checkpoint);}
    catch(const Error& error) {
        if(std::string_view(error.what())=="ReplayOperationLimit") throw Error("ExperienceSourceOperationLimit");
        throw;
    }
    ResearchGateFacts gate_facts{run.status,run.stop_reason,!run.task.holdout.empty(),
        run.task.planner!="experience-order" && run.task.experience_snapshot.empty(),
        run.strategy_runs.size()==(five ? 5u : four ? 4u : 3u),false};
    if(!research_gates(gate_facts).experience_base) throw Error("ExperienceRequiresCompletedHoldoutRun");
    ExperienceRecord record;record.indexed=indexed;record.selected=selected;record.powered=powered;record.cached=cached;record.rational_q=rational_q;record.modular=modular;record.domain_cached=domain_cached;record.witnessed=witnessed;
    if(five) record.votes.resize(5);
    else if(four) record.votes.resize(4);
    for(const auto& child:run.strategy_runs) {
        if(checkpoint) checkpoint();
        const unsigned index=child.task.strategy=="power-affine" ? 0 : child.task.strategy=="polynomial" ? 1 : child.task.strategy=="rational-linear" ? 2 : child.task.strategy=="rational-quadratic" ? 3 : 4;
        for(const auto& h:child.hypotheses) if(h.status=="VERIFIED_ON_SAMPLES") record.votes[index]=1;
    }
    gate_facts.verified_hypothesis=std::any_of(record.votes.begin(),record.votes.end(),[](auto v){return v!=0;});
    if(!research_gates(gate_facts).experience) throw Error("ExperienceRequiresVerifiedHypothesis");
    record.audit_operations=run.operations;
    const auto shape=plan_portfolio_order(run.task,[&](std::uint64_t n){if(checkpoint) checkpoint();record.audit_operations+=n;});
    record.source_digest=sha256(source); record.training_fingerprint=experience_training_key(run.task);
    record.context=experience_context_key(run.task,shape.reason); return record;
}
}
ExperienceRecord verify_experience_source(std::string_view source,const ReplayCheckpoint& checkpoint) {
    ReplayCheckpoint protected_checkpoint;
    if(checkpoint) protected_checkpoint=[&]{(void)detail::observe_replay_checkpoint(checkpoint);};
    try {return verify_source(source,nullptr,protected_checkpoint);}
    catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
}
void audit_experience_source(std::string_view source,const ReplayCheckpoint& checkpoint) {
    (void)verify_experience_source(source,checkpoint);
}
}
