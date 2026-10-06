#include "bruce/application/grid_batch.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
#include <set>
namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call) {
    bool failed=false;try {call();} catch(const bruce::Error&) {failed=true;}
    require(failed,"Invalid grid cache input accepted");
}
}
int main() {
    using namespace bruce;
    try {
        GridTask t;t.path="2,6";
        std::set<std::string> keys{grid_cache_key(t)};
        t.path="02,06";require(keys.contains(grid_cache_key(t)),"Equivalent address spelling split");
        for(const auto* path:{"4,3","1,1/2,6","2,6/1,1"}) {t.path=path;require(keys.insert(grid_cache_key(t)).second,"Address collision");}
        t.path="2,6";t.max_bits=7;require(keys.insert(grid_cache_key(t)).second,"Bit limit not isolated");
        t.max_bits=64;t.zero_zero="stop";require(keys.insert(grid_cache_key(t)).second,"Sentinel policy not isolated");
        t.zero_zero="one";t.mode="metric";require(keys.insert(grid_cache_key(t)).second,"Mode not isolated");
        t.mode="modular";t.modulus=7;require(keys.insert(grid_cache_key(t)).second,"Modular key collision");
        t.modulus=11;require(keys.insert(grid_cache_key(t)).second,"Modulus not isolated");
        GridCache cache(65536);
        t=GridTask{};t.path="2,6";
        require(cache.evaluate(t).run.value==64,"Exact value wrong");
        t.mode="modular";t.modulus=7;require(cache.evaluate(t).run.value==1,"Cross-mode pollution");
        t.modulus=11;require(cache.evaluate(t).run.value==9,"Cross-modulus pollution");
        t=GridTask{};t.path="2,6";t.max_bits=6;
        rejected([&]{(void)cache.evaluate(t);});
        t.max_bits=64;require(cache.evaluate(t).cache_event=="HIT","Valid entry lost after invalid request");
        t.path="0,0";t.zero_zero="stop";require(cache.evaluate(t).run.stopped,"Sentinel ignored");
        t.zero_zero="one";require(cache.evaluate(t).run.value==1,"Stop poisoned following policy");
        // FIFO stays FIFO on a hit; all 64 distinct local addresses fit the byte budget.
        GridCache fifo(1048576);
        for(unsigned i=0;i<64;++i) {t=GridTask{};t.path=std::to_string(i%8)+","+std::to_string(i/8);(void)fifo.evaluate(t);}
        t.path="0,0";require(fifo.evaluate(t).cache_event=="HIT","First entry missing");
        t.path="1,1/0,0";(void)fifo.evaluate(t);
        t.path="0,0";require(fifo.evaluate(t).cache_event=="MISS_INSERT","FIFO order changed on hit");
        require(fifo.stats().entries==64 && fifo.stats().evictions==2,"Entry cap failed");
        GridCache disabled(0);t.path="2,6";(void)disabled.evaluate(t);(void)disabled.evaluate(t);
        require(disabled.stats().entries==0 && disabled.stats().evaluations==2,"Disabled cache retained value");
        GridBatchTask batch;batch.paths={"2,6","02,06","4,3"};
        auto run=evaluate_grid_batch(batch);const auto bundle=pack_grid_batch(run);
        require(render_grid_batch(replay_grid_batch(bundle))==render_grid_batch(run),"Batch replay differs");
        const auto body=bundle.substr(bundle.find('\n',bundle.find('\n')+1)+1);
        for(std::size_t n=0;n<body.size();++n) {
            const auto part=body.substr(0,n);
            rejected([&]{(void)replay_grid_batch("BRUCE_GRID_BATCH_BUNDLE_V1\n"+sha256(part)+"\n"+part);});
        }
        batch.paths={};rejected([&]{(void)evaluate_grid_batch(batch);});
        batch.paths=std::vector<std::string>(1025,"1,1");rejected([&]{(void)evaluate_grid_batch(batch);});
        rejected([]{GridCache invalid(1048577);});
        GridBatchTask sequence;sequence.paths={"2,6","02,06","0,0","4,3"};sequence.spec.zero_zero="stop";
        for(std::size_t at=0;at<=sequence.paths.size();++at) {
            std::size_t calls=0;
            const auto cancelled=evaluate_grid_batch(sequence,[&](std::size_t n){++calls;return n==at;});
            require(cancelled.queries.size()==at,"Wrong cancellation boundary");
            require(cancelled.status==(at<4 ? "INCOMPLETE" : "COMPLETED"),"Cancellation status wrong");
            require(calls==(at<4 ? at+1 : 4),"Unexpected checkpoint count");
            if(at<4) require(cancelled.task.cancel_after_queries==at,"Cancellation position missing");
            require(render_grid_batch(replay_grid_batch(pack_grid_batch(cancelled)))==render_grid_batch(cancelled),"Cancelled batch replay differs");
        }
        sequence.query_steps=1;
        const auto both=evaluate_grid_batch(sequence,[](std::size_t n){return n==1;});
        require(both.stop_reason=="Cancelled","Cancellation precedence changed");
        sequence.query_steps=0;
        const auto empty=evaluate_grid_batch(sequence);
        require(empty.queries.empty() && empty.cache.entries==0 && empty.cache.evaluations==0,"Zero query budget did work");
        sequence.paths={"2,6","bad"};rejected([&]{(void)evaluate_grid_batch(sequence);});
        sequence.paths={"2,6"};sequence.cancel_after_queries=2;rejected([&]{(void)evaluate_grid_batch(sequence);});
        GridBatchTask resumable;resumable.paths={"2,6","02,06","4,3","2,6"};resumable.query_steps=1;
        const auto parent=pack_grid_batch(evaluate_grid_batch(resumable));
        for(std::size_t at=1;at<=4;++at) {
            const auto child=resume_grid_batch(parent,4,[at](std::size_t n){return n==at;});
            require(child.batch.queries.size()==at,"Resume cancellation boundary");
            require(render_grid_resume(replay_grid_resume(pack_grid_resume(child)))==render_grid_resume(child),"Resume cancellation replay");
        }
        resumable.paths.clear();resumable.cache_bytes=4000;
        for(unsigned n=0;n<70;++n) resumable.paths.push_back(std::to_string(n/8)+",0/"+std::to_string(n%8)+",1");
        // All coordinates must remain local 0..7.
        resumable.paths.resize(64);resumable.paths.push_back(resumable.paths.front());
        resumable.query_steps=32;
        const auto resumed=resume_grid_batch(pack_grid_batch(evaluate_grid_batch(resumable)),40);
        resumable.query_steps=65;
        const auto uninterrupted=evaluate_grid_batch(resumable);
        require(render_grid_batch(resumed.batch)==render_grid_batch(uninterrupted),"Resumed FIFO differs");
        require(resumed.batch.cache.evictions>0,"Resume FIFO eviction untested");
        std::cout<<"Grid identities, cache isolation, FIFO, bounds and replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
