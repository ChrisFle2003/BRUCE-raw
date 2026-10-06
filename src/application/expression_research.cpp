#include "bruce/application/expression_research.hpp"
#include "bruce/application/research.hpp"
#include <sstream>
#include <map>

namespace bruce {
ExactResearchRun run_expression_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled) {
    const bool cached=task.planner=="cached-grammar";
    const bool indexed=cached || task.planner=="indexed-grammar";
    const bool witnessed=task.expression_equivalence=="rational-q-witness";
    const bool modular=witnessed || task.expression_equivalence=="rational-q-modular";
    const bool rational_q=modular || task.expression_equivalence=="rational-q";
    const bool domain_cached=task.domain_cache=="equal-64";
    const bool grouped=rational_q || task.expression_equivalence=="rational";
    const NumberDomain domain(task.domain,task.max_bits);
    EqualDomainCache domain_cache(domain);
    std::map<std::string,std::size_t> groups;
    ExactResearchRun run; run.task=task;
    run.trace={indexed ? "START block-index-v1: indexed grammar blocks, unchanged expression identities" : grouped ? "START rational-domain-v1: exact fraction and retained exclusion polynomial" : "START binary-arithmetic-v1: bounded canonical syntax, explicit division conditions"};
    if(task.expression_operators!="+-*/") run.trace.front()="START selected-binary-v1: operators="+task.expression_operators+", local grammar identities";
    if(task.expression_operators.find('^')!=std::string::npos) run.trace.front()="START bounded-power-v1: operators="+task.expression_operators+", literal exponents 2..8 from declared constants";
    if(cached) run.trace.front()="START subtree-cache-v1: direct-map-64-size3-v1, operators="+task.expression_operators+", unchanged grammar identities";
    if(rational_q) run.trace.front()="START rational-q-domain-v1: representative-first, gcd-quadratic Q domains, operators="+task.expression_operators;
    if(modular) run.trace.front()="START rational-q-modular-domain-v1: representative-first, gcd-modular Q domains, operators="+task.expression_operators;
    if(witnessed) run.trace.front()="START rational-q-witness-domain-v1: representative-first, gcd-modular-witness16 Q domains, operators="+task.expression_operators;
    if(domain_cached) run.trace.front()+=" domain_cache=equal-64-key4096-v1";
    run.trace_bytes=run.trace.front().size();
    struct Stop {};
    auto stop=[&](std::string reason) {run.stop_reason=std::move(reason); throw Stop{};};
    auto spend=[&](std::uint64_t n) {
        if(task.budget.operations-run.operations<n) stop("OperationBudgetExceeded");
        run.operations+=n;
    };
    auto checkpoint=[&]() {
        if((task.cancel_after_checkpoints && run.checkpoints>=*task.cancel_after_checkpoints) ||
           (task.cancel_after_candidates && run.candidates>=*task.cancel_after_candidates) ||
           (cancelled && cancelled(run.candidates))) {
            run.task.cancel_after_checkpoints=run.checkpoints; stop("Cancelled");
        }
        ++run.checkpoints;
    };
    auto reserve=[&]() {
        if(run.trace.size()+2>task.budget.trace_events) stop("TraceBudgetExceeded");
        if(run.trace_bytes+512>task.max_trace_bytes) stop("TraceByteBudgetExceeded");
    };
    auto record=[&](std::string s) {run.trace_bytes+=s.size();run.trace.push_back(std::move(s));};
    std::optional<ExpressionGrammar> grammar;
    try {
        checkpoint(); reserve();
        auto before=run.operations;
        try {
            std::vector<ExactValue> constants;
            const auto low=domain.parse_integer(task.minimum),high=domain.parse_integer(task.maximum);
            for(mpz_class n=low;n<=high;++n) for(unsigned d=1;d<=task.max_denominator;++d) {
                spend(1); if(mpz_gcd_ui(nullptr,n.get_mpz_t(),d)!=1) continue;
                ExactValue value(n,mpz_class(d)); value.canonicalize(); constants.push_back(domain.check(value));
            }
            grammar.emplace(std::move(constants),task.max_nodes,spend,indexed,task.expression_operators,cached);
        } catch(const NumericLimit&) {run.planning_operations+=run.operations-before;record("PLAN stop=IntegerBitLimit");stop("IntegerBitLimit");}
          catch(const Stop&) {run.planning_operations+=run.operations-before;record("PLAN stop="+run.stop_reason);throw;}
        run.planning_operations+=run.operations-before; ++run.plans_completed;
        run.raw_combinations=grammar->size();run.index_entries=grammar->index_entries();
        record("PLAN nodes<="+std::to_string(task.max_nodes)+" expressions="+std::to_string(grammar->size()));
        for(std::uint64_t id=0;id<grammar->size();++id) {
            checkpoint();
            if(run.candidates>=task.budget.candidates) stop("CandidateBudgetExceeded");
            reserve(); ++run.candidates;
            std::string event="EXPRESSION id="+std::to_string(id);
            try {
                before=run.operations;
                Expression tree;
                try {tree=grammar->at(id,spend);}
                catch(const Stop&) {run.planning_operations+=run.operations-before;throw;}
                run.planning_operations+=run.operations-before;
                auto inspect=[&](const std::vector<ExactSample>& samples,std::string_view phase) {
                    for(std::size_t i=0;i<samples.size();++i) {
                        try {
                            const auto value=evaluate_expression(domain,tree,samples[i].x,spend);
                            spend(1);
                            if(value!=samples[i].y) {event+=" "+std::string(phase)+"["+std::to_string(i)+"] contradiction";return false;}
                            const auto counter=counter_evaluate_expression(domain,tree,samples[i].x,spend);
                            spend(1); if(counter!=value) throw Error("ExpressionCountercheckMismatch");
                        } catch(const ExpressionPole&) {event+=" "+std::string(phase)+"["+std::to_string(i)+"] pole";return false;}
                    }
                    return true;
                };
                if(inspect(task.training,"training")) {
                    if((grouped ? run.expression_sources : run.hypotheses.size())>=task.budget.results) stop("ResultBudgetExceeded");
                    if(grouped) ++run.expression_sources;
                    ExactHypothesis h{id,id/(1ULL<<25),id%(1ULL<<25),0,0,"CANDIDATE"}; h.expression=tree;
                    auto retain_incomplete=[&]() {
                        h.status="INCOMPLETE";
                        if(grouped) h.expression_origins.push_back({id,h.status,tree});
                        run.hypotheses.push_back(std::move(h));
                    };
                    if(!task.holdout.empty()) {
                        try {h.status=inspect(task.holdout,"holdout") ? "VERIFIED_ON_SAMPLES" : "REFUTED";}
                        catch(const Stop&) {retain_incomplete();throw;}
                        catch(const NumericLimit&) {retain_incomplete();throw;}
                    }
                    event+=" "+h.status;
                    if(grouped) {
                        const auto normal_before=run.operations;
                        try {
                            h.normal_form=normalize_expression(domain,tree,spend);
                            // Check the symbolic result against the original tree on all supplied inputs,
                            // including holes and refuted hold-out points; this is an additional consistency check.
                            for(const auto* samples:{&task.training,&task.holdout}) for(const auto& sample:*samples) {
                                std::optional<ExactValue> original,normalized;
                                try {original=evaluate_expression(domain,tree,sample.x,spend);} catch(const ExpressionPole&) {}
                                try {normalized=evaluate_normal_form(domain,*h.normal_form,sample.x,spend);} catch(const ExpressionPole&) {}
                                spend(1);if(original!=normalized) throw Error("ExpressionNormalizationMismatch");
                            }
                        } catch(const Stop&) {run.normalization_operations+=run.operations-normal_before;retain_incomplete();throw;}
                          catch(const NumericLimit&) {run.normalization_operations+=run.operations-normal_before;retain_incomplete();throw;}
                          catch(const ExpressionNormalizationLimit&) {
                              run.normalization_operations+=run.operations-normal_before;retain_incomplete();stop("ExpressionNormalizationDegreeLimit");
                          }
                        run.normalization_operations+=run.operations-normal_before;
                        std::optional<std::size_t> target_index;
                        if(rational_q) {
                            const auto domain_before=run.operations;
                            const auto checks_before=run.domain_comparisons,unknown_before=run.domain_unknown;
                            auto finish_domain=[&]() {
                                run.domain_comparison_operations+=run.operations-domain_before;
                                event+=" domain_checks="+std::to_string(run.domain_comparisons-checks_before)+
                                    " domain_unknown="+std::to_string(run.domain_unknown-unknown_before);
                            };
                            bool witness_recorded=false;
                            try {
                                auto equal=[&](const auto& a,const auto& b) {
                                    spend(1);if(a.size()!=b.size()) return false;
                                    for(std::size_t i=0;i<a.size();++i) {spend(1);if(a[i]!=b[i]) return false;}
                                    return true;
                                };
                                for(std::size_t i=0;i<run.hypotheses.size();++i) {
                                    const auto& other=*run.hypotheses[i].normal_form;
                                    spend(1);
                                    if(h.normal_form->empty_domain!=other.empty_domain) continue;
                                    if(!h.normal_form->empty_domain &&
                                       (!equal(h.normal_form->numerator,other.numerator) ||
                                        !equal(h.normal_form->denominator,other.denominator))) continue;
                                    ++run.domain_comparisons;
                                    const auto comparison=domain_cached ? domain_cache.compare(h.normal_form->excluded_polynomial,other.excluded_polynomial,spend) :
                                        compare_rational_domains(domain,h.normal_form->excluded_polynomial,other.excluded_polynomial,spend,modular,witnessed);
                                    run.domain_modular_certificates+=(comparison.left_modular_prime!=0)+(comparison.right_modular_prime!=0);
                                    if(witnessed) {
                                        run.domain_witness_candidates+=comparison.left_witness_candidates+comparison.right_witness_candidates;
                                        if(comparison.relation=="DIFFERENT") ++run.domain_different;
                                        // Only witnesses actually obtained by the bounded probe are
                                        // recorded here; analytic quadratic witnesses may be much larger.
                                        const bool bounded=comparison.witness &&
                                            (comparison.excluded_side=="left" ? comparison.left_witness_candidates>0 : comparison.right_witness_candidates>0);
                                        if(bounded) {
                                            ++run.domain_bounded_witnesses;
                                            if(!witness_recorded) {
                                                event+=" domain_witness="+exact_text(*comparison.witness)+
                                                    " witness_against="+std::to_string(run.hypotheses[i].id)+
                                                    " excluded_side="+comparison.excluded_side;
                                                witness_recorded=true;
                                            }
                                        }
                                    }
                                    if(comparison.relation=="UNKNOWN") ++run.domain_unknown;
                                    if(comparison.relation=="EQUAL") {target_index=i;break;}
                                }
                            } catch(const Stop&) {finish_domain();retain_incomplete();throw;}
                              catch(const NumericLimit&) {finish_domain();retain_incomplete();throw;}
                              catch(const ExpressionNormalizationLimit&) {
                                  finish_domain();retain_incomplete();stop("ExpressionNormalizationDegreeLimit");
                              }
                            finish_domain();
                        } else {
                            const auto key=normal_form_key(*h.normal_form);
                            const auto found=groups.find(key);
                            if(found!=groups.end()) target_index=found->second;
                            else groups.emplace(key,run.hypotheses.size());
                        }
                        h.expression_origins.push_back({id,h.status,tree});
                        if(!target_index) run.hypotheses.push_back(std::move(h));
                        else {
                            auto& target=run.hypotheses[*target_index];
                            target.expression_origins.push_back(std::move(h.expression_origins.front()));
                            if(target.status!=h.status) {target.status="INCOMPLETE";stop("ExpressionEquivalenceConflict");}
                            event+=" merge_into="+std::to_string(target.id);
                        }
                    } else run.hypotheses.push_back(std::move(h));
                }
            } catch(const Stop&) {record(event+" stop="+run.stop_reason);throw;}
              catch(const NumericLimit&) {record(event+" stop=IntegerBitLimit");stop("IntegerBitLimit");}
            record(event);
        }
    } catch(const Stop&) {}
    if(grammar) run.expression_cache=grammar->cache_stats();
    std::size_t survivors=0;
    for(const auto& h:run.hypotheses) if(h.status!="REFUTED" && h.status!="INCOMPLETE") ++survivors;
    run.status=run.stop_reason!="Completed" ? "INCOMPLETE" : survivors>1 ? "AMBIGUOUS" : survivors==1 ?
        (task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES") : run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    run.domain_cache_stats=domain_cache.stats();
    record("END "+run.status+" "+run.stop_reason); return run;
}
std::string render_expression_research(const ExactResearchRun& run) {
    const bool cached=run.task.planner=="cached-grammar";
    const bool indexed=cached || run.task.planner=="indexed-grammar";
    const bool witnessed=run.task.expression_equivalence=="rational-q-witness";
    const bool modular=witnessed || run.task.expression_equivalence=="rational-q-modular";
    const bool rational_q=modular || run.task.expression_equivalence=="rational-q";
    const bool domain_cached=run.task.domain_cache=="equal-64";
    const bool grouped=rational_q || run.task.expression_equivalence=="rational";
    std::ostringstream out;
    if(witnessed) out<<"{\"engine\":\"bruce-expression-v10\",\"rule_version\":\"q-domain-binary-v1\",\"policy_version\":\"defined-expression-samples-v10\",\"strategy\":\"expression\",\"planner\":"<<json_string(cached ? "subtree-cache-v1" : indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"cache_policy\":"<<json_string(cached ? "direct-map-64-size3-v1" : "none")<<",\"operators\":"<<json_string(run.task.expression_operators)
        <<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":\"rational-q-witness-domain-v1\",\"domain_comparison_policy\":\"rational-domain-gcd-modular-witness16-v1\"";
    else if(domain_cached) out<<"{\"engine\":\"bruce-expression-v9\",\"rule_version\":\"q-domain-binary-v1\",\"policy_version\":\"defined-expression-samples-v9\",\"strategy\":\"expression\",\"planner\":"<<json_string(cached ? "subtree-cache-v1" : indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"cache_policy\":"<<json_string(cached ? "direct-map-64-size3-v1" : "none")<<",\"operators\":"<<json_string(run.task.expression_operators)
        <<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":\"rational-q-modular-domain-v1\",\"domain_comparison_policy\":\"rational-domain-gcd-modular-v1\"";
    else if(modular) out<<"{\"engine\":\"bruce-expression-v8\",\"rule_version\":\"q-domain-binary-v1\",\"policy_version\":\"defined-expression-samples-v8\",\"strategy\":\"expression\",\"planner\":"<<json_string(cached ? "subtree-cache-v1" : indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"cache_policy\":"<<json_string(cached ? "direct-map-64-size3-v1" : "none")<<",\"operators\":"<<json_string(run.task.expression_operators)
        <<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":\"rational-q-modular-domain-v1\",\"domain_comparison_policy\":\"rational-domain-gcd-modular-v1\"";
    else if(rational_q) out<<"{\"engine\":\"bruce-expression-v7\",\"rule_version\":\"q-domain-binary-v1\",\"policy_version\":\"defined-expression-samples-v7\",\"strategy\":\"expression\",\"planner\":"<<json_string(cached ? "subtree-cache-v1" : indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"cache_policy\":"<<json_string(cached ? "direct-map-64-size3-v1" : "none")<<",\"operators\":"<<json_string(run.task.expression_operators)
        <<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":\"rational-q-domain-v1\",\"domain_comparison_policy\":\"rational-domain-gcd-quadratic-v1\"";
    else if(cached) out<<"{\"engine\":\"bruce-expression-v6\",\"rule_version\":\"cached-binary-v1\",\"policy_version\":\"defined-expression-samples-v6\",\"strategy\":\"expression\",\"planner\":\"subtree-cache-v1\",\"cache_policy\":\"direct-map-64-size3-v1\",\"operators\":"<<json_string(run.task.expression_operators)
        <<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":"<<json_string(grouped ? "rational-domain-v1" : "syntax-v1");
    else if(run.task.expression_operators.find('^')!=std::string::npos) out<<"{\"engine\":\"bruce-expression-v5\",\"rule_version\":\"bounded-power-v1\",\"policy_version\":\"defined-expression-samples-v5\",\"strategy\":\"expression\",\"planner\":"<<json_string(indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"operators\":"<<json_string(run.task.expression_operators)<<",\"power_exponent_policy\":\"literal-2-to-8-v1\",\"equivalence\":"<<json_string(grouped ? "rational-domain-v1" : "syntax-v1");
    else if(run.task.expression_operators!="+-*/") out<<"{\"engine\":\"bruce-expression-v4\",\"rule_version\":\"selected-binary-v1\",\"policy_version\":\"defined-expression-samples-v4\",\"strategy\":\"expression\",\"planner\":"<<json_string(indexed ? "block-index-v1" : "size-ranked-v1")
        <<",\"operators\":"<<json_string(run.task.expression_operators)<<",\"equivalence\":"<<json_string(grouped ? "rational-domain-v1" : "syntax-v1");
    else if(indexed) out<<"{\"engine\":\"bruce-expression-v3\",\"rule_version\":\"binary-arithmetic-v1\",\"policy_version\":\"defined-expression-samples-v3\",\"strategy\":\"expression\",\"planner\":\"block-index-v1\",\"equivalence\":"<<json_string(grouped ? "rational-domain-v1" : "syntax-v1");
    else if(grouped) out<<"{\"engine\":\"bruce-expression-v2\",\"rule_version\":\"binary-arithmetic-v1\",\"policy_version\":\"defined-expression-samples-v2\",\"strategy\":\"expression\",\"planner\":\"size-ranked-v1\",\"equivalence\":\"rational-domain-v1\"";
    else out<<"{\"engine\":\"bruce-expression-v1\",\"rule_version\":\"binary-arithmetic-v1\",\"policy_version\":\"defined-expression-samples-v1\",\"strategy\":\"expression\",\"planner\":\"size-ranked-v1\"";
    out <<",\"domain\":"<<json_string(run.task.domain)<<",\"max_nodes\":"<<run.task.max_nodes
       <<",\"minimum\":"<<json_string(run.task.minimum)<<",\"maximum\":"<<json_string(run.task.maximum)
       <<",\"max_denominator\":"<<run.task.max_denominator<<",\"max_bits\":"<<run.task.max_bits
       <<",\"status\":"<<json_string(run.status)<<",\"stop_reason\":"<<json_string(run.stop_reason)
       <<",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
       <<",\"claim_scope\":"<<json_string(rational_q ? "Bounded grammar; groups merge only after exact function and rational-domain equality; unresolved comparisons stay separate; sample checks are no general proof" : run.task.expression_operators.find('^')!=std::string::npos ? "Bounded grammar with selected operators; powers use declared literal integers 2..8; original domain exclusions retained; sample checks are no general proof" : run.task.expression_operators!="+-*/" ? "Only the declared operators and bounded constants/node counts were searched; IDs are local to this grammar; sample checks are no general proof" : grouped ? "Bounded grammar; exact reduced fractions with identical square-free exclusions may merge; fitted sample relation is no general proof" : "Bounded binary expression grammar over x and declared rational constants; explicit division conditions; sample checks are no general proof")
       <<",\"explanation\":"<<json_string(run.status=="INCOMPLETE" ? "Die Ausdruckssuche ist unvollstaendig; Treffer bleiben Teilbefunde." :
           run.status=="AMBIGUOUS" ? (rational_q ? "Mehrere Wertefunktionen oder rationale Definitionsbereiche bleiben; offene Bereichsvergleiche werden nicht zusammengefuehrt." : grouped ? "Mehrere Normalformen oder Ausschlussbedingungen bleiben. Unterschiedliche rationale Nullstellenmengen werden nicht allgemein auf Gleichheit geprueft." : "Mehrere Ausdrucksbaeume passen. Allgemeine algebraische Gleichheit wird nicht behauptet.") :
           run.status=="VERIFIED_ON_SAMPLES" ? (grouped ? "Eine Gruppe algebraisch gleicher Ausdruecke mit gleicher Ausschlussbedingung besteht die Samples. Herkunft bleibt erhalten; kein allgemeiner Beweis fuer die gefundene Datenbeziehung." : "Ein Ausdruck besteht Lern- und Hold-out-Pruefung sowie die getrennte Ganzzahl-Bruchrechnung; kein allgemeiner Beweis.") :
           run.status=="CANDIDATE" ? "Ein Ausdruck passt zu den Lernpaaren; Hold-out fehlt." :
           run.status=="REFUTED" ? "Alle Lernkandidaten scheitern an Hold-out-Werten oder Division durch null." :
           "Kein Ausdruck in der vollstaendig untersuchten Grammatik passt zu den Lernpaaren.")
       <<",\"grammar_expressions\":"<<run.raw_combinations<<",\"candidates_executed\":"<<run.candidates
       <<",\"operations_used\":"<<run.operations<<",\"planning_operations\":"<<run.planning_operations
       <<",\"plans_completed\":"<<run.plans_completed<<",\"checkpoints\":"<<run.checkpoints
       <<",\"trace_bytes\":"<<run.trace_bytes<<",\"training_samples\":"<<run.task.training.size()<<",\"holdout_samples\":"<<run.task.holdout.size()
       ;
    if(indexed) out<<",\"index_entries\":"<<run.index_entries;
    if(cached) out<<",\"cache_hits\":"<<run.expression_cache.hits<<",\"cache_misses\":"<<run.expression_cache.misses<<",\"cache_evictions\":"<<run.expression_cache.evictions<<",\"cache_entries\":"<<run.expression_cache.entries;
    if(domain_cached) {
        const auto& stats=run.domain_cache_stats;
        out<<",\"domain_cache_policy\":\"equal-64-key4096-v1\""
           <<",\"domain_cache_hits\":"<<stats.hits<<",\"domain_cache_misses\":"<<stats.misses
           <<",\"domain_cache_bypasses\":"<<stats.bypasses<<",\"domain_cache_inserts\":"<<stats.inserts
           <<",\"domain_cache_evictions\":"<<stats.evictions<<",\"domain_cache_entries\":"<<stats.entries
           <<",\"domain_cache_key_bytes\":"<<stats.key_bytes<<",\"domain_cache_operations\":"<<stats.operations;
    }
    if(witnessed) out<<",\"domain_witness_candidates\":"<<run.domain_witness_candidates
        <<",\"domain_bounded_witnesses\":"<<run.domain_bounded_witnesses<<",\"domain_different\":"<<run.domain_different;
    if(modular) out<<",\"domain_modular_certificates\":"<<run.domain_modular_certificates;
    if(rational_q) out<<",\"domain_comparison_operations\":"<<run.domain_comparison_operations<<",\"domain_comparisons\":"<<run.domain_comparisons<<",\"domain_unknown\":"<<run.domain_unknown;
    if(grouped) out<<",\"normalization_operations\":"<<run.normalization_operations<<",\"stored_source_hypotheses\":"<<run.expression_sources;
    out<<",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out<<',';
        first=false;
        out<<"{\"id\":"<<h.id<<",\"nodes\":"<<h.expression.size()<<",\"formula\":"<<json_string(expression_formula(h.expression))
           <<",\"status\":"<<json_string(h.status)<<",\"domain_conditions\":[";
        bool initial=true;
        for(const auto& c:expression_conditions(h.expression)) {if(!initial) out<<',';initial=false;out<<json_string(c);}
        out<<"],\"expression\":[";
        initial=true;
        for(const auto& n:h.expression) {
            if(!initial) out<<',';
            initial=false;
            out<<"{\"op\":"<<json_string(std::string(1,n.op));
            if(n.op=='c') out<<",\"value\":"<<json_string(exact_text(n.value));
            else if(n.op!='x') out<<",\"left\":"<<n.left<<",\"right\":"<<n.right;
            out<<'}';
        }
        out<<']';
        if(grouped) {
            out<<",\"normal_form\":";
            if(!h.normal_form) out<<"null";
            else {
                const auto& f=*h.normal_form;
                auto coefficients=[&](const std::vector<ExactValue>& values) {
                    out<<'[';bool initial=true;
                    for(const auto& c:values) {if(!initial) out<<',';initial=false;out<<json_string(exact_text(c));}
                    out<<']';
                };
                out<<"{\"numerator\":";coefficients(f.numerator);
                out<<",\"denominator\":";coefficients(f.denominator);
                out<<",\"excluded_polynomial\":";coefficients(f.excluded_polynomial);
                out<<",\"empty_domain\":"<<(f.empty_domain ? "true" : "false")<<'}';
            }
            out<<",\"expression_origins\":[";
            bool origin_first=true;
            for(const auto& origin:h.expression_origins) {
                if(!origin_first) out<<',';
                origin_first=false;
                out<<"{\"id\":"<<origin.id<<",\"status\":"<<json_string(origin.status)<<",\"formula\":"<<json_string(expression_formula(origin.expression))<<",\"expression\":[";
                bool node_first=true;
                for(const auto& n:origin.expression) {
                    if(!node_first) out<<',';
                    node_first=false;
                    out<<"{\"op\":"<<json_string(std::string(1,n.op));
                    if(n.op=='c') out<<",\"value\":"<<json_string(exact_text(n.value));
                    else if(n.op!='x') out<<",\"left\":"<<n.left<<",\"right\":"<<n.right;
                    out<<'}';
                }
                out<<"]}";
            }
            out<<']';
        }
        out<<'}';
    }
    out<<"],\"trace\":["; first=true;
    for(const auto& e:run.trace) {if(!first) out<<',';first=false;out<<json_string(e);}
    out<<"]}";return out.str();
}
}
