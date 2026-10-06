#include "bruce/application/execution_session.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw bruce::Error(m);}
struct Interrupted : bruce::Error {Interrupted():Error("grid research interruption") {}};
std::string grid(std::vector<std::string> paths){
 bruce::GridBatchTask t;t.paths=std::move(paths);return bruce::pack_grid_batch(bruce::evaluate_grid_batch(t));
}
void exercise(const bruce::GridResearchRun& run){
 using namespace bruce;const auto bundle=pack_grid_research(run);std::size_t total=0;
 const auto checked=replay_application(bundle,[&]{++total;});
 require(total>10 && pack_application(checked)==bundle && render_application(checked)==render_grid_research(run),"Composed replay changed evidence");
 for(std::size_t stop=1;stop<=total;++stop){
  std::size_t seen=0;bool interrupted=false;
  try{(void)replay_application(bundle,[&]{if(++seen==stop)throw Interrupted();});}
  catch(const Interrupted&){interrupted=true;}
  require(interrupted && seen==stop,"Composed replay swallowed or continued after checkpoint exception");
 }
}
}
int main(){try{using namespace bruce;
 const auto train=grid({"0,2","1,2","2,2"}),hold=grid({"3,2"});
 const auto ytrain=grid({"1,0","1,1","1,2"}),yhold=grid({"1,3"});
 ExactResearchTask t;t.domain="rational";t.strategy="polynomial";t.planner="interpolate";t.max_degree=2;
 const auto full=research_from_grid(t,train,hold);exercise(full);
 exercise(research_from_grid(t,ytrain,yhold,{},"y"));
 exercise(research_from_grid(t,train,hold,{},"x",train.size()+hold.size()+1000));
 exercise(research_from_grid(t,ytrain,yhold,{},"y",ytrain.size()+yhold.size()+1));
 exercise(research_from_grid(t,train,hold,[](std::uint64_t){return true;}));
 GridBatchTask partial;partial.paths={"0,2","1,2","2,2"};partial.query_steps=1;
 auto resumed=pack_grid_resume(resume_grid_batch(pack_grid_batch(evaluate_grid_batch(partial)),2));
 exercise(research_from_grid(t,resumed,hold));
 ExactResearchTask portfolio=t;portfolio.strategy="portfolio-rational";portfolio.planner="portfolio";
 portfolio.minimum="-2";portfolio.maximum="2";portfolio.max_power=2;
 auto evidence=research_from_grid(portfolio,train,hold);
 auto learned=portfolio;learned.planner="experience-order";learned.budget.operations=10000;
 learned.experience_snapshot=append_experience("",pack_grid_research(evidence));
 const auto import=train.size()+hold.size()+learned.experience_snapshot.size();
 exercise(research_from_grid(learned,train,hold,{},"x",import+10000));
 auto legacy_budget=research_from_grid(learned,train,hold);
 legacy_budget.total_operations=train.size()+hold.size()+10000;legacy_budget.requested_operations=10000;
 exercise(legacy_budget); // Historical V3 learned budgets do not charge experience bytes.
 auto wrong=full;auto other=t;other.training={{0,0},{1,1},{2,8}};other.holdout={{3,27}};other.max_degree=3;
 wrong.research=exact_research(other);bool binding=false;
 try{(void)replay_grid_research(pack_grid_research(wrong),10000000,{},[]{});}
 catch(const Error& e){binding=std::string(e.what())=="GridResearchSampleBindingMismatch";}
 require(binding,"Checkpoint bypassed sample binding");
 auto corrupt=full;corrupt.training_source[40]^=1;const auto nested=pack_grid_research(corrupt);
 bool denied=false;
 try{(void)replay_grid_research(nested,10000000,[](std::string_view){throw Error("Denied");},[]{});}
 catch(const Error& e){denied=std::string(e.what())=="Denied";}
 require(denied,"Source replay preceded research admission");
 const auto body="engine bruce-grid-research-v3\nwork_policy grid-source-bytes-plus-research-v1\ntotal_operations 1\nrequested_operations 1000000\ngrid_research_bytes "+std::to_string(nested.size())+"\n"+nested;
 bool budget=false;
 try{(void)replay_grid_research("BRUCE_GRID_RESEARCH_BUNDLE_V3\n"+sha256(body)+"\n"+body,10000000,{},[]{});}
 catch(const Error& e){budget=std::string(e.what())=="InsufficientGridImportBudget";}
 require(budget,"Checkpoint bypassed import admission");
 bool operations=false;
 try{(void)replay_grid_research(pack_grid_research(full),0,{},[]{});}
 catch(const Error& e){operations=std::string(e.what())=="ReplayOperationLimit";}
 require(operations,"Checkpoint bypassed research operations limit");
 unsigned calls=0;bool cancelled=false;
 try{(void)execute_checked_application(GridResearchTask{t,train,hold,"x",{}},{},{},{[&]{return ++calls==4;},false,true});}
 catch(const ExecutionSessionError& e){cancelled=e.failed_at()==ExecutionPhase::replaying && e.cause()=="VerificationCancelled";}
 require(cancelled && calls==4,"Composed session did not interrupt inner source replay");
 std::cout<<"Grid research V1-V4, X/Y, resumed sources, partials and admission guards preserve replay with checkpoints\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
