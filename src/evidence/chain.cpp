#include "bruce/evidence/chain.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_EVIDENCE_CHAIN_V1\n";
constexpr std::string_view native_header="BRUCE_EXACT_BUNDLE_V1\n";
constexpr std::size_t csv_limit=1024*1024;
constexpr std::size_t block_size=64*1024;

void require_route(const ExactResearchTask& task) {
    if(task.strategy!="affine" || task.planner!="enumerate" ||
       (task.domain!="bigint" && task.domain!="rational"))
        throw Error("UnsupportedEvidenceChainRoute");
}
bool same_samples(const std::vector<ExactSample>& left,const std::vector<ExactSample>& right,
                  const ReplayCheckpoint& checkpoint={}) {
    if(left.size()!=right.size()) return false;
    for(std::size_t i=0;i<left.size();++i) {
        if(checkpoint) checkpoint();
        if(left[i].x!=right[i].x || left[i].y!=right[i].y) return false;
    }
    return true;
}
void bind_csv(ExactResearchTask& task,const std::string& training,const std::string& holdout,
              const ReplayCheckpoint& checkpoint={}) {
    require_route(task);
    if(training.size()>csv_limit || holdout.size()>csv_limit) throw Error("InputByteLimit");
    const NumberDomain domain(task.domain,task.max_bits);
    auto train=parse_exact_samples(training,domain);
    auto test=parse_exact_samples(holdout,domain);
    if((!task.training.empty() && !same_samples(task.training,train,checkpoint)) ||
       (!task.holdout.empty() && !same_samples(task.holdout,test,checkpoint)))
        throw Error("EvidenceChainSampleBindingMismatch");
    task.training=std::move(train);
    task.holdout=std::move(test);
}
void verify_binding(const ExactEvidenceChainRun& run,const ReplayCheckpoint& checkpoint={}) {
    auto task=run.research.task;
    bind_csv(task,run.training_csv,run.holdout_csv,checkpoint);
    if(!same_samples(task.training,run.research.task.training,checkpoint) ||
       !same_samples(task.holdout,run.research.task.holdout,checkpoint))
        throw Error("EvidenceChainSampleBindingMismatch");
}
void append_chunked(std::string& out,std::string_view value,const ReplayCheckpoint& checkpoint) {
    for(std::size_t offset=0;offset<value.size();) {
        if(checkpoint) checkpoint();
        const auto count=std::min(block_size,value.size()-offset);
        out.append(value.substr(offset,count));
        offset+=count;
    }
}
std::string body(const ExactEvidenceChainRun& run,std::string_view native,
                 const ReplayCheckpoint& checkpoint={}) {
    std::string out="contract affine-evidence-chain-v1\n"
                    "independence declared-disjoint-v1\n"
                    "countercheck_policy affine-inverse-v1\n"
                    "countercheck_scope short-circuit-training-then-holdout-v1\n";
    auto block=[&](std::string_view name,std::string_view value) {
        out+=std::string(name)+" "+std::to_string(value.size())+"\n";
        append_chunked(out,value,checkpoint);
    };
    block("training_csv_bytes",run.training_csv);
    block("holdout_csv_bytes",run.holdout_csv);
    block("native_bundle_bytes",native);
    out+="RESULT\n";
    out+=render_exact_evidence_chain(run);
    out+='\n';
    return out;
}
class Reader {
    std::string_view data_;
public:
    explicit Reader(std::string_view data):data_(data) {}
    std::string field(std::string_view name) {
        const auto end=data_.find('\n');
        const auto prefix=std::string(name)+" ";
        if(end==std::string_view::npos || !data_.substr(0,end).starts_with(prefix))
            throw Error("InvalidEvidenceChainField");
        const auto value=std::string(data_.substr(prefix.size(),end-prefix.size()));
        data_.remove_prefix(end+1);
        return value;
    }
    std::string block(std::string_view name,std::size_t limit) {
        const auto count=integer<std::size_t>(field(name));
        if(count>limit || count>data_.size()) throw Error("InvalidEvidenceChainBlockSize");
        auto value=std::string(data_.substr(0,count));
        data_.remove_prefix(count);
        return value;
    }
};
}

ExactEvidenceChainRun create_exact_evidence_chain(
    ExactResearchTask task,std::string training_csv,std::string holdout_csv,
    const std::function<bool(std::uint64_t)>& cancelled) {
    bind_csv(task,training_csv,holdout_csv);
    ExactEvidenceChainRun run{exact_research(std::move(task),cancelled),
                              std::move(training_csv),std::move(holdout_csv)};
    (void)pack_exact_evidence_chain(run);
    return run;
}

std::string render_exact_evidence_chain(const ExactEvidenceChainRun& run) {
    verify_binding(run);
    return "{\"evidence_contract\":\"affine-evidence-chain-v1\","
           "\"independence\":\"declared-disjoint-v1\","
           "\"countercheck_policy\":\"affine-inverse-v1\","
           "\"countercheck_scope\":\"short-circuit-training-then-holdout-v1\","
           "\"training_samples\":"+std::to_string(run.research.task.training.size())+
           ",\"holdout_samples\":"+std::to_string(run.research.task.holdout.size())+
           ",\"result\":"+render_exact_research(run.research)+"}";
}

std::string pack_exact_evidence_chain(const ExactEvidenceChainRun& run,
                                      const ReplayCheckpoint& checkpoint) {
    verify_binding(run,checkpoint);
    const auto native=pack_exact_research(run.research,checkpoint);
    if(!native.starts_with(native_header)) throw Error("EvidenceChainRequiresExactNativeBundle");
    const auto canonical=replay_exact_research(native,10000000,{},checkpoint);
    if(render_exact_research(canonical)!=render_exact_research(run.research))
        throw Error("EvidenceChainNativeResultMismatch");
    const auto payload=body(run,native,checkpoint);
    const auto bundle=std::string(header)+sha256(payload,checkpoint)+"\n"+payload;
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    return bundle;
}

ExactEvidenceChainRun replay_exact_evidence_chain(std::string_view bundle,
                                                   std::uint64_t operation_limit,
                                                   const ReplayCheckpoint& checkpoint) {
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(header)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(header.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto payload=bundle.substr(65);
    if(sha256(payload)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    Reader reader(payload);
    if(reader.field("contract")!="affine-evidence-chain-v1" ||
       reader.field("independence")!="declared-disjoint-v1" ||
       reader.field("countercheck_policy")!="affine-inverse-v1" ||
       reader.field("countercheck_scope")!="short-circuit-training-then-holdout-v1")
        throw Error("UnknownEvidenceChainPolicy");
    auto training=reader.block("training_csv_bytes",csv_limit);
    auto holdout=reader.block("holdout_csv_bytes",csv_limit);
    const auto native=reader.block("native_bundle_bytes",max_file_bytes);
    if(!native.starts_with(native_header)) throw Error("EvidenceChainRequiresExactNativeBundle");
    auto research=replay_exact_research(native,operation_limit,{},checkpoint);
    ExactEvidenceChainRun run{std::move(research),std::move(training),std::move(holdout)};
    verify_binding(run,checkpoint);
    if(checkpoint) checkpoint();
    if(body(run,native)!=payload) throw Error("ReplayMismatch: evidence chain differs");
    return run;
}
}
