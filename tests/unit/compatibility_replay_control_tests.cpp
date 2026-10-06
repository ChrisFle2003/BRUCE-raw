#include "bruce/application/execution_session.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::CompatibilityRun& run){
 using namespace bruce;const auto bundle=pack_compatibility(run);unsigned total=0;
 require(pack_application(replay_application(bundle,[&]{++total;}))==bundle,"Observer changed compatibility evidence");
 require(total>=4,"Missing compatibility checkpoints");
 if(!run.task.origin.empty()){
  unsigned nested=0;(void)replay_application(run.task.origin,[&]{++nested;});
  require(total>=nested+4,"Compatibility failed to forward nested replay checkpoints");
 }
 for(unsigned stop=1;stop<=total;++stop){
  unsigned seen=0;bool raised=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){raised=true;}
  require(raised && seen==stop,"Callback exception swallowed or replay continued");
 }
 const auto first=bundle.find('\n'),second=bundle.find('\n',first+1);
 const auto body=bundle.substr(second+1)+"tampered\n";bool rejected=false;
 try{(void)replay_application(bundle.substr(0,first+1)+sha256(body)+"\n"+body,[]{});}
 catch(const Error& e){rejected=std::string(e.what()).starts_with("ReplayMismatch:");}
 require(rejected,"Observer bypassed compatibility evidence check");
}
}
int main(){try{using namespace bruce;
 CompatibilityTask t;t.rules="BRUCE_RULES_V1\nsnapshot test\nrule even positive value mod_eq 2 0 5 manual -\n";
 t.candidates="id,domain,layer,x,y,value\n1,math,0,0,0,2\n2,math,0,1,0,3\n";
 const auto full=run_compatibility(t);require(full.result.selection.ids==std::vector<std::uint64_t>{1},"Expected even candidate selected");exercise(full);
 auto partial=t;partial.budget.checks=1;exercise(run_compatibility(partial));
 partial=t;partial.budget.candidates=1;exercise(run_compatibility(partial));
 partial=t;partial.budget.trace=1;exercise(run_compatibility(partial));
 for(unsigned stop:{0U,1U})exercise(run_compatibility(t,[&](std::uint64_t n){return n==stop;}));
 auto origin=t;origin.candidates.clear();origin.at=2;
 Task legacy;legacy.minimum=-1;legacy.maximum=1;legacy.training={{0,0},{1,1}};
 origin.origin=pack_run(research(legacy));exercise(run_compatibility(origin));
 ExactResearchTask exact;exact.minimum="-1";exact.maximum="1";exact.training={{0,0},{1,1}};
 origin.origin=pack_exact_research(exact_research(exact));exercise(run_compatibility(origin));
 exact.training.clear();
 GridBatchTask grid;grid.paths={"0,1","1,1","2,1"};
 origin.origin=pack_grid_research(research_from_grid(exact,pack_grid_batch(evaluate_grid_batch(grid))));exercise(run_compatibility(origin));
 auto forged=run_compatibility(origin);forged.task.candidates="id,domain,layer,x,y,value\n999,math,0,0,0,2\n";
 bool binding=false;try{(void)replay_application(pack_compatibility(forged),[]{});}
 catch(const Error& e){binding=std::string(e.what())=="ResearchProjectionMismatch";}
 require(binding,"Observer bypassed origin projection binding");
 unsigned native=0,checks=0;bool stopped=false;
 try{(void)execute_checked_application(t,{[&]{++native;return false;}},{},{[&]{return ++checks==6;},false,false,false,true});}
 catch(const ExecutionSessionError& e){stopped=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled" && e.phases().current()==ExecutionPhase::failed;}
 require(stopped && checks==6 && native==2,"Session did not stop at first replay candidate independently");
 bool missing=false;try{(void)execute_checked_application(t,{},{},{{},false,false,false,true});}
 catch(const ExecutionSessionError& e){missing=e.cause()=="CompatibilityReplayCheckpointsRequireCallback";}
 require(missing,"Missing observer accepted");
 for(unsigned flag=0;flag<3;++flag){VerificationControl c{[]{return false;},flag==0,flag==1,flag==2,true};
  bool rejected=false;try{(void)execute_checked_application(t,{},{},c);}
  catch(const ExecutionSessionError& e){rejected=e.cause()=="ConflictingReplayCheckpoints";}
  require(rejected,"Conflicting observer modes accepted");
 }
 std::cout<<"Compatibility replay: complete, partial, cancellation, three origins, binding and every callback checked\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
