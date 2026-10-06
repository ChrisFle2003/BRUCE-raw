#include "bruce/application/execution_session.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const std::string& parent,std::size_t additional){
 using namespace bruce;unsigned replay_calls=0;
 const auto prefix=replay_grid_source(parent,[&]{++replay_calls;});
 const auto expected=replay_calls+prefix.queries.size();
 GridResumeTask task{parent,additional};const auto plain=execute_checked_application(task);
 unsigned seen=0;ExecutionControl c;c.resume_import_cancelled=[&]{++seen;return false;};
 const auto checked=execute_checked_application(task,c);
 require(checked.bundle==plain.bundle && seen==expected,"Resume import changed evidence, skipped cache rebuild or leaked into new queries/replay");
 for(unsigned stop=1;stop<=expected;++stop){unsigned calls=0,native=0;bool raised=false;
  c.cancelled=[&]{++native;return false;};c.resume_import_cancelled=[&]{return ++calls==stop;};
  try{(void)execute_checked_application(task,c);}catch(const ExecutionSessionError& e){raised=e.failed_at()==ExecutionPhase::executing && e.cause()=="ExecutionResumeImportCancelled";}
  require(raised && calls==stop && native==0,"Import cancellation reached new queries or wrong phase");
 }
 c.cancelled={};unsigned calls=0;c.resume_import_cancelled=[&]() -> bool {if(++calls==expected)throw NumericLimit();return false;};
 bool preserved=false;try{(void)execute_application(task,c);}catch(const NumericLimit&){preserved=true;}
 require(preserved && calls==expected,"Cache rebuild swallowed callback exception");
 calls=0;unsigned native=0;c.resume_import_cancelled=[&]{++calls;return false;};c.cancelled=[&]{++native;return true;};
 const auto stopped=std::get<GridResumeRun>(execute_application(task,c));
 require(calls==expected && native==1 && stopped.batch.stop_reason=="Cancelled" && stopped.batch.queries.size()==prefix.queries.size(),"Native query cancellation changed");
 require(pack_grid_resume(replay_grid_resume(pack_grid_resume(stopped)))==pack_grid_resume(stopped),"Native stopped continuation lost replay");
}
}
int main(){try{using namespace bruce;
 GridBatchTask t;t.paths={"2,6","2,6","4,3","0,0","1,1"};t.spec.zero_zero="stop";t.query_steps=1;
 auto parent=pack_grid_batch(evaluate_grid_batch(t));exercise(parent,1);exercise(parent,4);
 auto next=pack_grid_resume(resume_grid_batch(parent,1));exercise(next,1);exercise(next,3);
 t.cache_bytes=0;exercise(pack_grid_batch(evaluate_grid_batch(t)),2);
 t.query_steps=0;exercise(pack_grid_batch(evaluate_grid_batch(t)),2);
 auto cancelled=evaluate_grid_batch(t,[](std::size_t){return true;});exercise(pack_grid_batch(cancelled),2);
 unsigned calls=0;ExecutionControl c;c.resume_import_cancelled=[&]{++calls;return false;};
 auto invalid=parent;invalid.back()^=1;bool rejected=false;
 try{(void)execute_checked_application(GridResumeTask{invalid,1},c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="InvalidEvidence: digest mismatch";}
 require(rejected && calls==0,"Corrupt parent admitted or callback preceded root digest check");
 calls=0;rejected=false;try{(void)execute_checked_application(GridResumeTask{parent,0},c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="InvalidGridAdditionalQueries";}
 require(rejected && calls==0,"Additional-query admission changed");
 t.query_steps.reset();rejected=false;try{(void)execute_checked_application(GridResumeTask{pack_grid_batch(evaluate_grid_batch(t)),1},c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="GridResumeRequiresIncompleteSource";}
 require(rejected,"Completed parent accepted");
 GridBatchTask deep;deep.paths=std::vector<std::string>(16,"2,6");deep.query_steps=0;
 auto chain=pack_grid_batch(evaluate_grid_batch(deep));for(unsigned i=0;i<8;++i)chain=pack_grid_resume(resume_grid_batch(chain,1));
 rejected=false;try{(void)execute_checked_application(GridResumeTask{chain,1},c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="GridResumeGenerationLimit";}
 require(rejected,"Generation limit bypassed");
 calls=0;rejected=false;try{(void)execute_checked_application(t,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="ExecutionResumeImportRequiresResumeTask";}
 require(rejected && calls==0,"Wrong task reached import callback");
 std::cout<<"Resume import: parent chains, cache rebuild, every checkpoint, native stops and admission guards verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
