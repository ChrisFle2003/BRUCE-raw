#include "bruce/application/grid_stage_sweep.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <sstream>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1\n";
constexpr std::string_view cache_policy="fifo-64-canonical-evidence-bytes-v1";
std::string cache_json(const GridCacheStats& s) {
    return "{\"hits\":"+std::to_string(s.hits)+",\"misses\":"+std::to_string(s.misses)+
        ",\"inserts\":"+std::to_string(s.inserts)+",\"evictions\":"+std::to_string(s.evictions)+
        ",\"bypasses\":"+std::to_string(s.bypasses)+",\"entries\":"+std::to_string(s.entries)+
        ",\"bytes\":"+std::to_string(s.bytes)+",\"peak_bytes\":"+std::to_string(s.peak_bytes)+
        ",\"evaluations\":"+std::to_string(s.evaluations)+"}";
}
std::string payload(const GridStageSweepRun& run) {
    const auto& t=run.task;
    return "spec "+render_grid_spec(run.spec)+"\npath "+t.path+
        "\nfrom_stage "+std::to_string(t.from_stage)+"\nto_stage "+std::to_string(t.to_stage)+
        "\nmode "+t.mode+"\nmax_bits "+std::to_string(t.max_bits)+
        "\nmodulus "+(t.modulus ? std::to_string(*t.modulus) : "none")+
        "\ncache_bytes "+std::to_string(t.cache_bytes)+"\ndebug "+(t.debug ? "1" : "0")+
        "\nRESULT\n"+render_grid_stage_sweep(run)+"\n";
}
std::string field(std::istream& input,std::string_view name) {
    std::string line,key,value,extra;
    if(!std::getline(input,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
GridStageSweepRun evaluate_sweep(GridStageSweepTask task,const ReplayCheckpoint& checkpoint) {
    if(task.from_stage>task.to_stage || task.to_stage>57) throw Error("InvalidStageRange");
    task.path=canonical_grid_path(task.path);
    GridSpec spec;
    spec.rule=GridSpec::scaled_exponential_rule;spec.mode=task.mode;
    spec.max_bits=task.max_bits;spec.modulus=task.modulus;spec.zero_zero="one";
    (void)grid_spec(grid_task(spec,task.path));
    GridCache cache(task.cache_bytes);
    GridStageSweepRun run;run.task=std::move(task);run.spec=std::move(spec);
    for(auto stage=run.task.from_stage;stage<=run.task.to_stage;++stage) {
        if(checkpoint) checkpoint();
        auto query=grid_task(run.spec,run.task.path);query.stage=stage;
        try {
            auto cached=cache.evaluate(std::move(query));
            if(!run.task.debug) cached.run.trace.clear();
            run.records.push_back({run.records.size(),stage,std::move(cached.run),
                std::move(cached.key_digest),std::move(cached.cache_event),"EVALUATED"});
        } catch(const Error& error) {
            if(run.task.mode!="exact" || std::string_view(error.what())!="IntegerBitLimit") throw;
            run.status="INCOMPLETE";run.stop_reason="IntegerBitLimit";run.stopped_stage=stage;break;
        }
    }
    run.cache=cache.stats();return run;
}
}
GridStageSweepRun evaluate_grid_stage_sweep(GridStageSweepTask task) {
    return evaluate_sweep(std::move(task),{});
}
std::string render_grid_stage_sweep(const GridStageSweepRun& run) {
    const auto& t=run.task;
    std::string out="{\"version\":\"grid-stage-sweep-v1\",\"spec\":"+render_grid_spec(run.spec)+
        ",\"path\":"+json_string(t.path)+",\"stage_source\":\"explicit\",\"from_stage\":"+std::to_string(t.from_stage)+
        ",\"to_stage\":"+std::to_string(t.to_stage)+",\"requested_entries\":"+std::to_string(t.to_stage-t.from_stage+1)+
        ",\"processed_entries\":"+std::to_string(run.records.size())+",\"debug\":"+(t.debug ? "true" : "false")+
        ",\"cache_policy\":"+json_string(cache_policy)+",\"cache_byte_limit\":"+std::to_string(t.cache_bytes)+
        ",\"cache\":"+cache_json(run.cache)+",\"status\":"+json_string(run.status)+
        ",\"stop_reason\":"+json_string(run.stop_reason)+",\"stopped_stage\":"+
        (run.stopped_stage ? std::to_string(*run.stopped_stage) : "null")+",\"records\":[";
    bool first=true;
    for(const auto& record:run.records) {
        if(!first) out+=',';
        first=false;
        out+="{\"index\":"+std::to_string(record.index)+",\"stage\":"+std::to_string(record.stage)+
            ",\"stage_source\":\"explicit\",\"cache_key_digest\":"+json_string(record.key_digest)+
            ",\"cache_event\":"+json_string(record.cache_event)+",\"status\":"+json_string(record.status)+
            ",\"result\":"+render_grid(record.run);
        if(t.debug) {
            out+=",\"trace\":[";bool first_event=true;
            for(const auto& event:record.run.trace) {
                if(!first_event) out+=',';
                first_event=false;out+=json_string(event);
            }
            out+=']';
        }
        out+='}';
    }
    return out+"]}";
}
std::string pack_grid_stage_sweep(const GridStageSweepRun& run,const ReplayCheckpoint& checkpoint) {
    const auto body=payload(run);
    const auto bundle=std::string(header)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
GridStageSweepRun replay_grid_stage_sweep(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body,checkpoint)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    std::istringstream input{std::string(body)};
    (void)field(input,"spec"); // The regenerated full payload validates every spec byte.
    GridStageSweepTask task;task.path=field(input,"path");
    task.from_stage=integer<std::uint64_t>(field(input,"from_stage"));
    task.to_stage=integer<std::uint64_t>(field(input,"to_stage"));
    task.mode=field(input,"mode");task.max_bits=integer<unsigned>(field(input,"max_bits"));
    const auto modulus=field(input,"modulus");
    if(modulus!="none") task.modulus=integer<std::uint64_t>(modulus);
    task.cache_bytes=integer<std::size_t>(field(input,"cache_bytes"));
    const auto debug=field(input,"debug");
    if(debug!="0" && debug!="1") throw Error("InvalidBundleField: debug");
    task.debug=debug=="1";
    auto run=evaluate_sweep(std::move(task),checkpoint);
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid stage sweep task, records or diagnostics differ");
    return run;
}
}
