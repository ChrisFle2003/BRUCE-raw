#include "payload_run_stream_inspector_v1.hpp"
#include "bruce/artifact/artifact_payload_stream_v1.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <openssl/evp.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace bruce::artifact_v1::detail {
namespace {
using Clock=std::chrono::steady_clock;
[[noreturn]] void fail(){throw Error("PROTOCOL_ERROR");}
void ensure(bool ok){if(!ok)fail();}
struct Fd {
    int value;
    explicit Fd(int fd):value(fd){ensure(fd>=0);}
    ~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other) noexcept:value(other.value){other.value=-1;}
};
bool same(const struct stat& a,const struct stat& b){
    return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&
        a.st_gid==b.st_gid&&a.st_nlink==b.st_nlink&&a.st_size==b.st_size&&
        a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
struct Snapshot {
    std::filesystem::path path;
    struct stat metadata{};
    bool directory{};
    std::set<std::string> members;
    std::string digest;
};
class Reader {
public:
    explicit Reader(Clock::time_point until):deadline(until){}
    Clock::time_point deadline;
    std::vector<Snapshot> snapshots;
    std::vector<std::pair<ArtifactPayloadStreamV15,std::filesystem::path>> streams;
    void tick()const{ensure(Clock::now()<deadline);}
    Fd open(const std::filesystem::path& path,bool directory)const{
        tick();ensure(!path.empty()&&path.native().find('\0')==std::string::npos);
        Fd current(::open(path.is_absolute()?"/":".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        std::vector<std::string> parts;
        for(const auto& part:path){auto s=part.string();if(s=="/")continue;ensure(!s.empty()&&s!="."&&s!="..");parts.push_back(s);}
        ensure(!parts.empty());
        for(std::size_t i=0;i<parts.size();++i){tick();auto flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;if(i+1<parts.size()||directory)flags|=O_DIRECTORY;Fd next(::openat(current.value,parts[i].c_str(),flags));std::swap(current.value,next.value);}
        return current;
    }
    struct stat metadata(int fd,bool directory)const{
        tick();struct stat s{};ensure(::fstat(fd,&s)==0&&s.st_uid==::geteuid());
        ensure(directory?(S_ISDIR(s.st_mode)&&(s.st_mode&07777)==0700):(S_ISREG(s.st_mode)&&(s.st_mode&07777)==0600&&s.st_nlink==1&&s.st_size>=0));return s;
    }
    std::set<std::string> entries(int fd,std::size_t limit)const{
        tick();Fd copy(::openat(fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        DIR* raw=::fdopendir(copy.value);ensure(raw);copy.value=-1;
        std::unique_ptr<DIR,int(*)(DIR*)> dir(raw,::closedir);std::set<std::string> names;
        for(;;){tick();errno=0;auto* e=::readdir(dir.get());if(!e){ensure(errno==0);break;}std::string name=e->d_name;if(name=="."||name=="..")continue;ensure(names.size()<limit);ensure(names.insert(name).second);}return names;
    }
    std::set<std::string> directory(const std::filesystem::path& path,std::size_t limit){
        auto fd=open(path,true);auto before=metadata(fd.value,true);auto names=entries(fd.value,limit);ensure(same(before,metadata(fd.value,true)));snapshots.push_back({path,before,true,names,{}});return names;
    }
    void file_snapshot(const std::filesystem::path& path,std::uint64_t cap){
        auto fd=open(path,false);auto st=metadata(fd.value,false);ensure(static_cast<std::uint64_t>(st.st_size)<=cap);snapshots.push_back({path,st,false,{},{}});
    }
    std::string read_file(const std::filesystem::path& path,std::uint64_t cap,bool collect,bool record=true){
        auto fd=open(path,false);auto before=metadata(fd.value,false);const auto size=static_cast<std::uint64_t>(before.st_size);ensure(size<=cap);
        std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);ensure(bool(ctx));tick();ensure(EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1);
        std::array<char,65536> buffer{};std::uint64_t total{};std::string bytes;if(collect){ensure(cap<=1024);bytes.reserve(static_cast<std::size_t>(size));}
        while(total<size){tick();auto n=::read(fd.value,buffer.data(),static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(),size-total)));if(n<0&&errno==EINTR)continue;ensure(n>0);tick();ensure(EVP_DigestUpdate(ctx.get(),buffer.data(),static_cast<std::size_t>(n))==1);if(collect)bytes.append(buffer.data(),static_cast<std::size_t>(n));total+=static_cast<std::uint64_t>(n);}
        tick();unsigned char digest[EVP_MAX_MD_SIZE]{};unsigned length{};ensure(EVP_DigestFinal_ex(ctx.get(),digest,&length)==1&&length==32);ensure(same(before,metadata(fd.value,false)));std::string hash;for(unsigned i=0;i<length;++i){hash.push_back("0123456789abcdef"[digest[i]>>4]);hash.push_back("0123456789abcdef"[digest[i]&15]);}
        if(record)snapshots.push_back({path,before,false,{},hash});
        return collect?bytes:hash;
    }
    void finish(){
        // Reauthenticate content as well as metadata: some filesystems can
        // preserve/coarsen timestamps for same-size rewrites.
        for(const auto& [stream,input]:streams){tick();verify_payload_stream_v15(stream,input,deadline);}
        for(const auto& s:snapshots)if(!s.digest.empty())ensure(read_file(s.path,static_cast<std::uint64_t>(s.metadata.st_size),false,false)==s.digest);
        validate_metadata();
    }
    void validate_metadata(){
        for(const auto& s:snapshots){auto fd=open(s.path,s.directory);ensure(same(s.metadata,metadata(fd.value,s.directory)));if(s.directory)ensure(entries(fd.value,s.members.size()+1)==s.members);ensure(same(s.metadata,metadata(fd.value,s.directory)));}tick();
    }
};
// Parsing exact byte tokens rejects alternate whitespace, escaped strings,
// duplicate fields, depth, trailing data and noncanonical/overflowing numbers.
class Header {
public:
    explicit Header(std::string_view bytes):remaining(bytes){}
    void take(std::string_view token){ensure(remaining.starts_with(token));remaining.remove_prefix(token.size());}
    std::uint64_t number(){
        const auto end=remaining.find_first_not_of("0123456789");const auto count=end==std::string_view::npos?remaining.size():end;
        ensure(count&&!(count>1&&remaining.front()=='0'));std::uint64_t n{};auto result=std::from_chars(remaining.data(),remaining.data()+count,n);ensure(result.ec==std::errc{}&&result.ptr==remaining.data()+count);remaining.remove_prefix(count);return n;
    }
    ArtifactPayloadStreamV15 parse(){
        take("{\"stream_id\":\"payload\",\"sha256\":\"");ensure(remaining.size()>=64);auto hash=std::string(remaining.substr(0,64));for(char c:hash)ensure((c>='0'&&c<='9')||(c>='a'&&c<='f'));remaining.remove_prefix(64);
        take("\",\"size_bytes\":");auto size=number();ensure(size<=1048576);
        take(",\"chunk_bytes\":");auto chunk=number();ensure(chunk==65536);
        take(",\"chunk_count\":");auto count=number();ensure(count==size/65536+(size%65536!=0));take("}\n");ensure(remaining.empty());return {"payload","stream-payload",hash,size,chunk,count};
    }
private:std::string_view remaining;
};
std::string chunk_name(std::uint64_t n){auto digits=std::to_string(n);return "chunk-"+std::string(20-digits.size(),'0')+digits+".bin";}
PayloadStreamInspectionV1 inspect(const std::filesystem::path& root,const std::vector<std::string>& started,std::uint64_t cap,Clock::time_point deadline){
    auto reader=std::make_shared<Reader>(deadline);auto root_names=reader->directory(root,65536);
    PayloadStreamInspectionV1 result;result.validate_snapshot=[reader]{reader->finish();};
    result.validate_metadata=[reader]{reader->validate_metadata();};
    if(!root_names.contains("payload-inputs")){reader->finish();return result;}
    ensure(started.size()<=256);std::set<std::string> allowed(started.begin(),started.end());ensure(allowed.size()==started.size());
    const auto payload=root/"payload-inputs";auto attempts=reader->directory(payload,256);
    for(const auto& id:attempts){
        ensure(allowed.contains(id));auto attempt=payload/id;auto inputs=reader->directory(attempt,15);
        std::set<std::string> expected;for(std::size_t i=0;i<inputs.size();++i)expected.insert("input-"+std::to_string(i));ensure(inputs==expected);
        std::uint64_t total{};
        for(std::size_t i=0;i<inputs.size();++i){
            reader->tick();auto input=attempt/("input-"+std::to_string(i));ensure(reader->directory(input,2)==std::set<std::string>{"input.bin","stream-payload"});
            auto streamdir=input/"stream-payload";auto members=reader->directory(streamdir,18);
            auto header=reader->read_file(streamdir/"header.jsonl",1024,true);auto stream=Header(header).parse();
            ensure(total<=cap&&stream.size_bytes<=cap-total);total+=stream.size_bytes;
            expected={"header.jsonl","seal.json"};for(std::uint64_t j=0;j<stream.chunk_count;++j)expected.insert(chunk_name(j));ensure(members==expected);
            reader->file_snapshot(streamdir/"seal.json",1024);
            for(std::uint64_t j=0;j<stream.chunk_count;++j)reader->file_snapshot(streamdir/chunk_name(j),65536);
            reader->tick();verify_payload_stream_v15(stream,input,deadline);reader->tick();
            reader->streams.emplace_back(stream,input);
            const auto compatibility=input/"input.bin";auto hash=reader->read_file(compatibility,1048576,false);
            ensure(hash==stream.sha256&&static_cast<std::uint64_t>(reader->snapshots.back().metadata.st_size)==stream.size_bytes);
            ensure(result.bytes<=std::numeric_limits<std::uint64_t>::max()-stream.size_bytes);result.bytes+=stream.size_bytes;++result.count;
        }
    }
    reader->finish();return result;
}
} // namespace
PayloadStreamInspectionV1 inspect_payload_run_streams_v1(const std::filesystem::path& root,const std::vector<std::string>& started,std::uint64_t cap,Clock::time_point deadline){
    try{return inspect(root,started,cap,deadline);}catch(...){throw Error("PROTOCOL_ERROR");}
}
} // namespace bruce::artifact_v1::detail
