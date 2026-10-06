#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::CompatibilityTask& task){
 using namespace bruce;unsigned expected=0,origin_calls=0;
 (void)run_compatibility(task,{},[&]{++expected;});
 (void)replay_application(task.origin,[&]{++origin_calls;});
 require(expected>=origin_calls+2,"Native compatibility did not forward source checkpoints");
 auto original=execute_checked_application(task);unsigned seen=0;ExecutionControl c;
 c.compatibility_source_cancelled=[&]{++seen;return false;};
 auto checked=execute_checked_application(task,c);
 require(checked.bundle==original.bundle && seen==expected,"Source callback changed evidence or leaked into verification");
 for(unsigned stop=1;stop<=expected;++stop){unsigned calls=0,native=0;bool stopped=false;
  c.cancelled=[&]{++native;return false;};c.compatibility_source_cancelled=[&]{return ++calls==stop;};
  try{(void)execute_checked_application(task,c);}catch(const ExecutionSessionError& e){stopped=e.failed_at()==ExecutionPhase::executing && e.cause()=="ExecutionCompatibilitySourceCancelled";}
  require(stopped && calls==stop && native==0,"Source cancellation reached candidate evaluation or wrong phase");
 }
 c.cancelled={};unsigned calls=0;c.compatibility_source_cancelled=[&]() -> bool {if(++calls==3)throw NumericLimit();return false;};
 bool type=false;try{(void)execute_application(task,c);}catch(const NumericLimit&){type=true;}
 require(type && calls==3,"Nested source changed callback exception type");
}
std::string grid(std::vector<std::string> paths){bruce::GridBatchTask t;t.paths=std::move(paths);return bruce::pack_grid_batch(bruce::evaluate_grid_batch(t));}
}
int main(){try{using namespace bruce;
 CompatibilityTask t;t.rules="BRUCE_RULES_V1\nsnapshot test\nrule even positive value mod_eq 2 0 5 manual -\n";t.at=2;
 Task legacy;legacy.minimum=-1;legacy.maximum=1;legacy.training={{0,0},{1,1}};
 t.origin=pack_run(research(legacy));exercise(t);
 ExactResearchTask exact;exact.minimum="-1";exact.maximum="1";exact.training={{0,0},{1,1}};
 t.origin=pack_exact_research(exact_research(exact));exercise(t);
 auto partial=t;partial.budget.cancel_after_candidates=0;exercise(partial);
 exact.training.clear();t.origin=pack_grid_research(research_from_grid(exact,grid({"0,1","1,1","2,1"}),grid({"3,1"})));exercise(t);
 ExactResearchTask p;p.domain="rational";p.strategy="portfolio-rational";p.planner="portfolio";
 p.minimum="-1";p.maximum="1";p.max_power=2;p.max_degree=2;p.training={{0,0},{1,1},{2,2}};p.holdout={{3,3}};
 const auto snapshot=append_experience("",pack_exact_research(exact_research(p)));
 p.planner="experience-order";p.experience_snapshot=snapshot;p.training={{4,4},{5,5},{6,6}};p.holdout={{7,7}};
 t.origin=pack_exact_research(exact_research(p));exercise(t);
 p.training.clear();p.holdout.clear();t.origin=pack_grid_research(research_from_grid(p,grid({"4,1","5,1","6,1"}),grid({"7,1"})));exercise(t);
 unsigned native=0,imports=0;ExecutionControl c;c.cancelled=[&]{++native;return true;};c.compatibility_source_cancelled=[&]{++imports;return false;};
 const auto stopped=std::get<CompatibilityRun>(execute_application(t,c));
 require(imports>0 && native==1 && stopped.result.stop_reason=="Cancelled","Native candidate cancellation changed");
 c.cancelled={};auto forged=t;forged.candidates="id,domain,layer,x,y,value\n999,math,0,0,0,2\n";
 bool rejected=false;try{(void)execute_checked_application(forged,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="ResearchProjectionMismatch";}
 require(rejected,"Source callback bypassed projection binding");
 auto incomplete=legacy;incomplete.budget.operations=1;auto invalid=t;invalid.origin=pack_run(research(incomplete));rejected=false;
 try{(void)execute_checked_application(invalid,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="IncompleteResearchCannotBeRanked";}
 require(rejected,"Incomplete research admitted with source callback");
 auto manual=t;manual.origin.clear();manual.at=0;manual.candidates="id,domain,layer,x,y,value\n1,math,0,0,0,2\n";
 imports=0;rejected=false;try{(void)execute_checked_application(manual,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="ExecutionCompatibilitySourceRequiresOrigin";}
 require(rejected && imports==0,"Manual candidates accepted source-only mode");
 rejected=false;try{(void)execute_checked_application(exact,c);}catch(const ExecutionSessionError& e){rejected=e.cause()=="ExecutionCompatibilitySourceRequiresCompatibilityTask";}
 require(rejected && imports==0,"Wrong task accepted compatibility source callback");
 std::cout<<"Compatibility first-run import: legacy/exact/Grid/experience, every checkpoint and admission guards verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
