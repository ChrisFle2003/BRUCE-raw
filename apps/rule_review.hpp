#pragma once
// Local interactive CLI adapter; this is not an identity or access-control boundary.
#include "bruce/application/rule_proposal.hpp"
#include "bruce/compatibility/snapshot.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/core/json.hpp"
#include <iostream>
#include <sstream>
#include <unistd.h>
namespace bruce {
inline int rule_review(int argc,char** argv) {
    const std::string command=argv[1];
    if(argc!=5) throw Error("Usage: rules-propose OLD NEW PROPOSAL | rules-approve PROPOSAL CURRENT OUTPUT");
    auto canonical=[](const char* path){return CompatibilitySnapshot(read_bounded(path,1024*1024)).canonical();};
    if(command=="rules-propose") {
        const auto old=canonical(argv[2]),next=canonical(argv[3]);
        const auto proposal=RuleProposal::create(old,next);
        export_exclusive(argv[4],proposal.bytes());
        std::cout<<"{\"proposal\":"<<json_string(proposal.digest())<<",\"approved\":false}\n";
        return 0;
    }
    const RuleProposal proposal(read_bounded(argv[2],3*1024*1024));
    proposal.check_current(canonical(argv[3]));
    if(!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) throw Error("RuleApprovalRequiresTerminal");
    // Confirmation binds both canonical snapshots and the displayed output pathname.
    const auto token=proposal.confirmation(argv[4]);
    std::cout<<"Lokale Regelpruefung: vollstaendige alte (-) und neue (+) Fassung\n";
    auto show=[](const std::string& text,char prefix){std::istringstream lines(text); std::string line; while(std::getline(lines,line)) std::cout<<prefix<<' '<<line<<'\n';};
    show(proposal.previous(),'-'); show(proposal.proposed(),'+');
    std::cout<<"Ausgabedatei: "<<json_string(argv[4])<<"\nKeine automatische Aktivierung. Bestaetigung exakt eingeben:\nCONFIRM "<<token<<"\n> "<<std::flush;
    std::string answer;
    if(!std::getline(std::cin,answer) || answer!="CONFIRM "+token) throw Error("RuleApprovalDeclined");
    proposal.check_current(canonical(argv[3]));
    export_exclusive(argv[4],proposal.proposed());
    std::cout<<"{\"approved_export\":true,\"confirmation\":"<<json_string(token)<<",\"proposal\":"<<json_string(proposal.digest())<<"}\n";
    return 0;
}
}
