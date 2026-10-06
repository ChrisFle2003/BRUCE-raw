#include "bruce/application/research.hpp"
#include "bruce/compatibility/selection.hpp"
#include "bruce/math/exact.hpp"
#include <set>
#include <sstream>

namespace bruce {
void validate_task(const Task& task) {
    if(task.minimum>task.maximum || task.minimum < -1000000 || task.maximum>1000000)
        throw Error("InvalidSearchRange: expected -1000000 <= min <= max <= 1000000");
    if(task.training.empty() || task.training.size()>4096 || task.holdout.size()>4096)
        throw Error("InvalidSampleCount: training 1..4096, holdout 0..4096");
    const auto& b=task.budget;
    if(b.candidates<1 || b.candidates>1000000 || b.operations<1 || b.operations>10000000 ||
       b.trace_events<2 || b.trace_events>100002 || b.results<1 || b.results>4096)
        throw Error("InvalidBudget: candidates 1..1000000, operations 1..10000000, trace 2..100002, results 1..4096");
    std::set<std::int64_t> inputs;
    for(const auto& sample:task.training)
        if(!inputs.insert(sample.x).second) throw Error("DuplicateTrainingInput");
    for(const auto& sample:task.holdout)
        if(!inputs.insert(sample.x).second) throw Error("HoldoutOverlapOrDuplicate");
}

Run research(Task task,const std::function<bool(std::uint64_t)>& cancelled) {
    validate_task(task);
    Run run;
    run.task=task;
    run.trace.push_back("START affine-integer-v1: candidates generated lazily, a then b ascending");
    bool stop=false;
    auto stop_with=[&](std::string reason) {
        run.stop_reason=std::move(reason); stop=true;
    };
    for(auto a=task.minimum;a<=task.maximum && !stop;++a) {
        for(auto b=task.minimum;b<=task.maximum && !stop;++b) {
            if((task.cancel_after_candidates && run.candidates>=*task.cancel_after_candidates) ||
               (cancelled && cancelled(run.candidates))) {
                run.task.cancel_after_candidates=run.candidates;
                stop_with("Cancelled"); break;
            }
            if(run.candidates>=task.budget.candidates) { stop_with("CandidateBudgetExceeded"); break; }
            if(run.trace.size()+2>task.budget.trace_events) { stop_with("TraceBudgetExceeded"); break; }
            const auto id=run.candidates++;
            std::string event="CANDIDATE "+std::to_string(id)+" a="+std::to_string(a)+" b="+std::to_string(b);
            auto inspect=[&](const std::vector<Sample>& samples,std::string_view stage) {
                for(std::size_t i=0;i<samples.size();++i) {
                    if(task.budget.operations-run.operations<2) {
                        event+=" "+std::string(stage)+"["+std::to_string(i)+"] operation_limit";
                        stop_with("OperationBudgetExceeded"); return false;
                    }
                    // Each sample reserves one forward and one inverse check.
                    run.operations+=2;
                    bool forward=false;
                    try { forward=affine(a,b,samples[i].x)==samples[i].y; }
                    catch(const Error&) {
                        ++run.range_rejections;
                        event+=" "+std::string(stage)+"["+std::to_string(i)+"] integer_overflow";
                        return false;
                    }
                    const auto inverse=countercheck(a,b,samples[i]);
                    if(forward!=inverse) throw Error("CountercheckMismatch");
                    if(!forward) {
                        event+=" "+std::string(stage)+"["+std::to_string(i)+"] contradiction";
                        return false;
                    }
                }
                return true;
            };
            if(inspect(task.training,"training")) {
                if(run.hypotheses.size()>=task.budget.results) {
                    event+=" result_limit"; stop_with("ResultBudgetExceeded");
                } else {
                    std::string status="CANDIDATE";
                    if(!task.holdout.empty()) {
                        const bool passed=inspect(task.holdout,"holdout");
                        status=stop ? "INCOMPLETE" : passed ? "VERIFIED_ON_SAMPLES" : "REFUTED";
                    }
                    run.hypotheses.push_back({id,a,b,status});
                    event+=" "+status;
                }
            }
            run.trace.push_back(std::move(event));
        }
    }
    std::vector<RankedCandidate> ranked;
    for(const auto& h:run.hypotheses)
        ranked.push_back({h.id,0,0,h.status=="REFUTED" || h.status=="INCOMPLETE"});
    const auto selection=select_candidates(ranked);
    if(stop) run.status="INCOMPLETE";
    else if(selection.ambiguous) run.status="AMBIGUOUS";
    else if(selection.ids.size()==1) run.status=task.holdout.empty() ? "CANDIDATE" : "VERIFIED_ON_SAMPLES";
    else run.status=run.hypotheses.empty() ? "NO_CANDIDATE" : "REFUTED";
    run.trace.push_back("END "+run.status+" "+run.stop_reason);
    return run;
}



std::string render_run(const Run& run) {
    const auto width=static_cast<std::uint64_t>(run.task.maximum-run.task.minimum+1);
    std::ostringstream out;
    out << "{\"engine\":" << json_string(engine_version)
        << ",\"rule_version\":" << json_string(rule_version)
        << ",\"policy_version\":" << json_string(policy_version)
        << ",\"status\":" << json_string(run.status)
        << ",\"stop_reason\":" << json_string(run.stop_reason)
        << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"claim_scope\":\"Exact checks on supplied samples within the declared integer affine family; no general proof\""
        << ",\"explanation\":" << json_string(
            run.status=="VERIFIED_ON_SAMPLES" ? "Ein Kandidat passt zu Lern- und zurueckgehaltenen Pruefpaaren. Vorwaertsrechnung und Rueckpruefung stimmen ueberein. Keine allgemeine Formel bewiesen." :
            run.status=="CANDIDATE" ? "Ein Kandidat passt zu den Lernpaaren. Unabhaengige Datenpruefung fehlt." :
            run.status=="AMBIGUOUS" ? "Mehrere Kandidaten passen. Ihre stabile Reihenfolge entscheidet keine mathematische Wahrheit." :
            run.status=="INCOMPLETE" ? "Suche unvollstaendig. Budget oder Abbruch begrenzt die Aussage; keine abschliessende Auswahl." :
            run.status=="REFUTED" ? "Alle zu den Lernpaaren passenden Kandidaten scheitern an den Pruefpaaren." :
            "Kein passender Kandidat in dieser Familie und diesen Grenzen. Keine Aussage ueber andere Formeln.")
        << ",\"minimum\":" << run.task.minimum << ",\"maximum\":" << run.task.maximum
        << ",\"theoretical_candidates\":" << width*width
        << ",\"candidates_executed\":" << run.candidates
        << ",\"operations_used\":" << run.operations
        << ",\"integer_range_rejections\":" << run.range_rejections
        << ",\"training_samples\":" << run.task.training.size()
        << ",\"holdout_samples\":" << run.task.holdout.size()
        << ",\"hypotheses\":[";
    bool first=true;
    for(const auto& h:run.hypotheses) {
        if(!first) out << ',';
        first=false;
        out << "{\"id\":" << h.id << ",\"a\":" << h.a << ",\"b\":" << h.b
            << ",\"formula\":" << json_string(std::to_string(h.a)+"*x+("+std::to_string(h.b)+")")
            << ",\"status\":" << json_string(h.status) << '}';
    }
    out << "],\"trace\":[";
    first=true;
    for(const auto& event:run.trace) {
        if(!first) out << ',';
        first=false; out << json_string(event);
    }
    out << "]}";
    return out.str();
}
}
