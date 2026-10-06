#include "bruce/application/exact_research.hpp"
#include <iostream>
#include <map>

int main() {
    using namespace bruce;
    try {
        for(bool division:{false,true}) {
            ExactResearchTask task;
            task.strategy="expression";task.planner="grammar";task.domain="rational";
            task.expression_equivalence="rational";
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
        const auto x=[](){return Expression{{'x',0,0,0}};};
        const auto literal=[](ExactValue v){return Expression{{'c',0,0,v}};};
        const auto binary=[](char op,Expression left,const Expression& right) {
            const auto offset=static_cast<unsigned>(left.size());
            for(auto n:right) {
                if(n.op!='x' && n.op!='c') {n.left+=offset;n.right+=offset;}
                left.push_back(std::move(n));
            }
            left.push_back({op,offset-1,static_cast<unsigned>(left.size()-1),0});return left;
        };
        auto form=[&](const Expression& tree){return normalize_expression(d,tree,noop);};
        const auto one=form(literal(1));
        const auto hole=form(binary('/',x(),x()));
        if(hole.numerator!=std::vector<ExactValue>{1} || hole.denominator!=std::vector<ExactValue>{1} ||
           hole.excluded_polynomial!=std::vector<ExactValue>{0,1} || hole==one) throw Error("Cancellation erased x=0");
        const auto square=binary('*',x(),x());
        if(form(binary('/',square,square))!=hole) throw Error("Repeated divisor factors differ");
        const auto removable=form(binary('/',binary('-',square,literal(1)),binary('-',x(),literal(1))));
        if(removable.numerator!=std::vector<ExactValue>{1,1} || removable.denominator!=std::vector<ExactValue>{1} ||
           removable.excluded_polynomial!=std::vector<ExactValue>{-1,1}) throw Error("Polynomial gcd lost removable pole");
        const auto nested=form(binary('/',literal(1),binary('/',literal(1),x())));
        if(nested.numerator!=std::vector<ExactValue>{0,1} || nested.excluded_polynomial!=std::vector<ExactValue>{0,1})
            throw Error("Nested division lost inner hole");
        const auto scaled=binary('*',literal(ExactValue(-2,3)),x());
        if(form(binary('/',scaled,scaled))!=hole) throw Error("Rational scaling normalization differs");
        const auto empty=form(binary('*',literal(0),binary('/',literal(1),binary('-',x(),x()))));
        if(!empty.empty_domain || empty.excluded_polynomial!=std::vector<ExactValue>{0}) throw Error("Empty domain became zero function");
        const auto conservative=form(binary('/',literal(0),binary('+',square,literal(1))));
        if(conservative==form(literal(0)) || conservative.excluded_polynomial!=std::vector<ExactValue>{1,0,1})
            throw Error("Unsupported root filtering applied");
        ExpressionGrammar grammar({1},7,noop);
        std::map<std::string,RationalExpressionForm> forms;
        for(std::uint64_t i=0;i<grammar.size();++i) {
            const auto tree=grammar.at(i,noop);const auto normalized=form(tree);
            const auto key=normal_form_key(normalized);
            if(forms.contains(key) && forms.at(key)!=normalized) throw Error("Normal form key collision");
            forms.emplace(key,normalized);
            for(const ExactValue& input:{ExactValue(-2),ExactValue(-1),ExactValue(0),ExactValue(1),ExactValue(2),ExactValue(2,3)}) {
                std::optional<ExactValue> original,reduced;
                try {original=evaluate_expression(d,tree,input,noop);} catch(const ExpressionPole&) {}
                try {reduced=evaluate_normal_form(d,normalized,input,noop);} catch(const ExpressionPole&) {}
                if(original!=reduced) throw Error("Normalized evaluation or domain differs");
            }
        }
        bool bit_stop=false;
        try {(void)normalize_expression(NumberDomain("rational",1),binary('+',x(),x()),noop);} catch(const NumericLimit&) {bit_stop=true;}
        if(!bit_stop) throw Error("Symbolic coefficient bit limit ignored");
        std::cout<<"Rational normal forms, domain guards, provenance replay and budget boundaries passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
