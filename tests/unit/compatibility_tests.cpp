#include "bruce/application/transfer_control.hpp"
#include "bruce/application/compatibility_transfer.hpp"
#include "bruce/application/compatibility.hpp"
#include "bruce/application/research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        CompatibilityTask task;
        task.rules="BRUCE_RULES_V1\nsnapshot cancel-v1\nrule even positive value mod_eq 2 0 5 manual -\n";
        task.candidates="id,domain,layer,x,y,value\n1,math,0,0,0,2\n2,math,0,1,0,4\n";
        const auto run=run_compatibility(task,[](std::uint64_t count) { return count==1; });
        if(run.result.status!="INCOMPLETE" || run.result.stop_reason!="Cancelled" || !run.result.selection.ids.empty())
            throw Error("Cancellation cannot select a winner");
        if(render_compatibility(replay_compatibility(pack_compatibility(run)))!=render_compatibility(run))
            throw Error("Cancellation replay changed");
        task.budget.checks=1;
        const auto limited=run_compatibility(task);
        if(limited.result.groups[0].complete_mask!=1 || limited.result.groups[0].eligible_mask!=1)
            throw Error("Unprocessed lanes must remain closed");
        // The new dispatcher preserves the V1 research output. A frozen export
        // produced by the old binary is additionally covered by the CLI suite.
        Task old;
        old.training={{0,3},{1,5}};
        const auto research_run=research(old);
        if(replay_output(pack_run(research_run))!=render_run(research_run)) throw Error("Old replay changed");
        const auto source=pack_run(research_run);
        const auto transfer=CompatibilityTransfer::plan(source,task.rules,3);
        const CompatibilityTransfer decoded(transfer.bytes());
        CompatibilityTask projected;projected.origin=source;projected.rules=task.rules;projected.at=3;
        if(decoded.bytes()!=transfer.bytes() || pack_compatibility(decoded.apply(source,task.rules))!=pack_compatibility(run_compatibility(projected)))throw Error("Transfer changed native bundle");
        bool bounded=false;
        try {CompatibilityTransfer::plan(source,task.rules,3,{},0);}
        catch(const Error& e){bounded=std::string(e.what())=="CompatibilityTransferByteLimit";}
        if(!bounded)throw Error("Transfer ignored byte limit");
        ZBudget unsupported;unsupported.cancel_after_candidates=1;bool rejected=false;
        try {CompatibilityTransfer::plan(source,task.rules,3,unsupported);}
        catch(const Error& e){rejected=std::string(e.what())=="CompatibilityTransferInvalidBudget";}
        if(!rejected)throw Error("Transfer silently ignored cancellation parameter");
        std::uint64_t points=0;
        decoded.apply(source,task.rules,[&]{++points;});
        if(points<3)throw Error("Transfer checkpoints missing");
        for(const auto stop:{std::uint64_t(0),points/2,points-1}) {
            TransferControl control;control.maximum=stop;
            control.cancelled=[&]{return control.used()>=stop;};
            bool cancelled=false;
            try {decoded.apply(source,task.rules,control.callback());}
            catch(const Error& e){cancelled=std::string(e.what())=="TransferCancelled";}
            if(!cancelled || control.used()!=stop)throw Error("Transfer callback swallowed or priority changed");
        }
        std::uint64_t plan_points=0;
        CompatibilityTransfer::plan(source,task.rules,3,{},33554432,[&]{++plan_points;});
        if(plan_points<3)throw Error("Plan checkpoints missing");
        for(const auto stop:{std::uint64_t(0),plan_points/2,plan_points-1}) {
            TransferControl control;control.maximum=stop;
            control.cancelled=[&]{return control.used()>=stop;};
            bool cancelled=false;
            try {CompatibilityTransfer::plan(source,task.rules,3,{},33554432,control.callback());}
            catch(const Error& e){cancelled=std::string(e.what())=="TransferCancelled";}
            if(!cancelled || control.used()!=stop)throw Error("Plan callback swallowed or priority changed");
        }
        CompatibilityTask many=task;many.budget={};
        many.rules="BRUCE_RULES_V1\nsnapshot many\n";
        for(unsigned i=0;i<128;++i)many.rules+="rule r"+std::to_string(i)+" positive value ge 0 0 1 manual -\n";
        const CompatibilitySnapshot snapshot(many.rules);
        const auto candidates=parse_candidates(many.candidates);
        std::size_t rule_points=0;
        const auto selection=evaluate_compatibility(snapshot,candidates,{}, {},[&]{++rule_points;});
        if(rule_points!=256 || selection.checks!=256)throw Error("Native rule checkpoint coverage missing");
        ZBudget short_budget;short_budget.checks=1;std::size_t limited_points=0;
        const auto short_result=evaluate_compatibility(snapshot,candidates,short_budget,{},[&]{++limited_points;});
        if(short_result.status!="INCOMPLETE" || short_result.stop_reason!="OperationBudgetExceeded" || limited_points!=1)throw Error("Native budget semantics changed");
        for(std::size_t stop=1;stop<=rule_points;++stop){
            std::size_t seen=0;bool stopped=false;
            try {evaluate_compatibility(snapshot,candidates,{}, {},[&]{if(++seen==stop)throw Error("RuleEvaluationStop");});}
            catch(const Error& e){stopped=std::string(e.what())=="RuleEvaluationStop";}
            if(!stopped || seen!=stop)throw Error("Native rule stop swallowed");
        }
        std::size_t native_points=0;
        const auto controlled=run_compatibility(many,{}, {},[&]{++native_points;});
        if(native_points<=rule_points || pack_compatibility(controlled)!=pack_compatibility(run_compatibility(many)))throw Error("Native controlled result changed");
        for(const auto stop:{std::size_t(1),native_points/2,native_points}){
            std::size_t seen=0;bool stopped=false;
            try {run_compatibility(many,{}, {},[&]{if(++seen==stop)throw Error("NativeStop");});}
            catch(const Error& e){stopped=std::string(e.what())=="NativeStop";}
            if(!stopped || seen!=stop)throw Error("Native callback stop swallowed");
        }
        std::cout << "Z cancellation, partial masks and V1 dispatch checks passed\n";
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
