#include "bruce/application/rule_proposal.hpp"
#include "bruce/compatibility/snapshot.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iomanip>
#include <sstream>
#include <utility>
namespace bruce {
RuleProposal::RuleProposal(std::string bytes,const ReplayCheckpoint& checkpoint):bytes_(std::move(bytes)) {
    if(checkpoint)checkpoint();
    if(bytes_.size()>3*1024*1024)throw Error("InputByteLimit");
    const auto split=bytes_.find('\n');
    if(split==std::string::npos || sha256(bytes_.substr(split+1),checkpoint)!=bytes_.substr(0,split))throw Error("RuleProposalDigestMismatch");
    digest_=bytes_.substr(0,split);
    std::istringstream input(bytes_.substr(split+1));
    std::string header;
    std::getline(input,header);
    if(header!="BRUCE_RULE_PROPOSAL_V1" || !(input>>std::quoted(previous_)>>std::quoted(proposed_)))throw Error("InvalidRuleProposal");
    input>>std::ws;
    if(!input.eof() || CompatibilitySnapshot(previous_,checkpoint).canonical(checkpoint)!=previous_ || CompatibilitySnapshot(proposed_,checkpoint).canonical(checkpoint)!=proposed_ || previous_==proposed_)throw Error("InvalidRuleProposal");
}
RuleProposal RuleProposal::create(std::string_view previous,std::string_view proposed) {
    const auto old=CompatibilitySnapshot(previous).canonical(),next=CompatibilitySnapshot(proposed).canonical();
    if(old==next)throw Error("RuleProposalUnchanged");
    std::ostringstream body;
    body<<"BRUCE_RULE_PROPOSAL_V1\n"<<std::quoted(old)<<'\n'<<std::quoted(next)<<'\n';
    const auto payload=body.str();
    return RuleProposal(sha256(payload)+"\n"+payload);
}
void RuleProposal::check_current(std::string_view current,const ReplayCheckpoint& checkpoint) const {
    if(CompatibilitySnapshot(current,checkpoint).canonical(checkpoint)!=previous_)throw Error("RuleBaseChanged");
}
std::string RuleProposal::confirmation(std::string_view output_path) const {
    return sha256(bytes_+"\nOUTPUT\n"+std::string(output_path));
}
}
