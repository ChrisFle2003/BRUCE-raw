#pragma once
#include "bruce/application/rule_history.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/core/json.hpp"
#include "management_inspection_cli.hpp"
#include <iostream>
#include <sstream>
#include <unistd.h>
namespace bruce {
inline std::string active_rule_snapshot(const std::string& path,const ReplayCheckpoint& checkpoint={}){
    return LocalFiles::active_rules(path,checkpoint);
}
inline int rule_store_cli(int argc,char** argv){
    const std::string command=argv[1];
    if(command=="rules-status"){
        if(argc<3)throw Error("Usage: rules-status STORE");
        auto management=parse_management_inspection_options(argc,argv,3,"Usage: rules-status STORE");
        const auto checkpoint=management.checkpoint();
        if(checkpoint)checkpoint();
        const auto path=LocalFiles::rule_target(argv[2]);
        const auto report=LocalFiles::load_rules(path,false,checkpoint).report();
        std::cout<<management.render(report)<<'\n';return 0;
    }
    if(command=="rules-stage"){
        if(argc!=5)throw Error("Usage: rules-stage STORE NEW PROPOSAL");
        const auto path=LocalFiles::rule_target(argv[2]);
        const auto item=LocalFiles::load_rules(path,true).stage(LocalFiles::read(argv[3],1024*1024));
        LocalFiles::publish_new(argv[4],item.bytes());std::cout<<"{\"staged\":true,\"approved\":false}\n";return 0;
    }
    if(argc!=4)throw Error("Usage: rules-activate STORE PROPOSAL");
    const auto path=LocalFiles::rule_target(argv[2]);
    const auto item=RuleStage::parse(LocalFiles::read(argv[3],rule_history_limit));
    LocalFiles::load_rules(path,true).check(item);
    if(!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO))throw Error("RuleApprovalRequiresTerminal");
    auto show=[](const std::string& text,char prefix){std::istringstream in(text);std::string line;while(std::getline(in,line))std::cout<<prefix<<' '<<line<<'\n';};
    show(item.proposal.previous(),'-');show(item.proposal.proposed(),'+');
    std::cout<<"Aktiver Regelspeicher: "<<json_string(path)<<"\nGeneration: "<<item.generation<<" -> "<<item.generation+1<<"\nCONFIRM "<<item.confirmation()<<"\n> "<<std::flush;
    std::string answer;if(!std::getline(std::cin,answer) || answer!="CONFIRM "+item.confirmation())throw Error("RuleApprovalDeclined");
    std::cout<<LocalFiles::activate_rules(path,item,answer).report()<<'\n';return 0;
}
}
