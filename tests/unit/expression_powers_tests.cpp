#include "bruce/application/exact_research.hpp"
#include <iostream>
#include <set>

namespace {
bruce::Expression join(const bruce::Expression& a,const bruce::Expression& b,char op) {
    auto result=a;
    const unsigned shift=a.size();
    for(auto node:b) {
        if(node.op!='x' && node.op!='c') {node.left+=shift;node.right+=shift;}
        result.push_back(std::move(node));
    }
    result.push_back({op,shift-1,static_cast<unsigned>(result.size()-1),0});return result;
}
}
int main() {
    using namespace bruce;
    try {
        for(const auto& mode:{"syntax","rational"}) for(const auto& planner:{"grammar","indexed-grammar"}) {
            ExactResearchTask task;
            task.strategy="expression";task.planner=planner;task.domain="rational";
            task.expression_equivalence=mode;task.expression_operators="^";
            task.minimum="2";task.maximum="2";task.max_nodes=3;
            task.training={{2,4},{3,9}};
            task.holdout={{-1,1}};
            const auto full=exact_research(task);
            if(full.status!="VERIFIED_ON_SAMPLES" || full.candidates!=4 || full.checkpoints!=5) throw Error("Expression reference failed");
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
        const NumberDomain d("rational",4096);
        const Expression x{{'x',0,0,0}};
        const auto constant=[](ExactValue value){return Expression{{'c',0,0,std::move(value)}};};
        const auto same=[](const Expression& a,const Expression& b) {
            if(a.size()!=b.size()) throw Error("Power tree size differs");
            for(std::size_t i=0;i<a.size();++i)
                if(a[i].op!=b[i].op || a[i].left!=b[i].left || a[i].right!=b[i].right || a[i].value!=b[i].value)
                    throw Error("Power tree differs");
        };
        const auto power=[&](const Expression& a,ExactValue exponent){return join(a,constant(std::move(exponent)),'^');};
        for(unsigned exponent=2;exponent<=8;++exponent) for(const auto& value:{ExactValue(-2),ExactValue(-3,2),ExactValue(0),ExactValue(2)}) {
            const auto tree=power(x,exponent);
            mpz_class numerator,denominator;
            mpz_pow_ui(numerator.get_mpz_t(),value.get_num().get_mpz_t(),exponent);
            mpz_pow_ui(denominator.get_mpz_t(),value.get_den().get_mpz_t(),exponent);
            ExactValue expected(numerator,denominator);expected.canonicalize();
            const auto normal=normalize_expression(d,tree,noop);
            if(evaluate_expression(d,tree,value,noop)!=expected || counter_evaluate_expression(d,tree,value,noop)!=expected ||
               evaluate_normal_form(d,normal,value,noop)!=expected) throw Error("Power evaluators differ from GMP oracle");
        }
        for(const auto& exponent:{ExactValue(-1),ExactValue(0),ExactValue(1),ExactValue(9),ExactValue(1,2)}) {
            bool rejected=false;try {(void)evaluate_expression(d,power(x,exponent),2,noop);} catch(const Error&) {rejected=true;}
            if(!rejected) throw Error("Invalid literal exponent accepted");
        }
        for(const auto& exponent:{x,join(constant(1),constant(1),'+')}) {
            bool rejected=false;try {(void)normalize_expression(d,join(x,exponent,'^'),noop);} catch(const Error&) {rejected=true;}
            if(!rejected) throw Error("Computed exponent accepted");
        }
        for(const auto& base:{join(constant(1),x,'/'),join(x,x,'/'),join(constant(0),join(constant(1),x,'/'),'*')}) {
            const auto tree=power(base,2);const auto normal=normalize_expression(d,tree,noop);
            if(normal.excluded_polynomial!=std::vector<ExactValue>{0,1}) throw Error("Power lost original hole");
            for(unsigned path=0;path<3;++path) {
                bool pole=false;
                try {if(path==0) (void)evaluate_expression(d,tree,0,noop);else if(path==1) (void)counter_evaluate_expression(d,tree,0,noop);else (void)evaluate_normal_form(d,normal,0,noop);}
                catch(const ExpressionPole&) {pole=true;}
                if(!pole) throw Error("Power filled division hole");
            }
        }
        const auto empty=power(join(constant(1),join(x,x,'-'),'/'),2);
        if(!normalize_expression(d,empty,noop).empty_domain) throw Error("Power filled empty domain");
        const auto degree64=power(power(x,8),8);
        const auto normal64=normalize_expression(d,degree64,noop);
        if(normal64.numerator.size()!=65 || normal64.numerator.back()!=1) throw Error("Degree64 boundary failed");
        bool degree_limited=false;try {(void)normalize_expression(d,power(degree64,8),noop);} catch(const ExpressionNormalizationLimit&) {degree_limited=true;}
        if(!degree_limited) throw Error("Power escaped normalization degree limit");
        const NumberDomain tiny("rational",64);mpz_class huge=1;huge<<=40;
        for(unsigned path=0;path<2;++path) {
            bool limited=false;
            try {if(path==0) (void)evaluate_expression(tiny,power(x,8),ExactValue(huge),noop);else (void)counter_evaluate_expression(tiny,power(x,8),ExactValue(huge),noop);}
            catch(const NumericLimit&) {limited=true;}
            if(!limited) throw Error("Power escaped intermediate bit limit");
        }
        // Independent materialized Cartesian oracle: no count formulas or rank decoding.
        for(const auto& constants:{std::vector<ExactValue>{},std::vector<ExactValue>{2},std::vector<ExactValue>{-1,2,3,9,ExactValue(1,2)}}) {
            for(unsigned mask=16;mask<32;++mask) {
                std::string ops;for(unsigned bit=0;bit<5;++bit) if(mask&(1u<<bit)) ops+=std::string("+-*/^")[bit];
                std::array<std::vector<Expression>,6> sizes;
                sizes[1].push_back(x);for(const auto& c:constants) sizes[1].push_back(constant(c));
                for(unsigned size:{3u,5u}) for(char op:ops) for(unsigned left=1;left<size-1;left+=2) {
                    const unsigned right=size-left-1;const bool commutative=op=='+' || op=='*';
                    if(commutative && left>right) continue;
                    for(std::size_t a=0;a<sizes[left].size();++a) for(std::size_t b=0;b<sizes[right].size();++b) {
                        if(commutative && left==right && a>b) continue;
                        const auto& rhs=sizes[right][b];
                        if(op=='^' && (rhs.size()!=1 || rhs[0].op!='c' || rhs[0].value.get_den()!=1 || rhs[0].value<2 || rhs[0].value>8)) continue;
                        sizes[size].push_back(join(sizes[left][a],rhs,op));
                    }
                }
                const ExpressionGrammar plain(constants,5,noop,false,ops),indexed(constants,5,noop,true,ops);
                std::uint64_t id=0;
                for(unsigned size:{1u,3u,5u}) for(const auto& tree:sizes[size]) {
                    same(tree,plain.at(id,noop));same(tree,indexed.at(id,noop));++id;
                }
                if(id!=plain.size() || id!=indexed.size()) throw Error("Power grammar completeness differs");
            }
        }
        std::vector<ExactValue> constants;for(unsigned i=0;i<64;++i) constants.emplace_back(i);
        const ExpressionGrammar maximum(constants,9,noop,true,"+-*/^"),reference(constants,9,noop,false,"+-*/^");
        if(maximum.size()!=1450321801175ULL || maximum.index_entries()!=36) throw Error("Maximum power grammar differs");
        same(maximum.at(maximum.size()-1,noop),reference.at(reference.size()-1,noop));
        std::uint64_t seed=2442;
        for(unsigned i=0;i<2000;++i) {
            seed=seed*6364136223846793005ULL+1442695040888963407ULL;
            same(maximum.at(seed%maximum.size(),noop),reference.at(seed%reference.size(),noop));
        }
        std::cout<<"Expression powers: numeric oracle, degree/bit limits, domain holes, Cartesian grammar and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
