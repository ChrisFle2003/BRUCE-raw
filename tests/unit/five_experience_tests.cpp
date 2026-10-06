#include "bruce/application/exact_research.hpp"
#include "bruce/experience/store.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        const auto replay_equal=[](const ExactResearchRun& run) {
            if(render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run))
                throw Error("Five-family ordered replay differs");
        };
        for(const auto& planner:{"adaptive-order","experience-order"}) for(bool cross_family:{false,true}) {
            ExactResearchTask task;
            task.strategy="portfolio-expression";task.planner=planner;task.domain="rational";
            task.expression_equivalence="rational";task.minimum="1";task.maximum="1";
            task.max_nodes=3;task.max_power=1;task.max_degree=cross_family ? 1 : 0;
            auto value=[&](int x){return ExactValue(cross_family ? x+1 : x*x);};
            for(int x=2;x<=5;++x) task.training.push_back({x,value(x)});
            task.holdout={{0,value(0)},{7,value(7)}};
            if(task.planner=="experience-order") {
                auto source=task;source.planner="portfolio";source.training.clear();
                for(int x=1;x<=4;++x) source.training.push_back({x,value(x)});
                task.experience_snapshot=append_experience("",pack_exact_research(exact_research(source)));
            }
            const auto full=exact_research(task);
            if(full.status!="VERIFIED_ON_SAMPLES" || full.strategy_runs.size()!=5 || full.strategy_order.size()!=5)
                throw Error("Ordered five-family reference failed");
            if(task.planner=="experience-order" && (full.experience_records_used!=1 || (!cross_family && full.strategy_order.front()!=4)))
                throw Error("Expression experience not ranked");
            auto fixed_task=task;fixed_task.planner="portfolio";fixed_task.experience_snapshot.clear();
            const auto fixed=exact_research(fixed_task);
            if(full.hypotheses.size()!=fixed.hypotheses.size()) throw Error("Ordering changed group count");
            for(std::size_t i=0;i<fixed.hypotheses.size();++i) {
                const auto& a=full.hypotheses[i];const auto& b=fixed.hypotheses[i];
                if(a.id!=b.id || a.status!=b.status || a.normal_form!=b.normal_form || a.origins.size()!=b.origins.size())
                    throw Error("Ordering changed group identity");
                for(std::size_t j=0;j<a.origins.size();++j)
                    if(a.origins[j].strategy!=b.origins[j].strategy || a.origins[j].id!=b.origins[j].id || a.origins[j].status!=b.origins[j].status)
                        throw Error("Ordering changed group provenance");
            }
            std::uint64_t child_ops=0,checkpoints=1,source_count=0,events=full.trace.size(),normal=full.union_operations,bytes=0;
            for(const auto& entry:full.trace) bytes+=entry.size();
            for(const auto& child:full.strategy_runs) {
                child_ops+=child.operations;checkpoints+=child.checkpoints+child.hypotheses.size();events+=child.trace.size();
                normal+=child.normalization_operations;bytes+=child.trace_bytes;
                source_count+=child.task.strategy=="expression" ? child.expression_sources : child.hypotheses.size();
            }
            if(full.operations!=child_ops+full.union_operations+full.ordering_operations || full.normalization_operations!=normal ||
               full.checkpoints!=checkpoints || full.expression_sources!=source_count || full.trace_bytes!=bytes) throw Error("Ordered accounting failed");
            replay_equal(full);
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i) throw Error("Ordered checkpoint cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_checkpoints.reset();
            for(std::uint64_t i=0;i<full.candidates;++i) {
                task.cancel_after_candidates=i;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.candidates!=i) throw Error("Ordered candidate cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_candidates.reset();
            for(std::uint64_t boundary:{std::uint64_t(0),std::uint64_t(1),1+full.strategy_runs[0].checkpoints,full.checkpoints-1}) {
                unsigned polls=0;const auto run=exact_research(task,[&](std::uint64_t){return ++polls==boundary+1;});
                if(run.stop_reason!="Cancelled" || run.checkpoints!=boundary) throw Error("Ordered callback replay failed");
                replay_equal(run);
            }
            task.budget.operations=full.operations;task.budget.candidates=full.candidates+1;task.budget.trace_events=events;
            if(exact_research(task).status!=full.status) throw Error("Exact ordered budgets failed");
            for(std::uint64_t n=1;n<full.operations;++n) {
                task.budget.operations=n;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>n) throw Error("Ordered operation cutoff failed");
                replay_equal(run);
            }
        }
        std::cout<<"Five-family adaptive and experience order: scope, identities, budgets, checkpoints and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
