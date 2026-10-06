#include "bruce/application/exact_research.hpp"
#include "bruce/math/polynomial.hpp"
#include "bruce/coordinate/address.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        const auto replay_equal=[](const ExactResearchRun& run) {
            if(render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                throw Error("Five-family replay differs");
        };
        for(bool constant:{false,true}) {
            ExactResearchTask task;
            task.strategy="portfolio-expression";task.planner="portfolio";task.domain="rational";
            task.expression_equivalence="rational";task.minimum="1";task.maximum="1";
            task.max_nodes=3;task.max_power=1;task.max_degree=1;
            for(int x=1;x<=4;++x) task.training.push_back({x,constant ? 1 : x+1});
            task.holdout={{0,1},{5,constant ? 1 : 6}};
            const auto full=exact_research(task);
            if(full.status!=(constant ? "AMBIGUOUS" : "VERIFIED_ON_SAMPLES") || full.strategy_runs.size()!=5) throw Error("Five-family reference failed");
            std::uint64_t child_ops=0,child_checkpoints=0,source_count=0,events=full.trace.size(),child_normal=0,child_groups=0;
            for(const auto& child:full.strategy_runs) {
                child_ops+=child.operations;child_checkpoints+=child.checkpoints;events+=child.trace.size();
                child_normal+=child.normalization_operations;child_groups+=child.hypotheses.size();
                source_count+=child.task.strategy=="expression" ? child.expression_sources : child.hypotheses.size();
            }
            if(full.operations!=child_ops+full.union_operations || full.normalization_operations!=child_normal+full.union_operations ||
               full.checkpoints!=child_checkpoints+child_groups || full.expression_sources!=source_count) throw Error("Union accounting failed");
            std::uint64_t trace_bytes=0;
            for(const auto& entry:full.trace) trace_bytes+=entry.size();
            for(const auto& child:full.strategy_runs) trace_bytes+=child.trace_bytes;
            if(trace_bytes!=full.trace_bytes) throw Error("Union trace byte accounting failed");
            replay_equal(full);
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i) throw Error("Union checkpoint cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_checkpoints.reset();
            for(std::uint64_t i=0;i<full.candidates;++i) {
                task.cancel_after_candidates=i;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.candidates!=i) throw Error("Global candidate cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_candidates.reset();
            for(std::uint64_t boundary:{std::uint64_t(0),full.strategy_runs[0].checkpoints,full.checkpoints-1}) {
                unsigned polls=0;const auto run=exact_research(task,[&](std::uint64_t){return ++polls==boundary+1;});
                if(run.stop_reason!="Cancelled" || run.checkpoints!=boundary) throw Error("Union callback replay failed");
                replay_equal(run);
            }
            task.budget.operations=full.operations;task.budget.candidates=full.candidates+1;task.budget.trace_events=events;
            if(exact_research(task).status!=full.status) throw Error("Exact union operation/trace budgets failed");
            for(std::uint64_t n=1;n<full.operations;++n) {
                task.budget.operations=n;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>n) throw Error("Union operation cutoff failed");
                replay_equal(run);
            }
            task.budget.operations=full.operations;
            for(std::uint64_t n=1;n<source_count;++n) {
                task.budget.results=n;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.expression_sources>n) throw Error("Source result cap failed");
                replay_equal(run);
            }
        }
        const NumberDomain domain("rational",256);const auto noop=[](std::uint64_t){};
        // Explicit fractions must retain their original denominator zeros after cancellation.
        const auto cancelled=normalize_fraction(domain,{-1,0,1},{-1,1},noop);
        if(cancelled.numerator!=std::vector<ExactValue>{1,1} || cancelled.denominator!=std::vector<ExactValue>{1} ||
           cancelled.excluded_polynomial!=std::vector<ExactValue>{-1,1}) throw Error("Fraction conversion erased pole");
        const auto reduced=normalize_fraction(domain,{-2,0,2,0},{-2,2,0},noop);
        if(cancelled!=reduced) throw Error("Fraction scaling/trimming differs");
        std::vector<ExactValue> degree8(9,1);
        const auto form=normalize_fraction(domain,degree8,{1},noop);
        for(const ExactValue& x:{ExactValue(-2),ExactValue(0),ExactValue(2,3)})
            if(evaluate_normal_form(domain,form,x,noop)!=polynomial_sum(domain,degree8,x)) throw Error("Degree-eight conversion failed");
        if(!normalize_fraction(domain,{1},{0,0},noop).empty_domain) throw Error("Zero denominator accepted");
        bool invalid=false;try {(void)normalize_fraction(domain,{}, {1},noop);} catch(const ExpressionNormalizationLimit&) {invalid=true;}
        if(!invalid) throw Error("Empty polynomial accepted");
        // Upper-bound identity is lazy: generating the last tree does not enumerate the grammar.
        std::vector<ExactValue> constants;for(unsigned i=0;i<64;++i) constants.emplace_back(i);
        ExpressionGrammar grammar(constants,9,noop);
        const auto last=5*(grammar.size()-1)+4;
        (void)grammar.at(grammar.size()-1,noop);
        (void)bruce::pair(last/(1ULL<<31),last%(1ULL<<31));
        std::cout<<"Five-family union: provenance budgets, exact cutoffs, cancellation, fractions and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
