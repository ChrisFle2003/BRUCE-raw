#include "bruce/application/exact_research.hpp"
#include "bruce/application/portfolio_normalization.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>
#include <map>

namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
void replay_equal(const bruce::ExactResearchRun& r) {
    require(bruce::render_exact_research(bruce::replay_exact_research(bruce::pack_exact_research(r)))==bruce::render_exact_research(r),"Q portfolio replay mismatch");
}
void accounting(const bruce::ExactResearchRun& r) {
    std::uint64_t operations=r.union_operations+r.ordering_operations,sources=0,events=r.trace.size(),bytes=0;
    for(const auto& event:r.trace) bytes+=event.size();
    for(const auto& child:r.strategy_runs) {
        operations+=child.operations;events+=child.trace.size();bytes+=child.trace_bytes;
        sources+=child.task.strategy=="expression" ? child.expression_sources : child.hypotheses.size();
    }
    require(r.operations==operations && sources==r.expression_sources,"Q total costs or sources differ");
    require(events<=r.task.budget.trace_events && bytes==r.trace_bytes && bytes<=r.task.max_trace_bytes,"Deferred union trace exceeded budget");
}
}
// Exercise modular proofs specifically in the cross-family union, independently
// of the grammar's source cap and enumeration order.
void modular_union_boundaries() {
    using namespace bruce;
    const NumberDomain domain("rational",4096);
    ExactResearchRun seed;seed.task.domain="rational";seed.task.strategy="portfolio-expression";
    seed.task.expression_equivalence="rational-q-witness";seed.task.planner="portfolio";
    seed.task.training={{2,0}};seed.task.holdout={{3,0}};
    ExactResearchRun polynomial;polynomial.task.strategy="polynomial";
    ExactHypothesis zero{};zero.id=0;zero.status="VERIFIED_ON_SAMPLES";zero.coefficients={0};
    polynomial.hypotheses={zero};std::map<std::string,std::size_t> seen;
    require(merge_normalized_portfolio(seed,polynomial,1,seen,{}),"Cannot prepare zero representative");
    for(const auto& denominator:std::vector<std::vector<ExactValue>>{{-2,0,0,1},{-1,0,0,1},{-36,0,36,0,-11,0,1}}) {
        ExactResearchRun child;child.task.strategy="expression";
        ExactHypothesis source{};source.id=9;source.status="VERIFIED_ON_SAMPLES";
        source.normal_form=normalize_fraction(domain,{0},denominator,[](std::uint64_t){});
        child.hypotheses={source};
        auto full=seed;
        require(merge_normalized_portfolio(full,child,4,seen,{}),"Modular union incomplete");
        const bool rootless_certificate=denominator==std::vector<ExactValue>{-2,0,0,1};
        require(full.hypotheses.size()==(rootless_certificate ? 1u : 2u),"False modular merge or missed cubic proof");
        require(full.domain_modular_certificates==(rootless_certificate ? 1u : 0u),"Modular certificate counter wrong");
        if(denominator==std::vector<ExactValue>{-1,0,0,1}) {
            require(full.domain_bounded_witnesses==1 && full.domain_different==1 && full.domain_unknown==0,
                "Union witness not completed");
            require(full.union_witness_summary.find("domain_witness=1")!=std::string::npos,"Union witness not visible");
        }
        if(rootless_certificate) require(full.hypotheses[0].origins.size()==2,"Merged origin lost");
        for(std::uint64_t n=seed.operations;n<full.operations;++n) {
            auto partial=seed;partial.task.budget.operations=n;
            require(!merge_normalized_portfolio(partial,child,4,seen,{}),"Union ignored operation cutoff");
            require(partial.operations==n && partial.stop_reason=="OperationBudgetExceeded", "Modular union cutoff not exact");
            require(partial.hypotheses.size()==2 && partial.hypotheses.back().status=="INCOMPLETE" &&
                partial.hypotheses.back().origins.size()==1,"Unfinished union lost original or merged without proof");
            require(partial.domain_bounded_witnesses==0 && partial.union_witness_summary.empty(),"Partial union witness published");
            require(partial.domain_comparison_operations<=partial.union_operations,"Domain work not counted in union");
        }
        auto legacy=seed;legacy.task.expression_equivalence="rational-q";
        require(merge_normalized_portfolio(legacy,child,4,seen,{}) && legacy.hypotheses.size()==2 &&
                legacy.domain_modular_certificates==0,"Historical policy acquired modular proofs");
    }
}
int main() {
    using namespace bruce;
    try {
        modular_union_boundaries();
        for(const std::string equivalence:{"rational-q-witness"}) {
        // Actual cross-family grouping with formal x^2+1 exclusions: equal over Q.
        ExactResearchTask t;t.strategy="portfolio-expression";t.planner="portfolio";t.domain="rational";
        t.expression_equivalence=equivalence;t.minimum="0";t.maximum="1";t.max_power=1;t.max_degree=1;t.max_nodes=3;
        t.expression_operators="+/";t.training={{1,0},{2,0},{3,0}};t.holdout={{4,0}};
        const auto full=exact_research(t);accounting(full);replay_equal(full);
        require(full.stop_reason=="Completed" && full.strategy_runs.size()==5,"Cross-family Q run incomplete");
        bool combined=false;
        for(const auto& h:full.hypotheses) if(h.normal_form && h.normal_form->excluded_polynomial==std::vector<ExactValue>{1}) {
            bool polynomial=false,quadratic=false;
            for(const auto& o:h.origins) {polynomial|=o.strategy=="polynomial";quadratic|=o.strategy=="rational-quadratic";}
            combined|=polynomial && quadratic;
        }
        require(combined,"Rootless quadratic not merged with polynomial family");
        t.planner="adaptive-order";const auto adaptive=exact_research(t);accounting(adaptive);replay_equal(adaptive);
        require(adaptive.hypotheses.size()==full.hypotheses.size(),"Q grouping depends on search order");
        for(std::size_t i=0;i<full.hypotheses.size();++i) {
            require(adaptive.hypotheses[i].id==full.hypotheses[i].id && adaptive.hypotheses[i].normal_form==full.hypotheses[i].normal_form,
                "Q representative depends on search order");
        }
        // Smaller complete problem permits exhaustive interruption at every step.
        t.maximum="0";t.expression_operators="/";t.max_nodes=1;
        const auto source=exact_research(t);
        const auto store=append_experience("",pack_exact_research(source));
        for(const std::string order:{"portfolio","adaptive-order","experience-order"}) {
            t.planner=order;t.experience_snapshot=order=="experience-order" ? store : "";
            const auto reference=exact_research(t);accounting(reference);replay_equal(reference);
            require(reference.status!="INCOMPLETE","Q budget reference incomplete");
            for(std::uint64_t n=1;n<reference.operations;++n) {
                t.budget.operations=n;const auto r=exact_research(t);
                require(r.status=="INCOMPLETE" && r.operations<=n,"Q shared operation cutoff failed");
                accounting(r);replay_equal(r);
            }
            t.budget.operations=reference.operations;
            require(exact_research(t).status==reference.status,"Exact Q portfolio budget insufficient");
            t.budget.operations=1000000;
            for(std::uint64_t n=0;n<reference.checkpoints;++n) {
                t.cancel_after_checkpoints=n;const auto r=exact_research(t);
                require(r.status=="INCOMPLETE" && r.stop_reason=="Cancelled" && r.checkpoints==n,"Q union checkpoint cancellation failed");
                accounting(r);replay_equal(r);
            }
            t.cancel_after_checkpoints.reset();
            for(std::uint64_t n=0;n<reference.candidates;++n) {
                t.cancel_after_candidates=n;const auto r=exact_research(t);
                require(r.status=="INCOMPLETE" && r.stop_reason=="Cancelled","Q candidate cancellation failed");accounting(r);replay_equal(r);
            }
            t.cancel_after_candidates.reset();
            for(std::uint64_t n=2;n<30;++n) {t.budget.trace_events=n;const auto r=exact_research(t);accounting(r);replay_equal(r);}
            t.budget.trace_events=100002;
            for(std::uint64_t n=256;n<4096;n+=97) {t.max_trace_bytes=n;const auto r=exact_research(t);accounting(r);replay_equal(r);}
            t.max_trace_bytes=1048576;
        }
        }
        std::cout<<"Q cross-family union, deterministic grouping, all budget/cancel positions and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
