#include "bruce/application/exact_research.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* message) {if(!condition) throw bruce::Error(message);}
void replay_equal(const bruce::ExactResearchRun& run) {
    require(bruce::render_exact_research(bruce::replay_exact_research(bruce::pack_exact_research(run)))==
        bruce::render_exact_research(run),"Q expression replay differs");
}
}
int main() {
    using namespace bruce;
    try {
        for(const std::string equivalence:{"rational-q","rational-q-modular"})
        for(const std::string planner:{"grammar","indexed-grammar","cached-grammar"}) {
            ExactResearchTask task;
            task.strategy="expression";task.domain="rational";task.planner=planner;
            task.expression_equivalence=equivalence;task.expression_operators="/";
            task.minimum="1";task.maximum="1";task.max_nodes=3;
            task.training={{2,1},{3,1}};task.holdout={{4,1}};
            const auto full=exact_research(task);replay_equal(full);
            require(full.status=="AMBIGUOUS" && full.hypotheses.size()==2,"Q domain holes erased");
            require(full.expression_sources==3 && full.domain_comparisons>0 &&
                full.domain_comparison_operations>0 && full.domain_unknown==0,"Q comparison accounting failed");
            require(full.operations>=full.planning_operations+full.normalization_operations+full.domain_comparison_operations,
                "Q comparison operations not part of total");
            for(std::uint64_t limit=1;limit<full.operations;++limit) {
                task.budget.operations=limit;const auto run=exact_research(task);
                require(run.status=="INCOMPLETE" && run.stop_reason=="OperationBudgetExceeded" && run.operations<=limit,
                    "Q budget cutoff failed");
                require(run.operations>=run.planning_operations+run.normalization_operations+run.domain_comparison_operations,
                    "Partial Q cost accounting failed");
                std::uint64_t sources=0;
                for(const auto& h:run.hypotheses) sources+=h.expression_origins.size();
                require(sources==run.expression_sources,"Incomplete candidate provenance lost or duplicated");
                replay_equal(run);
            }
            task.budget.operations=full.operations;
            require(exact_research(task).status==full.status,"Exact Q operation budget failed");
            for(std::uint64_t checkpoint=0;checkpoint<full.checkpoints;++checkpoint) {
                task.cancel_after_checkpoints=checkpoint;
                const auto run=exact_research(task);
                require(run.status=="INCOMPLETE" && run.stop_reason=="Cancelled" && run.checkpoints==checkpoint,
                    "Q checkpoint cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_checkpoints.reset();
            const auto callback=exact_research(task,[](std::uint64_t n){return n==3;});
            require(callback.stop_reason=="Cancelled","Q callback cancellation failed");replay_equal(callback);
            task.budget.results=1;
            require(exact_research(task).stop_reason=="ResultBudgetExceeded","Q source retention limit ignored");
            task.budget.results=128;task.budget.trace_events=2;
            require(exact_research(task).stop_reason=="TraceBudgetExceeded","Q trace limit ignored");
            task.budget.trace_events=100002;task.max_trace_bytes=256;
            require(exact_research(task).stop_reason=="TraceByteBudgetExceeded","Q trace byte limit ignored");
        }
        std::cout<<"Q grouping, all operation/cancellation boundaries, source retention and replay passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
