#include "bruce/application/exact_research.hpp"
#include <iostream>
#include <set>

int main() {
    using namespace bruce;
    try {
        for(const auto& mode:{"syntax","rational"}) for(bool division:{false,true}) {
            ExactResearchTask task;
            task.strategy="expression";task.planner="indexed-grammar";task.domain="rational";
            task.expression_equivalence=mode;
            task.minimum="1";task.maximum="1";task.max_nodes=3;
            task.training=division ? std::vector<ExactSample>{{2,ExactValue(1,2)},{3,ExactValue(1,3)}} : std::vector<ExactSample>{{2,3},{3,4}};
            task.holdout={{-1,division ? ExactValue(-1) : ExactValue(0)}};
            const auto full=exact_research(task);
            if(full.status!="VERIFIED_ON_SAMPLES" || full.candidates!=16 || full.checkpoints!=17) throw Error("Expression reference failed");
            auto replay_equal=[](const ExactResearchRun& run) {
                if(render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run)) throw Error("Expression replay differs");
            };
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i) throw Error("Checkpoint cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_checkpoints.reset();
            for(std::uint64_t i=0;i<full.candidates;++i) {
                task.cancel_after_candidates=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.candidates!=i) throw Error("Candidate cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_candidates.reset();
            unsigned polls=0;
            const auto callback=exact_research(task,[&](std::uint64_t){return ++polls==3;});
            if(callback.stop_reason!="Cancelled" || callback.checkpoints!=2) throw Error("Callback checkpoint failed");
            replay_equal(callback);
            task.budget.operations=full.operations;task.budget.candidates=full.candidates;task.budget.trace_events=full.trace.size();
            if(exact_research(task).status!=full.status) throw Error("Exact budgets failed");
            for(std::uint64_t i=1;i<full.operations;++i) {
                task.budget.operations=i;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>i || run.stop_reason!="OperationBudgetExceeded") throw Error("Operation cutoff failed");
                replay_equal(run);
            }
        }
        const auto noop=[](std::uint64_t){};
        const auto same=[](const Expression& a,const Expression& b) {
            if(a.size()!=b.size()) throw Error("Indexed tree size differs");
            for(std::size_t i=0;i<a.size();++i)
                if(a[i].op!=b[i].op || a[i].left!=b[i].left || a[i].right!=b[i].right || a[i].value!=b[i].value)
                    throw Error("Indexed tree differs at identical rank");
        };
        for(const auto& constants:{std::vector<ExactValue>{},std::vector<ExactValue>{1},std::vector<ExactValue>{-1,0,ExactValue(2,3)}}) {
            const unsigned nodes=constants.size()>1 ? 7 : 9;
            const ExpressionGrammar reference(constants,nodes,noop),indexed(constants,nodes,noop,true);
            if(reference.size()!=indexed.size() || reference.index_entries()!=0 || indexed.index_entries()!=(nodes==9 ? 32u : 20u))
                throw Error("Indexed grammar count or storage bound differs");
            for(std::uint64_t id=0;id<reference.size();++id) same(reference.at(id,noop),indexed.at(id,noop));
            bool rejected=false;try {(void)indexed.at(indexed.size(),noop);} catch(const Error&) {rejected=true;}
            if(!rejected) throw Error("Out-of-range indexed ID accepted");
        }
        std::vector<ExactValue> constants;for(unsigned i=0;i<64;++i) constants.emplace_back(i);
        const ExpressionGrammar reference(constants,9,noop),indexed(constants,9,noop,true);
        if(indexed.size()!=1327858375545ULL || indexed.index_entries()!=32) throw Error("Maximum indexed grammar failed");
        for(std::uint64_t id=0;id<10;++id) {
            same(reference.at(id,noop),indexed.at(id,noop));
            const auto last=indexed.size()-1-id;same(reference.at(last,noop),indexed.at(last,noop));
        }
        std::uint64_t seed=123456;
        for(unsigned i=0;i<10000;++i) {
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
            const auto id=seed%indexed.size();same(reference.at(id,noop),indexed.at(id,noop));
        }
        const ExpressionGrammar leaf({1},1,noop,true);
        if(leaf.index_entries()!=0 || leaf.size()!=2) throw Error("Leaf-only indexed grammar failed");
        std::cout<<"Indexed grammar: rank equality, maximum space, storage bound and replay cutoffs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
