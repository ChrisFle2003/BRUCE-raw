#include "bruce/application/compatibility_transfer.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/core/json.hpp"
#include <sstream>
namespace bruce {
namespace {
bool valid_digest(const std::string& value){if(value.size()!=64)return false;for(char c:value)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;return true;}
}
void CompatibilityTransfer::validate() const {
    if(max_source_>max_file_bytes || !budget_.candidates || budget_.candidates>4096 || !budget_.checks || budget_.checks>1048576 || !budget_.trace || budget_.trace>65536 || budget_.cancel_after_candidates)throw Error("CompatibilityTransferInvalidBudget");
}
CompatibilityTransfer CompatibilityTransfer::plan(const std::string& source,const std::string& rules,std::int64_t at,ZBudget budget,std::size_t max_source,const ReplayCheckpoint& checkpoint){
    if(checkpoint)checkpoint();
    CompatibilityTransfer transfer;transfer.at_=at;transfer.budget_=budget;transfer.max_source_=max_source;transfer.validate();
    if(source.size()>max_source)throw Error("CompatibilityTransferByteLimit");
    transfer.source_digest_=sha256(source,checkpoint);transfer.rules_digest_=sha256(CompatibilitySnapshot(rules,checkpoint).canonical(checkpoint),checkpoint);
    transfer.apply(source,rules,checkpoint); // Full native source replay and selection; no file mutation.
    return transfer;
}
std::string CompatibilityTransfer::bytes() const {
    const auto body="BRUCE_COMPATIBILITY_TRANSFER_V1\n"+source_digest_+"\n"+rules_digest_+"\n"+std::to_string(at_)+"\n"+std::to_string(max_source_)+"\n"+std::to_string(budget_.candidates)+"\n"+std::to_string(budget_.checks)+"\n"+std::to_string(budget_.trace)+"\n";
    return sha256(body)+"\n"+body;
}
CompatibilityTransfer::CompatibilityTransfer(const std::string& bytes){
    if(bytes.size()>4096)throw Error("CompatibilityTransferPlanByteLimit");
    const auto split=bytes.find('\n');
    if(split==std::string::npos || bytes.substr(0,split)!=sha256(bytes.substr(split+1)))throw Error("CompatibilityTransferDigestMismatch");
    std::istringstream in(bytes.substr(split+1));std::string version;std::getline(in,version);
    if(version!="BRUCE_COMPATIBILITY_TRANSFER_V1" || !(in>>source_digest_>>rules_digest_>>at_>>max_source_>>budget_.candidates>>budget_.checks>>budget_.trace))throw Error("InvalidCompatibilityTransfer");
    in>>std::ws;
    if(!in.eof() || !valid_digest(source_digest_) || !valid_digest(rules_digest_) || this->bytes()!=bytes)throw Error("InvalidCompatibilityTransfer");
    validate();
}
CompatibilityRun CompatibilityTransfer::apply(const std::string& source,const std::string& rules,const ReplayCheckpoint& checkpoint) const {
    if(checkpoint)checkpoint();
    if(source.size()>max_source_)throw Error("CompatibilityTransferByteLimit");
    if(sha256(source,checkpoint)!=source_digest_)throw Error("CompatibilityTransferSourceChanged");
    const auto canonical=CompatibilitySnapshot(rules,checkpoint).canonical(checkpoint);
    if(sha256(canonical,checkpoint)!=rules_digest_)throw Error("CompatibilityTransferRulesChanged");
    CompatibilityTask task;task.origin=source;task.rules=canonical;task.at=at_;task.budget=budget_;
    ReplayCheckpoint guarded;
    if(checkpoint)guarded=[&]{(void)detail::observe_replay_checkpoint(checkpoint);};
    try {
        return run_compatibility(std::move(task),guarded ? std::function<bool(std::uint64_t)>{[&](auto){guarded();return false;}}:std::function<bool(std::uint64_t)>{},guarded,guarded);
    } catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
}
std::string CompatibilityTransfer::report() const {
    return "{\"contract\":\"compatibility-transfer-v1\",\"source_digest\":"+json_string(source_digest_)+",\"rules_digest\":"+json_string(rules_digest_)+",\"at\":"+json_string(std::to_string(at_))+",\"max_source_bytes\":"+std::to_string(max_source_)+",\"max_candidates\":"+std::to_string(budget_.candidates)+",\"max_checks\":"+std::to_string(budget_.checks)+",\"max_trace\":"+std::to_string(budget_.trace)+"}";
}
std::string CompatibilityTransfer::result_report(const CompatibilityRun& run) const {
    return "{\"contract\":\"compatibility-transfer-result-v1\",\"plan_digest\":"+json_string(sha256(bytes()))+",\"source_digest\":"+json_string(source_digest_)+",\"rules_digest\":"+json_string(rules_digest_)+",\"result\":"+render_compatibility(run)+"}";
}
}
