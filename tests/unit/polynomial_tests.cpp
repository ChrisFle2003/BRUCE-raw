#include "bruce/application/exact_research.hpp"
#include "bruce/math/polynomial.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        ExactResearchTask task;
        task.strategy="polynomial"; task.planner="interpolate"; task.max_degree=3;
        task.training={{0,1},{1,3},{2,7},{3,13}}; task.holdout={{-3,7}};
        const auto full=exact_research(task);
        if(full.status!="VERIFIED_ON_SAMPLES" || full.hypotheses.size()!=1) throw Error("Polynomial reference failed");
        for(std::uint64_t i=0;i<full.checkpoints;++i) {
            task.cancel_after_checkpoints=i;
            const auto run=exact_research(task);
            if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i ||
               render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                throw Error("Polynomial cancellation replay differs");
        }
        task.cancel_after_checkpoints.reset();
        unsigned polls=0;
        const auto callback=exact_research(task,[&](std::uint64_t) { return ++polls==2; });
        if(callback.candidates!=0 || callback.plans_completed!=1 || callback.stop_reason!="Cancelled" ||
           render_exact_research(replay_exact_research(pack_exact_research(callback)))!=render_exact_research(callback))
            throw Error("Polynomial callback cancellation checkpoint lost");
        task.budget.operations=full.operations; task.budget.candidates=full.candidates; task.budget.trace_events=full.trace.size();
        if(exact_research(task).status!=full.status) throw Error("Exactly sufficient polynomial budgets failed");
        --task.budget.operations;
        if(exact_research(task).stop_reason!="OperationBudgetExceeded") throw Error("Insufficient polynomial budget did not stop");
        const NumberDomain rational("rational",64);
        std::uint64_t used=0;
        const auto coefficients=interpolate_polynomial(rational,{0,ExactValue(2,2),2},{1,3,7},[&](std::uint64_t n) { used+=n; });
        if(coefficients!=std::vector<ExactValue>({1,1,1}) || used==0 ||
           polynomial_horner(rational,coefficients,ExactValue(-6,2))!=7 || polynomial_sum(rational,coefficients,-3)!=7)
            throw Error("Polynomial normalization or arithmetic incorrect");
        bool duplicate=false;
        try { (void)interpolate_polynomial(rational,{1,ExactValue(2,2)},{0,1},[](std::uint64_t) {}); }
        catch(const Error&) { duplicate=true; }
        if(!duplicate) throw Error("Duplicate interpolation x accepted");
        std::cout << "Polynomial checkpoints, budgets, normalization and exact arithmetic passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
