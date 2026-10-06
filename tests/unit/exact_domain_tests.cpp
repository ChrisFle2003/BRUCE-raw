#include "bruce/application/exact_research.hpp"
#include "bruce/math/power_affine.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        ExactResearchTask task;
        task.minimum="0"; task.maximum="1"; task.training={{0,0}};
        const auto cancelled=exact_research(task,[](std::uint64_t n) { return n==1; });
        if(cancelled.status!="INCOMPLETE" || cancelled.stop_reason!="Cancelled" || cancelled.candidates!=1)
            throw Error("Cancellation must preserve partial status");
        if(render_exact_research(replay_exact_research(pack_exact_research(cancelled)))!=render_exact_research(cancelled))
            throw Error("Cancellation replay changed");
        const auto full=exact_research(task);
        task.budget.operations=full.operations; task.budget.candidates=full.candidates;
        task.budget.trace_events=full.trace.size(); task.budget.results=full.hypotheses.size();
        if(exact_research(task).status!=full.status) throw Error("Exact budgets must finish");
        --task.budget.operations;
        const auto limited=exact_research(task);
        if(limited.status!="INCOMPLETE" || limited.stop_reason!="OperationBudgetExceeded")
            throw Error("Insufficient budget must be incomplete");
        task.budget.operations=full.operations; task.budget.results=1;
        if(exact_research(task).stop_reason!="ResultBudgetExceeded") throw Error("Result budget must stop");
        const NumberDomain rational("rational",8);
        if(exact_text(rational.parse("-2/-4"))!="1/2" || exact_text(rational.parse("0/-7"))!="0")
            throw Error("Rational normalization changed");
        if(rational.power(0,0)!=1) throw Error("Zero exponent policy changed");
        const NumberDomain bigint("bigint",8);
        if(!exact_countercheck(bigint,ExactValue(2,2),0,1,1))
            throw Error("Countercheck must use canonical coefficients");
        if(bigint.divide(ExactValue(2,2),ExactValue(3,3))!=1)
            throw Error("Arithmetic must use canonical operand values");
        task=ExactResearchTask{};
        task.strategy="power-affine"; task.max_power=3;
        task.minimum="1"; task.maximum="1"; task.training={{2,5}};
        const auto power_cancel=exact_research(task,[](std::uint64_t n) { return n==2; });
        if(power_cancel.status!="INCOMPLETE" || power_cancel.stop_reason!="Cancelled" ||
           power_cancel.hypotheses.size()!=1 || power_cancel.hypotheses.front().power!=2)
            throw Error("Power cancellation must preserve candidate and stop position");
        if(render_exact_research(replay_exact_research(pack_exact_research(power_cancel)))!=render_exact_research(power_cancel))
            throw Error("Power cancellation replay changed");
        const auto power_full=exact_research(task);
        task.budget.operations=power_full.operations; task.budget.candidates=power_full.candidates;
        task.budget.trace_events=power_full.trace.size();
        if(exact_research(task).status!=power_full.status) throw Error("Power exact budgets must finish");
        --task.budget.operations;
        if(exact_research(task).stop_reason!="OperationBudgetExceeded") throw Error("Power cost budget must stop");
        if(power_affine(rational,ExactValue(2,4),ExactValue(2,6),-2,3)!=ExactValue(-11,3) ||
           !power_countercheck(rational,ExactValue(2,4),ExactValue(2,6),-2,ExactValue(-11,3),3) ||
           power_countercheck(rational,ExactValue(2,4),ExactValue(2,6),-2,99,3))
            throw Error("Independent power evaluation or normalization failed");
        task=ExactResearchTask{};
        task.strategy="power-affine"; task.planner="adaptive"; task.max_power=4;
        task.training={{0,3},{1,5},{2,11}}; task.holdout={{-3,21}};
        const auto planned=exact_research(task);
        // Stop at every boundary, including after planning but before its candidate.
        for(std::uint64_t checkpoint=0;checkpoint<planned.checkpoints;++checkpoint) {
            task.cancel_after_checkpoints=checkpoint;
            const auto interrupted=exact_research(task);
            if(interrupted.status!="INCOMPLETE" || interrupted.stop_reason!="Cancelled" ||
               interrupted.checkpoints!=checkpoint ||
               render_exact_research(replay_exact_research(pack_exact_research(interrupted)))!=render_exact_research(interrupted))
                throw Error("Adaptive checkpoint replay changed");
        }
        task.cancel_after_checkpoints.reset();
        unsigned polls=0;
        const auto callback_cancel=exact_research(task,[&](std::uint64_t) { return ++polls==2; });
        if(callback_cancel.candidates!=0 || callback_cancel.plans_completed!=1 || callback_cancel.stop_reason!="Cancelled" ||
           render_exact_research(replay_exact_research(pack_exact_research(callback_cancel)))!=render_exact_research(callback_cancel))
            throw Error("Cancellation after a plan must not replay before the plan");
        task.budget.operations=planned.operations; task.budget.trace_events=planned.trace.size();
        task.budget.candidates=planned.candidates;
        if(exact_research(task).status!=planned.status) throw Error("Adaptive exact budgets must complete");
        --task.budget.operations;
        if(exact_research(task).stop_reason!="OperationBudgetExceeded") throw Error("Adaptive short operation budget must stop");
        std::cout << "Exact cancellation, replay, budgets and normalization checks passed\n";
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
