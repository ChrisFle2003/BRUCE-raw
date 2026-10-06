#include "bruce/application/exact_research.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        ExactResearchTask task;
        task.strategy="portfolio";task.planner="portfolio";task.max_power=4;
        task.training={{0,3},{1,5},{2,11},{3,21}};task.holdout={{-3,21}};
        const auto full=exact_research(task);
        if(full.status!="VERIFIED_ON_SAMPLES" || full.hypotheses.size()!=1 ||
           full.hypotheses.front().origins.size()!=2 || full.strategy_runs.size()!=2)
            throw Error("Portfolio reference did not merge correctly");
        for(std::uint64_t i=0;i<full.checkpoints;++i) {
            task.cancel_after_checkpoints=i;
            const auto partial=exact_research(task);
            if(partial.status!="INCOMPLETE" || partial.stop_reason!="Cancelled" || partial.checkpoints!=i ||
               render_exact_research(replay_exact_research(pack_exact_research(partial)))!=render_exact_research(partial))
                throw Error("Global checkpoint cancellation replay failed");
        }
        task.cancel_after_checkpoints.reset();
        unsigned polls=0;
        const auto first_checkpoints=full.strategy_runs.front().checkpoints;
        const auto boundary=exact_research(task,[&](std::uint64_t) { return ++polls==first_checkpoints+1; });
        if(boundary.strategy_runs.size()!=2 || boundary.strategy_runs.back().candidates!=0 || boundary.stop_reason!="Cancelled" ||
           render_exact_research(replay_exact_research(pack_exact_research(boundary)))!=render_exact_research(boundary))
            throw Error("Strategy transition cancellation replay failed");
        task.budget.operations=full.operations; task.budget.candidates=full.candidates;
        if(exact_research(task).status!=full.status) throw Error("Shared exact budgets must finish both strategies");
        --task.budget.operations;
        const auto limited=exact_research(task);
        if(limited.status!="INCOMPLETE" || limited.operations>task.budget.operations)
            throw Error("Shared budget overflow or false completion");
        std::cout << "Portfolio provenance, global checkpoints and shared budgets passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
