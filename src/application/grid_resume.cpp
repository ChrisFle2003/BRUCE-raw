#include "bruce/application/grid_resume.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <sstream>
#include <algorithm>
namespace bruce {
namespace detail {
std::string render_grid_batch_records(const GridBatchRun&,const ReplayCheckpoint&);
// Precondition: prefix was derived by replaying its complete evidence chain.
GridBatchRun continue_validated_grid_batch(GridBatchRun,std::size_t,const std::function<bool(std::size_t)>&,const ReplayCheckpoint&,const ReplayCheckpoint&);
}
namespace {
constexpr std::string_view header="BRUCE_GRID_RESUME_BUNDLE_V1\n";
constexpr std::string_view policy="verified-prefix-cache-rebuild-v1";
std::string field(std::istream& in,std::string_view name) {
    std::string line,key,value,extra;std::getline(in,line);std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidGridResumeField");
    return value;
}
std::string render_records(const GridResumeRun&,const ReplayCheckpoint&);
std::string payload(const GridResumeRun& r,const ReplayCheckpoint& records={}) {
    if(records) records();
    std::string out="policy "+std::string(policy)+"\nadditional_queries "+std::to_string(r.additional_queries)+
        "\ncancel_after "+(r.batch.task.cancel_after_queries ? std::to_string(*r.batch.task.cancel_after_queries) : "none")+
        "\nparent_bytes "+std::to_string(r.parent.size())+"\n";
    // The record checkpoint above admits the first block. Reserve once so that
    // subsequent appends do not recopy the entire accumulated parent on growth.
    if(r.parent.size()>out.max_size()-out.size())throw Error("BundleByteLimit");
    out.reserve(out.size()+r.parent.size());
    constexpr std::size_t copy_block_bytes=65536;
    for(std::size_t offset=0;offset<r.parent.size();){
        if(offset && records)records();
        const auto count=std::min(copy_block_bytes,r.parent.size()-offset);
        out.append(r.parent,offset,count);
        offset+=count;
    }
    out+="\nTRACE\n";
    for(const auto& event:r.batch.trace) {if(records) records();out+=event+'\n';}
    return out+"RESULT\n"+render_records(r,records)+"\n";
}
GridResumeRun replay_impl(std::string_view,std::size_t,const ReplayCheckpoint&);
GridResumeRun execute(std::string parent,std::size_t additional,const std::function<bool(std::size_t)>& cancelled,std::size_t depth,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& continuation_checkpoint) {
    if(depth>=8) throw Error("GridResumeGenerationLimit");
    if(!additional || additional>max_grid_batch_queries) throw Error("InvalidGridAdditionalQueries");
    if(parent.size()>max_file_bytes) throw Error("BundleByteLimit");
    GridResumeRun out;out.additional_queries=additional;out.generation=1;
    GridBatchRun prefix;
    if(parent.starts_with(header)) {
        auto previous=replay_impl(parent,depth+1,checkpoint);
        out.generation=previous.generation+1;
        out.audit_evaluations=previous.audit_evaluations+previous.new_evaluations;
        prefix=std::move(previous.batch);
    } else {
        prefix=replay_grid_batch(parent,checkpoint);out.audit_evaluations=prefix.cache.evaluations;
    }
    if(prefix.status!="INCOMPLETE") throw Error("GridResumeRequiresIncompleteSource");
    const auto before=prefix.cache.evaluations;out.audit_evaluations+=before;
    out.batch=detail::continue_validated_grid_batch(std::move(prefix),additional,cancelled,continuation_checkpoint,checkpoint);
    out.new_evaluations=out.batch.cache.evaluations-before;out.parent=std::move(parent);
    return out;
}
GridResumeRun replay_impl(std::string_view bundle,std::size_t depth,const ReplayCheckpoint& checkpoint) {
    if(depth>=8) throw Error("GridResumeGenerationLimit");
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    std::istringstream in{std::string(body)};
    if(field(in,"policy")!=policy) throw Error("UnknownGridResumePolicy");
    const auto additional=integer<std::size_t>(field(in,"additional_queries"));
    const auto cancel=field(in,"cancel_after");
    const auto bytes=integer<std::size_t>(field(in,"parent_bytes"));
    if(bytes>body.size()) throw Error("InvalidGridParentSize");
    std::string parent(bytes,'\0');in.read(parent.data(),static_cast<std::streamsize>(bytes));
    if(static_cast<std::size_t>(in.gcount())!=bytes) throw Error("TruncatedBundle");
    std::optional<std::size_t> boundary;
    if(cancel!="none") boundary=integer<std::size_t>(cancel);
    auto run=execute(std::move(parent),additional,[boundary](std::size_t n){return boundary && n>=*boundary;},depth,checkpoint,checkpoint);
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: grid continuation differs");
    return run;
}
}
GridResumeRun resume_grid_batch(std::string parent,std::size_t additional,const std::function<bool(std::size_t)>& cancelled,const ReplayCheckpoint& import_checkpoint) {
    return execute(std::move(parent),additional,cancelled,0,import_checkpoint,{});
}
namespace {
std::string render_records(const GridResumeRun& r,const ReplayCheckpoint& records) {
    return "{\"status\":"+json_string(r.batch.status)+",\"policy\":"+json_string(policy)+
        ",\"generation\":"+std::to_string(r.generation)+",\"audit_evaluations\":"+std::to_string(r.audit_evaluations)+
        ",\"new_evaluations\":"+std::to_string(r.new_evaluations)+",\"batch\":"+detail::render_grid_batch_records(r.batch,records)+"}";
}
}
std::string render_grid_resume(const GridResumeRun& r) {return render_records(r,{});}
std::string pack_grid_resume(const GridResumeRun& r,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    const auto body=payload(r,records);const auto out=std::string(header)+sha256(body,checkpoint)+"\n"+body;
    if(out.size()>max_file_bytes) throw Error("BundleByteLimit");
    return out;
}
GridResumeRun replay_grid_resume(std::string_view bundle,const ReplayCheckpoint& checkpoint) {return replay_impl(bundle,0,checkpoint);}
GridBatchRun replay_grid_source(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    return bundle.starts_with(header) ? replay_grid_resume(bundle,checkpoint).batch : replay_grid_batch(bundle,checkpoint);
}
}
