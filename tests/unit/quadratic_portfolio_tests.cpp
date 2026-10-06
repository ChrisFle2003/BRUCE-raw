#include "bruce/application/exact_research.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        for(const auto& planner:{"portfolio","adaptive-order","experience-order"}) for(unsigned shape=0;shape<4;++shape) {
            ExactResearchTask task;
            task.strategy="portfolio-quadratic";task.planner=planner;task.domain="rational";
            task.max_power=2;task.max_degree=2;task.minimum="0";task.maximum="1";
            task.training=shape==0 ? std::vector<ExactSample>{{0,1},{1,1},{2,1},{3,1}} : shape==1 ?
                std::vector<ExactSample>{{0,1},{1,ExactValue(1,2)},{2,ExactValue(1,3)},{3,ExactValue(1,4)}} :
                shape==2 ? std::vector<ExactSample>{{0,1},{1,2},{2,5},{3,10}} :
                std::vector<ExactSample>{{0,1},{1,ExactValue(1,2)},{2,ExactValue(1,5)},{3,ExactValue(1,10)}};
            task.holdout={{5,shape==0 ? ExactValue(1) : shape==1 ? ExactValue(1,6) : shape==2 ? ExactValue(26) : ExactValue(1,26)}};
            const bool ordered=task.planner!="portfolio";
            if(task.planner=="experience-order") {
                auto source=task; source.planner="portfolio";
                task.experience_snapshot=append_experience("",pack_exact_research(exact_research(source)));
            }
            const auto full=exact_research(task);
            if(full.status=="INCOMPLETE" || full.strategy_runs.size()!=4 || (ordered && full.strategy_order.size()!=4))
                throw Error("Adaptive reference incomplete");
            std::uint64_t child_checkpoints=0;
            for(const auto& child:full.strategy_runs) child_checkpoints+=child.checkpoints;
            if(full.checkpoints!=child_checkpoints+(ordered ? 1 : 0)) throw Error("Ordering checkpoint not charged");
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Adaptive global checkpoint replay failed");
            }
            task.cancel_after_checkpoints.reset();
            for(std::uint64_t boundary:{std::uint64_t(0),std::uint64_t(ordered),std::uint64_t(ordered)+full.strategy_runs[0].checkpoints}) {
                unsigned polls=0;
                const auto run=exact_research(task,[&](std::uint64_t){return ++polls==boundary+1;});
                if(run.stop_reason!="Cancelled" || run.checkpoints!=boundary ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Adaptive callback boundary replay failed");
            }
            // Each child requires a positive remaining candidate allowance, even if it later derives no candidate.
            task.budget.operations=full.operations;task.budget.candidates=full.candidates+1;
            task.budget.trace_events=full.trace.size();
            for(const auto& child:full.strategy_runs) task.budget.trace_events+=child.trace.size();
            if(exact_research(task).status!=full.status) throw Error("Exact operation/trace budgets failed");
            for(std::uint64_t n=1;n<full.operations;++n) {
                task.budget.operations=n;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>n ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Adaptive operation cutoff failed");
            }
        }
        std::cout<<"Four families with fixed, shape and experience planning: checkpoints, callback boundaries and operation cutoffs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
