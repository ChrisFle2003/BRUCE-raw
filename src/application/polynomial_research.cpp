#include "bruce/application/polynomial_research.hpp"
#include "bruce/application/research.hpp"
#include "bruce/math/polynomial.hpp"
#include <sstream>

namespace bruce {
ExactResearchRun run_polynomial_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled) {
    const NumberDomain domain(task.domain,task.max_bits),rational("rational",task.max_bits);
    const auto low=domain.parse_integer(task.minimum),high=domain.parse_integer(task.maximum);
    ExactResearchRun run; run.task=task;
    run.trace={"START newton-v1: ascending degree, training-only interpolation"};
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
    try {
        for(unsigned degree=0;degree<=task.max_degree;++degree) {
            checkpoint();
            if(task.training.size()<=degree) stop("InsufficientTrainingSamples");
            reserve();
            std::string event="PLAN degree="+std::to_string(degree)+" training[0.."+std::to_string(degree)+"]";
            const auto before=run.operations;
            std::vector<ExactValue> coefficients;
            try {
                std::vector<ExactValue> xs,ys;
                for(unsigned i=0;i<=degree;++i) { xs.push_back(task.training[i].x); ys.push_back(task.training[i].y); }
                coefficients=interpolate_polynomial(rational,xs,ys,spend);
            } catch(const NumericLimit&) {
                run.planning_operations+=run.operations-before;
                record(event+" stop=IntegerBitLimit"); stop("IntegerBitLimit");
            } catch(const Stop&) {
                run.planning_operations+=run.operations-before;
                record(event+" stop="+run.stop_reason); throw;
            }
            run.planning_operations+=run.operations-before; ++run.plans_completed;
            if(degree && coefficients.back()==0) { record(event+" duplicate_lower_degree"); continue; }
            bool allowed=true;
            for(const auto& c:coefficients)
                if(c.get_num()<low || c.get_num()>high || c.get_den()>task.max_denominator) allowed=false;
            if(!allowed) { record(event+" outside_family"); continue; }
            record(event+" derived");
            checkpoint();
            if(run.candidates>=task.budget.candidates) stop("CandidateBudgetExceeded");
            reserve(); ++run.candidates;
            event="CANDIDATE degree="+std::to_string(degree);
            auto inspect=[&](const std::vector<ExactSample>& samples,std::string_view phase) {
                for(std::size_t i=0;i<samples.size();++i) {
                    try {
                        spend(6*degree+4);
                        if(polynomial_horner(domain,coefficients,samples[i].x)!=samples[i].y) {
                            event+=" "+std::string(phase)+"["+std::to_string(i)+"] contradiction"; return false;
                        }
                        if(polynomial_sum(domain,coefficients,samples[i].x)!=samples[i].y) throw Error("CountercheckMismatch");
                    } catch(const NumericLimit&) {
                        event+=" "+std::string(phase)+"["+std::to_string(i)+"] bit_limit";
                        stop("IntegerBitLimit");
                    }
                }
                return true;
            };
            try {
                if(inspect(task.training,"training")) {
                    if(run.hypotheses.size()>=task.budget.results) stop("ResultBudgetExceeded");
                    ExactHypothesis hypothesis{degree,degree,0,0,0,"CANDIDATE",1,coefficients};
                    if(!task.holdout.empty()) {
                        try { hypothesis.status=inspect(task.holdout,"holdout") ? "VERIFIED_ON_SAMPLES" : "REFUTED"; }
                        catch(const Stop&) { hypothesis.status="INCOMPLETE"; run.hypotheses.push_back(hypothesis); throw; }
                    }
                    event+=" "+hypothesis.status; run.hypotheses.push_back(std::move(hypothesis));
                }
            } catch(const Stop&) { record(event+" stop="+run.stop_reason); throw; }
            record(event);
        }
    } catch(const Stop&) {}
    std::size_t survivors=0;
    for(const auto& h:run.hypotheses) if(h.status!="REFUTED" && h.status!="INCOMPLETE") ++survivors;
    run.status=run.stop_reason!="Completed" ? "INCOMPLETE" : survivors>1 ? "AMBIGUOUS" : survivors==1 ?
        (task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES") : run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    record("END "+run.status+" "+run.stop_reason);
    return run;
}

std::string render_polynomial_research(const ExactResearchRun& run) {
    std::ostringstream out;
    out << "{\"engine\":\"bruce-polynomial-v1\",\"rule_version\":\"polynomial-v1\",\"policy_version\":\"polynomial-samples-v1\",\"strategy\":\"polynomial\",\"planner\":\"newton-v1\""
        << ",\"domain\":" << json_string(run.task.domain) << ",\"max_degree\":" << run.task.max_degree
        << ",\"max_bits\":" << run.task.max_bits << ",\"max_denominator\":" << run.task.max_denominator
        << ",\"minimum\":" << json_string(run.task.minimum) << ",\"maximum\":" << json_string(run.task.maximum)
        << ",\"status\":" << json_string(run.status) << ",\"stop_reason\":" << json_string(run.stop_reason)
        << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"claim_scope\":\"Bounded polynomial interpolation and independent evaluation on supplied samples; no general proof\""
        << ",\"explanation\":" << json_string(run.status=="INCOMPLETE" ? "Suche unvollstaendig; niedrigere passende Grade sind nur Teilbefunde." :
            run.status=="VERIFIED_ON_SAMPLES" ? "Ein Polynom besteht Lern- und Hold-out-Pruefung mit getrennten Rechenwegen. Interpolation ist kein allgemeiner Beweis." :
            run.status=="CANDIDATE" ? "Ein Polynom passt zu den Lernpaaren; Hold-out-Pruefung fehlt." :
            run.status=="REFUTED" ? "Der Lernkandidat scheitert an Hold-out." :
            run.status=="AMBIGUOUS" ? "Mehrere Polynome bleiben gleichrangig." : "Kein Polynom in der vollstaendig untersuchten Familie passt.")
        << ",\"candidates_executed\":" << run.candidates << ",\"operations_used\":" << run.operations
        << ",\"planning_operations\":" << run.planning_operations << ",\"plans_completed\":" << run.plans_completed
        << ",\"checkpoints\":" << run.checkpoints << ",\"trace_bytes\":" << run.trace_bytes
        << ",\"training_samples\":" << run.task.training.size() << ",\"holdout_samples\":" << run.task.holdout.size() << ",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out<<',';
        first=false;
        out << "{\"id\":" << h.id << ",\"degree\":" << h.coefficients.size()-1 << ",\"coefficients\":[";
        for(std::size_t i=0;i<h.coefficients.size();++i) { if(i) out<<','; out<<json_string(exact_text(h.coefficients[i])); }
        out << "],\"formula\":" << json_string(polynomial_formula(h.coefficients)) << ",\"status\":" << json_string(h.status) << '}';
    }
    out << "],\"trace\":[";
    first=true;
    for(const auto& e:run.trace) { if(!first) out<<','; first=false; out<<json_string(e); }
    out << "]}"; return out.str();
}
}
