#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::ApplicationTask& task,const std::string& snapshot){
 using namespace bruce;unsigned expected=0;
 (void)read_experience(snapshot,[&]{++expected;});
 const auto plain=execute_checked_application(task);
 unsigned observed=0;ExecutionControl c;c.experience_cancelled=[&]{++observed;return false;};
 const auto checked=execute_checked_application(task,c);
 require(checked.bundle==plain.bundle && observed==expected,"Import callback changed evidence or leaked into verification");
 require(render_execution_session(checked).find("application-execution-v10")!=std::string::npos,"Missing execution import report");
 for(unsigned stop=1;stop<=expected;++stop){unsigned seen=0,native=0;bool raised=false;
  c.cancelled=[&]{++native;return false;};c.experience_cancelled=[&]{return ++seen==stop;};
  try{(void)execute_checked_application(task,c);}
  catch(const ExecutionSessionError& e){raised=e.failed_at()==ExecutionPhase::executing && e.cause()=="ExecutionExperienceCancelled" && e.phases().current()==ExecutionPhase::failed;}
  require(raised && seen==stop && native==0,"Import cancellation continued into native search or wrong phase");
 }
 c.cancelled={};unsigned calls=0;c.experience_cancelled=[&]() -> bool {if(++calls==4)throw NumericLimit();return false;};
 bool numeric=false;try{(void)execute_application(task,c);}catch(const NumericLimit&){numeric=true;}
 require(numeric && calls==4,"Import swallowed native callback exception");
 calls=0;c.experience_cancelled=[&]() -> bool {if(++calls==4)throw Error("ReplayOperationLimit");return false;};
 bool original=false;try{(void)execute_application(task,c);}catch(const Error& e){original=std::string(e.what())=="ReplayOperationLimit";}
 require(original,"Import translated caller exception into source limit");
}
}
int main(){try{using namespace bruce;
 ExactResearchTask source;source.domain="rational";source.strategy="portfolio-rational";source.planner="portfolio";
 source.minimum="-1";source.maximum="1";source.max_power=2;source.max_degree=2;
 source.training={{0,0},{1,1},{2,2}};source.holdout={{3,3}};
 const auto snapshot=append_experience("",pack_exact_research(exact_research(source)));
 auto learned=source;learned.planner="experience-order";learned.experience_snapshot=snapshot;
 learned.training={{4,4},{5,5},{6,6}};learned.holdout={{7,7}};
 require(exact_research(learned).experience_records_used==1,"Independent fixture does not use experience");
 exercise(learned,snapshot);
 auto partial=learned;partial.cancel_after_checkpoints=0;exercise(partial,snapshot);
 auto replacement=source;replacement.holdout.push_back({4,4});
 auto corrected=replace_experience(snapshot,read_experience(snapshot).records.front().id,"more holdout",pack_exact_research(exact_research(replacement)));
 learned.experience_snapshot=corrected;exercise(learned,corrected);
 GridBatchTask train;train.paths={"4,1","5,1","6,1"};GridBatchTask hold;hold.paths={"7,1"};
 auto gridtask=learned;gridtask.training.clear();gridtask.holdout.clear();
 GridResearchTask grid{gridtask,pack_grid_batch(evaluate_grid_batch(train)),pack_grid_batch(evaluate_grid_batch(hold)),"x",{}};
 exercise(grid,corrected);grid.total_operations=grid.training.size()+grid.holdout.size()+corrected.size()+10000;exercise(grid,corrected);
 // Search cancellation still creates its native partial after a completed import.
 unsigned imported=0,native=0;ExecutionControl stop{[&]{++native;return true;},[&]{++imported;return false;}};
 const auto run=std::get<ExactResearchRun>(execute_application(learned,stop));
 require(imported>0 && native==1 && run.stop_reason=="Cancelled","Native cancellation contract changed");
 for(ApplicationTask wrong:{ApplicationTask{source},ApplicationTask{GridTask{}}}){
  unsigned called=0;bool rejected=false;ExecutionControl c;c.experience_cancelled=[&]{++called;return true;};
  try{(void)execute_checked_application(wrong,c);}catch(const ExecutionSessionError& e){rejected=e.failed_at()==ExecutionPhase::executing && e.cause()=="ExecutionExperienceCheckpointsRequireLearnedResearch";}
  require(rejected && called==0,"Wrong execution type reached import callback");
 }
 std::cout<<"Experience import cancellation: every checkpoint, independent native search, corrections and Grid budgets verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
