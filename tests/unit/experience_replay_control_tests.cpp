#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise_store(const std::string& snapshot){
 using namespace bruce;unsigned total=0;
 const auto expected=render_experience(read_experience(snapshot));
 require(render_experience(read_experience(snapshot,[&]{++total;}))==expected,"Observer changed votes, chain or audit counters");
 require(total>4,"Missing nested source checkpoints");
 for(unsigned stop=1;stop<=total;++stop){unsigned seen=0;bool caught=false;
  try{(void)read_experience(snapshot,[&]{if(++seen==stop)throw Error("ReplayOperationLimit");});}
  catch(const Error& e){caught=std::string(e.what())=="ReplayOperationLimit";}
  require(caught && seen==stop,"Source audit translated callback exception or continued");
 }
 auto corrupt=snapshot;corrupt.back()^=1;bool rejected=false;unsigned seen=0;
 try{(void)read_experience(corrupt,[&]{++seen;});}
 catch(const Error& e){rejected=std::string(e.what())=="InvalidExperienceDigest";}
 require(rejected && seen==0,"Observer bypassed experience digest validation");
}
}
int main(){try{using namespace bruce;
 ExactResearchTask source;source.domain="rational";source.strategy="portfolio-rational";source.planner="portfolio";
 source.minimum="-1";source.maximum="1";source.max_power=2;source.max_degree=2;
 source.training={{0,0},{1,1},{2,2}};source.holdout={{3,3}};
 auto original=pack_exact_research(exact_research(source));
 auto snapshot=append_experience("",original);
 auto learned=source;learned.planner="experience-order";learned.experience_snapshot=snapshot;learned.cancel_after_checkpoints=0;
 const auto bundle=pack_exact_research(exact_research(learned));
 unsigned origin_calls=0,calls=0;
 (void)replay_application(original,[&]{++origin_calls;});
 require(pack_application(replay_application(bundle,[&]{++calls;}))==bundle,"Experience observer changed native result");
 require(calls>=origin_calls+4,"Learned replay does not forward observer into experience source audit");
 exercise_store(snapshot);
 const auto id=read_experience(snapshot).records.front().id;
 auto replacement=source;replacement.holdout.push_back({4,4});
 const auto corrected=replace_experience(snapshot,id,"additional holdout",pack_exact_research(exact_research(replacement)));
 exercise_store(corrected);
 const auto revoked=revoke_experience(corrected,read_experience(corrected).records.back().id,"withdrawn");
 exercise_store(revoked);
 GridBatchTask grid;grid.paths={"0,1","1,1","2,1"};GridBatchTask hold;hold.paths={"3,1"};
 auto gridtask=source;gridtask.training.clear();gridtask.holdout.clear();
 const auto gridorigin=pack_grid_research(research_from_grid(gridtask,pack_grid_batch(evaluate_grid_batch(grid)),pack_grid_batch(evaluate_grid_batch(hold))));
 exercise_store(append_experience("",gridorigin));
 for(unsigned stop=1;stop<=calls;++stop){unsigned seen=0;bool caught=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){caught=true;}
  require(caught && seen==stop,"Learned replay swallowed numeric callback exception");
 }
 unsigned checks=0,native=0;bool cancelled=false;
 try{(void)execute_checked_application(learned,{[&]{++native;return false;}},{},{[&]{return ++checks==12;},false,true});}
 catch(const ExecutionSessionError& e){cancelled=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled";}
 require(cancelled && checks==12 && native==0,"Session did not interrupt nested audit independently of saved native cancellation");
 // An actual oversized source must still receive the normal admission error.
 auto oversized=exact_research(source);oversized.task.budget.operations=1000001;
 bool limit=false;try{(void)append_experience("",pack_exact_research(oversized));}
 catch(const Error& e){limit=std::string(e.what())=="ExperienceSourceOperationLimit";}
 require(limit,"Real source admission error changed");
 std::cout<<"Experience source replay checkpoints reached\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
