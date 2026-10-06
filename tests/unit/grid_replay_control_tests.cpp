#include "bruce/application/execution_session.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
struct Interrupted : bruce::Error {Interrupted():Error("test interruption") {}};
void exercise(const std::string& bundle){
 using namespace bruce;
 std::size_t total=0;
 const auto expected=replay_application(bundle);
 const auto checked=replay_application(bundle,[&]{++total;});
 require(total>3,"No internal Grid replay checkpoints");
 require(pack_application(checked)==bundle && render_application(checked)==render_application(expected),"Checkpoint changed replay evidence");
 for(std::size_t stop=1;stop<=total;++stop){
  std::size_t seen=0;bool interrupted=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw Interrupted();});}
  catch(const Interrupted&){interrupted=true;}
  require(interrupted && seen==stop,"Replay continued after interruption or swallowed original exception");
 }
 // A callback returning normally never bypasses the final evidence comparison.
 const auto first=bundle.find('\n'),second=bundle.find('\n',first+1);
 const auto body=bundle.substr(second+1)+"tampered\n";
 const auto forged=bundle.substr(0,first+1)+sha256(body)+"\n"+body;
 bool rejected=false;
 try{(void)replay_application(forged,[]{});}catch(const Error& e){rejected=std::string(e.what()).starts_with("ReplayMismatch:");}
 require(rejected,"Checkpoint replay accepted tampered evidence");
}
}
int main(){try{
 using namespace bruce;
 GridBatchTask batch;batch.paths={"2,6","2,6","0,0","4,3","3,2"};batch.spec.zero_zero="stop";
 exercise(pack_grid_batch(evaluate_grid_batch(batch)));
 batch.query_steps=1;auto partial=evaluate_grid_batch(batch);exercise(pack_grid_batch(partial));
 batch.cancel_after_queries=0;exercise(pack_grid_batch(evaluate_grid_batch(batch)));
 auto parent=pack_grid_batch(partial);
 for(unsigned generation=0;generation<3;++generation){
  auto resumed=resume_grid_batch(parent,1);parent=pack_grid_resume(resumed);exercise(parent);
 }
 for(std::size_t stop:{1U,2U}){
  auto cancelled=resume_grid_batch(pack_grid_batch(partial),3,[stop](std::size_t n){return n>=stop;});
  require(cancelled.batch.stop_reason=="Cancelled","Native resume cancellation fixture failed");
  exercise(pack_grid_resume(cancelled));
  exercise(pack_grid_resume(resume_grid_batch(pack_grid_resume(cancelled),1)));
 }
 unsigned native_calls=0,checks=0;bool stopped=false;
 try{(void)execute_checked_application(GridResumeTask{parent,1},{[&]{++native_calls;return false;}},{},{[&]{return ++checks==4;},true});}
 catch(const ExecutionSessionError& e){stopped=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled";}
 require(stopped && checks==4 && native_calls==1,"Session did not interrupt inside replay independently of native execution");
 GridTask grid;grid.path="2,6";
 const auto single=pack_grid(evaluate_grid(grid));
 unsigned single_calls=0;
 const auto replayed=replay_application(single,[&]{++single_calls;});
 require(single_calls>=4 && pack_application(replayed)==single,"Single grid checkpoints changed evidence");
 for(unsigned stop=1;stop<=single_calls;++stop){
  unsigned seen=0;bool raised=false;
  try{(void)replay_application(single,[&]{if(++seen==stop)throw Error("SingleGridStop");});}
  catch(const Error& e){raised=std::string(e.what())=="SingleGridStop";}
  require(raised && seen==stop,"Single grid swallowed cancellation");
 }
 bool missing=false;
 try{(void)execute_checked_application(batch,{},{},{{},true});}catch(const ExecutionSessionError& e){missing=e.cause()=="GridReplayCheckpointsRequireCallback";}
 require(missing,"Missing replay callback silently ignored");
 std::cout<<"Grid replay checkpoints preserve evidence and interrupt all tested batch/resume boundaries\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
