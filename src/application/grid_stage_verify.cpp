#include "bruce/application/grid_stage_verify.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <bit>
#include <sstream>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_GRID_STAGE_VERIFY_BUNDLE_V1\n";
std::string record_json(const GridStageVerifyRecord& record) {
    return "{\"stage\":"+std::to_string(record.stage)+",\"expected\":"+json_string(std::to_string(record.expected))+
        ",\"observed\":"+json_string(std::to_string(record.observed))+",\"matches\":"+(record.matches ? "true" : "false")+"}";
}
std::string payload(const GridStageVerifyRun& run) {
    return "formula "+run.formula+"\nclaim_scope "+run.claim_scope+"\nsweep_digest "+run.sweep_digest+
        "\nsweep_bytes "+std::to_string(run.task.sweep_bundle.size())+"\nSOURCE\n"+run.task.sweep_bundle+
        "\nRESULT\n"+render_grid_stage_verify(run)+"\n";
}
std::string field(std::istream& input,std::string_view name) {
    std::string line,key,value,extra;
    if(!std::getline(input,line)) throw Error("TruncatedBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidBundleField: "+std::string(name));
    return value;
}
GridStageVerifyRun evaluate_verify(GridStageVerifyTask task,const ReplayCheckpoint& checkpoint) {
    GridStageSweepRun source;
    // A control callback keeps its original exception; source errors share one public rejection.
    ReplayCheckpoint guarded;
    if(checkpoint) guarded=[&]{(void)detail::observe_replay_checkpoint(checkpoint);};
    try {source=replay_grid_stage_sweep(task.sweep_bundle,guarded);}
    catch(const detail::ReplayCheckpointRaised& raised) {std::rethrow_exception(raised.original);}
    catch(const Error&) {throw Error("InvalidSweepVerificationSource");}
    if(source.spec.rule!=GridSpec::scaled_exponential_rule || source.status!="COMPLETED" ||
        source.stop_reason!="Completed" || source.stopped_stage || source.task.from_stage!=0 ||
        source.task.to_stage!=57 || source.records.size()!=58 ||
        (source.task.mode!="exact" && source.task.mode!="metric")) throw Error("InvalidSweepVerificationSource");
    for(std::size_t i=0;i<58;++i) {
        const auto& r=source.records[i];
        if(r.index!=i || r.stage!=i || r.run.task.stage!=i || r.run.stopped || r.status!="EVALUATED")
            throw Error("InvalidSweepVerificationSource");
    }
    GridStageVerifyRun run;run.task=std::move(task);run.mode=source.task.mode;
    run.sweep_digest=sha256(run.task.sweep_bundle,checkpoint);
    for(const auto& r:source.records) {
        if(checkpoint) checkpoint();
        // Independent bounded arithmetic: the largest intermediate is below 2^64.
        const std::uint64_t canonical=100*((std::uint64_t{1}<<r.stage)-1);
        const auto expected=run.mode=="metric" ? std::bit_width(canonical) : canonical;
        const auto observed=run.mode=="metric" ? std::bit_width(r.run.value) : r.run.value;
        GridStageVerifyRecord result{r.stage,expected,observed,expected==observed};
        run.records.push_back(result);
        if(!result.matches && !run.first_mismatch) {run.first_mismatch=result;run.status="REFUTED";}
    }
    return run;
}
}
GridStageVerifyRun evaluate_grid_stage_verify(GridStageVerifyTask task) {
    return evaluate_verify(std::move(task),{});
}
std::string render_grid_stage_verify(const GridStageVerifyRun& run) {
    std::string out="{\"version\":\"grid-stage-verify-v1\",\"formula\":"+json_string(run.formula)+
        ",\"claim_scope\":"+json_string(run.claim_scope)+",\"mode\":"+json_string(run.mode)+
        ",\"sweep_digest\":"+json_string(run.sweep_digest)+",\"verified_domain\":"+
        (run.status=="VERIFIED_ON_DOMAIN" ? "\"0..57\"" : "null")+
        ",\"checked_entries\":"+std::to_string(run.records.size())+",\"status\":"+json_string(run.status)+
        ",\"first_mismatch\":"+(run.first_mismatch ? record_json(*run.first_mismatch) : "null")+",\"records\":[";
    bool first=true;
    for(const auto& record:run.records) {
        if(!first) out+=',';
        first=false;out+=record_json(record);
    }
    return out+"]}";
}
std::string pack_grid_stage_verify(const GridStageVerifyRun& run,const ReplayCheckpoint& checkpoint) {
    const auto body=payload(run);
    const auto bundle=std::string(header)+sha256(body,checkpoint)+"\n"+body;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}
GridStageVerifyRun replay_grid_stage_verify(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body,checkpoint)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    std::istringstream input{std::string(body)};
    (void)field(input,"formula");(void)field(input,"claim_scope");(void)field(input,"sweep_digest");
    const auto size=integer<std::size_t>(field(input,"sweep_bytes"));
    std::string marker;
    if(!std::getline(input,marker) || marker!="SOURCE" || size>body.size()) throw Error("ReplayMismatch: invalid embedded sweep");
    GridStageVerifyTask task;task.sweep_bundle.resize(size);
    if(!input.read(task.sweep_bundle.data(),static_cast<std::streamsize>(size))) throw Error("ReplayMismatch: truncated embedded sweep");
    GridStageVerifyRun run;
    try {run=evaluate_verify(std::move(task),checkpoint);}
    catch(const Error& error) {
        if(std::string_view(error.what())=="InvalidSweepVerificationSource") throw Error("ReplayMismatch: invalid embedded sweep");
        throw;
    }
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid stage verification differs");
    return run;
}
}
