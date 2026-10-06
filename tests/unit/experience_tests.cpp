#include "bruce/application/transfer_control.hpp"
#include "bruce/application/experience_transfer.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/application/experience_task.hpp"
#include "bruce/application/experience_source.hpp"
#include "bruce/application/experience_routing.hpp"
#include "bruce/application/application_task.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        auto source=[](unsigned offset) {
            ExactResearchTask task;task.domain="rational";task.strategy="portfolio-rational";
            task.planner="portfolio";task.max_power=4;task.max_degree=2;task.max_denominator=8;
            for(unsigned x=offset;x<offset+4;++x) task.training.push_back({ExactValue(x),ExactValue(x+2,x+1)});
            task.holdout={{ExactValue(offset+5),ExactValue(offset+7,offset+6)}};
            return exact_research(task);
        };
        auto routing_task=source(0).task;
        ExperienceStore routing_store;
        ExperienceRecord vote;vote.training_fingerprint="different-input";
        vote.context=experience_context_key(routing_task,"shape");vote.votes={0,0,1};
        routing_store.records.push_back(vote);
        PortfolioOrder routing_plan;routing_plan.reason="shape";
        std::vector<std::uint64_t> routing_votes(3);std::uint64_t used=0,spent=0;
        rank_experience(routing_store,routing_task,routing_plan,routing_votes,used,[&](auto n){spent+=n;});
        if(routing_plan.strategies!=std::vector<unsigned>{2,0,1} || routing_votes!=vote.votes || used!=1 || spent!=7 || routing_plan.reason!="experience_votes:shape")throw Error("Routing extraction changed ordering or budget");
        routing_store.records[0].training_fingerprint=experience_training_key(routing_task);
        routing_plan=PortfolioOrder{};routing_plan.reason="shape";routing_votes.assign(3,0);used=0;
        rank_experience(routing_store,routing_task,routing_plan,routing_votes,used,[](auto){});
        if(used || routing_plan.strategies!=std::vector<unsigned>{0,1,2})throw Error("Routing used repeated input");
        const auto original=pack_exact_research(source(0));
        const auto transfer=ExperienceTransfer::plan(original,"");
        const ExperienceTransfer restored_transfer(transfer.bytes());
        if(restored_transfer.bytes()!=transfer.bytes() || restored_transfer.apply(original,"").snapshot!=append_experience("",original))throw Error("Transfer changed import bytes");
        bool bounded=false;
        try { ExperienceTransfer::plan(original,"",0,0); }
        catch(const Error& e){bounded=std::string(e.what())=="ExperienceTransferByteLimit";}
        if(!bounded)throw Error("Transfer ignored direct API byte budget");
        std::uint64_t points=0;
        restored_transfer.apply(original,"",[&]{++points;});
        if(points<3)throw Error("Transfer checkpoints missing");
        for(const auto stop:{std::uint64_t(0),points/2,points-1}) {
            TransferControl control;control.maximum=stop;
            control.cancelled=[&]{return control.used()>=stop;};
            bool cancelled=false;
            try {restored_transfer.apply(original,"",control.callback());}
            catch(const Error& e){cancelled=std::string(e.what())=="TransferCancelled";}
            if(!cancelled || control.used()!=stop)throw Error("Transfer callback swallowed or priority changed");
        }
        std::uint64_t plan_points=0;
        ExperienceTransfer::plan(original,"",1048576,1048576,[&]{++plan_points;});
        if(plan_points<3)throw Error("Plan checkpoints missing");
        for(const auto stop:{std::uint64_t(0),plan_points/2,plan_points-1}) {
            TransferControl control;control.maximum=stop;
            control.cancelled=[&]{return control.used()>=stop;};
            bool cancelled=false;
            try {ExperienceTransfer::plan(original,"",1048576,1048576,control.callback());}
            catch(const Error& e){cancelled=std::string(e.what())=="TransferCancelled";}
            if(!cancelled || control.used()!=stop)throw Error("Plan callback swallowed or priority changed");
        }
        unsigned audit_calls=0,gate_calls=0;
        audit_experience_source(original,[&]{++audit_calls;});
        const auto checked=evaluate_application_gates(original,[&]{++gate_calls;});
        if(!audit_calls || gate_calls<=audit_calls || !checked.experience_source.allowed ||
           render_application_gates(checked)!=inspect_application_gates(original)) throw Error("Gate checkpoints changed result or missed audit");
        for(unsigned stop:{1u,audit_calls}) {
            unsigned seen=0;bool raised=false;
            try {audit_experience_source(original,[&]{if(++seen==stop)throw Error("AuditStop");});}
            catch(const Error& e){raised=std::string(e.what())=="AuditStop";}
            if(!raised || seen!=stop)throw Error("Audit checkpoint swallowed");
        }
        for(unsigned stop:{1u,gate_calls}) {
            unsigned seen=0;bool raised=false;
            try {(void)evaluate_application_gates(original,[&]{if(++seen==stop)throw Error("GateStop");});}
            catch(const Error& e){raised=std::string(e.what())=="GateStop";}
            if(!raised || seen!=stop)throw Error("Gate cancellation became rejection");
        }
        unsigned numeric_seen=0;bool numeric_raised=false;
        try {(void)evaluate_application_gates(original,[&]{if(++numeric_seen==gate_calls)throw NumericLimit();});}
        catch(const NumericLimit&){numeric_raised=true;}
        if(!numeric_raised)throw Error("Gate swallowed numeric callback exception");
        const auto snapshot=append_experience("",original);
        const auto inspected=execute_experience(ExperienceInspectTask{snapshot});
        if(inspected.snapshot!=snapshot || inspected.report!=render_experience(read_experience(snapshot)))
            throw Error("Experience inspection changed snapshot or report");
        bool invalid_inspection=false;
        try{(void)execute_experience(ExperienceInspectTask{snapshot+"tampered"});}
        catch(const Error&){invalid_inspection=true;}
        if(!invalid_inspection)throw Error("Experience inspection skipped audit");
        const auto application_added=execute_experience(ExperienceAddTask{"",original});
        if(application_added.snapshot!=snapshot || application_added.report!=render_experience(read_experience(snapshot)))
            throw Error("Experience application add changed bytes or report");
        const auto store=read_experience(snapshot);
        const auto audited=verify_experience_source(original);
        if(audited.source_digest!=store.records[0].source_digest || audited.context!=store.records[0].context ||
           audited.training_fingerprint!=store.records[0].training_fingerprint || audited.votes!=store.records[0].votes)
            throw Error("Separated source audit differs from store import");
        bool source_stopped=false;
        try{(void)verify_experience_source(original,[]{throw Error("SourceAuditStop");});}
        catch(const Error& e){source_stopped=std::string(e.what())=="SourceAuditStop";}
        if(!source_stopped)throw Error("Source adapter swallowed cancellation");
        const auto id=store.records[0].id;
        const auto replacement=pack_exact_research(source(10));
        const auto replaced=execute_experience(ExperienceReplaceTask{snapshot,id,"reviewed correction",replacement});
        if(replaced.snapshot!=replace_experience(snapshot,id,"reviewed correction",replacement) ||
           replaced.report!=render_experience(read_experience(replaced.snapshot)))throw Error("Experience application replacement differs");
        const auto removed=execute_experience(ExperienceRevokeTask{snapshot,id,"withdrawn"});
        if(removed.snapshot!=revoke_experience(snapshot,id,"withdrawn") ||
           removed.report!=render_experience(read_experience(removed.snapshot)))throw Error("Experience application revoke differs");
        auto exercise_lifecycle_checkpoint=[&](auto operation,std::string_view name) {
            unsigned points=0;
            const auto controlled=operation([&]{++points;});
            if(controlled!=operation({}))throw Error(std::string(name)+" checkpoint changed snapshot bytes");
            if(!points)throw Error(std::string(name)+" skipped lifecycle checkpoints");
            for(unsigned limit=0;limit<points;++limit) {
                TransferControl control;control.maximum=limit;bool stopped=false;
                try{(void)operation(control.callback());}
                catch(const Error& e){stopped=std::string(e.what()).starts_with("TransferCheckpointBudgetExceeded");}
                if(!stopped || control.used()!=limit)throw Error(std::string(name)+" lifecycle checkpoint bypassed");
            }
        };
        exercise_lifecycle_checkpoint([&](const ReplayCheckpoint& checkpoint){
            return revoke_experience(snapshot,id,"withdrawn",checkpoint);
        },"Revoke");
        exercise_lifecycle_checkpoint([&](const ReplayCheckpoint& checkpoint){
            return replace_experience(snapshot,id,"reviewed correction",replacement,checkpoint);
        },"Replace");
        auto same_error=[](auto native,auto adapter){
            std::string expected,actual;
            try{native();}catch(const Error& e){expected=e.what();}
            try{adapter();}catch(const Error& e){actual=e.what();}
            if(expected.empty() || expected!=actual)throw Error("Experience application error changed");
        };
        same_error([&]{append_experience(snapshot,original);},[&]{execute_experience(ExperienceAddTask{snapshot,original});});
        same_error([&]{revoke_experience(snapshot,"missing","reason");},[&]{execute_experience(ExperienceRevokeTask{snapshot,"missing","reason"});});
        same_error([&]{replace_experience(snapshot,id,"",replacement);},[&]{execute_experience(ExperienceReplaceTask{snapshot,id,"",replacement});});
        same_error([&]{append_experience("",original+"tampered");},[&]{execute_experience(ExperienceAddTask{"",original+"tampered"});});
        if(read_experience(snapshot).records[0].active!=true)throw Error("Application changed original snapshot");
        if(store.records.size()!=1 || store.records[0].votes!=std::vector<std::uint64_t>{0,0,1} || !store.audit_operations)
            throw Error("Source experience incorrect");
        auto task=source(10).task;task.planner="experience-order";task.experience_snapshot=snapshot;
        const auto full=exact_research(task);
        if(full.status!="VERIFIED_ON_SAMPLES" || full.strategy_order[0]!=2 || full.experience_records_used!=1 ||
           full.experience_audit_operations!=store.audit_operations) throw Error("Experience routing failed");
        for(std::uint64_t i=0;i<full.checkpoints;++i) {
            task.cancel_after_checkpoints=i;
            const auto run=exact_research(task);
            if(run.stop_reason!="Cancelled" || run.checkpoints!=i ||
               render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                throw Error("Experience cancellation replay mismatch");
        }
        task.cancel_after_checkpoints.reset();
        for(std::uint64_t n=1;n<full.operations;++n) {
            task.budget.operations=n;
            const auto run=exact_research(task);
            if(run.status!="INCOMPLETE" || run.operations>n ||
               render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                throw Error("Experience operation cutoff failed");
        }
        task.budget.operations=full.operations;task.budget.candidates=full.candidates+1;
        if(exact_research(task).status!=full.status) throw Error("Exactly sufficient operation budget failed");
        const auto revoked=revoke_experience(snapshot,store.records[0].id,"counterexample review");
        task.experience_snapshot=revoked;task.budget={};
        const auto fallback=exact_research(task);
        if(fallback.experience_records_used || fallback.strategy_order[0]!=0 || read_experience(snapshot).records[0].active!=true)
            throw Error("Revocation modified past or failed fallback");
        std::string accumulated=snapshot;
        for(unsigned i=1;i<8;++i) accumulated=append_experience(accumulated,pack_exact_research(source(i*10)));
        bool record_limit=false;
        try {(void)append_experience(accumulated,pack_exact_research(source(90)));}
        catch(const Error& e) {record_limit=std::string(e.what())=="ExperienceRecordLimit";}
        if(!record_limit) throw Error("Experience source cap missing");
        const auto eight=read_experience(accumulated);
        for(const auto& r:eight.records) accumulated=revoke_experience(accumulated,r.id,"withdrawn");
        if(read_experience(accumulated).event_bodies.size()!=16) throw Error("Correction chain lost events");
        bool event_limit=false;
        try {(void)revoke_experience(accumulated,eight.records[0].id,"extra");}
        catch(const Error& e) {event_limit=std::string(e.what())=="ExperienceEventLimit";}
        if(!event_limit) throw Error("Experience event cap missing");
        auto oversized=source(0);oversized.task.budget.operations=1000001;
        bool work_limit=false;
        try {(void)append_experience("",pack_exact_research(oversized));}
        catch(const Error& e) {work_limit=std::string(e.what())=="ExperienceSourceOperationLimit";}
        if(!work_limit) throw Error("Source preflight work limit missing");
        std::cout<<"Experience checkpoints, cutoffs, immutable revocation and source/event/work caps passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
