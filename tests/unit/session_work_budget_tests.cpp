#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>
#include <limits>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::ApplicationTask& task,bool every=true){
 using namespace bruce;
 const auto plain=execute_checked_application(task);
 const auto checked=execute_checked_application(task,{},{},{},{{1000000}});
 const auto& u=checked.session_usage;
 require(u.max_checkpoints==1000000 && u.checkpoints==u.executing+u.serializing+u.replaying,"Usage sum/limit wrong");
 require(plain.bundle==checked.bundle && render_application(plain.run)==render_application(checked.run),"Budget changed mathematical evidence");
 require(!plain.session_usage.max_checkpoints,"Unrequested budget enabled");
 // Independent instrumentation of existing callback routes is the counting oracle.
 std::uint64_t native=0,serial=0,replayed=0;
 ExecutionControl c;c.cancelled=[&]{++native;return false;};
 if(const auto* exact=std::get_if<ExactResearchTask>(&task);exact && exact->planner=="experience-order")c.experience_cancelled=c.cancelled;
 if(const auto* grid=std::get_if<GridResearchTask>(&task)){
  c.grid_source_cancelled=c.cancelled;
  if(grid->research.planner=="experience-order")c.experience_cancelled=c.cancelled;
 }
 if(const auto* z=std::get_if<CompatibilityTask>(&task);z && !z->origin.empty())c.compatibility_source_cancelled=c.cancelled;
 if(std::holds_alternative<GridResumeTask>(task))c.resume_import_cancelled=c.cancelled;
 const auto observed=execute_application(task,c);
 require(pack_application(observed)==plain.bundle,"Counting callbacks changed native result");
 ReplayCheckpoint digest=[&]{++serial;};
 auto bundle=pack_application(plain.run,digest,(std::holds_alternative<GridBatchTask>(task)||std::holds_alternative<GridResumeTask>(task)||std::holds_alternative<GridResearchTask>(task)||std::holds_alternative<ExactResearchTask>(task)||std::holds_alternative<CompatibilityTask>(task)||std::holds_alternative<FractionComparisonTask>(task)) ? digest : ReplayCheckpoint{});
 if(!std::holds_alternative<GridTask>(task))(void)replay_application(bundle,[&]{++replayed;});
 require(u.executing==1+native && u.serializing==1+serial && u.replaying==2+replayed,"An existing checkpoint route bypassed shared budget");
 // Equal limit succeeds, the next attempted point fails without a partial result.
 require(execute_checked_application(task,{},{},{},{{u.checkpoints}}).bundle==plain.bundle,"Exact budget rejected");
 for(std::uint64_t limit=0;limit<u.checkpoints;++limit){
  if(!every && limit!=0 && limit!=1 && limit!=u.executing && limit!=u.executing+u.serializing && limit!=u.checkpoints-1)continue;
  bool rejected=false;
  try{(void)execute_checked_application(task,{},{},{},{{limit}});}
  catch(const ExecutionSessionError& e){
   rejected=true;
   const auto phase=limit<u.executing ? ExecutionPhase::executing : limit<u.executing+u.serializing ? ExecutionPhase::serializing : ExecutionPhase::replaying;
   require(e.failed_at()==phase && e.phases().current()==ExecutionPhase::failed,"Wrong shared budget failure phase");
   require(e.cause().starts_with("SessionCheckpointBudgetExceeded:"),"Budget exhaustion became mathematical failure");
   const auto& failed=e.session_usage();
   require(failed.checkpoints==limit && failed.executing+failed.serializing+failed.replaying==limit,"Failed work consumed or phase counter reset");
   bool nested=false;try{std::rethrow_if_nested(e);}catch(const Error& original){nested=std::string(original.what())==e.cause();}
   require(nested,"Original budget error lost");
  }
  require(rejected,"Shared budget accepted insufficient limit");
 }
}
std::string grid(std::vector<std::string> paths){bruce::GridBatchTask t;t.paths=std::move(paths);return bruce::pack_grid_batch(bruce::evaluate_grid_batch(t));}
}
int main(){try{using namespace bruce;
 Task legacy;legacy.minimum=-1;legacy.maximum=1;legacy.training={{0,0},{1,1}};exercise(legacy);
 ExactResearchTask exact;exact.minimum="-1";exact.maximum="1";exact.training={{0,0},{1,1}};exercise(exact);
 GridTask one;one.path="2,6";exercise(one);
 GridBatchTask batch;batch.paths={"0,2","1,2","2,2"};exercise(batch);
 auto partial=batch;partial.query_steps=1;exercise(partial);
 GridResumeTask resume{pack_grid_batch(evaluate_grid_batch(partial)),1};exercise(resume);
 resume.parent=pack_grid_resume(resume_grid_batch(resume.parent,1));exercise(resume);
 auto polynomial=exact;polynomial.training.clear();polynomial.domain="rational";polynomial.strategy="polynomial";polynomial.planner="interpolate";polynomial.max_degree=2;
 GridResearchTask gr{polynomial,grid({"0,2","1,2","2,2"}),grid({"3,2"}),"x",{}};exercise(gr);
 FractionComparisonTask fraction;fraction.left_numerator="0,1";fraction.right_numerator="0,1";exercise(fraction);
 fraction.domain_policy="modular";exercise(fraction);fraction.domain_policy="modular-witness";exercise(fraction);
 CompatibilityTask z;z.rules="BRUCE_RULES_V1\nsnapshot budget\n";z.origin=pack_exact_research(exact_research(exact));z.at=2;exercise(z);
 z.origin.clear();z.at=0;z.candidates="id,domain,layer,x,y,value\n1,math,0,0,0,2\n";exercise(z);
 ExactResearchTask learned;learned.domain="rational";learned.strategy="portfolio-rational";learned.planner="portfolio";
 learned.minimum="-1";learned.maximum="1";learned.max_power=2;learned.max_degree=2;learned.training={{0,0},{1,1},{2,2}};learned.holdout={{3,3}};
 learned.experience_snapshot=append_experience("",pack_exact_research(exact_research(learned)));learned.planner="experience-order";
 learned.training={{4,4},{5,5},{6,6}};learned.holdout={{7,7}};exercise(learned,false);
 learned.training.clear();learned.holdout.clear();gr={learned,grid({"4,1","5,1","6,1"}),grid({"7,1"}),"x",{}};exercise(gr,false);
 z.candidates.clear();z.at=2;z.origin=pack_grid_research(std::get<GridResearchRun>(execute_application(gr)));exercise(z,false);
 // Existing cancellation callbacks retain their number/order; native partials remain replayable.
 unsigned native=0,verify=0;
 ExecutionControl control{[&]{++native;return true;}};
 VerificationControl verification{[&]{++verify;return false;}};
 auto stopped=execute_checked_application(batch,control,{},verification,{{100000}});
 require(native==1 && verify==3 && std::get<GridBatchRun>(stopped.run).stop_reason=="Cancelled","Budget altered cancellation semantics");
 verification.serialization_grid_records=true;verification.grid_replay_checkpoints=true;
 native=verify=0;auto prior=execute_checked_application(batch,control,{},verification);const auto prior_calls=verify;
 native=verify=0;auto both=execute_checked_application(batch,control,{},verification,{{100000}});
 require(verify==prior_calls && native==1 && both.bundle==prior.bundle,"Budget repolled verification callbacks");
 // Session copies its admitted limit; caller cannot relax it during native work.
 SessionBudget mutable_budget{{2}};control.cancelled=[&]{mutable_budget.max_checkpoints=100000;return false;};bool fixed=false;
 try{(void)execute_checked_application(batch,control,{},{},mutable_budget);}catch(const ExecutionSessionError& e){fixed=e.session_usage().max_checkpoints==2;}
 require(fixed,"Caller mutated session budget");
 require(execute_checked_application(one,{},{},{},{{std::numeric_limits<std::uint64_t>::max()}}).session_usage.checkpoints>0,"UINT64_MAX rejected");
 // Work observer exceptions must not be swallowed by numeric-limit handling.
 control={};control.work_checkpoint=[] {throw NumericLimit();};bool numeric=false;
 try{(void)execute_application(fraction,control);}catch(const NumericLimit&){numeric=true;}
 require(numeric,"Native observer exception became mathematical partial result");
 std::cout<<"Shared budget: eight task types, every boundary, nested imports, unchanged evidence and independent cancellation verified\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
