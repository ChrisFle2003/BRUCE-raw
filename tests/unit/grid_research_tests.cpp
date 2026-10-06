#include "bruce/application/grid_research.hpp"
#include "bruce/application/grid_batch.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
namespace {
void require(bool condition,const char* message) {if(!condition) throw bruce::Error(message);}
template<class F> void rejected(F call) {
    bool failed=false;try {call();} catch(const bruce::Error&) {failed=true;}
    require(failed,"Invalid grid research accepted");
}
std::string grid(std::vector<std::string> paths) {
    bruce::GridBatchTask task;task.paths=std::move(paths);
    return bruce::pack_grid_batch(bruce::evaluate_grid_batch(task));
}
}
int main() {
    using namespace bruce;
    try {
        const auto train=grid({"0,2","1,2","2,2"}),hold=grid({"3,2"});
        ExactResearchTask t;t.domain="rational";t.strategy="polynomial";t.planner="interpolate";t.max_degree=2;
        auto run=research_from_grid(t,train,hold);
        require(run.research.status=="VERIFIED_ON_SAMPLES","Quadratic slice not verified");
        require(run.research.hypotheses[0].coefficients==std::vector<ExactValue>({0,0,1}),"Wrong polynomial");
        require(render_grid_research(replay_grid_research(pack_grid_research(run)))==render_grid_research(run),"Replay differs");
        // Both inner sources and research can be valid independently yet unrelated.
        auto other=t;other.training={{0,0},{1,1},{2,8}};other.holdout={{3,27}};other.max_degree=3;
        run.research=exact_research(other);
        rejected([&]{(void)replay_grid_research(pack_grid_research(run));});
        t.cancel_after_checkpoints=0;
        auto stopped=research_from_grid(t,train,hold);
        require(stopped.research.status=="INCOMPLETE" && stopped.research.stop_reason=="Cancelled","Cancellation lost");
        require(render_grid_research(replay_grid_research(pack_grid_research(stopped)))==render_grid_research(stopped),"Cancelled replay differs");
        t.cancel_after_checkpoints.reset();
        const auto prefixed=research_from_grid(t,grid({"01,01/0,2","1,1/1,2","1,1/2,2"}),grid({"1,1/3,2"}));
        require(prefixed.prefix=="1,1","Prefix alias not canonicalized");
        t.training={{0,0}};rejected([&]{(void)research_from_grid(t,train,hold);});t.training.clear();
        rejected([&]{(void)research_from_grid(t,"",hold);});
        rejected([&]{(void)research_from_grid(t,std::string(grid_research_source_limit+1,'x'));});
        rejected([&]{(void)research_from_grid(t,train,grid({"1,2"}));});
        const auto valid=pack_grid_research(prefixed);
        for(auto n:{std::size_t(0),valid.size()/2,valid.size()-1}) rejected([&]{(void)replay_grid_research(std::string_view(valid).substr(0,n));});
        const auto y_train=grid({"01,01/1,0","1,1/1,1","1,1/1,2"}),y_hold=grid({"1,1/1,3"});
        auto y=research_from_grid(t,y_train,y_hold,{},"y");
        require(y.axis=="y" && y.base==1 && y.prefix=="1,1","Wrong Y identity");
        require(y.research.task.training[2].x==2 && y.research.task.training[2].y==1,"Y projection reversed");
        require(render_grid_research(replay_grid_research(pack_grid_research(y)))==render_grid_research(y),"Y replay differs");
        auto wrong=t;wrong.training={{0,0},{1,1},{2,4}};wrong.holdout={{3,9}};
        y.research=exact_research(wrong);rejected([&]{(void)replay_grid_research(pack_grid_research(y));});
        t.cancel_after_checkpoints=0;
        y=research_from_grid(t,y_train,y_hold,{},"y");
        require(y.research.status=="INCOMPLETE","Y cancellation lost");
        require(render_grid_research(replay_grid_research(pack_grid_research(y)))==render_grid_research(y),"Cancelled Y replay differs");
        rejected([&]{(void)research_from_grid(t,y_train,y_hold,{},"z");});
        t.cancel_after_checkpoints.reset();
        const auto source_bytes=train.size()+hold.size();
        const auto full=research_from_grid(t,train,hold);
        const auto budgeted=research_from_grid(t,train,hold,{},"x",source_bytes+full.research.operations);
        require(budgeted.research.status==full.research.status,"Exact total threshold failed");
        require(render_grid_research(replay_grid_research(pack_grid_research(budgeted)))==render_grid_research(budgeted),"Budget replay differs");
        for(std::uint64_t remaining=1;remaining<full.research.operations;++remaining) {
            const auto partial=research_from_grid(t,train,hold,{},"x",source_bytes+remaining);
            require(partial.research.status=="INCOMPLETE" && partial.research.operations<=remaining,"Total bound exceeded");
            require(render_grid_research(replay_grid_research(pack_grid_research(partial)))==render_grid_research(partial),"Partial budget replay differs");
        }
        t.cancel_after_checkpoints=0;
        const auto cancelled_budget=research_from_grid(t,y_train,y_hold,{},"y",y_train.size()+y_hold.size()+1000);
        require(cancelled_budget.research.status=="INCOMPLETE","Budget cancellation lost");
        require(render_grid_research(replay_grid_research(pack_grid_research(cancelled_budget)))==render_grid_research(cancelled_budget),"Budget cancellation replay differs");
        rejected([&]{(void)research_from_grid(t,train,hold,{},"x",source_bytes);});
        // Admission must happen before a corrupted source is replayed.
        auto corrupt=full;corrupt.training_source[40]^=1;
        const auto nested=pack_grid_research(corrupt);
        const auto body="engine bruce-grid-research-v3\nwork_policy grid-source-bytes-plus-research-v1\ntotal_operations 1\nrequested_operations 1000000\ngrid_research_bytes "+std::to_string(nested.size())+"\n"+nested;
        try {
            (void)replay_grid_research("BRUCE_GRID_RESEARCH_BUNDLE_V3\n"+sha256(body)+"\n"+body);
            throw Error("Missing import admission");
        } catch(const Error& e) {require(std::string(e.what())=="InsufficientGridImportBudget","Source executed before admission");}
        ExactResearchTask independent;independent.domain="rational";independent.strategy="portfolio-rational";
        independent.planner="portfolio";independent.max_degree=2;independent.max_power=2;
        independent.minimum="-2";independent.maximum="2";
        const auto evidence=research_from_grid(independent,train,hold);
        auto learned=independent;learned.planner="experience-order";
        learned.experience_snapshot=append_experience("",pack_grid_research(evidence));
        const auto import=source_bytes+learned.experience_snapshot.size();
        const auto stopped_experience=research_from_grid(learned,train,hold,[](std::uint64_t){return true;},"x",import+10000);
        require(stopped_experience.budget_experience && stopped_experience.research.stop_reason=="Cancelled","Experience cancellation lost");
        require(render_grid_research(replay_grid_research(pack_grid_research(stopped_experience)))==render_grid_research(stopped_experience),"Experience cancellation replay differs");
        const auto y_experience=research_from_grid(learned,y_train,y_hold,{},"y",y_train.size()+y_hold.size()+learned.experience_snapshot.size()+10000);
        require(y_experience.budget_experience && y_experience.axis=="y","Y experience budget missing");
        require(render_grid_research(replay_grid_research(pack_grid_research(y_experience)))==render_grid_research(y_experience),"Y experience replay differs");
        // Parsed-task admission precedes the embedded snapshot's expensive verification.
        auto forged=stopped_experience.research;forged.task.experience_snapshot="invalid";
        bool admitted=false;
        try {
            (void)replay_exact_research(pack_exact_research(forged),10000000,[&](const ExactResearchTask& parsed) {
                admitted=true;require(parsed.experience_snapshot=="invalid","Admission sees wrong task");throw Error("AdmissionStopped");
            });
        } catch(const Error& e) {require(std::string(e.what())=="AdmissionStopped","Snapshot ran before admission");}
        require(admitted,"Parsed-task admission not called");
        std::cout<<"Grid source binding, cancellation, scope and replay passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
