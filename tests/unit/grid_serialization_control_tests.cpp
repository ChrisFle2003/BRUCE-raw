#include "bruce/application/execution_session.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::ApplicationRun& run,unsigned expected){
 using namespace bruce;const auto original=pack_application(run);unsigned records=0,digests=0;
 auto packed=pack_application(run,[&]{++digests;},[&]{++records;});
 require(packed==original && records==expected && digests>=2,"Record boundaries missing or bytes changed");
 require(render_application(replay_application(packed))==render_application(run),"Serialized record bundle does not replay");
 for(unsigned stop=1;stop<=records;++stop){unsigned seen=0,hashed=0;bool raised=false;
  try{(void)pack_application(run,[&]{++hashed;},[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){raised=true;}
  require(raised && seen==stop && hashed==0,"Record cancellation swallowed or hashing started after failure");
 }
}
void session(bruce::ApplicationTask task,unsigned records){
 using namespace bruce;
 for(unsigned stop=2;stop<=records+1;++stop){unsigned seen=0;VerificationControl c;
  c.serialization_grid_records=true;c.cancelled=[&]{return ++seen==stop;};bool raised=false;
  try{(void)execute_checked_application(task,{},{},c);}
  catch(const ExecutionSessionError& e){raised=e.failed_at()==ExecutionPhase::serializing && e.cause()=="VerificationCancelled" && e.phases().current()==ExecutionPhase::failed;}
  require(raised && seen==stop,"Session did not stop at a record boundary in SERIALIZING");
 }
}
}
int main(){try{using namespace bruce;
 GridBatchTask t;t.paths={"2,6","2,6"};exercise(evaluate_grid_batch(t),6);session(t,6);
 t.paths.push_back("0,0");t.spec.zero_zero="stop";exercise(evaluate_grid_batch(t),9);
 t.cache_bytes=0;exercise(evaluate_grid_batch(t),9);
 t.query_steps=1;const auto partial=evaluate_grid_batch(t);exercise(partial,6);session(t,6);
 auto cancelled=evaluate_grid_batch(t,[](std::size_t){return true;});exercise(cancelled,4);
 GridResumeTask resume{pack_grid_batch(partial),1};auto first=resume_grid_batch(resume.parent,1);
 exercise(first,6);session(resume,6);
 auto second=resume_grid_batch(pack_grid_resume(first),1);exercise(second,8);
 ExactResearchTask exact;exact.domain="rational";exact.strategy="polynomial";exact.planner="interpolate";exact.max_degree=2;
 GridBatchTask source;source.paths={"0,2","1,2","2,2"};
 GridBatchTask holdout;holdout.paths={"3,2"};
 GridResearchTask research{exact,pack_grid_batch(evaluate_grid_batch(source)),pack_grid_batch(evaluate_grid_batch(holdout)),"x",{}};
 const auto research_run=std::get<GridResearchRun>(execute_application(research));
 const auto research_plain=pack_application(research_run);unsigned research_records=0,research_digests=0;
 const auto research_checked=pack_application(research_run,[&]{++research_digests;},[&]{++research_records;});
 require(research_checked==research_plain && research_records>=3 && research_digests>=2,"Research record boundaries missing or bytes changed");
 require(render_application(replay_application(research_checked))==render_application(research_run),"Research record bundle does not replay");
 for(unsigned stop=1;stop<=research_records;++stop){unsigned seen=0,hashed=0;bool raised=false;
  try{(void)pack_application(research_run,[&]{++hashed;},[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){raised=true;}
  require(raised && seen==stop && hashed==0,"Research record cancellation swallowed or hashing started after failure");
 }
 session(research,research_records);
 VerificationControl c;c.serialization_grid_records=true;bool rejected=false;
 try{(void)execute_checked_application(t,{},{},c);}
 catch(const ExecutionSessionError& e){rejected=e.cause()=="SerializationCheckpointsRequireCallback";}
 require(rejected,"Missing record callback accepted");
 c.cancelled=[]{return false;};c.serialization_digest_checkpoints=true;rejected=false;
 try{(void)execute_checked_application(t,{},{},c);}
 catch(const ExecutionSessionError& e){rejected=e.cause()=="ConflictingSerializationCheckpoints";}
 require(rejected,"Conflicting serialization modes accepted");
 GridTask single;single.path="2,6";
 rejected=false;try{(void)pack_application(evaluate_grid(single),{},[]{});}
 catch(const Error& e){rejected=std::string(e.what())=="SerializationRecordsRequireBatchOrResume";}
 require(rejected,"Unsupported direct record callback silently ignored");
 std::cout<<"Grid record serialization: full, partial, cache, sentinel, cancellation and continuation verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
