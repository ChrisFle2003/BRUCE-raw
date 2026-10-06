#include "bruce/application/rule_history.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/core/json.hpp"
#include <iomanip>
#include <sstream>
namespace bruce {
namespace {
std::string wrap(const std::string& body,const ReplayCheckpoint& checkpoint={}){return sha256(body,checkpoint)+"\n"+body;}
std::string unwrap(const std::string& bytes,const ReplayCheckpoint& checkpoint={}){
    if(bytes.size()>rule_history_limit)throw Error("RuleHistoryByteLimit");
    const auto split=bytes.find('\n');
    if(split==std::string::npos || bytes.substr(0,split)!=sha256(bytes.substr(split+1),checkpoint))throw Error("RuleHistoryDigestMismatch");
    return bytes.substr(split+1);
}
std::string record(const std::string& stage,std::uint64_t time){std::ostringstream out;out<<std::quoted(stage)<<' '<<time<<'\n';return out.str();}
}
std::string RuleStage::bytes() const {
    std::ostringstream out;out<<"BRUCE_RULE_STAGE_V1\n"<<std::quoted(target)<<'\n'<<std::quoted(base)<<'\n'<<generation<<'\n'<<std::quoted(proposal.bytes())<<'\n';return wrap(out.str());
}
std::string RuleStage::confirmation() const {return sha256("BRUCE_ACTIVATE_V1\n"+bytes());}
RuleStage RuleStage::parse(const std::string& bytes,const ReplayCheckpoint& checkpoint){
    if(checkpoint)checkpoint();
    std::istringstream in(unwrap(bytes,checkpoint));std::string version,target,base,proposal;std::uint64_t generation;
    std::getline(in,version);
    if(version!="BRUCE_RULE_STAGE_V1" || !(in>>std::quoted(target)>>std::quoted(base)>>generation>>std::quoted(proposal)))throw Error("InvalidRuleStage");
    in>>std::ws;if(!in.eof())throw Error("InvalidRuleStage");
    RuleStage result{target,base,generation,RuleProposal(proposal,checkpoint)};
    if(result.bytes()!=bytes)throw Error("InvalidRuleStage");
    return result;
}
RuleHistory::RuleHistory(std::string target,const std::string& bytes,const ReplayCheckpoint& checkpoint):target_(std::move(target)){
    if(checkpoint)checkpoint();
    head_=sha256("BRUCE_RULE_HISTORY_V1\n"+target_,checkpoint);
    if(bytes.empty())return;
    std::istringstream in(unwrap(bytes,checkpoint));std::string version,stored_target;std::getline(in,version);
    if(version!="BRUCE_RULE_HISTORY_V1" || !(in>>std::quoted(stored_target)) || stored_target!=target_)throw Error("RuleStoreTargetMismatch");
    while(true){
        in>>std::ws;if(in.eof())break;
        if(checkpoint)checkpoint();
        std::string stage_bytes;std::uint64_t time;
        if(!(in>>std::quoted(stage_bytes)>>time))throw Error("InvalidRuleHistory");
        const auto item=RuleStage::parse(stage_bytes,checkpoint);check(item,checkpoint);
        decisions_.emplace_back(stage_bytes,time);active_=item.proposal.proposed();head_=sha256(head_+"\n"+record(stage_bytes,time),checkpoint);
    }
    if(decisions_.empty() || serialize(checkpoint)!=bytes)throw Error("InvalidRuleHistory");
}
RuleStage RuleHistory::stage(std::string_view next) const {return {target_,head_,decisions_.size(),RuleProposal::create(active_,next)};}
void RuleHistory::check(const RuleStage& item,const ReplayCheckpoint& checkpoint) const {
    if(item.target!=target_)throw Error("RuleStoreTargetMismatch");
    if(item.base!=head_ || item.generation!=decisions_.size())throw Error("RuleStoreBaseChanged");
    item.proposal.check_current(active_,checkpoint);
    if(decisions_.size()>=128)throw Error("RuleHistoryDecisionLimit");
}
std::string RuleHistory::serialize(const ReplayCheckpoint& checkpoint) const {
    std::ostringstream out;out<<"BRUCE_RULE_HISTORY_V1\n"<<std::quoted(target_)<<'\n';
    for(const auto& [stage,time]:decisions_){if(checkpoint)checkpoint();out<<record(stage,time);}
    auto bytes=wrap(out.str(),checkpoint);if(bytes.size()>rule_history_limit)throw Error("RuleHistoryByteLimit");return bytes;
}
std::string RuleHistory::append(const RuleStage& item,std::string_view confirmation,std::uint64_t time) const {
    check(item);if(confirmation!="CONFIRM "+item.confirmation())throw Error("RuleApprovalDeclined");
    auto next=*this;next.decisions_.emplace_back(item.bytes(),time);return next.serialize();
}
std::string RuleHistory::report() const {
    std::ostringstream out;out<<"{\"contract\":\"rule-history-v1\",\"target\":"<<json_string(target_)<<",\"generation\":"<<decisions_.size()<<",\"head\":"<<json_string(head_)<<",\"active_digest\":"<<json_string(sha256(active_))<<",\"active_rules\":"<<json_string(active_)<<",\"decisions\":[";
    bool first=true;for(const auto& [bytes,time]:decisions_){const auto item=RuleStage::parse(bytes);if(!first)out<<',';first=false;out<<"{\"generation\":"<<item.generation+1<<",\"unix_time\":"<<time<<",\"decision\":\"approved\",\"proposal\":"<<json_string(item.proposal.digest())<<",\"confirmation\":"<<json_string(item.confirmation())<<",\"previous_digest\":"<<json_string(sha256(item.proposal.previous()))<<",\"active_digest\":"<<json_string(sha256(item.proposal.proposed()))<<",\"previous_rules\":"<<json_string(item.proposal.previous())<<",\"proposed_rules\":"<<json_string(item.proposal.proposed())<<'}';}
    out<<"]}";return out.str();
}
}
