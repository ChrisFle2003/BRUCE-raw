#include "bruce/application/execution_session.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
#include <set>
#include <filesystem>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const std::string& bundle){
 using namespace bruce;
 std::size_t total=0;
 const auto expected=replay_application(bundle);
 const auto checked=replay_application(bundle,[&]{++total;});
 require(total>2,"Research replay has no checkpoints");
 require(pack_application(checked)==bundle && render_application(checked)==render_application(expected),"Observer changed research evidence");
 for(auto stop:std::set<std::size_t>{1,(total+1)/2,total}){
  std::size_t seen=0;bool interrupted=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){interrupted=true;}
  require(interrupted && seen==stop,"Research swallowed callback exception as mathematical limit");
 }
 const auto first=bundle.find('\n'),second=bundle.find('\n',first+1);
 const auto body=bundle.substr(second+1)+"tampered\n";
 const auto forged=bundle.substr(0,first+1)+sha256(body)+"\n"+body;
 bool rejected=false;
 try{(void)replay_application(forged,[]{});}catch(const Error& e){rejected=std::string(e.what()).starts_with("ReplayMismatch:");}
 require(rejected,"Research checkpoint bypassed evidence comparison");
}
}
int main(int argc,char** argv){try{
 using namespace bruce;
 Task legacy;legacy.minimum=0;legacy.maximum=1;legacy.training={{0,1},{1,2}};legacy.holdout={{2,3}};
 exercise(pack_run(research(legacy)));
 exercise(pack_run(research(legacy,[](std::uint64_t n){return n>=1;})));
 auto zero=legacy;zero.cancel_after_candidates=0;exercise(pack_run(research(zero)));
 ExactResearchTask source;source.domain="rational";source.minimum="0";source.maximum="1";
 source.training={{0,1},{1,2},{2,3}};source.holdout={{3,4}};
 source.strategy="portfolio-rational";source.planner="portfolio";source.max_power=2;source.max_degree=2;
 const auto snapshot=append_experience("",pack_exact_research(exact_research(source)));
 unsigned routes=0;
 for(const auto& strategy:research_strategies())for(auto planner:strategy.planners){
  ExactResearchTask t;t.domain="rational";t.minimum="0";t.maximum="1";t.training=source.training;t.holdout=source.holdout;
  t.strategy=strategy.id;t.planner=planner;t.max_nodes=3;t.budget.operations=3000;t.budget.candidates=1000;
  if(t.strategy=="power-affine" || t.strategy.starts_with("portfolio"))t.max_power=2;
  if(t.strategy=="polynomial" || t.strategy.starts_with("portfolio"))t.max_degree=2;
  if(t.strategy=="portfolio-expression")t.expression_equivalence="rational";
  if(planner=="experience-order")t.experience_snapshot=snapshot;
  auto run=exact_research(t);exercise(pack_exact_research(run));
  exercise(pack_exact_research(exact_research(t,[](std::uint64_t n){return n>=1;})));
  ++routes;
 }
 unsigned native_calls=0,checks=0;bool stopped=false;
 try{(void)execute_checked_application(legacy,{[&]{++native_calls;return false;}},{},{[&]{return ++checks==8;},false,true});}
 catch(const ExecutionSessionError& e){stopped=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled";}
 require(stopped && checks==8 && native_calls==4,"Research session did not stop inside replay independently of native search");
 bool conflict=false;try{(void)execute_checked_application(legacy,{},{},{[]{return false;},true,true});}
 catch(const ExecutionSessionError& e){conflict=e.cause()=="ConflictingReplayCheckpoints";}
 require(conflict,"Conflicting replay modes accepted");
 bool missing=false;try{(void)execute_checked_application(legacy,{},{},{{},false,true});}
 catch(const ExecutionSessionError& e){missing=e.cause()=="ResearchReplayCheckpointsRequireCallback";}
 require(missing,"Missing research callback ignored");
 const auto source_bundle=pack_exact_research(exact_research(source));
 bool admitted=false,limited=false;
 try{(void)replay_exact_research(source_bundle,0,[&](const auto&){admitted=true;},[]{});}
 catch(const Error& e){limited=std::string(e.what())=="ReplayOperationLimit";}
 require(limited && !admitted,"Checkpoint bypassed operation limit before admission");
 bool admission_rejected=false;
 try{(void)replay_exact_research(source_bundle,10000000,[](const auto&){throw Error("AdmissionRejected");},[]{});}
 catch(const Error& e){admission_rejected=std::string(e.what())=="AdmissionRejected";}
 require(admission_rejected,"Checkpoint bypassed task admission");
 require(argc==2,"Expected historical fixture directory");unsigned historical=0;
 for(const auto& path:std::filesystem::directory_iterator(argv[1])){
  if(path.path().extension()!=".bruce")continue;
  const auto bundle=read_bounded(path.path().string());const auto expected=replay_application(bundle);
  if(!std::holds_alternative<Run>(expected) && !std::holds_alternative<ExactResearchRun>(expected))continue;
  std::size_t checkpoints=0;const auto checked=replay_application(bundle,[&]{++checkpoints;});
  require(checkpoints>0 && render_application(checked)==render_application(expected),"Historical research callback replay differs");
  ++historical;
 }
 require(historical>0,"No historical research fixtures tested");
 std::cout<<routes<<" strategy/planner routes and "<<historical<<" historical fixtures preserve research replay\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
