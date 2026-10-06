#include "bruce/application/exact_research.hpp"
#include "bruce/math/rational_linear.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        for(bool singular:{false,true}) {
            ExactResearchTask task;
            task.strategy="rational-linear"; task.planner="solve"; task.domain="rational";
            task.minimum="0"; task.maximum="1";
            task.training=singular ? std::vector<ExactSample>{{2,1},{3,1}} :
                std::vector<ExactSample>{{0,1},{1,ExactValue(1,2)},{2,ExactValue(1,3)}};
            task.holdout={{4,singular ? ExactValue(1) : ExactValue(1,5)}};
            const auto full=exact_research(task);
            if(full.status!=(singular ? "AMBIGUOUS" : "VERIFIED_ON_SAMPLES")) throw Error("Reference failed");
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Cancellation replay differs");
            }
            task.cancel_after_checkpoints.reset();
            unsigned polls=0;
            const auto callback=exact_research(task,[&](std::uint64_t) { return ++polls==2; });
            if(callback.candidates!=0 || callback.plans_completed!=1 || callback.stop_reason!="Cancelled" ||
               render_exact_research(replay_exact_research(pack_exact_research(callback)))!=render_exact_research(callback))
                throw Error("Callback cancellation checkpoint lost");
            task.budget.operations=full.operations; task.budget.candidates=full.candidates; task.budget.trace_events=full.trace.size();
            if(exact_research(task).status!=full.status) throw Error("Exact budgets failed");
            for(std::uint64_t budget=1;budget<full.operations;++budget) {
                task.budget.operations=budget;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>budget || run.stop_reason!="OperationBudgetExceeded" ||
                   render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                    throw Error("Operation cutoff failed");
            }
        }
        const NumberDomain d("rational",64);
        std::uint64_t work=0;
        const auto plan=solve_rational_linear(d,{0,ExactValue(2,2),2},{1,ExactValue(1,2),ExactValue(1,3)},
            [&](std::uint64_t n){work+=n;});
        if(plan.decision!="derived" || plan.rank!=3 || plan.coefficients!=std::array<ExactValue,3>{0,1,1} || !work)
            throw Error("Solver normalization failed");
        if(rational_linear(d,0,1,1,3)!=ExactValue(1,4) ||
           !rational_linear_countercheck(d,0,1,1,3,ExactValue(1,4)) || rational_linear_countercheck(d,0,1,1,3,1))
            throw Error("Independent math check failed");
        for(bool countercheck:{false,true}) {
            bool pole=false;
            try {
                if(countercheck) (void)rational_linear_countercheck(d,1,0,0,0,1);
                else (void)rational_linear(d,1,0,0,0);
            } catch(const RationalPole&) {pole=true;}
            if(!pole) throw Error("Removable pole accepted");
        }
        const auto inconsistent=solve_rational_linear(d,{0,1,2,3},{1,ExactValue(1,2),ExactValue(1,3),7},[](std::uint64_t){});
        if(inconsistent.decision!="inconsistent") throw Error("Inconsistent row ignored");
        std::cout<<"Rational function checkpoints, operation cutoffs, normalization and poles passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
