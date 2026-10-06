#include "bruce/application/exact_research.hpp"
#include <iostream>

int main() {
    using namespace bruce;
    try {
        const auto same=[](const Expression& a,const Expression& b) {
            if(a.size()!=b.size()) throw Error("Cached tree size differs");
            for(std::size_t i=0;i<a.size();++i) if(a[i].op!=b[i].op || a[i].left!=b[i].left || a[i].right!=b[i].right || a[i].value!=b[i].value)
                throw Error("Cached tree structure differs");
            validate_expression(a);
        };
        auto free=[](std::uint64_t){};
        const std::string alphabet="+-*/^";
        for(unsigned mask=1;mask<32;++mask) {
            std::string ops;
            for(unsigned i=0;i<5;++i) if(mask&(1u<<i)) ops+=alphabet[i];
            ExpressionGrammar plain({2,3,ExactValue(1,2)},5,free,false,ops);
            ExpressionGrammar cached({2,3,ExactValue(1,2)},5,free,true,ops,true);
            for(std::uint64_t i=0;i<plain.size();++i) same(plain.at(i,free),cached.at(i,free));
            for(auto i=plain.size();i-->0;) same(plain.at(i,free),cached.at(i,free));
            if(cached.cache_stats().entries>64) throw Error("Unbounded cache");
        }
        std::vector<ExactValue> constants;for(int i=0;i<64;++i) constants.push_back(i);
        ExpressionGrammar plain(constants,9,free,false,alphabet),cached(constants,9,free,true,alphabet,true);
        std::uint64_t random=731;
        for(unsigned i=0;i<2000;++i) {
            random=random*6364136223846793005ULL+1;
            const auto id=random%plain.size();same(plain.at(id,free),cached.at(id,free));
        }
        same(plain.at(plain.size()-1,free),cached.at(plain.size()-1,free));
        if(!cached.cache_stats().evictions || cached.cache_stats().entries>64) throw Error("Cache collision probe failed");
        // The same subtree at different offsets must relocate all internal references.
        ExpressionGrammar small({1},7,free,false),warm({1},7,free,true,"+-*/",true);
        for(std::uint64_t i=0;i<small.size();++i) same(small.at(i,free),warm.at(i,free));
        if(!warm.cache_stats().hits || !warm.cache_stats().misses) throw Error("Cache never reused");
        // An interrupted lookup/build cannot corrupt subsequent independent calls.
        const auto last=small.size()-1;
        std::uint64_t cost=0;ExpressionGrammar probe({1},7,free,true,"+-*/",true);
        probe.at(last,[&](std::uint64_t n){cost+=n;});
        for(std::uint64_t budget=0;budget<cost;++budget) {
            ExpressionGrammar interrupted({1},7,free,true,"+-*/",true);std::uint64_t used=0;bool stopped=false;
            try {interrupted.at(last,[&](std::uint64_t n){if(n>budget-used) throw Error("stop");used+=n;});}
            catch(const Error& e) {if(std::string_view(e.what())!="stop") throw;stopped=true;}
            if(!stopped || used>budget) throw Error("Cache budget not enforced");
            same(small.at(last,free),interrupted.at(last,free));
        }
        for(const auto& mode:{"syntax","rational"}) {
            ExactResearchTask t;t.strategy="expression";t.planner="cached-grammar";t.domain="rational";
            t.expression_equivalence=mode;t.expression_operators="+^";t.minimum="2";t.maximum="2";t.max_nodes=5;
            t.training={{2,6},{3,11}};t.holdout={{0,2},{-1,3}};
            const auto full=exact_research(t);
            if(full.status!="VERIFIED_ON_SAMPLES" || !full.expression_cache.hits) throw Error("Cached research incomplete");
            auto replay=[](const ExactResearchRun& r) {
                if(render_exact_research(replay_exact_research(pack_exact_research(r)))!=render_exact_research(r)) throw Error("Cached replay differs");
            };
            replay(full);
            for(std::uint64_t n=1;n<full.operations;++n) {
                t.budget.operations=n;const auto r=exact_research(t);
                if(r.status!="INCOMPLETE" || r.operations>n) throw Error("Cache operation cutoff differs");
                replay(r);
            }
            t.budget.operations=full.operations;
            if(exact_research(t).status!=full.status) throw Error("Exact cache budget fails");
            for(std::uint64_t n=0;n<full.checkpoints;++n) {
                t.cancel_after_checkpoints=n;const auto r=exact_research(t);
                if(r.status!="INCOMPLETE" || r.checkpoints!=n || r.stop_reason!="Cancelled") throw Error("Cache checkpoint differs");
                replay(r);
            }
            t.cancel_after_checkpoints.reset();
            for(std::uint64_t n=0;n<full.candidates;++n) {
                t.cancel_after_candidates=n;const auto r=exact_research(t);
                if(r.status!="INCOMPLETE" || r.candidates!=n || r.stop_reason!="Cancelled") throw Error("Cache candidate cutoff differs");
                replay(r);
            }
            t.cancel_after_candidates.reset();
            unsigned polls=0;const auto cancelled=exact_research(t,[&](std::uint64_t){return ++polls==4;});
            if(cancelled.stop_reason!="Cancelled") throw Error("Cache callback ignored");
            replay(cancelled);
        }
        std::cout<<"Cached grammar: all operator subsets, relocation, collisions, interruption, bounds and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
