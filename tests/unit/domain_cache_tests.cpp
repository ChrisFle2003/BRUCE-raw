#include "bruce/math/domain_cache.hpp"
#include "bruce/application/exact_research.hpp"
#include <iostream>
namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
std::uint64_t slot(const std::string& key) {
    std::uint64_t hash=14695981039346656037ULL;
    for(const unsigned char c:key) hash=(hash^c)*1099511628211ULL;
    return hash%64;
}
void replay(const bruce::ExactResearchRun& r) {
    require(bruce::render_exact_research(bruce::replay_exact_research(bruce::pack_exact_research(r)))==bruce::render_exact_research(r),"Cache replay differs");
}
}
int main() {
    using namespace bruce;
    try {
        NumberDomain d("rational",16384);
        const auto spend=[](std::uint64_t){};
        EqualDomainCache cache(d);
        require(cache.compare({1},{1},spend).relation=="EQUAL","Initial equality failed");
        require(cache.compare({1},{1},spend).reason=="CACHED_EQUAL" && cache.stats().hits==1,"Repeated proof not reused");
        // Find a same-slot key with an actual rational hole. Hash identity must
        // never stand in for exact key comparison.
        unsigned hole=1;
        while(slot("["+std::to_string(hole)+";1;][1;]")!=slot("[1;][1;]")) ++hole;
        require(cache.compare({ExactValue(hole),1},{1},spend).relation=="DIFFERENT","Hash collision erased hole");
        require(cache.stats().entries==1 && cache.stats().evictions==0,"Different result was cached");
        require(cache.compare({1},{1},spend).reason=="CACHED_EQUAL","Non-equality replaced proof");
        unsigned rootless=1;
        while(slot("["+std::to_string(rootless)+";0;1;][1;]")!=slot("[1;][1;]")) ++rootless;
        require(cache.compare({ExactValue(rootless),0,1},{1},spend).relation=="EQUAL" && cache.stats().evictions==1,"Collision replacement failed");
        const auto hits=cache.stats().hits;
        require(cache.compare({1},{1},spend).relation=="EQUAL" && cache.stats().hits==hits,"Evicted entry returned stale hit");
        const auto inserts=cache.stats().inserts;
        for(unsigned i=0;i<2;++i) require(cache.compare({-36,0,36,0,-11,0,1},{1},spend).relation=="UNKNOWN","Unknown became truth");
        require(cache.stats().inserts==inserts,"Unknown result cached");
        mpz_class large=1;large<<=14000;
        require(cache.compare({ExactValue(large)},{1},spend).relation=="EQUAL" && cache.stats().bypasses==1,"Oversize key not bypassed");
        for(unsigned i=1;i<300;++i) (void)cache.compare({ExactValue(i),0,1},{1},spend);
        require(cache.stats().entries<=64 && cache.stats().key_bytes<=64*4096,"Cache storage unbounded");
        // Every cutoff through key creation, proof and insertion. Reuse the
        // interrupted cache to ensure no half-computed equality was published.
        EqualDomainCache reference(d);std::uint64_t total=0;
        (void)reference.compare({-2,0,0,1},{1},[&](std::uint64_t n){total+=n;});
        for(std::uint64_t limit=0;limit<total;++limit) {
            EqualDomainCache interrupted(d);std::uint64_t used=0;bool stopped=false;
            try {(void)interrupted.compare({-2,0,0,1},{1},[&](std::uint64_t n){if(n>limit-used) throw Error("stop");used+=n;});}
            catch(const Error&) {stopped=true;}
            require(stopped && used==limit && interrupted.stats().entries==0,"Incomplete proof entered cache");
            require(interrupted.compare({-2,0,0,1},{1},spend).relation=="EQUAL" && interrupted.stats().hits==0,"Partial entry reused");
        }
        for(const std::string planner:{"grammar","indexed-grammar","cached-grammar"}) {
            ExactResearchTask t;t.domain="rational";t.strategy="expression";t.planner=planner;
            t.expression_equivalence="rational-q-modular";t.domain_cache="equal-64";
            t.minimum="1";t.maximum="1";t.max_nodes=3;t.expression_operators="/";
            t.training={{2,1},{3,1}};t.holdout={{4,1}};
            const auto full=exact_research(t);replay(full);
            require(full.status=="AMBIGUOUS" && full.hypotheses.size()==2,"Cached grouping erased x/x hole");
            for(std::uint64_t limit=1;limit<full.operations;++limit) {
                t.budget.operations=limit;const auto r=exact_research(t);replay(r);
                require(r.status=="INCOMPLETE" && r.operations<=limit,"Cache operation limit ignored");
                require(r.domain_cache_stats.operations<=r.domain_comparison_operations,"Cache overhead not in shared budget");
            }
            t.budget.operations=full.operations;require(exact_research(t).status==full.status,"Exact cached budget failed");
            for(std::uint64_t cut=0;cut<full.checkpoints;++cut) {
                t.cancel_after_checkpoints=cut;const auto r=exact_research(t);replay(r);
                require(r.status=="INCOMPLETE" && r.stop_reason=="Cancelled","Cache cancellation ignored");
            }
        }
        std::cout<<"Cache identity, collision, eviction, bypass, every insertion cutoff and research replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
