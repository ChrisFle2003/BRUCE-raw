#include "bruce/application/exact_research.hpp"
#include <iostream>

namespace {
void require(bool condition,const char* message) {if(!condition) throw bruce::Error(message);}
void replay_equal(const bruce::ExactResearchRun& run) {
    require(bruce::render_exact_research(bruce::replay_exact_research(bruce::pack_exact_research(run)))==
        bruce::render_exact_research(run),"Witness expression replay differs");
}
}
int main() {
    using namespace bruce;
    try {
        for(const std::string equivalence:{"rational-q-witness"})
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
        for(const std::string planner:{"grammar","indexed-grammar","cached-grammar"}) {
            ExactResearchTask t;t.strategy="expression";t.domain="rational";t.planner=planner;
            t.expression_equivalence="rational-q-witness";t.expression_operators="-^/";
            t.minimum=t.maximum="3";t.max_nodes=9;t.budget.results=256;t.budget.operations=10000000;
            t.training={{-3,0},{-2,0},{-1,0},{1,0},{2,0},{3,0}};t.holdout={{5,0}};
            const auto full=exact_research(t);
            require(full.domain_bounded_witnesses>0 && full.expression_sources==256,"Bounded witness path not reached");
            replay_equal(full);
            // Find the operation at which the first completed witness is published.
            // Cuts on either side must keep completed evidence and origins intact.
            std::uint64_t low=1,high=full.operations;
            while(low<high) {
                const auto middle=low+(high-low)/2;t.budget.operations=middle;
                if(exact_research(t).domain_bounded_witnesses) high=middle;else low=middle+1;
            }
            for(std::uint64_t limit=low-3;limit<=low+3;++limit) {
                t.budget.operations=limit;const auto run=exact_research(t);
                require(run.status=="INCOMPLETE" && run.operations<=limit,"Witness integration cutoff failed");
                require((run.domain_bounded_witnesses>0)==(limit>=low),"Partial witness counted");
                require(run.operations>=run.planning_operations+run.normalization_operations+run.domain_comparison_operations,
                    "Witness integration accounting failed");
                std::uint64_t origins=0;for(const auto& h:run.hypotheses) origins+=h.expression_origins.size();
                require(origins==run.expression_sources,"Witness integration lost an origin");
                require(run.trace_bytes<=t.max_trace_bytes && run.trace.size()<=t.budget.trace_events,"Witness trace overflow");
                replay_equal(run);
            }
            t.budget.operations=10000000;
            for(const auto bytes:{256U,512U,1024U,4096U}) {
                t.max_trace_bytes=bytes;const auto run=exact_research(t);
                require(run.status=="INCOMPLETE" && run.trace_bytes<=bytes,"Witness trace byte limit failed");
                replay_equal(run);
            }
            t.max_trace_bytes=1024*1024;t.budget.results=257;
            bool rejected=false;try {(void)exact_research(t);} catch(const Error&) {rejected=true;}
            require(rejected,"Witness source limit ignored");
            t.budget.results=256;t.expression_equivalence="rational-q-modular";
            rejected=false;try {(void)exact_research(t);} catch(const Error&) {rejected=true;}
            require(rejected,"Legacy source limit changed");
        }
        std::cout<<"Witness grouping, cutoff boundaries, source retention and replay passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
