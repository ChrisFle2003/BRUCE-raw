#include "../../src/artifact/payload_run_stream_inspector_v1.hpp"
#include "bruce/artifact/artifact_payload_stream_v1.hpp"
#include "bruce/evidence/bundle.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace bruce;
using namespace bruce::artifact_v1;
namespace fs=std::filesystem;
static std::string watched;
static std::function<void()> on_read;
static bool expired=false;
static unsigned reads_after_expiry=0;
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" std::chrono::steady_clock::time_point __real__ZNSt6chrono3_V212steady_clock3nowEv();
extern "C" std::chrono::steady_clock::time_point __wrap__ZNSt6chrono3_V212steady_clock3nowEv() {
 auto now=__real__ZNSt6chrono3_V212steady_clock3nowEv();return expired?now+std::chrono::seconds(60):now;
}
extern "C" ssize_t __wrap_read(int fd,void* data,size_t size) {
 if(expired)++reads_after_expiry;
 char name[4096]{};auto n=::readlink(("/proc/self/fd/"+std::to_string(fd)).c_str(),name,sizeof(name)-1);
 auto result=__real_read(fd,data,size);
 if(n>0&&std::string(name,static_cast<std::size_t>(n))==watched){watched.clear();auto action=std::move(on_read);action();}
 return result;
}
namespace {
void check(bool ok,const char* msg){if(!ok)throw Error(msg);}
void put(const fs::path& p,std::string_view s){std::ofstream f(p,std::ios::binary);f<<s;f.close();check(bool(f),"fixture write");check(chmod(p.c_str(),0600)==0,"fixture mode");}
std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void dir(const fs::path& p){fs::create_directories(p);chmod(p.c_str(),0700);}
std::map<std::string,std::string> snapshot(const fs::path& root){
 std::map<std::string,std::string> result;
 auto record=[&](const fs::path& path){struct stat s{};check(lstat(path.c_str(),&s)==0,"snapshot stat");std::string value=std::to_string(s.st_dev)+":"+std::to_string(s.st_ino)+":"+std::to_string(s.st_mode)+":"+std::to_string(s.st_uid)+":"+std::to_string(s.st_gid)+":"+std::to_string(s.st_nlink)+":"+std::to_string(s.st_size)+":"+std::to_string(s.st_mtim.tv_sec)+":"+std::to_string(s.st_mtim.tv_nsec);if(S_ISREG(s.st_mode))value+=":"+get(path);if(S_ISLNK(s.st_mode))value+=":"+fs::read_symlink(path).string();result[path.string()]=std::move(value);};
 record(root);for(const auto& entry:fs::recursive_directory_iterator(root))record(entry.path());return result;
}
struct Fixture {
 fs::path root;
 Fixture(){char name[]="/tmp/bruce-stream-inspect-XXXXXX";check(mkdtemp(name),"temp");root=name;}
 ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
 fs::path input(std::string id="a-1",unsigned index=0,std::size_t size=8){
  dir(root/"payload-inputs");dir(root/"payload-inputs"/id);auto p=root/"payload-inputs"/id/("input-"+std::to_string(index));dir(p);
  std::string bytes(size,'x');put(root/"source",bytes);int fd=open((root/"source").c_str(),O_RDONLY|O_CLOEXEC);
  write_payload_stream_v15(fd,p,"payload",{sha256(bytes),size},1048576,std::chrono::steady_clock::now()+std::chrono::seconds(30));close(fd);put(p/"input.bin",bytes);return p;
 }
 auto inspect(std::vector<std::string> ids={"a-1"},std::uint64_t cap=1048576){
  const bool compare=watched.empty();const auto before=compare?snapshot(root):std::map<std::string,std::string>{};
  try{auto result=bruce::artifact_v1::detail::inspect_payload_run_streams_v1(root,ids,cap,std::chrono::steady_clock::now()+std::chrono::seconds(30));if(compare)check(snapshot(root)==before,"inspection modified files");return result;}
  catch(...){if(compare)check(snapshot(root)==before,"failed inspection modified files");throw;}
 }
 void reject(std::vector<std::string> ids={"a-1"},std::uint64_t cap=1048576){try{inspect(ids,cap);}catch(const Error& e){check(std::string(e.what())=="PROTOCOL_ERROR","unsafe error");return;}throw Error("bad stream accepted");}
};
void tests(){
 {Fixture f;auto p=f.input();auto r=f.inspect();put(p/"input.bin","yyyyyyyy");try{r.validate_snapshot();throw std::runtime_error("late mutation accepted");}catch(const Error&){} }
 {Fixture f;auto r=f.inspect();dir(f.root/"payload-inputs");try{r.validate_snapshot();throw std::runtime_error("late tree accepted");}catch(const Error&){} }

 {Fixture f;check(f.inspect().count==0,"absent tree");dir(f.root/"payload-inputs");check(f.inspect().count==0,"empty tree");}
 for(auto size:{0U,1U,65536U,65537U,1048576U}){Fixture f;f.input("a-1",0,size);auto r=f.inspect();check(r.count==1&&r.bytes==size,"canonical size/count");}
 {Fixture f;f.input("a-1",0,8);f.input("a-1",1,9);f.input("a-2",0,10);auto r=f.inspect({"a-1","a-2"},17);check(r.count==3&&r.bytes==27,"per attempt budget");f.reject({"a-1","a-2"},16);}
 for(auto name:{"input.bin","stream-payload/header.jsonl","stream-payload/seal.json","stream-payload/chunk-00000000000000000000.bin"}){Fixture f;auto p=f.input()/name;put(p,"wrong");f.reject();check(get(p)=="wrong","inspector wrote fixture");}
 {Fixture f;f.input("reserved");f.reject();}
 for(auto index:{1U,15U}){Fixture f;f.input("a-1",index);f.reject();}
 for(auto name:{"source.tmp",".stream-staging-left","legacy.bin"}){Fixture f;auto p=f.input()/name;put(p,"private");f.reject();check(get(p)=="private","extra file removed");}
 {Fixture f;dir(f.root/"payload-inputs");put(f.root/"payload-inputs/a-1","legacy");f.reject();}
 for(auto name:{"payload-inputs","payload-inputs/a-1","payload-inputs/a-1/input-0","payload-inputs/a-1/input-0/stream-payload"}){Fixture f;f.input();auto p=f.root/name;chmod(p.c_str(),0755);f.reject();}
 for(auto name:{"input.bin","stream-payload/header.jsonl","stream-payload/seal.json","stream-payload/chunk-00000000000000000000.bin"}){
  for(unsigned kind=0;kind<4;++kind){Fixture f;auto p=f.input()/name;if(kind==0)chmod(p.c_str(),0400);else if(kind==1)fs::create_hard_link(p,f.root/"hardlink");else{fs::rename(p,f.root/"saved");if(kind==2)fs::create_symlink(f.root/"saved",p);else check(mkfifo(p.c_str(),0600)==0,"fifo fixture");}f.reject();}
 }
 for(auto name:{"payload-inputs","payload-inputs/a-1","payload-inputs/a-1/input-0","payload-inputs/a-1/input-0/stream-payload"}){Fixture f;f.input();auto p=f.root/name;fs::rename(p,f.root/"saved");fs::create_directory_symlink(f.root/"saved",p);f.reject();}
 for(auto name:{"header.jsonl","seal.json","chunk-00000000000000000000.bin"}){Fixture f;auto p=f.input();watched=(p/"input.bin").string();on_read=[p,name]{auto file=p/"stream-payload"/name;fs::rename(file,file.string()+".old");put(file,get(file.string()+".old"));};f.reject();check(watched.empty(),"mutation not triggered");}
 {Fixture f;auto p=f.input();watched=(p/"input.bin").string();on_read=[p]{fs::rename(p,p.string()+".old");fs::copy(p.string()+".old",p,fs::copy_options::recursive);};f.reject();}
 {Fixture f;auto p=f.input();watched=(p/"input.bin").string();on_read=[p]{fs::rename(p/"input.bin",p/"old");put(p/"input.bin","xxxxxxxx");};f.reject();}
 {Fixture f;auto p=f.input();f.input("a-1",1);watched=(p/"input.bin").string();on_read=[]{expired=true;};f.reject();expired=false;check(reads_after_expiry==0,"read after deadline");}
 {Fixture f;auto p=f.input();auto header=get(p/"stream-payload/header.jsonl");auto at=header.find("\"size_bytes\":8");header.replace(at,14,"\"size_bytes\":18446744073709551615");put(p/"stream-payload/header.jsonl",header);f.reject();}
}
}
int main(){try{tests();std::cout<<"payload stream inspector tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
