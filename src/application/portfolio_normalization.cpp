#include "bruce/application/portfolio_normalization.hpp"
#include "bruce/application/research.hpp"
#include "bruce/math/polynomial.hpp"
#include <sstream>
#include "bruce/application/portfolio_version.hpp"

namespace bruce {
bool merge_normalized_portfolio(ExactResearchRun& run,const ExactResearchRun& child,unsigned index,
    std::map<std::string,std::size_t>& seen,const std::function<bool(std::uint64_t)>& cancelled,EqualDomainCache* domain_cache) {
    const bool witnessed=run.task.expression_equivalence=="rational-q-witness";
    const bool modular=witnessed || run.task.expression_equivalence=="rational-q-modular";
    const bool rational_q=modular || run.task.expression_equivalence=="rational-q";
    if(domain_cache && (witnessed || !modular || run.task.domain_cache!="equal-64")) throw Error("InvalidPortfolioDomainCache");
    run.union_witness_summary.clear();
    const NumberDomain domain(run.task.domain,run.task.max_bits);
    struct Stop {};
    auto stop=[&](std::string reason){run.stop_reason=std::move(reason);throw Stop{};};
    auto spend=[&](std::uint64_t cost) {
        if(run.task.budget.operations-run.operations<cost) stop("OperationBudgetExceeded");
        run.operations+=cost;run.union_operations+=cost;run.normalization_operations+=cost;
    };
    auto domain_spend=[&](std::uint64_t cost) {
        if(run.task.budget.operations-run.operations<cost) stop("OperationBudgetExceeded");
        run.operations+=cost;run.union_operations+=cost;run.domain_comparison_operations+=cost;
    };
    for(const auto& source:child.hypotheses) {
        auto merged=source;
        merged.id=5*source.id+index;
        merged.a_index=merged.id/(1ULL<<31);merged.b_index=merged.id%(1ULL<<31);
        merged.origins={{child.task.strategy,source.id,source.status}};
        auto partial=[&]() {merged.status="INCOMPLETE";run.hypotheses.push_back(std::move(merged));};
        try {
            if((run.task.cancel_after_checkpoints && run.checkpoints>=*run.task.cancel_after_checkpoints) ||
               (run.task.cancel_after_candidates && run.candidates>=*run.task.cancel_after_candidates) ||
               (cancelled && cancelled(run.candidates))) {
                run.task.cancel_after_checkpoints=run.checkpoints;stop("Cancelled");
            }
            ++run.checkpoints;
            spend(1);
            if(source.status=="INCOMPLETE") {partial();continue;}
            if(index<4) {
                RationalExpressionForm original;
                if(index==0) {
                    original.numerator.assign(source.power+1,0);
                    original.numerator[0]=source.b;original.numerator[source.power]=source.a;
                } else if(index==1) original.numerator=source.coefficients;
                else {original.numerator={source.b,source.a};original.denominator=source.denominator;original.excluded_polynomial=source.denominator;}
                merged.normal_form=normalize_fraction(domain,original.numerator,original.denominator,spend);
                for(const auto* samples:{&run.task.training,&run.task.holdout}) for(const auto& sample:*samples) {
                    std::optional<ExactValue> before,after;
                    try {before=evaluate_normal_form(domain,original,sample.x,spend);} catch(const ExpressionPole&) {}
                    try {after=evaluate_normal_form(domain,*merged.normal_form,sample.x,spend);} catch(const ExpressionPole&) {}
                    spend(1);if(before!=after) throw Error("PortfolioNormalizationMismatch");
                }
            }
            if(!merged.normal_form) throw Error("MissingPortfolioNormalForm");
            std::optional<std::size_t> match;
            if(rational_q) {
                auto equal=[&](const auto& a,const auto& b) {
                    domain_spend(1);if(a.size()!=b.size()) return false;
                    for(std::size_t i=0;i<a.size();++i) {domain_spend(1);if(a[i]!=b[i]) return false;}
                    return true;
                };
                for(std::size_t i=0;i<run.hypotheses.size();++i) {
                    const auto& target=run.hypotheses[i];domain_spend(1);
                    if(target.status=="INCOMPLETE" || !target.normal_form) continue;
                    const auto& a=*merged.normal_form;const auto& b=*target.normal_form;
                    if(a.empty_domain!=b.empty_domain) continue;
                    if(!a.empty_domain && (!equal(a.numerator,b.numerator) || !equal(a.denominator,b.denominator))) continue;
                    ++run.domain_comparisons;
                    const auto comparison=domain_cache ? domain_cache->compare(a.excluded_polynomial,b.excluded_polynomial,domain_spend) :
                        compare_rational_domains(domain,a.excluded_polynomial,b.excluded_polynomial,domain_spend,modular,witnessed);
                    run.domain_modular_certificates+=(comparison.left_modular_prime!=0)+(comparison.right_modular_prime!=0);
                    if(witnessed) {
                        run.domain_witness_candidates+=comparison.left_witness_candidates+comparison.right_witness_candidates;
                        if(comparison.relation=="DIFFERENT") ++run.domain_different;
                        if(comparison.witness && (comparison.excluded_side=="left" ? comparison.left_witness_candidates>0 : comparison.right_witness_candidates>0)) {
                            ++run.domain_bounded_witnesses;
                            if(run.union_witness_summary.empty()) run.union_witness_summary=" domain_witness="+exact_text(*comparison.witness)+
                                " candidate="+std::to_string(merged.id)+" witness_against="+std::to_string(target.id)+" excluded_side="+comparison.excluded_side;
                        }
                    }
                    if(comparison.relation=="UNKNOWN") ++run.domain_unknown;
                    if(comparison.relation=="EQUAL") {match=i;break;}
                }
            } else {
                const auto key=normal_form_key(*merged.normal_form);
                const auto found=seen.find(key);
                if(found!=seen.end()) match=found->second;
                else seen.emplace(key,run.hypotheses.size());
            }
            if(!match) run.hypotheses.push_back(std::move(merged));
            else {
                auto& target=run.hypotheses[*match];
                target.origins.push_back(merged.origins.front());
                // Preserve fixed-family identity even if this family executed later.
                if(run.task.planner!="portfolio" && (index<target.id%5 || (index==target.id%5 && merged.id<target.id))) {
                    target.id=merged.id;target.a_index=merged.a_index;target.b_index=merged.b_index;
                }
                if(target.status!=source.status) {
                    target.status="INCOMPLETE";stop("StrategyResultConflict");
                }
            }
        } catch(const Stop&) {
            // A conflict already retained both origins in its group.
            if(run.stop_reason!="StrategyResultConflict") partial();
            return false;
        } catch(const NumericLimit&) {run.stop_reason="IntegerBitLimit";partial();return false;}
          catch(const ExpressionNormalizationLimit&) {run.stop_reason="ExpressionNormalizationDegreeLimit";partial();return false;}
    }
    return true;
}
std::string render_normalized_portfolio(const ExactResearchRun& run) {
    const auto version=portfolio_version(run.task);
    const bool domain_cached=run.task.domain_cache=="equal-64";
    const bool witnessed=run.task.expression_equivalence=="rational-q-witness";
    const bool modular=witnessed || run.task.expression_equivalence=="rational-q-modular";
    const bool rational_q=modular || run.task.expression_equivalence=="rational-q";
    const bool cached=run.task.expression_planner=="cached-grammar";
    const bool powered=run.task.expression_operators.find('^')!=std::string::npos;
    const bool selected=run.task.expression_operators!="+-*/";
    const bool indexed=run.task.expression_planner=="indexed-grammar";
    const bool learned=run.task.planner=="experience-order";
    const bool adaptive=learned || run.task.planner=="adaptive-order";
    std::ostringstream out;
    std::uint64_t events=run.trace.size();
    for(const auto& child:run.strategy_runs) events+=child.trace.size();
    auto coefficients=[&](const std::vector<ExactValue>& values) {
        out<<'[';for(std::size_t i=0;i<values.size();++i) {if(i) out<<',';out<<json_string(exact_text(values[i]));}out<<']';
    };
    out<<"{\"engine\":"<<json_string(version.engine)<<",\"rule_version\":"<<json_string(rational_q ? "q-domain-union-v1" : "normal-form-union-v1")<<",\"policy_version\":"<<json_string(witnessed ? "shared-budget-v4" : rational_q ? "shared-budget-v3" : "shared-budget-v2")<<",\"strategy\":\"portfolio-expression\",\"planner\":"<<json_string(version.planner)<<",\"equivalence\":"<<json_string(witnessed ? "rational-q-witness-domain-v1" : modular ? "rational-q-modular-domain-v1" : rational_q ? "rational-q-domain-v1" : "rational-domain-v1")
       <<",\"domain\":"<<json_string(run.task.domain)<<",\"max_power\":"<<run.task.max_power<<",\"max_degree\":"<<run.task.max_degree
       <<",\"max_nodes\":"<<run.task.max_nodes<<",\"max_bits\":"<<run.task.max_bits<<",\"max_denominator\":"<<run.task.max_denominator
       <<",\"minimum\":"<<json_string(run.task.minimum)<<",\"maximum\":"<<json_string(run.task.maximum)
       <<",\"status\":"<<json_string(run.status)<<",\"stop_reason\":"<<json_string(run.stop_reason)
       <<",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
       <<",\"claim_scope\":"<<json_string(rational_q ? "Five bounded families; fixed-order union using proven Q-domain equality; unknown comparisons stay separate; sample checks are no general proof" : powered ? "Five bounded families; only expression uses selected operators and declared literal powers 2..8; exclusions retained; sample checks are no general proof" : selected ? "Five bounded families; operator selection restricts only the expression family; IDs depend on that grammar; sample checks are no general proof" : "Five bounded families; equal reduced fractions and exclusion polynomials merge; sample checks are no general proof")
       <<",\"explanation\":"<<json_string(run.status=="INCOMPLETE" ? "Mindestens eine Familie oder ihre Zusammenfuehrung bleibt unvollstaendig; Treffer sind Teilbefunde." :
           run.status=="VERIFIED_ON_SAMPLES" ? "Eine Formelgruppe mit erhaltener Ausschlussbedingung besteht die Samples. Alle Herkuenfte bleiben in den Teilruns erhalten; kein allgemeiner Beweis der Datenbeziehung." :
           run.status=="AMBIGUOUS" ? "Mehrere Normalformen oder Ausschlussbedingungen bleiben gleichrangig; unterschiedliche Gruppen beweisen keine Ungleichheit." :
           run.status=="CANDIDATE" ? "Eine Formelgruppe passt zu den Lernpaaren; Hold-out fehlt." :
           run.status=="REFUTED" ? "Alle Lernkandidaten scheitern an Hold-out." : "Kein Kandidat in den vollstaendig untersuchten begrenzten Familien passt.")
       <<",\"operations_used\":"<<run.operations<<",\"planning_operations\":"<<run.planning_operations
       <<",\"normalization_operations\":"<<run.normalization_operations<<",\"union_operations\":"<<run.union_operations
       <<",\"candidates_executed\":"<<run.candidates<<",\"stored_source_hypotheses\":"<<run.expression_sources
       <<",\"plans_completed\":"<<run.plans_completed<<",\"checkpoints\":"<<run.checkpoints
       <<",\"trace_bytes\":"<<run.trace_bytes<<",\"trace_events_used\":"<<events
       ;
    if(rational_q) out<<",\"expression_operators\":"<<json_string(run.task.expression_operators)<<",\"expression_planner\":"<<json_string(cached ? "subtree-cache-v1" : indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"cache_policy\":"<<json_string(cached ? "direct-map-64-size3-v1" : "none")<<",\"domain_comparison_policy\":"<<json_string(witnessed ? "rational-domain-gcd-modular-witness16-v1" : modular ? "rational-domain-gcd-modular-v1" : "rational-domain-gcd-quadratic-v1")
        <<",\"domain_comparison_operations\":"<<run.domain_comparison_operations<<",\"domain_comparisons\":"<<run.domain_comparisons<<",\"domain_unknown\":"<<run.domain_unknown;
    else if(cached) out<<",\"expression_operators\":"<<json_string(run.task.expression_operators)<<",\"expression_planner\":\"subtree-cache-v1\",\"cache_policy\":\"direct-map-64-size3-v1\"";
    else if(selected) out<<",\"expression_operators\":"<<json_string(run.task.expression_operators)<<",\"expression_planner\":"<<json_string(indexed ? "block-index-v1" : "size-ranked-v1");
    else if(indexed) out<<",\"expression_planner\":\"block-index-v1\"";
    if(domain_cached) {
        const auto& stats=run.domain_cache_stats;
        out<<",\"domain_cache_policy\":\"equal-64-key4096-v1\",\"domain_cache_scope\":\"expression-and-union-separate-v1\""
           <<",\"domain_cache_hits\":"<<stats.hits<<",\"domain_cache_misses\":"<<stats.misses
           <<",\"domain_cache_bypasses\":"<<stats.bypasses<<",\"domain_cache_inserts\":"<<stats.inserts
           <<",\"domain_cache_evictions\":"<<stats.evictions<<",\"domain_cache_entries\":"<<stats.entries
           <<",\"domain_cache_key_bytes\":"<<stats.key_bytes<<",\"domain_cache_operations\":"<<stats.operations
           <<",\"union_domain_cache_hits\":"<<run.union_domain_cache_stats.hits
           <<",\"union_domain_cache_operations\":"<<run.union_domain_cache_stats.operations;
    }
    if(witnessed) out<<",\"domain_witness_candidates\":"<<run.domain_witness_candidates<<",\"domain_bounded_witnesses\":"<<run.domain_bounded_witnesses<<",\"domain_different\":"<<run.domain_different;
    if(modular) out<<",\"domain_modular_certificates\":"<<run.domain_modular_certificates;
    if(rational_q || cached || powered) out<<",\"power_exponent_policy\":\"literal-2-to-8-v1\"";
    if(adaptive) {
        out<<",\"strategy_order\":[";
        for(std::size_t i=0;i<run.strategy_order.size();++i) {
            if(i) out<<',';
            const auto n=run.strategy_order[i];
            out<<json_string(n==0 ? "power-affine" : n==1 ? "polynomial" : n==2 ? "rational-linear" : n==3 ? "rational-quadratic" : "expression");
        }
        out<<"],\"ordering_reason\":"<<json_string(run.ordering_reason)<<",\"ordering_operations\":"<<run.ordering_operations;
    } else out<<",\"strategy_order\":[\"power-affine\",\"polynomial\",\"rational-linear\",\"rational-quadratic\",\"expression\"]";
    if(learned) {
        out<<",\"experience_digest\":"<<json_string(run.experience_digest)<<",\"experience_audit_operations\":"<<run.experience_audit_operations
           <<",\"experience_records_used\":"<<run.experience_records_used<<",\"experience_votes\":[";
        for(std::size_t i=0;i<run.experience_votes.size();++i) {if(i) out<<',';out<<run.experience_votes[i];}
        out<<']';
    }
    out<<",\"hypotheses\":[";
    for(std::size_t i=0;i<run.hypotheses.size();++i) {
        if(i) out<<',';
        const auto& h=run.hypotheses[i];
        out<<"{\"id\":"<<h.id<<",\"kind\":\"rational-normal-form\",\"status\":"<<json_string(h.status)<<",\"normal_form\":";
        if(!h.normal_form) out<<"null,\"formula\":null";
        else {
            const auto& f=*h.normal_form;
            out<<"{\"numerator\":";coefficients(f.numerator);
            out<<",\"denominator\":";coefficients(f.denominator);
            out<<",\"excluded_polynomial\":";coefficients(f.excluded_polynomial);
            out<<",\"empty_domain\":"<<(f.empty_domain ? "true" : "false")<<'}';
            out<<",\"formula\":"<<json_string(f.empty_domain ? "undefined" : "("+polynomial_formula(f.numerator)+")/("+polynomial_formula(f.denominator)+"), "+polynomial_formula(f.excluded_polynomial)+" != 0");
        }
        out<<",\"origins\":[";
        for(std::size_t j=0;j<h.origins.size();++j) {
            if(j) out<<',';
            const auto& o=h.origins[j];
            out<<"{\"strategy\":"<<json_string(o.strategy)<<",\"id\":"<<o.id<<",\"status\":"<<json_string(o.status)<<'}';
        }
        out<<"]}";
    }
    out<<"],\"strategy_runs\":[";
    for(std::size_t i=0;i<run.strategy_runs.size();++i) {if(i) out<<',';out<<render_exact_research(run.strategy_runs[i]);}
    out<<"],\"trace\":[";
    for(std::size_t i=0;i<run.trace.size();++i) {if(i) out<<',';out<<json_string(run.trace[i]);}
    out<<"]}";return out.str();
}
}
