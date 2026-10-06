#include "bruce/application/execution_session.hpp"
#include <iostream>
namespace {void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
template<class F> void rejected(F f){bool b=false;try{f();}catch(const bruce::Error&){b=true;}require(b,"Illegal transition accepted");}}
int main(){try{using namespace bruce;
 ExecutionPhases phases;rejected([&]{phases.advance(ExecutionPhase::ready);});
 for(auto p:{ExecutionPhase::executing,ExecutionPhase::serializing,ExecutionPhase::replaying,ExecutionPhase::ready}) phases.advance(p);
 require(phases.history().size()==5,"Phase history lost");rejected([&]{phases.advance(ExecutionPhase::failed);});
 for(auto fail_after:{ExecutionPhase::executing,ExecutionPhase::serializing,ExecutionPhase::replaying}){
  ExecutionPhases f;f.advance(ExecutionPhase::executing);
  if(fail_after!=ExecutionPhase::executing)f.advance(ExecutionPhase::serializing);
  if(fail_after==ExecutionPhase::replaying)f.advance(ExecutionPhase::replaying);
  f.advance(ExecutionPhase::failed);rejected([&]{f.advance(ExecutionPhase::ready);});
 }
 GridBatchTask batch;batch.paths={"2,6","4,3"};unsigned calls=0;
 auto session=execute_checked_application(batch,{[&]{return ++calls==2;}});
 require(calls==2,"Replay repolled external cancellation");
 require(std::get<GridBatchRun>(session.run).status=="INCOMPLETE","Cancellation lost");
 require(render_application(replay_application(session.bundle))==render_application(session.run),"Checked partial replay differs");
 // A callback exception is a failure, not a normal cancellation or a result.
 bool callback_failed=false;
 std::exception_ptr saved_failure;
 const std::string cause="callback \"failed\"\nline two";
 try{(void)execute_checked_application(batch,{[&]() -> bool {throw std::domain_error(cause);}});}
 catch(const ExecutionSessionError& e){
  callback_failed=true;
  saved_failure=std::current_exception();
  require(e.failed_at()==ExecutionPhase::executing && e.phases().current()==ExecutionPhase::failed,"Wrong callback failure phase");
  require(e.phases().history()==std::vector<ExecutionPhase>{ExecutionPhase::created,ExecutionPhase::executing,ExecutionPhase::failed},"Callback failure history lost");
  require(e.cause()==cause,"Original callback message lost");
  bool original=false;try{std::rethrow_if_nested(e);}catch(const std::domain_error& nested){original=nested.what()==cause;}
  require(original,"Original exception type lost");
  require(render_execution_failure(e).find('\n')==std::string::npos,"Failure JSON contains raw newline");
 }
 require(callback_failed,"Callback failure accepted");
 bool retained=false;
 try{std::rethrow_exception(saved_failure);}catch(const ExecutionSessionError& e){
  try{std::rethrow_if_nested(e);}catch(const std::domain_error& nested){retained=nested.what()==cause;}
 }
 require(retained,"Original exception did not survive the caller's catch scope");
 // A valid, zero-padded path fits the input bound but its duplicated bundle payload does not.
 GridTask large;large.path=std::string(max_file_bytes/2+1024,'0')+"2,6";
 require(evaluate_grid(large).value==64,"Serialization fixture failed before serialization");
 bool serialization_failed=false;
 try{(void)execute_checked_application(std::move(large));}
 catch(const ExecutionSessionError& e){
  serialization_failed=true;
  require(e.failed_at()==ExecutionPhase::serializing && e.cause()=="BundleByteLimit","Serialization failure misclassified");
  require(e.phases().history()==std::vector<ExecutionPhase>{ExecutionPhase::created,ExecutionPhase::executing,ExecutionPhase::serializing,ExecutionPhase::failed},"Serialization failure history lost");
 }
 require(serialization_failed,"Oversized bundle accepted");
 VerificationBudget budget{{0}};
 bool fixed_budget=false;
 try{(void)execute_checked_application(batch,{[&]{budget.max_replay_bytes.reset();return false;}},budget);}
 catch(const ExecutionSessionError& e){
  fixed_budget=e.failed_at()==ExecutionPhase::replaying;
  require(e.phases().history()==std::vector<ExecutionPhase>{ExecutionPhase::created,ExecutionPhase::executing,ExecutionPhase::serializing,ExecutionPhase::replaying,ExecutionPhase::failed},"Replay admission failure history lost");
 }
 require(fixed_budget,"Caller changed admitted verification budget through callback");
 for(unsigned stop=1;stop<=3;++stop){
  unsigned checks=0;bool cancelled=false;
  try{(void)execute_checked_application(batch,{},{},{[&]{return ++checks==stop;}});}
  catch(const ExecutionSessionError& e){
   cancelled=true;
   require(e.cause()=="VerificationCancelled","Verification cancellation cause lost");
   require(e.failed_at()==(stop==1 ? ExecutionPhase::serializing : ExecutionPhase::replaying),"Wrong verification cancellation phase");
   require(e.phases().current()==ExecutionPhase::failed && e.phases().history().size()==(stop==1 ? 4U : 5U),"Cancelled verification reached READY");
  }
  require(cancelled && checks==stop,"Missed verification boundary");
 }
 unsigned native_calls=0,verification_calls=0;
 auto checked_partial=execute_checked_application(batch,{[&]{++native_calls;return true;}},{},{[&]{++verification_calls;return false;}});
 require(native_calls==1 && verification_calls==3,"Native and verification cancellation were mixed");
 require(std::get<GridBatchRun>(checked_partial.run).status=="INCOMPLETE","Independent verification lost partial result");
 unsigned cancel_before_budget=0;bool cancellation_wins=false;
 try{(void)execute_checked_application(batch,{},{{0}},{[&]{return ++cancel_before_budget==2;}});}
 catch(const ExecutionSessionError& e){cancellation_wins=e.cause()=="VerificationCancelled";}
 require(cancellation_wins,"Byte budget obscured requested verification cancellation");
 // At a shared checkpoint, session admission runs before verification cancellation.
 GridTask one;one.path="2,6";
 unsigned serialization_cancel_calls=0;bool serialization_budget_wins=false;
 try{(void)execute_checked_application(one,{},{},{[&]{++serialization_cancel_calls;return true;}},{{1}});}
 catch(const ExecutionSessionError& e){
  serialization_budget_wins=e.failed_at()==ExecutionPhase::serializing &&
   e.cause().starts_with("SessionCheckpointBudgetExceeded:") && serialization_cancel_calls==0;
 }
 require(serialization_budget_wins,"Serialization checkpoint changed budget-before-cancellation priority");
 unsigned replay_cancel_calls=0;bool replay_budget_wins=false;
 const auto replay_usage=execute_checked_application(one,{},{},{},{{100000}}).session_usage;
 const auto replay_limit=replay_usage.executing+replay_usage.serializing;
 try{(void)execute_checked_application(one,{},{},{[&]{return ++replay_cancel_calls==2;}},{{replay_limit}});}
 catch(const ExecutionSessionError& e){
  replay_budget_wins=e.failed_at()==ExecutionPhase::replaying &&
   e.cause().starts_with("SessionCheckpointBudgetExceeded:") && replay_cancel_calls==1;
 }
 require(replay_budget_wins,"Replay checkpoint changed budget-before-cancellation priority");
 unsigned throwing_checks=0;bool verification_error=false;
 try{(void)execute_checked_application(batch,{},{},{[&]{if(++throwing_checks==2)throw std::domain_error("verification callback failed");return false;}});}
 catch(const ExecutionSessionError& e){
  require(e.failed_at()==ExecutionPhase::replaying && e.cause()=="verification callback failed","Verification callback exception mislabeled");
  try{std::rethrow_if_nested(e);}catch(const std::domain_error& original){verification_error=std::string(original.what())=="verification callback failed";}
 }
 require(verification_error,"Verification callback original exception lost");
 VerificationControl original_control{[]{return true;}};bool control_copied=false;
 try{(void)execute_checked_application(batch,{[&]{original_control.cancelled={};return false;}},{},original_control);}
 catch(const ExecutionSessionError& e){control_copied=e.failed_at()==ExecutionPhase::serializing && e.cause()=="VerificationCancelled";}
 require(control_copied,"Caller mutated admitted verification control");
 std::cout<<"Legal phase transitions, terminal failures and cancellation preserved\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
