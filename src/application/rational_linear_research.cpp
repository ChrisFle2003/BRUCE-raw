#include "bruce/application/rational_linear_research.hpp"
#include "bruce/application/research.hpp"
#include "bruce/math/rational_linear.hpp"
#include <sstream>
#include <algorithm>

namespace bruce {
ExactResearchRun run_rational_linear_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled) {
    const bool quadratic=task.strategy=="rational-quadratic";
    const NumberDomain domain(task.domain,task.max_bits);
    const auto low=domain.parse_integer(task.minimum),high=domain.parse_integer(task.maximum);
    const mpz_class width=high-low+1;
    const std::uint64_t slots=width.get_ui()*task.max_denominator;
    ExactResearchRun run; run.task=task; run.raw_combinations=slots*slots*slots;
    if(quadratic) run.raw_combinations*=slots;
    run.trace={quadratic ? "START quadratic-system-v1: training-only equations, explicit denominator condition" : "START linear-system-v1: training-only equations, explicit excluded pole"};
    run.trace_bytes=run.trace.front().size();
    struct Stop {};
    auto stop=[&](std::string reason) { run.stop_reason=std::move(reason); throw Stop{}; };
    auto spend=[&](std::uint64_t cost) {
        if(task.budget.operations-run.operations<cost) stop("OperationBudgetExceeded");
        run.operations+=cost;
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
    auto record=[&](std::string event) { run.trace_bytes+=event.size(); run.trace.push_back(std::move(event)); };
    auto coefficient=[&](std::uint64_t slot)->std::optional<ExactValue> {
        const mpz_class n=low+static_cast<unsigned long>(slot/task.max_denominator);
        const unsigned long d=1+slot%task.max_denominator;
        if(mpz_gcd_ui(nullptr,n.get_mpz_t(),d)!=1) return std::nullopt;
        ExactValue result(n,mpz_class(d)); result.canonicalize(); return result;
    };
    try {
        checkpoint(); reserve();
        const auto before=run.operations;
        RationalQuadraticPlan plan;
        try {
            std::vector<ExactValue> xs,ys;
            for(const auto& s:task.training) { xs.push_back(s.x); ys.push_back(s.y); }
            if(quadratic) plan=solve_rational_quadratic(domain,xs,ys,spend);
            else {
                const auto linear=solve_rational_linear(domain,xs,ys,spend);
                plan.decision=linear.decision; plan.rank=linear.rank; plan.rows=linear.rows;
                std::copy(linear.coefficients.begin(),linear.coefficients.end(),plan.coefficients.begin());
            }
        } catch(const NumericLimit&) {
            run.planning_operations+=run.operations-before;
            record("PLAN stop=IntegerBitLimit"); stop("IntegerBitLimit");
        } catch(const Stop&) {
            run.planning_operations+=run.operations-before;
            record("PLAN stop="+run.stop_reason); throw;
        }
        run.planning_operations+=run.operations-before; ++run.plans_completed;
        std::string event="PLAN rank="+std::to_string(plan.rank)+" rows="+std::to_string(plan.rows)+" "+plan.decision;
        std::array<std::uint64_t,4> start{0,0,0,0},end{slots,slots,slots,quadratic ? slots : 1};
        bool allowed=plan.decision!="inconsistent";
        if(plan.decision=="derived") {
            for(unsigned i=0;i<(quadratic ? 4u : 3u);++i) {
                const auto& c=plan.coefficients[i];
                if(c.get_num()<low || c.get_num()>high || c.get_den()>task.max_denominator) { allowed=false; break; }
                const mpz_class offset=c.get_num()-low;
                start[i]=offset.get_ui()*task.max_denominator+c.get_den().get_ui()-1; end[i]=start[i]+1;
            }
            if(!allowed) event+=" outside_family";
        }
        record(event);
        if(allowed) for(auto ai=start[0];ai<end[0];++ai) {
            const auto a=coefficient(ai); if(!a) continue;
            for(auto bi=start[1];bi<end[1];++bi) {
                const auto b=coefficient(bi); if(!b) continue;
                for(auto ci=start[2];ci<end[2];++ci) {
                    const auto c=coefficient(ci); if(!c) continue;
                    for(auto di=start[3];di<end[3];++di) {
                        const auto d=quadratic ? coefficient(di) : std::optional<ExactValue>{0}; if(!d) continue;
                        checkpoint();
                        if(run.candidates>=task.budget.candidates) stop("CandidateBudgetExceeded");
                        reserve(); ++run.candidates;
                        const auto linear_id=(ai*slots+bi)*slots+ci;
                        const auto id=quadratic ? linear_id*slots+di : linear_id;
                        event="CANDIDATE id="+std::to_string(id);
                        auto inspect=[&](const std::vector<ExactSample>& samples,std::string_view phase) {
                            for(std::size_t i=0;i<samples.size();++i) {
                                try {
                                    spend(quadratic ? 24 : 12);
                                    const auto value=quadratic ? rational_quadratic(domain,*a,*b,*c,*d,samples[i].x) : rational_linear(domain,*a,*b,*c,samples[i].x);
                                    if(value!=samples[i].y) {
                                        event+=" "+std::string(phase)+"["+std::to_string(i)+"] contradiction"; return false;
                                    }
                                    const bool checked=quadratic ? rational_quadratic_countercheck(domain,*a,*b,*c,*d,samples[i].x,samples[i].y) : rational_linear_countercheck(domain,*a,*b,*c,samples[i].x,samples[i].y);
                                    if(!checked) throw Error("CountercheckMismatch");
                                } catch(const RationalPole&) {
                                    event+=" "+std::string(phase)+"["+std::to_string(i)+"] pole"; return false;
                                } catch(const NumericLimit&) {
                                    event+=" "+std::string(phase)+"["+std::to_string(i)+"] bit_limit"; stop("IntegerBitLimit");
                                }
                            }
                            return true;
                        };
                        try {
                            if(inspect(task.training,"training")) {
                                if(run.hypotheses.size()>=task.budget.results) stop("ResultBudgetExceeded");
                                ExactHypothesis h{id,quadratic ? id/(1ULL<<30) : ai*slots+bi,quadratic ? id%(1ULL<<30) : ci,*a,*b,"CANDIDATE",1,{*b,*a},{},
                                    quadratic ? std::vector<ExactValue>{*d,*c,1} : std::vector<ExactValue>{*c,1}};
                                if(!task.holdout.empty()) {
                                    try { h.status=inspect(task.holdout,"holdout") ? "VERIFIED_ON_SAMPLES" : "REFUTED"; }
                                    catch(const Stop&) { h.status="INCOMPLETE"; run.hypotheses.push_back(h); throw; }
                                }
                                event+=" "+h.status; run.hypotheses.push_back(std::move(h));
                            }
                        } catch(const Stop&) { record(event+" stop="+run.stop_reason); throw; }
                        record(event);
                    }
                }
            }
        }
    } catch(const Stop&) {}
    std::size_t survivors=0;
    for(const auto& h:run.hypotheses) if(h.status!="REFUTED" && h.status!="INCOMPLETE") ++survivors;
    run.status=run.stop_reason!="Completed" ? "INCOMPLETE" : survivors>1 ? "AMBIGUOUS" : survivors==1 ?
        (task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES") : run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    record("END "+run.status+" "+run.stop_reason); return run;
}
std::string render_rational_linear_research(const ExactResearchRun& run) {
    const bool quadratic=run.task.strategy=="rational-quadratic";
    std::ostringstream out;
    out << (quadratic ? "{\"engine\":\"bruce-rational-quadratic-v1\",\"rule_version\":\"monic-quadratic-fraction-v1\",\"policy_version\":\"defined-samples-v1\",\"strategy\":\"rational-quadratic\",\"planner\":\"quadratic-system-v1\"" : "{\"engine\":\"bruce-rational-linear-v1\",\"rule_version\":\"monic-linear-fraction-v1\",\"policy_version\":\"defined-samples-v1\",\"strategy\":\"rational-linear\",\"planner\":\"linear-system-v1\"")
        << ",\"domain\":" << json_string(run.task.domain) << ",\"max_bits\":" << run.task.max_bits << ",\"max_denominator\":" << run.task.max_denominator
        << ",\"minimum\":" << json_string(run.task.minimum) << ",\"maximum\":" << json_string(run.task.maximum)
        << ",\"status\":" << json_string(run.status) << ",\"stop_reason\":" << json_string(run.stop_reason)
        << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"claim_scope\":" << json_string(quadratic ? "Bounded (a*x+b)/(x^2+c*x+d), denominator nonzero; exact supplied-sample checks, no general proof" : "Bounded (a*x+b)/(x+c), x!=-c; exact supplied-sample checks, no general proof")
        << ",\"explanation\":" << json_string(run.status=="INCOMPLETE" ? "Die begrenzte Suche ist unvollstaendig; vorhandene Treffer sind Teilbefunde." :
            run.status=="VERIFIED_ON_SAMPLES" ? (quadratic ? "Eine Bruchfunktion besteht Lern- und Hold-out-Pruefung mit getrennten Rechenwegen. Die Nennerbedingung bleibt erhalten; kein allgemeiner Beweis." : "Eine Bruchfunktion besteht Lern- und Hold-out-Pruefung mit getrennten Rechenwegen. Der ausgeschlossene Definitionspunkt bleibt erhalten; kein allgemeiner Beweis.") :
            run.status=="CANDIDATE" ? "Eine Bruchfunktion passt zu den Lernpaaren; Hold-out-Pruefung fehlt." :
            run.status=="AMBIGUOUS" ? "Mehrere Bruchfunktionen bleiben gleichrangig; unterschiedliche Definitionsluecken bleiben getrennt." :
            run.status=="REFUTED" ? "Alle Lernkandidaten scheitern an Hold-out-Werten oder Nennernullstellen." :
            "Keine Bruchfunktion in der vollstaendig untersuchten Familie passt zu allen Lernpaaren.")
        << ",\"raw_parameter_combinations\":" << run.raw_combinations
        << ",\"candidates_executed\":" << run.candidates << ",\"operations_used\":" << run.operations
        << ",\"planning_operations\":" << run.planning_operations << ",\"plans_completed\":" << run.plans_completed
        << ",\"checkpoints\":" << run.checkpoints << ",\"trace_bytes\":" << run.trace_bytes
        << ",\"training_samples\":" << run.task.training.size() << ",\"holdout_samples\":" << run.task.holdout.size() << ",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out<<',';
        first=false;
        const auto a=exact_text(h.a),b=exact_text(h.b),c=exact_text(h.denominator[quadratic ? 1 : 0]);
        out << "{\"id\":" << h.id << ",\"a\":" << json_string(a) << ",\"b\":" << json_string(b) << ",\"c\":" << json_string(c);
        if(quadratic) {
            const auto d=exact_text(h.denominator[0]);
            const auto denominator="x^2+("+c+")*x+("+d+")";
            out << ",\"d\":" << json_string(d) << ",\"numerator\":[" << json_string(b) << ',' << json_string(a)
                << "],\"denominator\":[" << json_string(d) << ',' << json_string(c) << ",\"1\"]"
                << ",\"domain_condition\":" << json_string(denominator+" != 0")
                << ",\"formula\":" << json_string("("+a+"*x+("+b+"))/("+denominator+"), "+denominator+" != 0");
        } else {
            const ExactValue excluded=-h.denominator[0];
            out << ",\"numerator\":[" << json_string(b) << ',' << json_string(a) << "],\"denominator\":[" << json_string(c) << ",\"1\"]"
                << ",\"excluded_x\":" << json_string(exact_text(excluded))
                << ",\"formula\":" << json_string("("+a+"*x+("+b+"))/(x+("+c+")), x != "+exact_text(excluded));
        }
        out << ",\"status\":" << json_string(h.status) << '}';
    }
    out << "],\"trace\":["; first=true;
    for(const auto& e:run.trace) { if(!first) out<<','; first=false; out<<json_string(e); }
    out << "]}"; return out.str();
}
}
