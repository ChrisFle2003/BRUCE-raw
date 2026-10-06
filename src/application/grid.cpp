#include "bruce/application/grid.hpp"
#include "bruce/application/grid_spec.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/math/exact.hpp"
#include <bit>
#include <sstream>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_GRID_BUNDLE_V1\n";
constexpr std::string_view engine=GridSpec::engine;
constexpr std::string_view rule=GridSpec::value_rule;
constexpr std::string_view policy=GridSpec::policy;
std::string modulus_text(const GridTask& t) {return t.modulus ? std::to_string(*t.modulus) : "none";}
std::string payload(const GridRun& run) {
    const auto& t=run.task;
    std::string out="engine "+std::string(engine)+"\nrule "+std::string(rule)+"\npolicy "+std::string(policy)+
        "\npath "+t.path+"\nmode "+t.mode+"\nmax_bits "+std::to_string(t.max_bits)+
        "\nmodulus "+modulus_text(t)+"\nzero_zero "+t.zero_zero+"\nTRACE\n";
    for(const auto& event:run.trace) out+=event+'\n';
    return out+"RESULT\n"+render_grid(run)+"\n";
}
std::string field(std::istream& input,std::string_view name) {
    std::string line,key,value,extra;
    if(!std::getline(input,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
}
GridRun evaluate_grid(GridTask task) {
    if(task.path.size()>max_file_bytes) throw Error("GridInputByteLimit");
    auto path=parse_path(task.path);
    (void)grid_spec(task);
    const auto leaf=path.steps.back();
    GridRun run;run.task=std::move(task);run.path=std::move(path);
    run.stopped=leaf.x==0 && leaf.y==0 && run.task.zero_zero=="stop";
    if(!run.stopped) run.value=run.task.mode=="modular" ? modular_power(leaf.x,leaf.y,*run.task.modulus) :
        power(leaf.x,leaf.y,run.task.mode=="metric" ? 64 : run.task.max_bits);
    run.trace={"PATH_VALIDATED depth="+std::to_string(run.path.steps.size()),
        "LEAF x="+std::to_string(leaf.x)+" y="+std::to_string(leaf.y),
        "CONFIG mode="+run.task.mode+" max_bits="+std::to_string(run.task.max_bits)+
            " modulus="+modulus_text(run.task)+" zero_zero="+run.task.zero_zero,
        run.stopped ? "STOP GridSentinel scope=BRANCH evaluations=0" : "EVALUATED evaluations=1"};
    return run;
}
std::string render_grid(const GridRun& run) {
    std::ostringstream out;
    out<<"{\"rule\":\""<<rule<<"\",\"path\":"<<json_string(run.task.path)
       <<",\"depth\":"<<run.path.steps.size()<<",\"theoretical_space\":"<<json_string("64^"+std::to_string(run.path.steps.size()))
       <<",\"evaluations\":"<<(run.stopped ? 0 : 1)<<",\"mode\":"<<json_string(run.task.mode)
       <<",\"status\":"<<json_string(run.stopped ? "STOPPED" : "EVALUATED")
       <<",\"stop_scope\":"<<json_string(run.stopped ? "BRANCH" : "NONE")
       <<",\"stop_reason\":"<<json_string(run.stopped ? "GridSentinel" : "Completed");
    if(!run.stopped && run.task.mode=="metric") out<<",\"bit_length\":"<<std::bit_width(run.value)<<",\"metric_exact\":true";
    else if(!run.stopped) out<<",\"value\":"<<json_string(std::to_string(run.value));
    return out.str()+"}";
}
std::string pack_grid(const GridRun& run,const ReplayCheckpoint& checkpoint) {
    const auto body=payload(run);
    const auto bundle=std::string(header)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
GridRun replay_grid(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body,checkpoint)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    std::istringstream input{std::string(body)};
    if(field(input,"engine")!=engine || field(input,"rule")!=rule || field(input,"policy")!=policy)
        throw Error("UnknownRuleOrEngineVersion");
    GridTask task;task.path=field(input,"path");task.mode=field(input,"mode");
    task.max_bits=integer<unsigned>(field(input,"max_bits"));
    const auto modulus=field(input,"modulus");
    if(modulus!="none") task.modulus=integer<std::uint64_t>(modulus);
    task.zero_zero=field(input,"zero_zero");
    if(checkpoint) checkpoint();
    auto run=evaluate_grid(std::move(task));
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid task, result or trace differs");
    return run;
}
}
