#include "bruce/application/exact_research.hpp"
#include <iostream>
#include <set>

int main() {
    using namespace bruce;
    try {
        for(const auto& mode:{"syntax","rational"}) for(bool division:{false,true}) for(const auto& planner:{"grammar","indexed-grammar"}) {
            ExactResearchTask task;
            task.strategy="expression";task.planner=planner;task.domain="rational";
            task.expression_equivalence=mode;task.expression_operators=division ? "/" : "+";
            task.minimum="1";task.maximum="1";task.max_nodes=3;
            task.training=division ? std::vector<ExactSample>{{2,ExactValue(1,2)},{3,ExactValue(1,3)}} : std::vector<ExactSample>{{2,3},{3,4}};
            task.holdout={{-1,division ? ExactValue(-1) : ExactValue(0)}};
            const auto full=exact_research(task);
            if(full.status!="VERIFIED_ON_SAMPLES" || full.candidates!=(division ? 6u : 5u) || full.checkpoints!=full.candidates+1) throw Error("Expression reference failed");
            auto replay_equal=[](const ExactResearchRun& run) {
                if(render_exact_research(replay_exact_research(pack_exact_research(run)))!=render_exact_research(run)) throw Error("Expression replay differs");
            };
            for(std::uint64_t i=0;i<full.checkpoints;++i) {
                task.cancel_after_checkpoints=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.checkpoints!=i) throw Error("Checkpoint cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_checkpoints.reset();
            for(std::uint64_t i=0;i<full.candidates;++i) {
                task.cancel_after_candidates=i;
                const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.stop_reason!="Cancelled" || run.candidates!=i) throw Error("Candidate cancellation failed");
                replay_equal(run);
            }
            task.cancel_after_candidates.reset();
            unsigned polls=0;
            const auto callback=exact_research(task,[&](std::uint64_t){return ++polls==3;});
            if(callback.stop_reason!="Cancelled" || callback.checkpoints!=2) throw Error("Callback checkpoint failed");
            replay_equal(callback);
            task.budget.operations=full.operations;task.budget.candidates=full.candidates;task.budget.trace_events=full.trace.size();
            if(exact_research(task).status!=full.status) throw Error("Exact budgets failed");
            for(std::uint64_t i=1;i<full.operations;++i) {
                task.budget.operations=i;const auto run=exact_research(task);
                if(run.status!="INCOMPLETE" || run.operations>i || run.stop_reason!="OperationBudgetExceeded") throw Error("Operation cutoff failed");
                replay_equal(run);
            }
        }
        const auto noop=[](std::uint64_t){};
        const auto same=[](const Expression& a,const Expression& b) {
            if(a.size()!=b.size()) throw Error("Selected tree size differs");
            for(std::size_t i=0;i<a.size();++i)
                if(a[i].op!=b[i].op || a[i].left!=b[i].left || a[i].right!=b[i].right || a[i].value!=b[i].value)
                    throw Error("Selected tree differs");
        };
        // The old full grammar, filtered after tree generation, is an independent
        // oracle for membership and relative rank order in each selected grammar.
        for(const auto& constants:{std::vector<ExactValue>{},std::vector<ExactValue>{1},std::vector<ExactValue>{-1,0,ExactValue(2,3)}}) {
            const unsigned nodes=constants.empty() ? 9 : constants.size()==1 ? 7 : 5;
            const ExpressionGrammar full(constants,nodes,noop);
            for(unsigned mask=1;mask<16;++mask) {
                std::string ops;
                for(unsigned i=0;i<4;++i) if(mask&(1u<<i)) ops+=std::string("+-*/")[i];
                const ExpressionGrammar selected(constants,nodes,noop,false,ops),indexed(constants,nodes,noop,true,ops);
                std::uint64_t rank=0;
                for(std::uint64_t i=0;i<full.size();++i) {
                    const auto tree=full.at(i,noop);
                    bool allowed=true;
                    for(const auto& n:tree) if(n.op!='x' && n.op!='c' && ops.find(n.op)==std::string::npos) allowed=false;
                    if(!allowed) continue;
                    same(tree,selected.at(rank,noop));same(tree,indexed.at(rank,noop));++rank;
                }
                if(rank!=selected.size() || rank!=indexed.size() || indexed.index_entries()>32) throw Error("Selected grammar membership differs");
                bool rejected=false;try {(void)indexed.at(rank,noop);} catch(const Error&) {rejected=true;}
                if(!rejected) throw Error("Out-of-range selected ID accepted");
            }
        }
        const ExpressionGrammar add({1},3,noop,true,"+"),sub({1},3,noop,true,"-"),leaf({1},1,noop,true,"/");
        if(add.size()!=5 || sub.size()!=6 || leaf.size()!=2 || leaf.index_entries()!=0) throw Error("Small selected grammar count differs");
        if(expression_formula(add.at(2,noop))!="(x+x)" || expression_formula(sub.at(3,noop))!="(x-1)") throw Error("Canonical selected tree order differs");
        std::vector<ExactValue> constants;for(unsigned i=0;i<64;++i) constants.emplace_back(i);
        for(unsigned mask=1;mask<16;++mask) {
            std::string ops;for(unsigned i=0;i<4;++i) if(mask&(1u<<i)) ops+=std::string("+-*/")[i];
            const ExpressionGrammar plain(constants,9,noop,false,ops),indexed(constants,9,noop,true,ops);
            if(plain.size()!=indexed.size() || plain.size()>1327858375545ULL) throw Error("Selected maximum space differs");
            same(plain.at(plain.size()-1,noop),indexed.at(indexed.size()-1,noop));
            std::uint64_t seed=7419;
            for(unsigned i=0;i<200;++i) {
                seed=seed*6364136223846793005ULL+1442695040888963407ULL;
                const auto rank=seed%plain.size();same(plain.at(rank,noop),indexed.at(rank,noop));
            }
        }
        for(const auto& bad:{"","++","x","+/*-+"," +","%"}) {
            bool rejected=false;try {ExpressionGrammar g({1},3,noop,true,bad);} catch(const Error&) {rejected=true;}
            if(!rejected) throw Error("Malformed operators accepted");
        }
        if(canonical_expression_operators("*+")!="+*") throw Error("Operator order not canonical");
        ExactResearchTask invalid;invalid.expression_operators="+";
        bool rejected=false;try {(void)exact_research(invalid);} catch(const Error&) {rejected=true;}
        if(!rejected) throw Error("Foreign strategy accepted operators");
        std::cout<<"Selected operators: full-space filtering, ranks, budgets, holes and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
