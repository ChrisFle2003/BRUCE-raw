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
constexpr std::string_view explicit_stage_header="BRUCE_GRID_BUNDLE_V2\n";
constexpr std::string_view engine=GridSpec::engine;
constexpr std::string_view policy=GridSpec::policy;
std::uint64_t scaled_exponential_v1(std::uint64_t stage,unsigned max_bits) {
    const auto exponential=power(2,stage,max_bits);
    if(exponential<1) throw Error("IntegerBitLimit");
    const auto limit=max_bits==64 ? UINT64_MAX : (std::uint64_t{1}<<max_bits)-1;
    const __uint128_t value=__uint128_t(exponential-1)*100;
    if(value>limit) throw Error("IntegerBitLimit");
    return static_cast<std::uint64_t>(value);
}
std::uint64_t modular_scaled_exponential_v1(std::uint64_t stage,std::uint64_t modulus) {
    const auto exponential=modular_power(2,stage,modulus);
    const auto difference=exponential ? exponential-1 : modulus-1;
    return static_cast<std::uint64_t>((__uint128_t(100%modulus)*difference)%modulus);
}
std::string modulus_text(const GridTask& t) {return t.modulus ? std::to_string(*t.modulus) : "none";}
std::string payload(const GridRun& run) {
    const auto& t=run.task;
    std::string out="engine "+std::string(engine)+"\nrule "+t.rule+"\npolicy "+std::string(policy)+
        "\npath "+t.path+"\nmode "+t.mode+"\nmax_bits "+std::to_string(t.max_bits)+
        "\nmodulus "+modulus_text(t)+"\nzero_zero "+t.zero_zero+
        (t.stage ? "\nstage_source explicit\nstage "+std::to_string(*t.stage) : "")+"\nTRACE\n";
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
    if(!run.stopped) {
        const auto bits=run.task.mode=="metric" ? 64 : run.task.max_bits;
        if(run.task.rule==GridSpec::leaf_local_power_rule)
            run.value=run.task.mode=="modular" ? modular_power(leaf.x,leaf.y,*run.task.modulus) : power(leaf.x,leaf.y,bits);
        else
            run.value=run.task.mode=="modular" ? modular_scaled_exponential_v1(run.task.stage.value_or(leaf.y),*run.task.modulus) : scaled_exponential_v1(run.task.stage.value_or(leaf.y),bits);
    }
    run.trace={"PATH_VALIDATED depth="+std::to_string(run.path.steps.size()),
        "LEAF x="+std::to_string(leaf.x)+" y="+std::to_string(leaf.y),
        "CONFIG mode="+run.task.mode+" max_bits="+std::to_string(run.task.max_bits)+
            " modulus="+modulus_text(run.task)+" zero_zero="+run.task.zero_zero,
        run.stopped ? "STOP GridSentinel scope=BRANCH evaluations=0" : "EVALUATED evaluations=1"};
    if(run.task.rule==GridSpec::scaled_exponential_rule)
        run.trace.insert(run.trace.end()-1,"RULE rule="+run.task.rule+
            (run.task.stage ? " stage_source=explicit" : "")+" stage="+std::to_string(run.task.stage.value_or(leaf.y))+
            " base=2 scale=100 formula=100*(2^stage-1)");
    return run;
}
std::string render_grid(const GridRun& run) {
    (void)grid_spec(run.task);
    std::ostringstream out;
    out<<"{\"rule\":"<<json_string(run.task.rule)<<",\"path\":"<<json_string(run.task.path)
       <<",\"depth\":"<<run.path.steps.size()<<",\"theoretical_space\":"<<json_string("64^"+std::to_string(run.path.steps.size()))
       <<",\"evaluations\":"<<(run.stopped ? 0 : 1)<<",\"mode\":"<<json_string(run.task.mode)
       <<",\"status\":"<<json_string(run.stopped ? "STOPPED" : "EVALUATED")
       <<",\"stop_scope\":"<<json_string(run.stopped ? "BRANCH" : "NONE")
       <<",\"stop_reason\":"<<json_string(run.stopped ? "GridSentinel" : "Completed");
    if(run.task.rule==GridSpec::scaled_exponential_rule) {
        if(run.task.stage) out<<",\"stage_source\":\"explicit\"";
        out<<",\"stage\":"<<run.task.stage.value_or(run.path.steps.back().y)<<",\"base\":2,\"scale\":100,\"formula\":\"100*(2^stage-1)\"";
    }
    if(!run.stopped && run.task.mode=="metric") out<<",\"bit_length\":"<<std::bit_width(run.value)<<",\"metric_exact\":true";
    else if(!run.stopped) out<<",\"value\":"<<json_string(std::to_string(run.value));
    return out.str()+"}";
}
std::string pack_grid(const GridRun& run,const ReplayCheckpoint& checkpoint) {
    const auto body=payload(run);
    const auto bundle=std::string(run.task.stage ? explicit_stage_header : header)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
GridRun replay_grid(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    const bool explicit_stage=bundle.starts_with(explicit_stage_header);
    if(!explicit_stage && !bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(explicit_stage ? explicit_stage_header.size() : header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body,checkpoint)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    std::istringstream input{std::string(body)};
    if(field(input,"engine")!=engine) throw Error("UnknownRuleOrEngineVersion");
    GridTask task;task.rule=field(input,"rule");
    if(field(input,"policy")!=policy) throw Error("UnknownRuleOrEngineVersion");
    task.path=field(input,"path");task.mode=field(input,"mode");
    task.max_bits=integer<unsigned>(field(input,"max_bits"));
    const auto modulus=field(input,"modulus");
    if(modulus!="none") task.modulus=integer<std::uint64_t>(modulus);
    task.zero_zero=field(input,"zero_zero");
    if(explicit_stage) {
        std::string line;
        if(!std::getline(input,line) || line!="stage_source explicit") throw Error("InvalidBundleField: stage_source");
        if(!std::getline(input,line) || !line.starts_with("stage ")) throw Error("InvalidBundleField: stage");
        const auto text=std::string_view(line).substr(6);
        std::uint64_t stage;
        try {stage=integer<std::uint64_t>(text);} catch(const Error&) {throw Error("InvalidBundleField: stage");}
        if(text!=std::to_string(stage)) throw Error("InvalidBundleField: stage");
        task.stage=stage;
    }
    if(checkpoint) checkpoint();
    auto run=evaluate_grid(std::move(task));
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid task, result or trace differs");
    return run;
}
}
