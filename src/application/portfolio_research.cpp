#include "bruce/application/portfolio_research.hpp"
#include "bruce/application/portfolio_normalization.hpp"
#include "bruce/application/research.hpp"
#include "bruce/math/polynomial.hpp"
#include "bruce/application/portfolio_order.hpp"
#include "bruce/application/experience_routing.hpp"
#include <algorithm>
#include <map>
#include <sstream>

namespace bruce {
ExactResearchRun run_portfolio_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& experience_checkpoint) {
    const bool learned=task.planner=="experience-order";
    const bool adaptive=learned || task.planner=="adaptive-order";
    const auto experience=learned ? read_experience(task.experience_snapshot,experience_checkpoint) : ExperienceStore{};
    const bool five=task.strategy=="portfolio-expression";
    const bool domain_cached=task.domain_cache=="equal-64";
    std::optional<EqualDomainCache> union_cache;
    if(domain_cached) union_cache.emplace(NumberDomain(task.domain,task.max_bits));
    const bool witnessed=task.expression_equivalence=="rational-q-witness";
    const bool modular=witnessed || task.expression_equivalence=="rational-q-modular";
    const bool rational_q=five && (modular || task.expression_equivalence=="rational-q");
    const bool quadratic=five || task.strategy=="portfolio-quadratic";
    const bool extended=quadratic || task.strategy=="portfolio-rational";
    ExactResearchRun run; run.task=task;
    if(five) run.experience_votes.resize(5);
    else if(quadratic) run.experience_votes.resize(4);
    if(learned) {run.experience_digest=experience.digest; run.experience_audit_operations=experience.audit_operations;}
    run.trace={five ? (learned ? "START experience-order-v3: replayed routing evidence, five families retained" : adaptive ? "START sample-shape-v3: training-prefix scheduling, five families retained" : "START five-family-v1: shared budgets, exact normal forms and retained exclusions") : quadratic ? (learned ? "START experience-order-v2: replayed routing evidence, four families retained" : adaptive ? "START sample-shape-v2: training-prefix scheduling, four families retained" : "START four-family-v1: shared budgets, explicit denominator conditions") : learned ? "START experience-order-v1: replayed routing evidence, all families retained" : adaptive ? "START sample-shape-v1: training-prefix scheduling, all families retained" : extended ? "START power-polynomial-rational-v1: shared budgets, domain-preserving union" : "START power-then-polynomial-v1: shared budgets, union of declared families"};
    if(five && task.expression_planner=="indexed-grammar")
        run.trace.front()=learned ? "START experience-order-v4: indexed expressions, five families retained" : adaptive ? "START sample-shape-v4: indexed expressions, five families retained" : "START five-family-indexed-v1: shared budgets, indexed expressions";
    if(five && task.expression_operators!="+-*/")
        run.trace.front()=std::string(learned ? "START experience-order-v5" : adaptive ? "START sample-shape-v5" : "START five-family-selected-v1")+": expression operators="+task.expression_operators+", other families unchanged";
    if(five && task.expression_operators.find('^')!=std::string::npos)
        run.trace.front()=std::string(learned ? "START experience-order-v6" : adaptive ? "START sample-shape-v6" : "START five-family-power-v1")+": expression operators="+task.expression_operators+", literal-2-to-8-v1, other families unchanged";
    if(five && task.expression_planner=="cached-grammar")
        run.trace.front()=std::string(learned ? "START experience-order-v7" : adaptive ? "START sample-shape-v7" : "START five-family-cached-v1")+": direct-map-64-size3-v1, expression operators="+task.expression_operators+", five families retained";
    if(rational_q) run.trace.front()=std::string(learned ? "START experience-order-v8" : adaptive ? "START sample-shape-v8" : "START five-family-q-v1")+": rational-q-domain-v1, deferred fixed-family union";
    if(modular) run.trace.front()=std::string(learned ? "START experience-order-v9" : adaptive ? "START sample-shape-v9" : "START five-family-q-modular-v1")+": rational-q-modular-domain-v1, deferred fixed-family union";
    if(domain_cached) run.trace.front()=std::string(learned ? "START experience-order-v10" : adaptive ? "START sample-shape-v10" : "START five-family-q-cache-v1")+": equal-64-key4096-v1, expression-and-union-separate-v1";
    if(witnessed) run.trace.front()=std::string(learned ? "START experience-order-v11" : adaptive ? "START sample-shape-v11" : "START five-family-q-witness-v1")+": rational-q-witness-domain-v1, deferred fixed-family union";
    run.trace_bytes=run.trace.front().size();
    std::uint64_t child_events=0,child_bytes=0,stored_results=0;
    std::map<std::string,std::size_t> seen;
    bool incomplete=false,conflict=false;
    std::vector<unsigned> order=five ? std::vector<unsigned>{0,1,2,3,4} : quadratic ? std::vector<unsigned>{0,1,2,3} : extended ? std::vector<unsigned>{0,1,2} : std::vector<unsigned>{0,1};
    const std::uint64_t wrapper_bytes=witnessed ? (adaptive ? 3328 : 3072) : rational_q ? (adaptive ? 2048 : 1792) : five ? (adaptive ? 1280 : 1024) : adaptive ? 512 : 256;
    if(adaptive) {
        struct Stop {};
        auto stop=[&](std::string reason) { incomplete=true; run.stop_reason=std::move(reason); throw Stop{}; };
        bool reserved=false;
        try {
            if((task.cancel_after_checkpoints && *task.cancel_after_checkpoints==0) ||
               (task.cancel_after_candidates && *task.cancel_after_candidates==0) || (cancelled && cancelled(0))) {
                run.task.cancel_after_checkpoints=0; stop("Cancelled");
            }
            ++run.checkpoints;
            if(run.trace.size()+2>task.budget.trace_events) stop("TraceBudgetExceeded");
            if(run.trace_bytes+256>task.max_trace_bytes) stop("TraceByteBudgetExceeded");
            reserved=true;
            auto spend=[&](std::uint64_t cost) {
                if(task.budget.operations-run.operations<cost) stop("OperationBudgetExceeded");
                run.operations+=cost;
            };
            auto plan=plan_portfolio_order(task,spend);
            if(learned) rank_experience(experience,task,plan,run.experience_votes,run.experience_records_used,spend);
            order.assign(plan.strategies.begin(),plan.strategies.end());
            run.strategy_order=order; run.ordering_reason=plan.reason; ++run.plans_completed;
            std::string event="ORDER "+plan.reason+" indices=";
            for(std::size_t i=0;i<order.size();++i) {if(i) event+=','; event+=std::to_string(order[i]);}
            run.trace.push_back(std::move(event));
        } catch(const NumericLimit&) {
            incomplete=true; run.stop_reason="IntegerBitLimit";
            run.trace.push_back("ORDER stop=IntegerBitLimit");
        } catch(const Stop&) { if(reserved) run.trace.push_back("ORDER stop="+run.stop_reason); }
        run.ordering_operations=run.operations; run.planning_operations=run.operations;
        if(run.trace.size()>1) run.trace_bytes+=run.trace.back().size();
    }
    for(const unsigned index:order) {
        if(adaptive && run.strategy_order.empty()) break;
        const auto pending_union_events=rational_q ? run.strategy_runs.size() : 0;
        auto exhausted=[&](bool condition,std::string reason) {
            if(condition) { incomplete=true; run.stop_reason=std::move(reason); }
            return condition;
        };
        if(exhausted(run.operations>=task.budget.operations,"OperationBudgetExceeded") ||
           exhausted(run.candidates>=task.budget.candidates,"CandidateBudgetExceeded") ||
           exhausted(stored_results>=task.budget.results,"ResultBudgetExceeded") ||
           exhausted(child_events+run.trace.size()+pending_union_events+(five ? 4 : 3)>task.budget.trace_events,"TraceBudgetExceeded") ||
           exhausted(child_bytes+wrapper_bytes+256>task.max_trace_bytes,"TraceByteBudgetExceeded")) break;
        auto child=task;
        child.experience_snapshot.clear();
        child.strategy=index==0 ? "power-affine" : index==1 ? "polynomial" : index==2 ? "rational-linear" : index==3 ? "rational-quadratic" : "expression";
        child.domain_cache=index==4 ? task.domain_cache : "none";
        child.expression_equivalence=index==4 ? task.expression_equivalence : "syntax";
        child.expression_operators=index==4 ? task.expression_operators : "+-*/";
        child.planner=index==0 ? "adaptive" : index==1 ? "interpolate" : index==4 ? task.expression_planner : "solve";
        child.expression_planner="grammar";
        if(index!=0) child.max_power=1;
        child.cancel_after_candidates.reset(); child.cancel_after_checkpoints.reset();
        child.budget.operations-=run.operations;
        child.budget.candidates-=run.candidates;
        child.budget.results-=stored_results;
        if(witnessed && index<4) child.budget.results=std::min<std::uint64_t>(child.budget.results,128);
        child.budget.trace_events-=child_events+run.trace.size()+pending_union_events+(five ? 2 : 1);
        child.max_trace_bytes-=child_bytes+wrapper_bytes;
        const auto base=run.candidates;
        auto result=exact_research(std::move(child),[&](std::uint64_t local_count) {
            if((task.cancel_after_checkpoints && run.checkpoints>=*task.cancel_after_checkpoints) ||
               (task.cancel_after_candidates && base+local_count>=*task.cancel_after_candidates) ||
               (cancelled && cancelled(base+local_count))) {
                run.task.cancel_after_checkpoints=run.checkpoints; return true;
            }
            ++run.checkpoints; return false;
        });
        run.domain_cache_stats.hits+=result.domain_cache_stats.hits;
        run.domain_cache_stats.misses+=result.domain_cache_stats.misses;
        run.domain_cache_stats.bypasses+=result.domain_cache_stats.bypasses;
        run.domain_cache_stats.inserts+=result.domain_cache_stats.inserts;
        run.domain_cache_stats.evictions+=result.domain_cache_stats.evictions;
        run.domain_cache_stats.entries+=result.domain_cache_stats.entries;
        run.domain_cache_stats.key_bytes+=result.domain_cache_stats.key_bytes;
        run.domain_cache_stats.operations+=result.domain_cache_stats.operations;
        run.operations+=result.operations; run.candidates+=result.candidates;
        run.planning_operations+=result.planning_operations; run.plans_completed+=result.plans_completed;
        child_events+=result.trace.size(); child_bytes+=result.trace_bytes;
        stored_results+=index==4 ? result.expression_sources : result.hypotheses.size();
        bool union_completed=true;
        if(five) {
            run.expression_sources=stored_results;
            run.normalization_operations+=result.normalization_operations;
            if(rational_q) {
                run.domain_comparison_operations+=result.domain_comparison_operations;
                run.domain_comparisons+=result.domain_comparisons;
                run.domain_unknown+=result.domain_unknown;
                run.domain_witness_candidates+=result.domain_witness_candidates;
                run.domain_bounded_witnesses+=result.domain_bounded_witnesses;
                run.domain_different+=result.domain_different;
                run.domain_modular_certificates+=result.domain_modular_certificates;
            }
            if(!rational_q) {
            union_completed=merge_normalized_portfolio(run,result,index,seen,cancelled);
            const auto event="UNION "+result.task.strategy+" groups="+std::to_string(run.hypotheses.size())+
                " operations="+std::to_string(run.union_operations)+(union_completed ? "" : " stop="+run.stop_reason);
            run.trace.push_back(event);run.trace_bytes+=event.size();
            if(!union_completed) incomplete=true;
            }
        }
        if(!five) for(const auto& source:result.hypotheses) {
            auto coefficients=source.coefficients;
            if(index==0) {
                coefficients.assign(source.power+1,0);
                coefficients[0]=source.b; coefficients[source.power]=source.a;
            }
            while(index<2 && coefficients.size()>1 && coefficients.back()==0) coefficients.pop_back();
            std::string key=extended ? (index==3 ? "quadratic-fraction;" : index==2 ? "fraction;" : "polynomial;") : "";
            for(const auto& c:coefficients) key+=exact_text(c)+';';
            if(index>=2) key+="denominator;"+exact_text(source.denominator[0]);
            if(index==3) key+=';'+exact_text(source.denominator[1]);
            const HypothesisOrigin origin{result.task.strategy,source.id,source.status};
            const auto found=seen.find(key);
            if(found==seen.end()) {
                auto merged=source;
                merged.id=source.id*(quadratic ? 4 : extended ? 3 : 2)+index;
                // Version-specific splits keep the merged IDs and Cantor coordinates within uint64.
                const std::uint64_t split=quadratic ? (1ULL<<31) : extended ? (1ULL<<25) : 65536;
                merged.a_index=merged.id/split; merged.b_index=merged.id%split;
                merged.coefficients=std::move(coefficients); merged.origins={origin};
                seen.emplace(std::move(key),run.hypotheses.size());
                run.hypotheses.push_back(std::move(merged));
            } else {
                auto& merged=run.hypotheses[found->second];
                merged.origins.push_back(origin);
                // Keep the existing power-first identity even when polynomial execution came first.
                if(adaptive && index==0) {
                    merged.id=source.id*(quadratic ? 4 : 3);
                    const auto split=quadratic ? (1ULL<<31) : (1ULL<<25);
                    merged.a_index=merged.id/split; merged.b_index=merged.id%split;
                }
                if(merged.status!=source.status) {
                    if(merged.status!="INCOMPLETE" && source.status!="INCOMPLETE") conflict=true;
                    merged.status="INCOMPLETE";
                }
            }
        }
        const bool was_cancelled=result.stop_reason=="Cancelled";
        if(union_completed && result.status=="INCOMPLETE") { incomplete=true; run.stop_reason=was_cancelled ? "Cancelled" : "ChildIncomplete"; }
        run.strategy_runs.push_back(std::move(result));
        if(was_cancelled || !union_completed) break;
    }
    // Q comparisons may remain undecided. A fixed union order makes the
    // representative decisions independent of the order of completed searches.
    if(rational_q) for(unsigned index=0;index<5;++index) {
        const std::string family=index==0 ? "power-affine" : index==1 ? "polynomial" : index==2 ? "rational-linear" : index==3 ? "rational-quadratic" : "expression";
        const auto found=std::find_if(run.strategy_runs.begin(),run.strategy_runs.end(),[&](const auto& r){return r.task.strategy==family;});
        if(found==run.strategy_runs.end()) continue;
        const auto comparisons=run.domain_comparisons,unknown=run.domain_unknown;
        const bool complete=merge_normalized_portfolio(run,*found,index,seen,cancelled,union_cache ? &*union_cache : nullptr);
        const auto event="UNION "+family+" groups="+std::to_string(run.hypotheses.size())+
            " operations="+std::to_string(run.union_operations)+" domain_checks="+std::to_string(run.domain_comparisons-comparisons)+
            " domain_unknown="+std::to_string(run.domain_unknown-unknown)+(witnessed ? run.union_witness_summary : "")+(complete ? "" : " stop="+run.stop_reason);
        run.trace.push_back(event);run.trace_bytes+=event.size();
        if(!complete) {incomplete=true;break;}
    }
    if(adaptive) {
        std::sort(run.hypotheses.begin(),run.hypotheses.end(),[five](const auto& a,const auto& b) {
            if(five && a.id%5!=b.id%5) return a.id%5<b.id%5;
            return a.id<b.id;
        });
        for(auto& h:run.hypotheses) std::stable_sort(h.origins.begin(),h.origins.end(),[five](const auto& a,const auto& b) {
            if(five) {
                auto rank=[](const std::string& s){return s=="power-affine" ? 0 : s=="polynomial" ? 1 : s=="rational-linear" ? 2 : s=="rational-quadratic" ? 3 : 4;};
                if(rank(a.strategy)!=rank(b.strategy)) return rank(a.strategy)<rank(b.strategy);
                return a.id<b.id;
            }
            return a.strategy=="power-affine" && b.strategy!="power-affine";
        });
    }
    if(conflict) { incomplete=true; run.stop_reason="StrategyResultConflict"; }
    std::size_t survivors=0;
    for(const auto& h:run.hypotheses) if(h.status!="REFUTED" && h.status!="INCOMPLETE") ++survivors;
    run.status=incomplete ? "INCOMPLETE" : survivors>1 ? "AMBIGUOUS" : survivors==1 ?
        (task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES") : run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    run.trace.push_back("END "+run.status+" "+run.stop_reason);
    run.trace_bytes+=child_bytes+run.trace.back().size();
    if(union_cache) {
        run.union_domain_cache_stats=union_cache->stats();
        run.domain_cache_stats.hits+=run.union_domain_cache_stats.hits;
        run.domain_cache_stats.misses+=run.union_domain_cache_stats.misses;
        run.domain_cache_stats.bypasses+=run.union_domain_cache_stats.bypasses;
        run.domain_cache_stats.inserts+=run.union_domain_cache_stats.inserts;
        run.domain_cache_stats.evictions+=run.union_domain_cache_stats.evictions;
        run.domain_cache_stats.entries+=run.union_domain_cache_stats.entries;
        run.domain_cache_stats.key_bytes+=run.union_domain_cache_stats.key_bytes;
        run.domain_cache_stats.operations+=run.union_domain_cache_stats.operations;
    }
    return run;
}

std::string render_portfolio_research(const ExactResearchRun& run) {
    if(run.task.strategy=="portfolio-expression") return render_normalized_portfolio(run);
    const bool learned=run.task.planner=="experience-order";
    const bool adaptive=learned || run.task.planner=="adaptive-order";
    const bool quadratic=run.task.strategy=="portfolio-quadratic";
    const bool extended=quadratic || run.task.strategy=="portfolio-rational";
    std::uint64_t events=run.trace.size(),stored=0;
    for(const auto& child:run.strategy_runs) { events+=child.trace.size(); stored+=child.hypotheses.size(); }
    std::ostringstream out;
    if(quadratic) {
        out << "{\"engine\":" << json_string(learned ? "bruce-portfolio-v7" : adaptive ? "bruce-portfolio-v6" : "bruce-portfolio-v5")
            << ",\"rule_version\":\"domain-union-v2\",\"policy_version\":\"shared-budget-v1\",\"strategy\":\"portfolio-quadratic\",\"planner\":"
            << json_string(learned ? "experience-order-v2" : adaptive ? "sample-shape-v2" : "four-family-v1");
    } else out << (learned ? "{\"engine\":\"bruce-portfolio-v4\",\"rule_version\":\"domain-union-v1\",\"policy_version\":\"shared-budget-v1\",\"strategy\":\"portfolio-rational\",\"planner\":\"experience-order-v1\"" : adaptive ? "{\"engine\":\"bruce-portfolio-v3\",\"rule_version\":\"domain-union-v1\",\"policy_version\":\"shared-budget-v1\",\"strategy\":\"portfolio-rational\",\"planner\":\"sample-shape-v1\"" : extended ? "{\"engine\":\"bruce-portfolio-v2\",\"rule_version\":\"domain-union-v1\",\"policy_version\":\"shared-budget-v1\",\"strategy\":\"portfolio-rational\",\"planner\":\"power-polynomial-rational-v1\"" : "{\"engine\":\"bruce-portfolio-v1\",\"rule_version\":\"portfolio-union-v1\",\"policy_version\":\"shared-budget-v1\",\"strategy\":\"portfolio\",\"planner\":\"power-then-polynomial-v1\"")
    ;
    out << ",\"domain\":" << json_string(run.task.domain) << ",\"max_power\":" << run.task.max_power << ",\"max_degree\":" << run.task.max_degree
        << ",\"max_bits\":" << run.task.max_bits << ",\"max_denominator\":" << run.task.max_denominator
        << ",\"minimum\":" << json_string(run.task.minimum) << ",\"maximum\":" << json_string(run.task.maximum)
        << ",\"status\":" << json_string(run.status) << ",\"stop_reason\":" << json_string(run.stop_reason)
        << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"claim_scope\":" << json_string(quadratic ? "Union of four bounded families, preserving explicit denominator conditions; supplied-sample checks are no general proof" : extended ? "Union of bounded power, polynomial and rational-linear families, preserving excluded points; supplied-sample checks are no general proof" : "Union of bounded power and polynomial families on supplied samples; repeated inputs do not create independent evidence")
        << ",\"explanation\":" << json_string(run.status=="INCOMPLETE" ? "Mindestens eine Familie bleibt unvollstaendig; Teilbefunde sind keine abschliessende Gesamtauswahl." :
            run.status=="VERIFIED_ON_SAMPLES" ? "Eine kanonische Formel besteht die angegebenen Pruefpaare. Mehrere Strategien sind keine zusaetzlichen unabhaengigen Datenbelege." :
            run.status=="AMBIGUOUS" ? "Mehrere verschiedene Formeln bleiben gleichrangig." :
            run.status=="CANDIDATE" ? "Eine kanonische Formel passt zu den Lernpaaren; Hold-out fehlt." :
            run.status=="REFUTED" ? "Alle Lernkandidaten scheitern an Hold-out." : "Kein Kandidat in der vollstaendig untersuchten Familienvereinigung passt.")
        << ",\"operations_used\":" << run.operations << ",\"planning_operations\":" << run.planning_operations
        << ",\"candidates_executed\":" << run.candidates << ",\"stored_source_hypotheses\":" << stored
        << ",\"plans_completed\":" << run.plans_completed << ",\"checkpoints\":" << run.checkpoints
        << ",\"trace_bytes\":" << run.trace_bytes << ",\"trace_events_used\":" << events;
    if(adaptive) {
        out << ",\"ordering_reason\":" << json_string(run.ordering_reason) << ",\"ordering_operations\":" << run.ordering_operations
            << ",\"strategy_order\":[";
        for(std::size_t i=0;i<run.strategy_order.size();++i) {
            if(i) out<<',';
            const auto index=run.strategy_order[i];
            out<<json_string(index==0 ? "power-affine" : index==1 ? "polynomial" : index==2 ? "rational-linear" : "rational-quadratic");
        }
        out << ']';
    }
    if(learned) { out << ",\"experience_digest\":" << json_string(run.experience_digest)
        << ",\"experience_audit_operations\":" << run.experience_audit_operations
        << ",\"experience_records_used\":" << run.experience_records_used
        << ",\"experience_votes\":[";
        for(std::size_t i=0;i<run.experience_votes.size();++i) {if(i) out<<','; out<<run.experience_votes[i];}
        out<<']';
    }
    out << ",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out<<',';
        first=false;
        out << "{\"id\":" << h.id;
        if(extended) out << ",\"kind\":" << json_string(h.denominator.empty() ? "polynomial" : h.denominator.size()==3 ? "rational-quadratic" : "rational-linear");
        if(h.denominator.size()==3) {
            const auto a=exact_text(h.a),b=exact_text(h.b),c=exact_text(h.denominator[1]),d=exact_text(h.denominator[0]);
            const auto denominator="x^2+("+c+")*x+("+d+")";
            out << ",\"a\":" << json_string(a) << ",\"b\":" << json_string(b) << ",\"c\":" << json_string(c) << ",\"d\":" << json_string(d)
                << ",\"numerator\":[" << json_string(b) << ',' << json_string(a) << "],\"denominator\":[" << json_string(d) << ',' << json_string(c) << ",\"1\"]"
                << ",\"domain_condition\":" << json_string(denominator+" != 0")
                << ",\"formula\":" << json_string("("+a+"*x+("+b+"))/("+denominator+"), "+denominator+" != 0");
        } else if(!h.denominator.empty()) {
            const auto a=exact_text(h.a),b=exact_text(h.b),c=exact_text(h.denominator[0]);
            const ExactValue excluded=-h.denominator[0];
            out << ",\"a\":" << json_string(a) << ",\"b\":" << json_string(b) << ",\"c\":" << json_string(c)
                << ",\"numerator\":[" << json_string(b) << ',' << json_string(a) << "],\"denominator\":[" << json_string(c) << ",\"1\"]"
                << ",\"excluded_x\":" << json_string(exact_text(excluded))
                << ",\"formula\":" << json_string("("+a+"*x+("+b+"))/(x+("+c+")), x != "+exact_text(excluded));
        } else {
            out << ",\"degree\":" << h.coefficients.size()-1 << ",\"coefficients\":[";
            for(std::size_t i=0;i<h.coefficients.size();++i) { if(i) out<<','; out<<json_string(exact_text(h.coefficients[i])); }
            out << "],\"formula\":" << json_string(polynomial_formula(h.coefficients));
        }
        out << ",\"status\":" << json_string(h.status) << ",\"origins\":[";
        for(std::size_t i=0;i<h.origins.size();++i) {
            if(i) out<<',';
            const auto& origin=h.origins[i];
            out << "{\"strategy\":" << json_string(origin.strategy) << ",\"id\":" << origin.id << ",\"status\":" << json_string(origin.status) << '}';
        }
        out << "]}";
    }
    out << "],\"strategy_runs\":[";
    for(std::size_t i=0;i<run.strategy_runs.size();++i) { if(i) out<<','; out<<render_exact_research(run.strategy_runs[i]); }
    out << "],\"trace\":[";
    for(std::size_t i=0;i<run.trace.size();++i) { if(i) out<<','; out<<json_string(run.trace[i]); }
    out << "]}";
    return out.str();
}
}
