#include "bruce/application/research_contract.hpp"
#include "bruce/application/research_gates.hpp"
#include <iostream>
#include <set>
namespace {
void require(bool b,const char* msg){if(!b) throw bruce::Error(msg);}
template<class F> void rejected(F f){bool caught=false;try{f();}catch(const bruce::Error&){caught=true;}require(caught,"Invalid contract accepted");}
}
int main(){try{
 using namespace bruce;
 std::set<std::string_view> names,planners;
 for(const auto& s:research_strategies()){require(names.insert(s.id).second,"Duplicate strategy");for(auto p:s.planners) planners.insert(p);}
 require(names.size()==10,"Missing strategies");
 for(const auto& s:research_strategies()){
  require(s.default_planner==s.planners.front(),"Bad default planner");
  for(auto planner:planners){
   bool allowed=false;for(auto p:s.planners) allowed|=p==planner;
   if(allowed) validate_research_route(s.id,planner);else rejected([&]{validate_research_route(s.id,planner);});
  }
  ExactResearchTask t;t.strategy=s.id;t.planner=s.default_planner;t.domain="rational";t.minimum="0";t.maximum="1";
  t.training={{0,1},{1,2}};t.holdout={{2,3}};
  if(s.id=="portfolio-expression") t.expression_equivalence="rational";
  require(validate_exact_task(t,NumberDomain(t.domain,t.max_bits))==2,"Task normalization changed");
  auto bad=t;bad.holdout={{1,2}};rejected([&]{validate_exact_task(bad,NumberDomain(bad.domain,bad.max_bits));});
  bad=t;bad.budget.operations=0;rejected([&]{validate_exact_task(bad,NumberDomain(bad.domain,bad.max_bits));});
 }
 rejected([]{validate_research_route("unknown","enumerate");});
 rejected([]{validate_research_route("affine","unknown");});
 ResearchGateFacts f{"VERIFIED_ON_SAMPLES","Completed",true,true,true,true};
 auto g=research_gates(f);require(g.selection && g.experience,"Verified gate closed");
 require(!g.learn_promotion && !g.publish && !g.general_proof,"Truth promoted");
 for(const auto* status:{"CANDIDATE","NO_CANDIDATE","REFUTED","AMBIGUOUS","VERIFIED_ON_SAMPLES"}){
  f.status=status;require(research_gates(f).selection,"Completed selection blocked");
 }
 f.status="INCOMPLETE";require(!research_gates(f).selection && !research_gates(f).experience,"Partial promoted");
 f.status="unknown";require(!research_gates(f).selection,"Unknown status accepted");
 f.status="VERIFIED_ON_SAMPLES";f.independent=false;require(!research_gates(f).experience,"Recursive experience accepted");
 f.independent=true;f.has_holdout=false;require(!research_gates(f).experience,"No holdout accepted");
 f.has_holdout=true;f.complete_portfolio=false;require(!research_gates(f).experience,"Partial portfolio accepted");
 f.complete_portfolio=true;f.verified_hypothesis=false;require(!research_gates(f).experience,"No verified hypothesis accepted");
 f.verified_hypothesis=true;f.stop_reason="Cancelled";require(!research_gates(f).selection,"Cancelled selection accepted");
 for(const auto* status:{"CANDIDATE","NO_CANDIDATE","REFUTED","AMBIGUOUS","VERIFIED_ON_SAMPLES","INCOMPLETE","unknown"}) {
  for(unsigned mask=0;mask<32;++mask) {
   ResearchGateFacts facts{status,(mask&1)?"Completed":"Cancelled",bool(mask&2),bool(mask&4),bool(mask&8),bool(mask&16)};
   const auto d=research_gates(facts);
   const bool known=std::string_view(status)!="INCOMPLETE" && std::string_view(status)!="unknown";
   const auto selection=!known?ResearchGateReason::status_not_completed:!(mask&1)?ResearchGateReason::stop_not_completed:ResearchGateReason::allowed;
   const auto base=selection!=ResearchGateReason::allowed?selection:!(mask&2)?ResearchGateReason::holdout_missing:!(mask&4)?ResearchGateReason::dependent_source:!(mask&8)?ResearchGateReason::portfolio_incomplete:ResearchGateReason::allowed;
   const auto experience=base!=ResearchGateReason::allowed?base:!(mask&16)?ResearchGateReason::verified_hypothesis_missing:ResearchGateReason::allowed;
   require(d.selection_reason==selection && d.experience_base_reason==base && d.experience_reason==experience,"Gate reason precedence mismatch");
   require(d.selection==(selection==ResearchGateReason::allowed) && d.experience_base==(base==ResearchGateReason::allowed) && d.experience==(experience==ResearchGateReason::allowed),"Reason and decision disagree");
   require(!d.learn_promotion && !d.publish && !d.general_proof,"Gate authority exceeded");
  }
 }
 require(research_gate_reason_name(ResearchGateReason::holdout_missing)=="HoldoutMissing","Unstable reason code");
 std::cout<<"Strategy matrix, task validation and research gates passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
