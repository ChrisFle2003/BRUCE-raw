#include "bruce/application/exact_research.hpp"
#include <iostream>
#include <set>

int main() {
    using namespace bruce;
    try {
        for(bool division:{false,true}) {
            ExactResearchTask task;
            task.strategy="expression";task.planner="grammar";task.domain="rational";
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
        const NumberDomain d("rational",256);
        ExpressionGrammar grammar({1},7,noop);
        if(grammar.size()!=2802) throw Error("Grammar count differs");
        std::set<std::string> seen;
        for(std::uint64_t i=0;i<grammar.size();++i) {
            const auto tree=grammar.at(i,noop);
            if(!seen.insert(expression_formula(tree)).second) throw Error("Duplicate grammar expression");
            for(const ExactValue& x:{ExactValue(-1),ExactValue(0),ExactValue(2,3)}) {
                std::optional<ExactValue> forward,counter;
                try {forward=evaluate_expression(d,tree,x,noop);} catch(const ExpressionPole&) {}
                try {counter=counter_evaluate_expression(d,tree,x,noop);} catch(const ExpressionPole&) {}
                if(forward!=counter) throw Error("Counterevaluation or pole mismatch");
            }
        }
        std::vector<ExactValue> constants;
        for(unsigned i=0;i<64;++i) constants.emplace_back(i);
        const ExpressionGrammar large(constants,9,noop);
        if(large.size()!=1327858375545ULL || large.at(large.size()-1,noop).size()!=9) throw Error("Maximum grammar rank failed");
        for(const auto& tree:{Expression{},Expression{{'+',0,0,0}},Expression{{'x',0,0,0},{'c',0,0,1},{'+',0,0,0}}}) {
            bool rejected=false;
            try {(void)evaluate_expression(d,tree,1,noop);} catch(const Error&) {rejected=true;}
            if(!rejected) throw Error("Malformed tree accepted");
        }
        const Expression hole{{'c',0,0,0},{'c',0,0,1},{'c',0,0,0},{'/',1,2,0},{'*',0,3,0}};
        for(bool counter:{false,true}) {
            bool rejected=false;
            try {if(counter) (void)counter_evaluate_expression(d,hole,1,noop); else (void)evaluate_expression(d,hole,1,noop);}
            catch(const ExpressionPole&) {rejected=true;}
            if(!rejected) throw Error("Zero multiplication erased a division hole");
        }
        std::cout<<"Expression grammar, counterexecution, holes, checkpoints and operation cutoffs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
