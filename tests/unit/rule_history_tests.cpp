#include "bruce/application/rule_history.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
int main(){
    using namespace bruce;
    auto rejects=[](const std::function<void()>& action,std::string_view error){try{action();}catch(const Error& e){return e.what()==error;}return false;};
    RuleHistory current("/store","");
    const std::string a="BRUCE_RULES_V1\nsnapshot a\n",b="BRUCE_RULES_V1\nsnapshot b\n";
    const auto initial=current.stage(a);
    if(!rejects([&]{current.append(initial,"no",1);},"RuleApprovalDeclined"))return 1;
    auto bytes=current.append(initial,"CONFIRM "+initial.confirmation(),1);
    RuleHistory next("/store",bytes);
    if(next.active()!=a)return 2;
    if(!rejects([&]{RuleHistory wrong("/other",bytes);},"RuleStoreTargetMismatch"))return 3;
    if(!rejects([&]{next.check(initial);},"RuleStoreBaseChanged"))return 4;
    if(!rejects([&]{RuleHistory bad("/store",bytes+"x");},"RuleHistoryDigestMismatch"))return 5;
    for(unsigned i=1;i<128;++i){const auto stage=next.stage(i%2 ? b:a);bytes=next.append(stage,"CONFIRM "+stage.confirmation(),i+1);next=RuleHistory("/store",bytes);}
    std::uint64_t points=0;
    RuleHistory controlled("/store",bytes,[&]{++points;});
    if(controlled.report()!=next.report() || points<128)return 8;
    for(const auto stop:{std::uint64_t(1),points/2,points}) {
        std::uint64_t seen=0;
        if(!rejects([&]{RuleHistory stopped("/store",bytes,[&]{if(++seen==stop)throw Error("HistoryStop");});},"HistoryStop") || seen!=stop)return 9;
    }
    if(!rejects([&]{RuleHistory damaged("/store",bytes+"x",[]{});},"RuleHistoryDigestMismatch"))return 10;
    const auto full=next.stage(a);
    if(!rejects([&]{next.append(full,"CONFIRM "+full.confirmation(),129);},"RuleHistoryDecisionLimit"))return 6;
    if(!rejects([&]{RuleHistory huge("/store",std::string(rule_history_limit+1,'x'));},"RuleHistoryByteLimit"))return 7;
}
