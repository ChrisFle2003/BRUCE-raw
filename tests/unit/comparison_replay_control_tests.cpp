#include "bruce/application/execution_session.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
void exercise(const bruce::FractionComparisonRun& run){
 using namespace bruce;const auto bundle=pack_fraction_comparison(run);std::size_t total=0;
 const auto checked=replay_application(bundle,[&]{++total;});
 require(total>=3 && pack_application(checked)==bundle && render_application(checked)==render_fraction_comparison(run),"Comparison checkpoint changed evidence");
 for(std::size_t stop=1;stop<=total;++stop){
  std::size_t seen=0;bool interrupted=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw NumericLimit();});}
  catch(const NumericLimit&){interrupted=true;}
  require(interrupted && seen==stop,"Comparison swallowed callback as numeric limit or continued");
 }
 const auto first=bundle.find('\n'),second=bundle.find('\n',first+1);
 const auto body=bundle.substr(second+1)+"tampered\n";
 bool rejected=false;
 try{(void)replay_application(bundle.substr(0,first+1)+sha256(body)+"\n"+body,[]{});}
 catch(const Error& e){rejected=std::string(e.what()).starts_with("ReplayMismatch:");}
 require(rejected,"Comparison observer bypassed evidence check");
}
}
int main(){try{using namespace bruce;
 unsigned cases=0;
 for(const std::string policy:{"quadratic","modular","modular-witness"}){
  for(const std::string denominator:{"1","-1,0,1","-2,0,0,1","-8,0,0,1","0"}){
   FractionComparisonTask t;t.domain_policy=policy;t.left_numerator=t.left_denominator=denominator;
   t.right_numerator="1";
   const auto full=compare_fractions(t);require(full.status!="INCOMPLETE","Full comparison fixture incomplete");exercise(full);
   t.max_operations=1;exercise(compare_fractions(t));t.max_operations=1000000;
   for(std::uint64_t stop:{0U,7U}){t.cancel_after_operations=stop;exercise(compare_fractions(t));}
   ++cases;
  }
  FractionComparisonTask different;different.domain_policy=policy;different.left_numerator="2";different.right_numerator="1";
  auto run=compare_fractions(different);require(run.status=="PROVEN_DIFFERENT","Value-difference fixture failed");exercise(run);++cases;
 }
 FractionComparisonTask task;unsigned nested_checks=0;bool normalization=false;
 try{(void)replay_fraction_comparison(pack_fraction_comparison(compare_fractions(task)),[&]{if(++nested_checks==3)throw ExpressionNormalizationLimit();});}
 catch(const ExpressionNormalizationLimit&){normalization=true;}
 require(normalization,"Comparison swallowed normalization exception from callback");
 unsigned expected_calls=0,native_calls=0,checks=0;
 (void)compare_fractions(task,[&](std::uint64_t){++expected_calls;return false;});
 bool stopped=false;
 try{(void)execute_checked_application(task,{[&]{++native_calls;return false;}},{},{[&]{return ++checks==5;},false,false,true});}
 catch(const ExecutionSessionError& e){stopped=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled";}
 require(stopped && checks==5 && native_calls==expected_calls,"Comparison session did not interrupt inside replay independently");
 bool missing=false;try{(void)execute_checked_application(task,{},{},{{},false,false,true});}
 catch(const ExecutionSessionError& e){missing=e.cause()=="ComparisonReplayCheckpointsRequireCallback";}
 require(missing,"Missing comparison callback accepted");
 for(auto control:{VerificationControl{[]{return false;},true,false,true},VerificationControl{[]{return false;},false,true,true}}){
  bool conflict=false;try{(void)execute_checked_application(task,{},{},control);}
  catch(const ExecutionSessionError& e){conflict=e.cause()=="ConflictingReplayCheckpoints";}
  require(conflict,"Conflicting comparison mode accepted");
 }
 std::cout<<cases<<" comparison cases preserve V1-V3 results, partials and every replay checkpoint\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
