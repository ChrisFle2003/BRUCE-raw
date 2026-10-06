#include "bruce/application/rule_proposal.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
#include "bruce/compatibility/snapshot.hpp"
#include <iostream>
int main() {
    using namespace bruce;
    const std::string old="BRUCE_RULES_V1\nsnapshot old\n", next="BRUCE_RULES_V1\nsnapshot new\n";
    auto rejects=[](const std::function<void()>& action,std::string_view expected){
        try {action();} catch(const Error& e) {return std::string(e.what())==expected;}
        return false;
    };
    const auto proposal=RuleProposal::create(old,next);
    const std::string payload="BRUCE_RULE_PROPOSAL_V1\n\""+old+"\"\n\""+next+"\"\n";
    if(proposal.bytes()!=sha256(payload)+"\n"+payload)return 1;
    const RuleProposal parsed(proposal.bytes());
    if(parsed.previous()!=old || parsed.proposed()!=next || parsed.digest()!=sha256(payload))return 2;
    parsed.check_current("BRUCE_RULES_V1\nsnapshot   old\n");
    if(!rejects([&]{parsed.check_current(next);},"RuleBaseChanged"))return 3;
    if(parsed.confirmation("out")!=sha256(proposal.bytes()+"\nOUTPUT\nout"))return 4;
    if(parsed.confirmation("out")==parsed.confirmation("./out"))return 5;
    if(!rejects([&]{RuleProposal p(proposal.bytes()+"x");},"RuleProposalDigestMismatch"))return 6;
    if(!rejects([&]{RuleProposal::create(old,old);},"RuleProposalUnchanged"))return 7;
    const std::string invalid="BRUCE_RULE_PROPOSAL_V1\n\""+old+"\"\n\""+old+"\"\n";
    if(!rejects([&]{RuleProposal p(sha256(invalid)+"\n"+invalid);},"InvalidRuleProposal"))return 8;
    if(!rejects([&]{RuleProposal p(std::string(3*1024*1024+1,'x'));},"InputByteLimit"))return 9;
    std::string many="BRUCE_RULES_V1\nsnapshot many\n";
    for(unsigned i=0;i<128;++i)many+="rule r"+std::to_string(i)+" positive value ge 0 0 1 manual -\n";
    const auto large=RuleProposal::create(old,many);
    std::size_t count=0;
    RuleProposal controlled(large.bytes(),[&]{++count;});
    if(controlled.bytes()!=large.bytes() || controlled.proposed()!=many || count<128)return 10;
    for(const auto stop:{std::size_t(1),count/2,count}) {
        std::size_t seen=0;
        if(!rejects([&]{RuleProposal interrupted(large.bytes(),[&]{if(++seen==stop)throw Error("ProposalStop");});},"ProposalStop") || seen!=stop)return 11;
    }
    count=0;controlled.check_current(old,[&]{++count;});
    if(!count || !rejects([&]{controlled.check_current(old,[]{throw Error("CurrentStop");});},"CurrentStop"))return 12;
    if(!rejects([&]{RuleProposal bad(large.bytes()+"x",[]{});},"RuleProposalDigestMismatch"))return 13;

    count=0;const CompatibilitySnapshot rules(many,[&]{++count;});
    if(count!=129)return 14;
    count=0;if(rules.canonical([&]{++count;})!=many || count!=129)return 15;
    for(std::size_t stop=1;stop<=129;++stop){
        std::size_t seen=0;
        if(!rejects([&]{CompatibilitySnapshot stopped(many,[&]{if(++seen==stop)throw Error("RuleStop");});},"RuleStop") || seen!=stop)return 16;
        seen=0;
        if(!rejects([&]{rules.canonical([&]{if(++seen==stop)throw Error("RuleStop");});},"RuleStop") || seen!=stop)return 17;
    }

}
