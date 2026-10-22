#include "bruce/artifact/payload_run_inspector_v1.hpp"
#include "../../apps/dag_scheduler_v5_cli.hpp"
#include <fstream>
#include <iostream>
#include <functional>
#include <sys/stat.h>
using namespace bruce;
namespace fs = std::filesystem;
namespace cli = bruce::dag_scheduler_v5_cli_detail;
// Test-only fault injection at real read and monotonic-clock boundaries.
static std::string mutate_on_read, expire_on_read;
static bool expired=false;
static unsigned reads_after_expiry=0;
static std::string watched_read;
static unsigned watched_read_count=0, trigger_read_count=0;
static std::function<void()> after_watched_read;
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" std::chrono::steady_clock::time_point __real__ZNSt6chrono3_V212steady_clock3nowEv();
extern "C" std::chrono::steady_clock::time_point __wrap__ZNSt6chrono3_V212steady_clock3nowEv() {
 auto t=__real__ZNSt6chrono3_V212steady_clock3nowEv();return expired?t+std::chrono::seconds(31):t;
}
extern "C" ssize_t __wrap_read(int fd,void* data,size_t size) {
 if(expired)++reads_after_expiry;
 char name[4096]{};auto n=::readlink(("/proc/self/fd/"+std::to_string(fd)).c_str(),name,sizeof(name)-1);
 std::string path=n<0?"":std::string(name,static_cast<std::size_t>(n));
 auto count=__real_read(fd,data,size);
 if(!watched_read.empty()&&path==watched_read&&++watched_read_count==trigger_read_count){auto action=std::move(after_watched_read);watched_read.clear();action();}
 if(!mutate_on_read.empty()&&path==mutate_on_read){const auto target=mutate_on_read;mutate_on_read.clear();fs::rename(target,target+".old");fs::copy_file(target+".old",target);}
 if(!expire_on_read.empty()&&path==expire_on_read){expire_on_read.clear();expired=true;}
 return count;
}
namespace {
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
void write(const fs::path& p, std::string_view s) { std::ofstream f(p, std::ios::binary); f << s; f.close(); chmod(p.c_str(),0600); }
std::string read(const fs::path& p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f),{}}; }
void reject(const fs::path& p) { try { (void)artifact_v1::inspect_payload_run_v1(p); } catch(const Error& e) { check(std::string(e.what())=="PROTOCOL_ERROR","unsafe error"); return; } throw std::runtime_error("invalid fixture accepted"); }
struct Fixture {
 fs::path bundle,root; cli::Options options; dag_scheduler_v3_cli_detail::ParsedDagSchedulerSpecV3 parsed; std::map<std::string,cli::V5NodeState> states; std::string log; unsigned seq{}; bool v14{},multi{};
 Fixture(fs::path b, bool ext=false, bool multiple=false):bundle(b),root(b.string()+".run"),v14(ext),multi(multiple) {
  options.retry=3; parsed.raw_bytes="fixture"; parsed.spec.dag_id="dag";
  DagSchedulerNodeSpecV2 n; n.node_id="n"; n.callback_id="literal-v1"; n.request_bytes="request"; n.request_digest=sha256(n.request_bytes); parsed.spec.nodes.push_back(n);
  if(multi){ n.node_id="m"; parsed.spec.nodes.push_back(n); }
  for(const auto& node:parsed.spec.nodes){ cli::V5NodeState s; s.spec=&node; s.result.status=worker_v1::WorkerStatus::FAILED; s.result.failure_code=worker_v1::FailureCode::FAILED; s.result.result_code="ARTIFACT_REFERENCE_INVALID"; states.emplace(node.node_id,std::move(s)); }
  for(auto d:{root,root/"inputs",root/"attempts",root/"control"}) {fs::create_directories(d); chmod(d.c_str(),0700);}
  for(const auto& node:parsed.spec.nodes)write(root/"inputs"/(node.node_id+".bin"),"request");
  event("RUN_CREATED","READY");
 }
 void event(std::string name,std::string status,std::string id={},bool output=false,bool usage=false){ worker_v1::WorkerLogEvent e; e.run_id="run-1"; e.seq=++seq; e.ts_unix_ms=1; e.event=name;e.status=status;if(!id.empty())e.attempt_id=id;if(output){e.digest=sha256("data");e.size_bytes=4;e.reference="attempts/"+id+"/output/result.bin";}if(usage)e.usage=worker_v1::WorkerUsage{1,7,output?4U:0U,1,1};log+="{\"schema\":\"worker-run-log-v1\",\"run_id\":\"run-1\"";if(e.attempt_id)log+=",\"attempt_id\":\""+id+"\"";log+=",\"seq\":"+std::to_string(e.seq)+",\"ts_unix_ms\":1,\"event\":\""+name+"\",\"status\":\""+status+"\"";if(output)log+=",\"digest\":\""+*e.digest+"\",\"reference\":\""+*e.reference+"\",\"size_bytes\":4";if(usage)log+=",\"usage\":{\"wall_ms\":1,\"input_bytes\":7,\"output_bytes\":"+std::to_string(output?4:0)+",\"operations\":1,\"peak_memory_bytes\":1}";log+="}\n"; }
 void attempt(std::string node,std::string id,bool accepted,bool publication=false){
  auto& s=states.at(node);s.attempt_id=id;
  for(auto d:{root/"attempts"/id,root/"attempts"/id/"output"}){fs::create_directory(d);chmod(d.c_str(),0700);}
  event("ATTEMPT_STARTED","READY",id);event("STATUS","RUNNING",id);
  const bool ready=accepted||publication;event("PROCESS_EXITED",ready?"READY":"FAILED",id,false,false);event("RESULT_RECEIVED",ready?"READY":"FAILED",id,ready,true);
  s.result={};s.result.status=ready?worker_v1::WorkerStatus::READY:worker_v1::WorkerStatus::FAILED;s.result.failure_code=ready?worker_v1::FailureCode::READY:worker_v1::FailureCode::TIMEOUT;s.result.result_code=ready?"LITERAL_READY":"TIMEOUT";s.result.usage={1,7,ready?4U:0U,1,1};
  if(ready){write(root/"attempts"/id/"output/result.bin","data");s.result.payload_digest=sha256("data");s.result.payload_size=4;}
  if(accepted){event("ARTIFACT_ACCEPTED","READY",id,true,true);s.accepted_public_reference="artifact/"+node;}
  if(publication){s.result={};s.result.status=worker_v1::WorkerStatus::FAILED;s.result.failure_code=worker_v1::FailureCode::FAILED;s.result.result_code="ARTIFACT_STORE_VIOLATION";}
  s.history.push_back({id,s.result,accepted});
 }
 void finish(bool success=false,bool end=true){if(end)event("RUN_FINISHED",success?"READY":"FAILED");write(root/"control/run-log.jsonl",log);std::string out;if(multi)out=cli::render_multi_report(parsed,options,states,success,true,v14?sha256("sidecar"):"");else {const auto&s=states.at("n");out=cli::render_report(parsed,options,s.result,s.accepted_public_reference,s.history,true,v14?sha256("sidecar"):"")+"\n";}write(bundle,out);}
};
fs::path add_stream(Fixture& f,const fs::path& base) {
 const auto input=f.root/"payload-inputs/a-1/input-0";
 for(const auto& p:{f.root/"payload-inputs",f.root/"payload-inputs/a-1",input}){fs::create_directory(p);chmod(p.c_str(),0700);}
 write(base/"stream-source","stream-data");int fd=::open((base/"stream-source").c_str(),O_RDONLY|O_CLOEXEC);
 artifact_v1::write_payload_stream_v15(fd,input,"payload",{sha256("stream-data"),11},1048576,std::chrono::steady_clock::now()+std::chrono::seconds(30));::close(fd);
 write(input/"input.bin","stream-data");
 return input;
}
void mutate_same_size(const fs::path& path) {
 // Force a distinct mtime as well as changing one byte, so the final metadata
 // regression does not depend on the filesystem's timestamp resolution.
 struct stat before{};check(::stat(path.c_str(),&before)==0&&before.st_size>0,"mutation stat");
 const int fd=::open(path.c_str(),O_WRONLY);check(fd>=0,"mutation open");
 const auto n=::pwrite(fd,"!",1,0);
 const timespec times[2]={{0,UTIME_OMIT},{before.st_mtim.tv_sec+1,before.st_mtim.tv_nsec}};
 const auto result=::futimens(fd,times);::close(fd);
 check(n==1&&result==0,"same-size mutation write");
 struct stat after{};check(::stat(path.c_str(),&after)==0,"mutation final stat");
 check(after.st_size==before.st_size&&after.st_mtim.tv_sec>before.st_mtim.tv_sec,"mutation must retain size and advance mtime");
}
void stream_integration(const fs::path& base) {
 Fixture f(base/"stream-integration");f.attempt("n","a-1",true);f.finish(true);
 const auto input=add_stream(f,base);
 const auto r=artifact_v1::inspect_payload_run_v1(f.bundle,{true});check(r.streams_requested&&r.verified_stream_count==1&&r.verified_stream_bytes==11,"stream counts from core");
 // Alter a previously authenticated stream while the core rereads its report.
 // This final boundary observes metadata only: equal-size writes with unchanged
 // metadata are outside its guarantee, so make this mutation observable.
 watched_read=f.bundle.string();watched_read_count=0;trigger_read_count=2;
 after_watched_read=[input]{mutate_same_size(input/"input.bin");};
 try{(void)artifact_v1::inspect_payload_run_v1(f.bundle,{true});throw std::runtime_error("cross-phase stream mutation accepted");}catch(const Error& e){check(std::string(e.what())=="PROTOCOL_ERROR","unsafe error");}
 check(watched_read.empty()&&watched_read_count==2,"cross-phase hook unused");
 check(read(input/"input.bin")=="!tream-data","cross-phase fixture bytes unchanged");
 check(artifact_v1::inspect_payload_run_v1(f.bundle).accepted_artifact_count==1,"unrequested stream scan");
}
void stream_finalization_regressions(const fs::path& base) {
 // Core evidence must still be checked after the last long stream/input read.
 for(const std::string target:{"bundle","log","request","output"}) {
  Fixture f(base/("core-during-stream-"+target));f.attempt("n","a-1",true);f.finish(true);
  const auto input=add_stream(f,base);
  const auto path=target=="bundle"?f.bundle:target=="log"?f.root/"control/run-log.jsonl":target=="request"?f.root/"inputs/n.bin":f.root/"attempts/a-1/output/result.bin";
  watched_read=(input/"input.bin").string();watched_read_count=0;trigger_read_count=3;
  after_watched_read=[path]{mutate_same_size(path);};
  try{(void)artifact_v1::inspect_payload_run_v1(f.bundle,{true});throw std::runtime_error("core mutation during final stream read accepted: "+target);}catch(const Error& e){check(std::string(e.what())=="PROTOCOL_ERROR","unsafe error");}
  check(watched_read.empty()&&watched_read_count==3,"final stream mutation hook unused");
 }
 // Reordering full finish calls alone would reopen this opposite direction.
 // All stream file types remain covered by the shared final metadata pass.
 for(const std::string source:{"bundle","log","request","output"})for(const std::string target:{"input.bin","header.jsonl","seal.json","chunk-00000000000000000000.bin"}) {
  Fixture f(base/("stream-during-core-"+source+"-"+target));f.attempt("n","a-1",true);f.finish(true);
  const auto input=add_stream(f,base);
  const auto path=target=="input.bin"?input/target:input/"stream-payload"/target;
  watched_read=(source=="bundle"?f.bundle:source=="log"?f.root/"control/run-log.jsonl":source=="request"?f.root/"inputs/n.bin":f.root/"attempts/a-1/output/result.bin").string();watched_read_count=0;trigger_read_count=2;
  after_watched_read=[path]{mutate_same_size(path);};
  try{(void)artifact_v1::inspect_payload_run_v1(f.bundle,{true});throw std::runtime_error("stream mutation during final core read accepted: "+source+"/"+target);}catch(const Error& e){check(std::string(e.what())=="PROTOCOL_ERROR","unsafe error");}
  check(watched_read.empty()&&watched_read_count==2,"final core mutation hook unused");
 }
}
void receipt_regressions(const fs::path& base) {
 for(bool external:{false,true})for(bool multi:{false,true}) {
  Fixture f(base/("receipt-"+std::to_string(external)+std::to_string(multi)),external,multi);
  f.attempt("n",multi?"a-1-1":"a-1",true);
  if(multi)f.attempt("m","a-2-1",true);
  f.finish(true);
  const auto good=read(f.bundle);
  const std::string reference="\"reference\":\"artifact/n\"";
  auto bad=good;const auto pos=bad.find(reference);check(pos!=std::string::npos,"missing test receipt reference");
  bad.replace(pos,reference.size(),"\"reference\":null");write(f.bundle,bad);reject(f.bundle);
  write(f.bundle,good);
  check(artifact_v1::inspect_payload_run_v1(f.bundle).accepted_artifact_count==(multi?2U:1U),"accepted receipt restored");
 }
}
std::vector<std::string> log_lines(const std::string& log) {
 std::vector<std::string> lines;std::size_t begin=0;
 while(begin<log.size()){const auto end=log.find('\n',begin);check(end!=std::string::npos,"test log line");lines.push_back(log.substr(begin,end-begin));begin=end+1;}
 return lines;
}
void reorder_log(Fixture& f,const std::vector<std::size_t>& order) {
 auto lines=log_lines(f.log);std::string reordered;unsigned seq=0;
 check(order.size()==lines.size(),"test log order size");
 for(auto index:order){auto line=lines.at(index);const auto begin=line.find("\"seq\":")+6;const auto end=line.find(',',begin);line.replace(begin,end-begin,std::to_string(++seq));reordered+=line+"\n";}
 write(f.root/"control/run-log.jsonl",reordered);
}
void retry_regressions(const fs::path& base) {
 // History stays failed-first/successful-second; only event chronology changes.
 for(const std::string variant:{"reversed","overlap"}) {
  Fixture f(base/("retry-"+variant));f.attempt("n","a-1",false);f.attempt("n","a-1-2",true);f.finish(true);
  check(artifact_v1::inspect_payload_run_v1(f.bundle).attempt_count==2,"ordered retry");
  if(variant=="reversed")reorder_log(f,{0,5,6,7,8,9,1,2,3,4,10});
  else reorder_log(f,{0,1,2,5,6,3,4,7,8,9,10});
  reject(f.bundle);
 }
 // Independent nodes may overlap, including a retry while another node runs.
 Fixture f(base/"retry-interleaved",true,true);f.options.workers=2;
 f.attempt("n","a-1-1",false);f.attempt("n","a-1-2",true);f.attempt("m","a-2-1",true);f.finish(true);
 reorder_log(f,{0,1,2,10,11,3,4,5,6,12,13,14,7,8,9,15});
 const auto r=artifact_v1::inspect_payload_run_v1(f.bundle);check(r.attempt_count==3&&r.accepted_artifact_count==2,"legal interleaving");
}
void final_snapshot_regressions(const fs::path& base) {
 for(const std::string target:{"bundle","log","directory","deadline"}) {
  Fixture f(base/("final-cross-mutation-"+target));f.attempt("n","a-1",true);f.finish(true);
  watched_read=(f.root/"inputs/n.bin").string();watched_read_count=0;trigger_read_count=2;
  after_watched_read=[&]{
   if(target=="deadline"){expired=true;return;}
   if(target=="directory"){write(f.root/"attempts/a-1/output/unexpected.bin","extra");return;}
   const auto path=target=="bundle"?f.bundle:f.root/"control/run-log.jsonl";
   const int fd=::open(path.c_str(),O_WRONLY);check(fd>=0,"mutation open");const auto n=::pwrite(fd,"!",1,0);::close(fd);check(n==1,"mutation write");
  };
  reject(f.bundle);check(watched_read.empty()&&watched_read_count==2,"final mutation did not fire");
  if(target=="deadline"){check(expired&&reads_after_expiry==0,"final read after deadline");expired=false;}
 }
}
}
int main(int argc,char** argv){try{
 auto temporary=(fs::temp_directory_path()/"bruce-payload-inspect-tests-XXXXXX").string();
 check(::mkdtemp(temporary.data())!=nullptr,"temporary test directory");const fs::path base=temporary;
 const std::string group=argc>1?argv[1]:"all";
 if(group=="all"||group=="receipt"){receipt_regressions(base);stream_integration(base);}
 if(group=="all"||group=="stream-finalization")stream_finalization_regressions(base);
 if(group=="all"||group=="retry")retry_regressions(base);
 if(group=="all"||group=="snapshot")final_snapshot_regressions(base);
 if(group!="all"){fs::remove_all(base);std::cout<<group<<" regressions passed\n";return 0;}
 for(bool v14:{false,true}){Fixture f(base/(v14?"v14":"v13"),v14);f.attempt("n","a-1",true);f.finish(true);auto r=artifact_v1::inspect_payload_run_v1(f.bundle);check(r.run_finished&&r.run_status=="READY"&&r.attempt_count==1&&r.accepted_artifact_count==1,"valid payload");auto streams=artifact_v1::inspect_payload_run_v1(f.bundle,{true});check(streams.streams_requested&&streams.verified_stream_count==0&&streams.verified_stream_bytes==0,"absent streams are not historical proof"); }
 {Fixture f(base/"preflight");f.finish();fs::remove(f.root/"attempts");auto r=artifact_v1::inspect_payload_run_v1(f.bundle);check(r.attempt_count==0&&r.run_status=="FAILED","preflight");}
 {Fixture f(base/"publication");f.attempt("n","a-1",false,true);f.finish();auto r=artifact_v1::inspect_payload_run_v1(f.bundle);check(r.accepted_artifact_count==0&&r.run_status=="FAILED","publication");write(f.root/"attempts/a-1/output/result.bin","fake");reject(f.bundle);}
 {Fixture f(base/"retry",false,true);f.attempt("n","a-1-1",false);auto&s=f.states.at("n");s.attempt_id="a-1-2";s.result={};s.result.status=worker_v1::WorkerStatus::FAILED;s.result.failure_code=worker_v1::FailureCode::FAILED;s.result.result_code="TIMEOUT";cli::mark_blocked(f.states.at("m"));f.finish();check(artifact_v1::inspect_payload_run_v1(f.bundle).attempt_count==1,"retry preflight");}
 {Fixture f(base/"incomplete");f.attempt("n","a-1",true);f.finish(true,false);auto r=artifact_v1::inspect_payload_run_v1(f.bundle);check(!r.run_finished&&r.run_status=="INCOMPLETE","incomplete");write(f.root/"control/run-log.jsonl",f.log+"{");reject(f.bundle);}
 {Fixture f(base/"tamper");f.attempt("n","a-1",true);f.finish(true);const auto good=read(f.bundle);for(auto pair:std::vector<std::pair<std::string,std::string>>{{"\"retry\":3","\"retry\":4"},{"\"workers\":1","\"workers\":0"},{"\"accepted\":true","\"accepted\":false"},{"\"status\":\"READY\"","\"status\":\"FAILED\""},{"\"input_bytes\":7","\"input_bytes\":8"},{"\"attempt_id\":\"a-1\"","\"attempt_id\":\"a-2\""}}){auto s=good;auto p=s.find(pair.first);check(p!=std::string::npos,"test replacement absent");s.replace(p,pair.first.size(),pair.second);write(f.bundle,s);reject(f.bundle);}write(f.bundle,good);
  for(auto p:{f.bundle,f.root/"control/run-log.jsonl",f.root/"inputs/n.bin",f.root/"attempts/a-1/output/result.bin"}){auto backup=p.string()+".saved";fs::rename(p,backup);fs::create_symlink(backup,p);reject(f.bundle);fs::remove(p);fs::create_hard_link(backup,p);reject(f.bundle);fs::remove(p);mkfifo(p.c_str(),0600);reject(f.bundle);fs::remove(p);fs::rename(backup,p);}
 }
 {Fixture f(base/"forged");f.finish();auto s=read(f.bundle);auto p=s.find("ARTIFACT_REFERENCE_INVALID");s.replace(p,26,"EXTERNAL_FAKE");write(f.bundle,s);reject(f.bundle);}

 {Fixture f(base/"small-budget");f.options.budget.max_input_bytes=1;f.states.at("n").result.result_code="INPUT_LIMIT_EXCEEDED";f.finish();check(artifact_v1::inspect_payload_run_v1(f.bundle).run_status=="FAILED","failed request remains inspectable");}
 {Fixture f(base/"multi-success",true,true);f.attempt("n","a-1-1",true);f.attempt("m","a-2-1",true);f.finish(true);check(artifact_v1::inspect_payload_run_v1(f.bundle).accepted_artifact_count==2,"multi success");}
 for(const std::string target:{"bundle","log","request","output"}){
  Fixture f(base/("mutation-"+target));f.attempt("n","a-1",true);f.finish(true);
  mutate_on_read=(target=="bundle"?f.bundle:target=="log"?f.root/"control/run-log.jsonl":target=="request"?f.root/"inputs/n.bin":f.root/"attempts/a-1/output/result.bin").string();
  reject(f.bundle);check(mutate_on_read.empty(),"mutation did not fire");
 }
 {Fixture f(base/"deadline");f.attempt("n","a-1",true);f.finish(true);expire_on_read=(f.root/"attempts/a-1/output/result.bin").string();reject(f.bundle);check(expired&&reads_after_expiry==0,"read after deadline");expired=false;}
 fs::remove_all(base);std::cout<<"payload run inspector contracts passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
