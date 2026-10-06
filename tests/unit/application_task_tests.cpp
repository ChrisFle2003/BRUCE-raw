#include "bruce/application/application_task.hpp"
#include "bruce/application/execution_session.hpp"
#include <iostream>
#include <type_traits>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
template<class Task,class Run,class Render,class Pack>
void same(Task t,Run native,Render render,Pack pack){
 const auto native_bundle=[&]{
  if constexpr(std::is_same_v<Task,bruce::GridBatchTask> || std::is_same_v<Task,bruce::GridResumeTask>)return pack(native,{},{});
  else return pack(native,{});
 }();
 const auto gates=bruce::evaluate_application_gates(native_bundle);
 require(gates.kind==bruce::application_kind(bruce::ApplicationRun(native)),"Gate report kind mismatch");
 require(gates.replay_checked,"Gate report not replay checked");
 const bool research=std::is_same_v<Run,bruce::Run> || std::is_same_v<Run,bruce::ExactResearchRun> || std::is_same_v<Run,bruce::GridResearchRun>;
 require(gates.applicable==research,"Gate applicability mismatch");
 require(!gates.learn_promotion && !gates.publish && !gates.general_proof,"Gate authority expanded");
 require(bruce::render_application_gates(gates)==bruce::inspect_application_gates(native_bundle),"Typed gate rendering changed CLI contract");
 if(!research) require(!gates.selection.allowed && !gates.experience_source.allowed && gates.selection.reason=="UnsupportedApplicationKind","Non-research gate admitted");
 bool gate_tampering=false;
 try{(void)bruce::evaluate_application_gates(native_bundle+"tampered");}catch(const bruce::Error&){gate_tampering=true;}
 require(gate_tampering,"Typed gate accepted corrupt bundle");
 auto session=bruce::execute_checked_application(t);
 require(session.bundle==native_bundle,"Session changed native bundle");
 unsigned digest_calls=0;
 const auto observed=bruce::pack_application(session.run,[&]{++digest_calls;});
 require(observed==session.bundle && digest_calls>=2,"Bundle digest callback missing or changed bytes");
 for(unsigned stop=1;stop<=digest_calls;++stop){
  unsigned seen=0;bool raised=false;
  try{(void)bruce::pack_application(session.run,[&]{if(++seen==stop)throw bruce::NumericLimit();});}
  catch(const bruce::NumericLimit&){raised=true;}
  require(raised && seen==stop,"Bundle serialization swallowed callback or continued");
 }
 unsigned serialization_calls=0;bool stopped=false;
 bruce::VerificationControl serial;serial.serialization_digest_checkpoints=true;
 serial.cancelled=[&]{return ++serialization_calls==2;};
 try{(void)bruce::execute_checked_application(t,{},{},serial);}
 catch(const bruce::ExecutionSessionError& e){stopped=e.failed_at()==bruce::ExecutionPhase::serializing && e.cause()=="VerificationCancelled";}
 require(stopped,"Session did not stop inside SERIALIZING");
 serial.cancelled=[] {return false;};
 const auto serialized=bruce::execute_checked_application(t,{},{},serial);
 require(serialized.bundle==session.bundle && bruce::render_execution_session(serialized).find("application-execution-v8")!=std::string::npos,"Checked serialization changed native bundle");
 unsigned verification_calls=0;
 auto cancellable=bruce::execute_checked_application(t,{},{},{[&]{++verification_calls;return false;}});
 require(verification_calls==3 && cancellable.bundle==session.bundle,"Verification checkpoints changed native result");
 auto bounded=bruce::execute_checked_application(t,{},{{session.bundle.size()}});
 require(bounded.bundle==session.bundle,"Replay budget changed native bundle");
 bool limited=false;
 try{(void)bruce::execute_checked_application(t,{},{{0}});}
 catch(const bruce::ExecutionSessionError& e){limited=e.failed_at()==bruce::ExecutionPhase::replaying && e.cause().starts_with("ReplayByteBudgetExceeded:");}
 require(limited,"Task adapter bypassed replay byte budget");
 auto run=bruce::execute_application(std::move(t));
 const auto assessment=bruce::assess_application(run);
 require(assessment.kind==bruce::application_kind(run),"Assessment lost task kind");
 const auto legacy_status=bruce::application_status(run);
 require(assessment.status==legacy_status.first && assessment.stop_reason==legacy_status.second,"Assessment changed native status");
 require(assessment.complete==(legacy_status.first!="INCOMPLETE"),"Assessment changed completeness");
 if constexpr(!std::is_same_v<Task,bruce::FractionComparisonTask>)require(!assessment.proof_established,"Non-comparison promoted to proof");
 require(bruce::render_application(run)==render(native),"Unified result differs");
 require(bruce::pack_application(run)==native_bundle,"Unified export differs");
 auto replayed=bruce::replay_application(bruce::pack_application(run));
 require(replayed.index()==run.index(),"Replay lost result type");
 require(bruce::render_application(replayed)==render(native),"Typed replay differs");
 require(bruce::inspect_application(native_bundle).find(std::string(bruce::application_kind(run)))!=std::string::npos,"Inspection kind missing");
}
}
int main(){try{
 using namespace bruce;
 Task legacy;legacy.training={{0,1},{1,2}};legacy.holdout={{2,3}};
 same(legacy,research(legacy),render_run,pack_run);
 ExactResearchTask exact;exact.training={{0,1},{1,2}};exact.holdout={{2,3}};
 same(exact,exact_research(exact),render_exact_research,pack_exact_research);
 GridTask grid;grid.path="2,6";same(grid,evaluate_grid(grid),render_grid,pack_grid);
 GridBatchTask batch;batch.paths={"0,2","1,2","2,2"};same(batch,evaluate_grid_batch(batch),render_grid_batch,pack_grid_batch);
 auto partial=batch;partial.query_steps=1;GridResumeTask resume{pack_grid_batch(evaluate_grid_batch(partial)),2};
 same(resume,resume_grid_batch(resume.parent,2),render_grid_resume,pack_grid_resume);
 GridBatchTask hold;hold.paths={"3,2"};GridResearchTask gr;
 gr.research.strategy="polynomial";gr.research.planner="interpolate";gr.research.max_degree=2;
 gr.training=pack_grid_batch(evaluate_grid_batch(batch));gr.holdout=pack_grid_batch(evaluate_grid_batch(hold));
 same(gr,research_from_grid(gr.research,gr.training,gr.holdout),render_grid_research,pack_grid_research);
 FractionComparisonTask fraction;same(fraction,compare_fractions(fraction),render_fraction_comparison,pack_fraction_comparison);
 CompatibilityTask z;z.rules="BRUCE_RULES_V1\nsnapshot unified-test\n";z.origin=pack_exact_research(exact_research(exact));z.at=3;
 same(z,run_compatibility(z),render_compatibility,pack_compatibility);
 unsigned calls=0;ExecutionControl cancel{[&]{++calls;return true;}};
 auto stopped=execute_application(batch,cancel);require(std::get<GridBatchRun>(stopped).stop_reason=="Cancelled" && calls==1,"Batch cancellation lost");
 calls=0;auto atomic=execute_application(grid,cancel);require(calls==0 && std::get<GridRun>(atomic).value==64,"Atomic grid changed");
 require(std::get<Run>(execute_application(legacy,cancel)).stop_reason=="Cancelled","Legacy cancellation lost");
 require(std::get<ExactResearchRun>(execute_application(exact,cancel)).stop_reason=="Cancelled","Exact cancellation lost");
 require(std::get<GridResumeRun>(execute_application(resume,cancel)).batch.stop_reason=="Cancelled","Resume cancellation lost");
 require(std::get<GridResearchRun>(execute_application(gr,cancel)).research.stop_reason=="Cancelled","Grid research cancellation lost");
 require(std::get<FractionComparisonRun>(execute_application(fraction,cancel)).stop_reason=="Cancelled","Comparison cancellation lost");
 auto z_cancelled=execute_application(z,cancel);
 require(render_application(z_cancelled)==render_compatibility(run_compatibility(z,[](std::uint64_t){return true;})),"Compatibility cancellation differs");
 require(replay_output(pack_application(stopped))==render_application(stopped),"Partial adapter replay differs");
 bool rejected=false;grid.path="bad";try{(void)execute_application(grid);}catch(const Error&){rejected=true;}
 require(rejected,"Invalid native task accepted");
 std::cout<<"All eight adapters preserve results, exports and local cancellation\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
