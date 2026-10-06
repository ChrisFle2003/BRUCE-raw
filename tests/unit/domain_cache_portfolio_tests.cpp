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
    std::uint64_t cache_ops=r.union_domain_cache_stats.operations,cache_hits=r.union_domain_cache_stats.hits;
    for(const auto& child:r.strategy_runs) {cache_ops+=child.domain_cache_stats.operations;cache_hits+=child.domain_cache_stats.hits;}
    require(r.domain_cache_stats.operations==cache_ops && r.domain_cache_stats.hits==cache_hits,"Portfolio cache statistics not summed exactly");
    require(cache_ops<=r.domain_comparison_operations && r.union_domain_cache_stats.operations<=r.union_operations,
            "Cache work not part of shared domain/union budget");
    require(r.domain_cache_stats.entries<=128 && r.domain_cache_stats.key_bytes<=128*4096,"Separate caches exceed storage bound");
    require(r.operations==operations && sources==r.expression_sources,"Q total costs or sources differ");
    require(events<=r.task.budget.trace_events && bytes==r.trace_bytes && bytes<=r.task.max_trace_bytes,"Deferred union trace exceeded budget");
}
}
// A completed cubic proof must be reusable across later union origins;
// every cutoff before the cache hit finishes keeps that new origin separate.
void cached_union_cutoffs() {
    using namespace bruce;
    NumberDomain d("rational",4096);EqualDomainCache cache(d);
    ExactResearchRun seed;seed.task.domain="rational";seed.task.strategy="portfolio-expression";
    seed.task.expression_equivalence="rational-q-modular";seed.task.domain_cache="equal-64";
    seed.task.planner="portfolio";seed.task.training={{2,0}};seed.task.holdout={{3,0}};
    ExactResearchRun child;child.task.strategy="polynomial";
    ExactHypothesis h{};h.status="VERIFIED_ON_SAMPLES";h.coefficients={0};child.hypotheses={h};
    std::map<std::string,std::size_t> seen;
    require(merge_normalized_portfolio(seed,child,1,seen,{},&cache),"Cannot prepare cached union");
    child.task.strategy="expression";h.id=7;
    h.normal_form=normalize_fraction(d,{0},{-2,0,0,1},[](std::uint64_t){});child.hypotheses={h};
    require(merge_normalized_portfolio(seed,child,4,seen,{},&cache) && seed.hypotheses.size()==1 && cache.stats().inserts==1,
            "Union failed to store completed cubic proof");
    child.hypotheses[0].id=8;auto full=seed;auto warm=cache;
    require(merge_normalized_portfolio(full,child,4,seen,{},&warm) && warm.stats().hits==1 &&
            full.hypotheses.size()==1 && full.hypotheses[0].origins.size()==3,"Union did not reuse proof or lost provenance");
    for(std::uint64_t n=seed.operations;n<full.operations;++n) {
        auto partial=seed;auto interrupted=cache;partial.task.budget.operations=n;
        require(!merge_normalized_portfolio(partial,child,4,seen,{},&interrupted),"Union cache ignored cutoff");
        require(partial.operations==n && partial.hypotheses.size()==2 && partial.hypotheses.back().status=="INCOMPLETE",
                "Unfinished cache match erased original");
        require(interrupted.stats().hits==0 && interrupted.stats().entries==1,"Unfinished cache lookup reported hit");
    }
}
int main() {
    using namespace bruce;
    try {
        cached_union_cutoffs();

        for(const std::string equivalence:{"rational-q-modular"}) {
        // Actual cross-family grouping with formal x^2+1 exclusions: equal over Q.
        ExactResearchTask t;t.strategy="portfolio-expression";t.planner="portfolio";t.domain="rational";
        t.expression_equivalence=equivalence;t.domain_cache="equal-64";t.minimum="0";t.maximum="1";t.max_power=1;t.max_degree=1;t.max_nodes=3;
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
