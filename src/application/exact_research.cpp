#include "bruce/application/exact_research.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/application/research.hpp"
#include "bruce/compatibility/selection.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/math/power_affine.hpp"
#include "bruce/application/exact_planner.hpp"
#include "bruce/application/polynomial_research.hpp"
#include "bruce/application/portfolio_research.hpp"
#include "bruce/application/rational_linear_research.hpp"
#include "bruce/application/expression_research.hpp"
#include <set>
#include <sstream>

namespace bruce {
namespace {
std::optional<ExactValue> coefficient(const mpz_class& minimum,std::uint64_t slot,unsigned denominators) {
    const mpz_class numerator=minimum+static_cast<unsigned long>(slot/denominators);
    const unsigned long denominator=1+slot%denominators;
    if(mpz_gcd_ui(nullptr,numerator.get_mpz_t(),denominator)!=1) return std::nullopt;
    ExactValue result(numerator,mpz_class(denominator));
    result.canonicalize();
    return result;
}
}
ExactResearchRun exact_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& experience_checkpoint) {
    const NumberDomain domain(task.domain,task.max_bits);
    const auto slots=validate_exact_task(task,domain);
    switch(research_strategy(task.strategy).executor) {
        case ResearchExecutor::expression: return run_expression_research(std::move(task),cancelled);
        case ResearchExecutor::rational: return run_rational_linear_research(std::move(task),cancelled);
        case ResearchExecutor::portfolio: return run_portfolio_research(std::move(task),cancelled,experience_checkpoint);
        case ResearchExecutor::polynomial: return run_polynomial_research(std::move(task),cancelled);
        case ResearchExecutor::affine_power: break;
    }
    const mpz_class minimum=domain.parse_integer(task.minimum);
    ExactResearchRun run;
    const bool nonlinear=task.strategy=="power-affine";
    const bool adaptive=task.planner=="adaptive";
    run.task=task; run.raw_combinations=slots*slots*task.max_power;
    run.trace={adaptive ? "START two-point-v1: training-only coefficient planning, power ascending" :
        nonlinear ? "START power-affine-v1: power ascending, canonical coefficients, numerator then denominator ascending" :
        "START affine-exact-v1: canonical coefficients, numerator then denominator ascending"};
    run.trace_bytes=run.trace.front().size();
    bool stopped=false;
    auto stop=[&](std::string reason) { run.stop_reason=std::move(reason); stopped=true; };
    auto checkpoint=[&]() {
        if((task.cancel_after_checkpoints && run.checkpoints>=*task.cancel_after_checkpoints) ||
           (task.cancel_after_candidates && run.candidates>=*task.cancel_after_candidates) ||
           (cancelled && cancelled(run.candidates))) {
            run.task.cancel_after_checkpoints=run.checkpoints; stop("Cancelled"); return false;
        }
        ++run.checkpoints; return true;
    };
    struct PlanningStop {};
    for(unsigned power=1;power<=task.max_power && !stopped;++power) {
        CoefficientPlan plan;
        if(adaptive) {
            if(!checkpoint()) break;
            if(run.trace.size()+2>task.budget.trace_events) { stop("TraceBudgetExceeded"); break; }
            // Small index-only decision plus a possible stop and final event.
            if(run.trace_bytes+512>task.max_trace_bytes) { stop("TraceByteBudgetExceeded"); break; }
            const auto before=run.operations;
            std::string event="PLAN power="+std::to_string(power);
            try {
                plan=plan_coefficients(task,power,[&](std::uint64_t cost) {
                    if(task.budget.operations-run.operations<cost) throw PlanningStop{};
                    run.operations+=cost;
                });
                ++run.plans_completed;
                event+=" "+plan.decision+" training[0,"+std::to_string(plan.second_sample)+"]";
                if(plan.decision=="derived") event+=" slots="+std::to_string(plan.a_slot)+","+std::to_string(plan.b_slot);
            } catch(const PlanningStop&) { stop("OperationBudgetExceeded"); }
              catch(const NumericLimit&) { stop("IntegerBitLimit"); }
            run.planning_operations+=run.operations-before;
            if(stopped) event+=" stop="+run.stop_reason;
            run.trace_bytes+=event.size(); run.trace.push_back(std::move(event));
            if(stopped) break;
            if(plan.decision!="fallback" && plan.decision!="derived") continue;
        }
        const bool derived=adaptive && plan.decision=="derived";
        for(std::uint64_t a_slot=derived ? plan.a_slot : 0;
            a_slot<(derived ? plan.a_slot+1 : slots) && !stopped;++a_slot) {
            const auto ai=(power-1)*slots+a_slot;
            const auto a=coefficient(minimum,a_slot,task.max_denominator);
            if(!a) continue;
            if(power>1 && *a==0) continue;
            for(std::uint64_t bi=derived ? plan.b_slot : 0;bi<(derived ? plan.b_slot+1 : slots) && !stopped;++bi) {
                const auto b=coefficient(minimum,bi,task.max_denominator);
                if(!b) continue;
                if(adaptive) { if(!checkpoint()) break; }
                else if((task.cancel_after_candidates && run.candidates>=*task.cancel_after_candidates) || (cancelled && cancelled(run.candidates))) {
                    run.task.cancel_after_candidates=run.candidates; stop("Cancelled"); break;
                }
                if(run.candidates>=task.budget.candidates) { stop("CandidateBudgetExceeded"); break; }
                if(run.trace.size()+2>task.budget.trace_events) { stop("TraceBudgetExceeded"); break; }
                const auto id=ai*slots+bi;
                std::string event="CANDIDATE "+std::to_string(id)+" a="+exact_text(*a)+" b="+exact_text(*b);
                if(nonlinear) event+=" power="+std::to_string(power);
                // Reserve enough for a failure suffix and final event before work.
                if(run.trace_bytes+event.size()+256>task.max_trace_bytes) { stop("TraceByteBudgetExceeded"); break; }
                ++run.candidates;
                auto inspect=[&](const std::vector<ExactSample>& samples,std::string_view stage) {
                    for(std::size_t i=0;i<samples.size();++i) {
                        const auto cost=nonlinear ? 3*power+4 : 4;
                        if(task.budget.operations-run.operations<cost) {
                            event+=" "+std::string(stage)+"["+std::to_string(i)+"] operation_limit";
                            stop("OperationBudgetExceeded"); return false;
                        }
                        run.operations+=cost;
                        try {
                            const auto value=nonlinear ? power_affine(domain,*a,*b,samples[i].x,power) :
                                exact_affine(domain,*a,*b,samples[i].x);
                            if(value!=samples[i].y) {
                                event+=" "+std::string(stage)+"["+std::to_string(i)+"] contradiction";
                                return false;
                            }
                            const bool checked=nonlinear ? power_countercheck(domain,*a,*b,samples[i].x,samples[i].y,power) :
                                exact_countercheck(domain,*a,*b,samples[i].x,samples[i].y);
                            if(!checked) throw Error("CountercheckMismatch");
                        } catch(const NumericLimit&) {
                            event+=" "+std::string(stage)+"["+std::to_string(i)+"] bit_limit";
                            stop("IntegerBitLimit"); return false;
                        }
                    }
                    return true;
                };
                if(inspect(task.training,"training")) {
                    if(run.hypotheses.size()>=task.budget.results) { event+=" result_limit"; stop("ResultBudgetExceeded"); }
                    else {
                        std::string status="CANDIDATE";
                        if(!task.holdout.empty()) {
                            const bool passed=inspect(task.holdout,"holdout");
                            status=stopped ? "INCOMPLETE" : passed ? "VERIFIED_ON_SAMPLES" : "REFUTED";
                        }
                        run.hypotheses.push_back({id,ai,bi,*a,*b,status,power});
                        event+=" "+status;
                    }
                }
                run.trace_bytes+=event.size(); run.trace.push_back(std::move(event));
            }
        }
    }
    std::vector<RankedCandidate> ranked;
    for(const auto& h:run.hypotheses) ranked.push_back({h.id,0,0,h.status=="REFUTED" || h.status=="INCOMPLETE"});
    const auto selected=select_candidates(ranked);
    run.status=stopped ? "INCOMPLETE" : selected.ambiguous ? "AMBIGUOUS" : selected.ids.size()==1 ?
        (task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES") : run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    const auto end="END "+run.status+" "+run.stop_reason;
    run.trace_bytes+=end.size(); run.trace.push_back(end);
    return run;
}

std::string render_exact_research(const ExactResearchRun& run) {
    if(run.task.strategy=="expression") return render_expression_research(run);
    if(run.task.strategy=="rational-linear" || run.task.strategy=="rational-quadratic") return render_rational_linear_research(run);
    if(run.task.strategy=="portfolio" || (run.task.strategy=="portfolio-rational" || run.task.strategy=="portfolio-quadratic" || run.task.strategy=="portfolio-expression")) return render_portfolio_research(run);
    if(run.task.strategy=="polynomial") return render_polynomial_research(run);
    std::ostringstream out;
    const bool nonlinear=run.task.strategy=="power-affine";
    if(run.task.planner=="adaptive") out << "{\"engine\":\"bruce-adaptive-v1\",\"rule_version\":\"power-affine-v1\",\"policy_version\":\"adaptive-samples-v1\",\"strategy\":\"power-affine\",\"planner\":\"two-point-v1\",\"max_power\":" << run.task.max_power
        << ",\"plans_completed\":" << run.plans_completed << ",\"planning_operations\":" << run.planning_operations << ",\"checkpoints\":" << run.checkpoints;
    else if(nonlinear) out << "{\"engine\":\"bruce-power-v1\",\"rule_version\":\"power-affine-v1\",\"policy_version\":\"power-samples-v1\",\"strategy\":\"power-affine\",\"max_power\":" << run.task.max_power;
    else out << "{\"engine\":\"bruce-exact-v1\",\"rule_version\":\"affine-exact-v1\",\"policy_version\":\"exact-samples-v1\"";
    out << ",\"domain\":" << json_string(run.task.domain) << ",\"max_bits\":" << run.task.max_bits
        << ",\"max_denominator\":" << run.task.max_denominator << ",\"minimum\":" << json_string(run.task.minimum)
        << ",\"maximum\":" << json_string(run.task.maximum) << ",\"status\":" << json_string(run.status)
        << ",\"stop_reason\":" << json_string(run.stop_reason) << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"claim_scope\":" << json_string(nonlinear ? "Exact arithmetic on supplied samples in the declared bounded a*x^p+b family; no general proof" :
            "Exact arithmetic on supplied samples in the declared bounded affine coefficient family; no general proof")
        << ",\"explanation\":" << json_string(run.status=="INCOMPLETE" ? "Suche unvollstaendig; keine abschliessende Aussage ueber die Kandidatenfamilie." :
            run.status=="VERIFIED_ON_SAMPLES" ? "Ein Kandidat besteht Lern- und Hold-out-Pruefung samt Rueckrechnung. Kein allgemeiner Beweis." :
            run.status=="AMBIGUOUS" ? "Mehrere Kandidaten passen; die Beispiele entscheiden nicht eindeutig." :
            run.status=="CANDIDATE" ? "Ein Kandidat passt zu den Lernpaaren; Hold-out-Pruefung fehlt." :
            run.status=="REFUTED" ? "Alle gefundenen Lernkandidaten scheitern an den Hold-out-Paaren." :
            nonlinear ? "Kein Kandidat der vollstaendig untersuchten Potenzfamilie passt zu den Lernpaaren." :
            "Kein Kandidat der vollstaendig untersuchten affinen Familie passt zu den Lernpaaren.")
        << ",\"raw_parameter_combinations\":" << run.raw_combinations << ",\"candidates_executed\":" << run.candidates
        << ",\"operations_used\":" << run.operations << ",\"trace_bytes\":" << run.trace_bytes
        << ",\"training_samples\":" << run.task.training.size() << ",\"holdout_samples\":" << run.task.holdout.size()
        << ",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out<<',';
        first=false;
        out << "{\"id\":" << h.id << ",\"a_index\":" << h.a_index << ",\"b_index\":" << h.b_index
            << ",\"a\":" << json_string(exact_text(h.a)) << ",\"b\":" << json_string(exact_text(h.b));
        if(nonlinear) out << ",\"power\":" << h.power;
        out << ",\"formula\":" << json_string("("+exact_text(h.a)+")*x"+(nonlinear ? "^"+std::to_string(h.power) : "")+"+("+exact_text(h.b)+")")
            << ",\"status\":" << json_string(h.status) << '}';
    }
    out << "],\"trace\":[";
    first=true;
    for(const auto& e:run.trace) { if(!first) out<<','; first=false; out<<json_string(e); }
    out << "]}";
    return out.str();
}

