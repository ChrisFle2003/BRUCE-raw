#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
std::string grid(std::vector<std::string> paths){bruce::GridBatchTask t;t.paths=std::move(paths);return bruce::pack_grid_batch(bruce::evaluate_grid_batch(t));}
void exercise(const bruce::GridResearchTask& task){
 using namespace bruce;unsigned source_points=0;
 auto train=replay_grid_source(task.training,[&]{++source_points;});
 source_points+=train.queries.size();
 if(!task.holdout.empty()){auto hold=replay_grid_source(task.holdout,[&]{++source_points;});source_points+=hold.queries.size();}
 const auto original=execute_checked_application(task);unsigned seen=0;
 ExecutionControl c;c.grid_source_cancelled=[&]{++seen;return false;};
 const auto checked=execute_checked_application(task,c);
 require(checked.bundle==original.bundle && seen==source_points,"Grid source callback changed evidence or leaked beyond import");
 for(unsigned stop=1;stop<=source_points;++stop){unsigned calls=0,native=0;bool caught=false;
  c.cancelled=[&]{++native;return false;};c.grid_source_cancelled=[&]{return ++calls==stop;};
  try{(void)execute_checked_application(task,c);}catch(const ExecutionSessionError& e){caught=e.failed_at()==ExecutionPhase::executing && e.cause()=="ExecutionGridSourceCancelled";}
  require(caught && calls==stop && native==0,"Source cancellation continued into research or wrong phase");
 }
 unsigned calls=0;c.cancelled={};c.grid_source_cancelled=[&]() -> bool {if(++calls==2)throw NumericLimit();return false;};
 bool preserved=false;try{(void)execute_application(task,c);}catch(const NumericLimit&){preserved=true;}
 require(preserved && calls==2,"Source callback exception changed");
}
}
int main(){try{using namespace bruce;
 ExactResearchTask exact;exact.domain="rational";exact.strategy="polynomial";exact.planner="interpolate";exact.max_degree=2;
 GridResearchTask t{exact,grid({"0,2","1,2","2,2"}),grid({"3,2"}),"x",{}};exercise(t);
 t.total_operations=t.training.size()+t.holdout.size()+10000;exercise(t);t.total_operations.reset();
 auto y=t;y.training=grid({"1,0","1,1","1,2"});y.holdout=grid({"1,3"});y.axis="y";exercise(y);
 GridBatchTask partial;partial.paths={"0,2","1,2","2,2"};partial.query_steps=1;
 t.training=pack_grid_resume(resume_grid_batch(pack_grid_batch(evaluate_grid_batch(partial)),2));exercise(t);
 unsigned calls=0;ExecutionControl c;c.grid_source_cancelled=[&]{++calls;return false;};
 auto denied=t;denied.total_operations=1;bool budget=false;
 try{(void)execute_checked_application(denied,c);}catch(const ExecutionSessionError& e){budget=e.cause()=="InsufficientGridImportBudget";}
 require(budget && calls==0,"Grid budget admission moved behind source replay");
 auto incomplete=t;incomplete.training=pack_grid_batch(evaluate_grid_batch(partial));bool rejected=false;
 try{(void)execute_checked_application(incomplete,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="IncompleteGridSource";}
 require(rejected,"Incomplete source admitted with callback");
 auto overlap=t;overlap.holdout=grid({"2,2"});rejected=false;
 try{(void)execute_checked_application(overlap,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="DuplicateOrOverlappingGridInput";}
 require(rejected,"Projection validation bypassed");
 ExactResearchTask p;p.domain="rational";p.strategy="portfolio-rational";p.planner="portfolio";
 p.minimum="-1";p.maximum="1";p.max_power=2;p.max_degree=2;p.training={{0,0},{1,1},{2,2}};p.holdout={{3,3}};
 const auto snapshot=append_experience("",pack_exact_research(exact_research(p)));
 p.training.clear();p.holdout.clear();p.planner="experience-order";p.experience_snapshot=snapshot;
 GridResearchTask learned{p,grid({"4,1","5,1","6,1"}),grid({"7,1"}),"x",{}};
 unsigned sources=0,experience=0,native=0;ExecutionControl both{[&]{++native;return false;},[&]{++experience;return false;},[&]{++sources;return false;}};
 auto complete=execute_checked_application(learned,both);
 require(sources>0 && experience>0 && native>0 && render_execution_session(complete).find("grid-and-experience-checkpoints")!=std::string::npos,"Combined imports not executed");
 sources=experience=native=0;both.grid_source_cancelled=[&]{++sources;return true;};bool stopped=false;
 try{(void)execute_checked_application(learned,both);}catch(const ExecutionSessionError& e){stopped=e.cause()=="ExecutionGridSourceCancelled";}
 require(stopped && sources==1 && experience==0 && native==0,"Source cancellation reached experience import");
 both.grid_source_cancelled=[]{return false;};both.experience_cancelled=[]{return true;};stopped=false;
 try{(void)execute_checked_application(learned,both);}catch(const ExecutionSessionError& e){stopped=e.cause()=="ExecutionExperienceCancelled" && e.failed_at()==ExecutionPhase::executing;}
 require(stopped && native==0,"Combined mode lost experience cancellation");
 std::cout<<"Grid import checkpoints: X/Y, resumed sources, budgets, projection and combined imports verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
