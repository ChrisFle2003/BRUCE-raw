#include "bruce/application/exact_research.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        for(bool singular:{false,true}) {
            ExactResearchTask task;
            task.strategy="portfolio-rational";task.planner="portfolio";task.domain="rational";
            task.max_power=2;task.max_degree=2;task.minimum="0";task.maximum="1";
            task.training=singular ? std::vector<ExactSample>{{2,1},{3,1},{4,1}} :
                std::vector<ExactSample>{{0,1},{1,ExactValue(1,2)},{2,ExactValue(1,3)}};
            task.holdout={{5,singular ? ExactValue(1) : ExactValue(1,6)}};
            const auto full=exact_research(task);
            if(full.status!=(singular ? "AMBIGUOUS" : "VERIFIED_ON_SAMPLES") || full.strategy_runs.size()!=3)
                throw Error("Three-family reference failed");
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Global checkpoint replay failed");
            }
            task.cancel_after_checkpoints.reset();
            std::uint64_t first_two=full.strategy_runs[0].checkpoints+full.strategy_runs[1].checkpoints;
            unsigned polls=0;
            const auto boundary=exact_research(task,[&](std::uint64_t){return ++polls==first_two+1;});
            if(boundary.strategy_runs.size()!=3 || boundary.strategy_runs[2].candidates!=0 || boundary.stop_reason!="Cancelled" ||
               render_exact_research(replay_exact_research(pack_exact_research(boundary)))!=render_exact_research(boundary))
                throw Error("Third strategy transition cancellation failed");
            task.budget.operations=full.operations;task.budget.candidates=full.candidates;
            if(exact_research(task).status!=full.status) throw Error("Exact shared budget failed");
            for(std::uint64_t n=1;n<full.operations;++n) {
                task.budget.operations=n;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>n ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Operation budget cutoff failed");
            }
        }
        std::cout<<"Three-family global checkpoints, transition and exact budget cutoffs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
