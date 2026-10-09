#include "bruce/io/local_files.hpp"
#include "bruce/evidence/bundle.hpp"
#include <filesystem>
#include <functional>
#include <iostream>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace bruce;
std::string blocked_unlink_target;
extern "C" int unlink(const char* path){
    if(!blocked_unlink_target.empty() && std::string_view(path).starts_with(blocked_unlink_target+".tmp.") && std::filesystem::exists(blocked_unlink_target)){
        errno=EACCES;
        return -1;
    }
    return ::unlinkat(AT_FDCWD,path,0);
}
void expect_error(const std::function<void()>& action,const std::string& prefix){
    try{action();}catch(const Error& e){if(std::string(e.what()).starts_with(prefix))return;throw;}
    throw Error("ExpectedErrorMissing: "+prefix);
}
void write_failure(const std::function<void()>& action,const std::string& expected){
    const auto child=fork();if(child<0)throw Error("TestForkFailed");
    if(child==0){
        std::signal(SIGXFSZ,SIG_IGN);
        const rlimit limit{512,512};if(setrlimit(RLIMIT_FSIZE,&limit)!=0)_exit(2);
        try{expect_error(action,expected);_exit(0);}catch(...){_exit(3);}
    }
    int status=0;if(waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=0)throw Error("WriteFailureNotObserved");
}
int main(){
    char directory[]="/tmp/bruce-local-files-XXXXXX";if(!mkdtemp(directory))return 1;
    const auto root=std::filesystem::path(directory), output=root/"output", store=root/"store";
    auto clean=[&]{for(const auto& item:std::filesystem::directory_iterator(root))if(item.path().filename().string().find(".tmp.")!=std::string::npos)throw Error("TemporaryFileLeak");};
    try{
        LocalFiles::publish_new(output,"original");
        if(LocalFiles::read(output,8)!="original")throw Error("ReadMismatch");
        clean();
        expect_error([&]{LocalFiles::publish_new(output,"replacement");},"ExportExistsOrUnavailable");
        if(LocalFiles::read(output,8)!="original")throw Error("ExistingExportChanged");
        clean();
        const auto atomic_target=root/"atomic-publication";
        LocalFiles::publish_new_atomically(atomic_target,"retained");
        expect_error([&]{LocalFiles::publish_new_atomically(atomic_target,"replacement");},"ExportExistsOrUnavailable");
        if(LocalFiles::read(atomic_target,8)!="retained")throw Error("AtomicPublicationOverwroteTarget");
        const auto atomic_symlink=root/"atomic-symlink";
        if(::symlink(atomic_target.c_str(),atomic_symlink.c_str())!=0)throw Error("TestSymlinkFailed");
        expect_error([&]{LocalFiles::publish_new_atomically(atomic_symlink,"replacement");},"ExportExistsOrUnavailable");
        if(!std::filesystem::is_symlink(atomic_symlink) || LocalFiles::read(atomic_target,8)!="retained")throw Error("AtomicPublicationReplacedSymlink");
        clean();
        const auto cleanup_target=root/"cleanup-failure";
        blocked_unlink_target=cleanup_target.string();
        expect_error([&]{LocalFiles::publish_new(cleanup_target,"published");},"ExportTemporaryCleanupFailed");
        blocked_unlink_target.clear();
        if(LocalFiles::read(cleanup_target,9)!="published")throw Error("PublishedTargetLostAfterCleanupFailure");
        bool found_temp=false;
        for(const auto& item:std::filesystem::directory_iterator(root)){
            if(item.path().string().starts_with(cleanup_target.string()+".tmp.")){
                found_temp=true;
                std::filesystem::remove(item.path());
            }
        }
        if(!found_temp)throw Error("UnlinkFailureNotInjected");
        clean();
        expect_error([&]{LocalFiles::publish_new(root/"cancelled","data",[]{throw Error("Stop");});},"Stop");
        if(std::filesystem::exists(root/"cancelled") || LocalFiles::read(output,8)!="original")throw Error("CancellationChangedTarget");
        expect_error([&]{LocalFiles::publish_new(root/"absent"/"new","data");},"CannotCreateExport");
        write_failure([&]{LocalFiles::publish_new(root/"failed",std::string(8192,'x'));},"ExportWriteFailure");
        if(std::filesystem::exists(root/"failed"))throw Error("FailedExportPublished");
        clean();
        const auto target=LocalFiles::rule_target(store);
        const auto first=LocalFiles::load_rules(target,true).stage("BRUCE_RULES_V1\nsnapshot first\n");
        expect_error([&]{LocalFiles::activate_rules(target,first,"wrong");},"RuleApprovalDeclined");
        if(std::filesystem::exists(store))throw Error("UnconfirmedStorePublished");
        LocalFiles::activate_rules(target,first,"CONFIRM "+first.confirmation());
        const auto before=LocalFiles::read(store,rule_history_limit);
        if(LocalFiles::active_rules(store)!="BRUCE_RULES_V1\nsnapshot first\n")throw Error("ActiveRulesMismatch");
        std::string rules="BRUCE_RULES_V1\nsnapshot second\n";
        for(unsigned i=0;i<128;++i)rules+="rule r"+std::to_string(i)+" positive value ge 0 0 1 manual -\n";
        const auto next=LocalFiles::load_rules(target).stage(rules);
        write_failure([&]{LocalFiles::activate_rules(target,next,"CONFIRM "+next.confirmation());},"RuleStoreWriteFailed");
        if(LocalFiles::read(store,rule_history_limit)!=before)throw Error("FailedCommitChangedStore");
        clean();
        LocalFiles::activate_rules(target,next,"CONFIRM "+next.confirmation());
        expect_error([&]{LocalFiles::activate_rules(target,first,"CONFIRM "+first.confirmation());},"RuleStoreBaseChanged");
        if(LocalFiles::active_rules(store)!=rules)throw Error("StaleCommitChangedStore");
        clean();
        std::filesystem::remove_all(root);std::cout<<"Local file adapter acceptance passed\n";
    }catch(const std::exception& e){std::filesystem::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}
}
