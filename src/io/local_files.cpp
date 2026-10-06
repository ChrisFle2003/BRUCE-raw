#include "bruce/io/local_files.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace bruce {
namespace {
struct Descriptor {
    int fd;
    explicit Descriptor(int value):fd(value){if(fd<0)throw Error("RuleStoreFileOpenFailed");}
    ~Descriptor(){::close(fd);}
    Descriptor(const Descriptor&)=delete;
    Descriptor& operator=(const Descriptor&)=delete;
};
void commit(const std::string& path,const std::string& bytes){
    Descriptor directory(::open(std::filesystem::path(path).parent_path().c_str(),O_RDONLY|O_DIRECTORY));
    std::string temp=path+".tmp.XXXXXX";
    Descriptor file(::mkstemp(temp.data()));
    try {
        std::size_t offset=0;
        while(offset<bytes.size()){
            const auto n=::write(file.fd,bytes.data()+offset,bytes.size()-offset);
            if(n<0 && errno==EINTR)continue;
            if(n<=0)throw Error("RuleStoreWriteFailed");
            offset+=static_cast<std::size_t>(n);
        }
        if(::fsync(file.fd)!=0)throw Error("RuleStoreSyncFailed");
        if(::rename(temp.c_str(),path.c_str())!=0)throw Error("RuleStoreReplaceFailed");
        if(::fsync(directory.fd)!=0)throw Error("RuleStoreCommitDurabilityUnknown");
    }catch(...){::unlink(temp.c_str());throw;}
}
}
std::string LocalFiles::read(const std::string& path,std::size_t limit,const ReplayCheckpoint& checkpoint) {
    if(checkpoint)checkpoint();
    std::ifstream input(path,std::ios::binary);
    if(!input) throw Error("CannotReadFile: "+path);
    std::string result;
    std::array<char,8192> buffer{};
    while(input.read(buffer.data(),buffer.size()) || input.gcount()) {
        if(checkpoint)checkpoint();
        const auto count=static_cast<std::size_t>(input.gcount());
        if(count>limit-result.size()) throw Error("InputByteLimit");
        result.append(buffer.data(),count);
    }
    if(checkpoint)checkpoint();
    if(!input.eof()) throw Error("FileReadError");
    return result;
}
void LocalFiles::publish_new(const std::string& path,std::string_view content,const ReplayCheckpoint& checkpoint) {
    if(checkpoint)checkpoint();
    if(content.size()>max_file_bytes) throw Error("BundleByteLimit");
    const auto parent=std::filesystem::path(path).parent_path();
    const int directory_fd=::open(parent.empty()?".":parent.c_str(),O_RDONLY|O_DIRECTORY);
    if(directory_fd<0) throw Error("CannotCreateExport: "+std::string(std::strerror(errno)));
    // mkstemp creates privately and exclusively on the same filesystem.
    std::string temp=path+".tmp.XXXXXX";
    const int fd=::mkstemp(temp.data());
    if(fd<0) {
        const auto message="CannotCreateExport: "+std::string(std::strerror(errno));
        ::close(directory_fd);
        throw Error(message);
    }
    bool closed=false;
    try {
        std::size_t offset=0;
        while(offset<content.size()) {
            if(checkpoint)checkpoint();
            const auto count=checkpoint ? std::min<std::size_t>(65536,content.size()-offset):content.size()-offset;
            const auto n=::write(fd,content.data()+offset,count);
            if(n<0 && errno==EINTR) continue;
            if(n<=0) throw Error("ExportWriteFailure");
            offset+=static_cast<std::size_t>(n);
        }
        if(checkpoint)checkpoint();
        if(::fsync(fd)!=0) throw Error("ExportSyncFailure");
        const auto close_result=::close(fd);
        closed=true;
        if(close_result!=0) throw Error("ExportCloseFailure");
        if(checkpoint)checkpoint();
        // Unlike rename(), link() never replaces an existing destination.
        if(::link(temp.c_str(),path.c_str())!=0)
            throw Error("ExportExistsOrUnavailable: "+std::string(std::strerror(errno)));
        if(::unlink(temp.c_str())!=0) throw Error("ExportTemporaryCleanupFailed");
        if(::fsync(directory_fd)!=0) throw Error("ExportCommitDurabilityUnknown");
    } catch(...) {
        if(!closed) ::close(fd);
        ::unlink(temp.c_str());
        ::close(directory_fd);
        throw;
    }
    ::close(directory_fd);
}
std::string LocalFiles::rule_target(const std::string& path){
    return std::filesystem::weakly_canonical(std::filesystem::absolute(path)).string();
}
RuleHistory LocalFiles::load_rules(const std::string& path,bool allow_missing,const ReplayCheckpoint& checkpoint){
    if(checkpoint)checkpoint();
    if(allow_missing && !std::filesystem::exists(path))return RuleHistory(path,"",checkpoint);
    const auto bytes=read(path,rule_history_limit,checkpoint);
    if(bytes.empty())throw Error("InvalidRuleHistory");
    return RuleHistory(path,bytes,checkpoint);
}
std::string LocalFiles::active_rules(const std::string& path,const ReplayCheckpoint& checkpoint){
    if(checkpoint)checkpoint();
    const auto resolved=rule_target(path);
    return load_rules(resolved,false,checkpoint).active();
}
RuleHistory LocalFiles::activate_rules(const std::string& path,const RuleStage& stage,const std::string& confirmation){
    Descriptor lock(::open((path+".lock").c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600));
    if(::flock(lock.fd,LOCK_EX|LOCK_NB)!=0)throw Error("RuleStoreBusy");
    const auto current=load_rules(path,true);
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(now<0)throw Error("RuleStoreInvalidClock");
    const auto bytes=current.append(stage,confirmation,static_cast<std::uint64_t>(now));
    commit(path,bytes);
    return RuleHistory(path,bytes);
}
}