std::vector<ExactSample> parse_exact_samples(std::string_view bytes,const NumberDomain& domain) {
    if(bytes.size()>1024*1024) throw Error("InputByteLimit");
    std::istringstream input{std::string(bytes)};
    std::string line;
    if(!std::getline(input,line)) throw Error("MissingSampleHeader");
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="x,y") throw Error("InvalidSampleHeader: expected x,y");
    std::vector<ExactSample> samples;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        const auto comma=line.find(',');
        if(comma==std::string::npos) throw Error("InvalidSampleRow");
        samples.push_back({domain.parse(std::string_view(line).substr(0,comma)),domain.parse(std::string_view(line).substr(comma+1))});
        if(samples.size()>4096) throw Error("SampleBudgetExceeded");
    }
    if(samples.empty()) throw Error("EmptySamples");
    return samples;
}

std::vector<ExactSample> read_exact_samples(const std::string& path,const NumberDomain& domain) {
    return parse_exact_samples(read_bounded(path,1024*1024),domain);
}

std::string calculate_exact(const NumberDomain& domain,std::string_view op,std::string_view left,std::string_view right,std::string_view modulus) {
    const auto a=domain.parse(left);
    std::ostringstream out;
    out << "{\"domain\":" << json_string(domain.name()) << ",\"operation\":" << json_string(op) << ",\"max_bits\":" << domain.max_bits();
    if(!modulus.empty() && op!="modpow") throw Error("ModulusRequiresModpow");
    if(op=="divmod") {
        if(domain.name()!="bigint") throw Error("DivmodRequiresBigint");
        const auto b=domain.parse(right);
        const auto [q,r]=domain.divmod(a.get_num(),b.get_num());
        out << ",\"quotient\":" << json_string(q.get_str()) << ",\"remainder\":" << json_string(r.get_str());
    } else {
        ExactValue value;
        if(op=="pow") value=domain.power(a,integer<std::uint64_t>(right));
        else if(op=="modpow") {
            if(domain.name()!="bigint") throw Error("ModpowRequiresBigint");
            value=domain.modular_power(a.get_num(),integer<std::uint64_t>(right),domain.parse_integer(modulus));
        } else {
            const auto b=domain.parse(right);
            if(op=="add") value=domain.add(a,b);
            else if(op=="sub") value=domain.subtract(a,b);
            else if(op=="mul") value=domain.multiply(a,b);
            else if(op=="div") value=domain.divide(a,b);
            else throw Error("UnknownExactOperation");
        }
        out << ",\"value\":" << json_string(exact_text(value));
    }
    out << '}';
    return out.str();
}
}
