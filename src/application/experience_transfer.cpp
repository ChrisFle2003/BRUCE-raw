#include "bruce/application/experience_transfer.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/core/json.hpp"
#include <sstream>
namespace bruce {
namespace {
bool digest_valid(const std::string& value){if(value.size()!=64)return false;for(char c:value)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;return true;}
}
void ExperienceTransfer::check_sizes(const std::string& source,const std::string& previous) const {
    if(max_source_>1048576 || max_previous_>1048576)throw Error("ExperienceTransferInvalidBudget");
    if(source.size()>max_source_ || previous.size()>max_previous_)throw Error("ExperienceTransferByteLimit");
}
ExperienceTransfer ExperienceTransfer::plan(const std::string& source,const std::string& previous,std::size_t max_source,std::size_t max_previous,const ReplayCheckpoint& checkpoint){
    if(checkpoint)checkpoint();
    ExperienceTransfer transfer;transfer.max_source_=max_source;transfer.max_previous_=max_previous;
    transfer.check_sizes(source,previous);
    // Dry-run the complete destination operation, including source audit and store checks.
    execute_experience(ExperienceAddTask{previous,source},checkpoint);
    transfer.source_digest_=sha256(source,checkpoint);transfer.previous_digest_=sha256(previous,checkpoint);return transfer;
}
std::string ExperienceTransfer::bytes() const {
    const auto body="BRUCE_EXPERIENCE_ADD_TRANSFER_V1\n"+source_digest_+"\n"+previous_digest_+"\n"+std::to_string(max_source_)+"\n"+std::to_string(max_previous_)+"\n";
    return sha256(body)+"\n"+body;
}
ExperienceTransfer::ExperienceTransfer(const std::string& bytes){
    if(bytes.size()>4096)throw Error("ExperienceTransferPlanByteLimit");
    const auto split=bytes.find('\n');
    if(split==std::string::npos || sha256(bytes.substr(split+1))!=bytes.substr(0,split))throw Error("ExperienceTransferDigestMismatch");
    std::istringstream in(bytes.substr(split+1));std::string version;std::getline(in,version);
    if(version!="BRUCE_EXPERIENCE_ADD_TRANSFER_V1" || !(in>>source_digest_>>previous_digest_>>max_source_>>max_previous_))throw Error("InvalidExperienceTransfer");
    in>>std::ws;
    if(!in.eof() || !digest_valid(source_digest_) || !digest_valid(previous_digest_) || max_source_>1048576 || max_previous_>1048576 || this->bytes()!=bytes)throw Error("InvalidExperienceTransfer");
}
ExperienceResult ExperienceTransfer::apply(const std::string& source,const std::string& previous,const ReplayCheckpoint& checkpoint) const {
    if(checkpoint)checkpoint();
    check_sizes(source,previous);
    if(sha256(source,checkpoint)!=source_digest_)throw Error("ExperienceTransferSourceChanged");
    if(sha256(previous,checkpoint)!=previous_digest_)throw Error("ExperienceTransferBaseChanged");
    return execute_experience(ExperienceAddTask{previous,source},checkpoint);
}
std::string ExperienceTransfer::report() const {
    return "{\"contract\":\"experience-add-transfer-v1\",\"action\":\"experience-add\",\"source_digest\":"+json_string(source_digest_)+",\"previous_digest\":"+json_string(previous_digest_)+",\"max_source_bytes\":"+std::to_string(max_source_)+",\"max_store_bytes\":"+std::to_string(max_previous_)+"}";
}
std::string ExperienceTransfer::result_report(const ExperienceResult& result) const {
    return "{\"contract\":\"experience-transfer-result-v1\",\"source_digest\":"+json_string(source_digest_)+",\"previous_digest\":"+json_string(previous_digest_)+",\"plan_digest\":"+json_string(sha256(bytes()))+",\"snapshot_digest\":"+json_string(sha256(result.snapshot))+",\"result\":"+result.report+"}";
}
}
