#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <sstream>

namespace bruce {
namespace {
std::string task_text(const Task& task) {
    std::ostringstream out;
    out << "engine " << engine_version << "\nrule " << rule_version << "\npolicy " << policy_version
        << "\nminimum " << task.minimum << "\nmaximum " << task.maximum
        << "\ncandidates " << task.budget.candidates << "\noperations " << task.budget.operations
        << "\ntrace " << task.budget.trace_events << "\nresults " << task.budget.results
        << "\ncancel " << (task.cancel_after_candidates ? std::to_string(*task.cancel_after_candidates) : "none")
        << "\ntraining " << task.training.size() << '\n';
    for(const auto& s:task.training) out << s.x << ' ' << s.y << '\n';
    out << "holdout " << task.holdout.size() << '\n';
    for(const auto& s:task.holdout) out << s.x << ' ' << s.y << '\n';
    return out.str();
}
std::string field(std::istream& stream,std::string_view name) {
    std::string key,value,extra,line;
    if(!std::getline(stream,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
std::vector<Sample> samples(std::istream& stream,std::size_t count,const ReplayCheckpoint& checkpoint) {
    if(count>4096) throw Error("SampleBudgetExceeded");
    std::vector<Sample> result;
    for(std::size_t i=0;i<count;++i) {
        if(checkpoint) checkpoint();
        std::string line,x,y,extra;
        if(!std::getline(stream,line)) throw Error("TruncatedSamples");
        std::istringstream row(line);
        if(!(row>>x>>y) || (row>>extra)) throw Error("InvalidSample");
        result.push_back({integer<std::int64_t>(x),integer<std::int64_t>(y)});
    }
    return result;
}
}

std::string pack_run(const Run& run,const ReplayCheckpoint& checkpoint) {
    const auto payload=task_text(run.task)+"RESULT\n"+render_run(run)+"\n";
    return "BRUCE_BUNDLE_V1\n"+sha256(payload,checkpoint)+"\n"+payload;
}

Run replay(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    constexpr std::string_view prefix="BRUCE_BUNDLE_V1\n";
    if(!bundle.starts_with(prefix)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(prefix.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto digest=bundle.substr(0,64);
    const auto payload=bundle.substr(65);
    if(sha256(payload)!=digest) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    std::istringstream input{std::string(payload)};
    if(field(input,"engine")!=engine_version || field(input,"rule")!=rule_version ||
       field(input,"policy")!=policy_version) throw Error("UnknownRuleOrEngineVersion");
    Task task;
    task.minimum=integer<std::int64_t>(field(input,"minimum"));
    task.maximum=integer<std::int64_t>(field(input,"maximum"));
    task.budget.candidates=integer<std::uint64_t>(field(input,"candidates"));
    task.budget.operations=integer<std::uint64_t>(field(input,"operations"));
    task.budget.trace_events=integer<std::uint64_t>(field(input,"trace"));
    task.budget.results=integer<std::uint64_t>(field(input,"results"));
    const auto cancel=field(input,"cancel");
    if(cancel!="none") task.cancel_after_candidates=integer<std::uint64_t>(cancel);
    task.training=samples(input,integer<std::size_t>(field(input,"training")),checkpoint);
    task.holdout=samples(input,integer<std::size_t>(field(input,"holdout")),checkpoint);
    std::string marker;
    if(!std::getline(input,marker) || marker!="RESULT") throw Error("MissingResult");
    if(checkpoint) checkpoint();
    std::function<bool(std::uint64_t)> observe;
    if(checkpoint) observe=[&](std::uint64_t){return detail::observe_replay_checkpoint(checkpoint);};
    Run run;
    try{run=research(task,observe);}catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
    if(checkpoint) checkpoint();
    const auto expected=task_text(task)+"RESULT\n"+render_run(run)+"\n";
    if(payload!=expected) throw Error("ReplayMismatch: input, result or logical trace differs");
    return run;
}

std::vector<Sample> read_samples(const std::string& path) {
    std::istringstream input(read_bounded(path,1024*1024));
    std::string line;
    if(!std::getline(input,line)) throw Error("MissingSampleHeader");
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="x,y") throw Error("InvalidSampleHeader: expected x,y (input,expected_output)");
    std::vector<Sample> result;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        const auto comma=line.find(',');
        if(comma==std::string::npos) throw Error("InvalidSampleRow: expected x,y");
        result.push_back({integer<std::int64_t>(std::string_view(line).substr(0,comma)),
                          integer<std::int64_t>(std::string_view(line).substr(comma+1))});
        if(result.size()>4096) throw Error("SampleBudgetExceeded");
    }
    if(result.empty()) throw Error("EmptySamples");
    return result;
}
}
