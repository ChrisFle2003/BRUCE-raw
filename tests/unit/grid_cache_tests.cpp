#include "bruce/application/grid_batch.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/application/application_task.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
#include <set>
namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call) {
    bool failed=false;try {call();} catch(const bruce::Error&) {failed=true;}
    require(failed,"Invalid grid cache input accepted");
}
std::string body_of(const std::string& bundle) {
    return bundle.substr(bundle.find('\n',bundle.find('\n')+1)+1);
}
std::string rehashed(std::string_view header,const std::string& body) {
    return std::string(header)+"\n"+bruce::sha256(body)+"\n"+body;
}
}
int main() {
    using namespace bruce;
    try {
        // A stage must participate in cache identity even at the same address.
        GridTask explicit_task;explicit_task.path="2,6";explicit_task.rule=GridSpec::scaled_exponential_rule;
        GridCache stage_cache(65536);explicit_task.stage=6;
        const auto stage6=stage_cache.evaluate(explicit_task);
        explicit_task.stage=57;const auto stage57=stage_cache.evaluate(explicit_task);
        require(stage6.run.value==6300 && stage57.run.value==14411518807585587100ULL,"Stage cache values wrong");
        require(stage6.key_digest!=stage57.key_digest,"Explicit stages share cache identity");
        require(stage6.cache_event=="MISS_INSERT" && stage57.cache_event=="MISS_INSERT" &&
            stage_cache.stats().misses==2 && stage_cache.stats().entries==2,"Explicit stages share cache entry");
        require(stage_cache.evaluate(explicit_task).cache_event=="HIT","Explicit stage cache not retained");
        GridBatchTask explicit_batch;explicit_batch.spec.rule=GridSpec::scaled_exponential_rule;
        explicit_batch.spec.stage=57;explicit_batch.paths={"2,6","02,06","4,3","2,6"};
        const auto explicit_run=evaluate_grid_batch(explicit_batch);
        const auto explicit_bundle=pack_grid_batch(explicit_run);
        require(explicit_bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V3\n"),"Explicit complete batch header wrong");
        require(explicit_bundle.find("spec grid-spec-v2\n")!=std::string::npos &&
            explicit_bundle.find("zero_zero one\nstage_source explicit\nstage 57\ncache_bytes ")!=std::string::npos,
            "Explicit batch stage fields missing or out of order");
        require(explicit_run.queries[1].cache_event=="HIT" &&
            explicit_run.queries[2].run.value==14411518807585587100ULL,"Batch stage not shared by all addresses");
        require(pack_grid_batch(replay_grid_batch(explicit_bundle))==explicit_bundle,"Explicit batch replay differs");
        require(std::holds_alternative<GridBatchRun>(replay_application(explicit_bundle)),"V3 dispatcher failed");
        for(const auto* mutation:{"stage_source leaf-y\nstage 57\n","stage 57\n", "stage_source explicit\nstage 057\n",
                "stage_source explicit\nstage 58\n", "stage_source explicit\nstage 6\n",
                "stage 57\nstage_source explicit\n", "stage_source explicit\nstage 57\nstage 57\n"}) {
            auto changed=body_of(explicit_bundle);
            const auto at=changed.find("stage_source explicit\nstage 57\n");
            changed.replace(at,std::string("stage_source explicit\nstage 57\n").size(),mutation);
            rejected([&]{(void)replay_grid_batch(rehashed("BRUCE_GRID_BATCH_BUNDLE_V3",changed));});
        }
        rejected([&]{(void)replay_grid_batch(rehashed("BRUCE_GRID_BATCH_BUNDLE_V1",body_of(explicit_bundle)));});
        rejected([&]{(void)replay_grid_batch(rehashed("BRUCE_GRID_BATCH_BUNDLE_V5",body_of(explicit_bundle)));});
        explicit_batch.query_steps=1;
        const auto explicit_parent=pack_grid_batch(evaluate_grid_batch(explicit_batch));
        require(explicit_parent.starts_with("BRUCE_GRID_BATCH_BUNDLE_V4\n"),"Explicit partial batch header wrong");
        require(pack_grid_batch(replay_grid_batch(explicit_parent))==explicit_parent,"Explicit partial batch replay differs");
        require(std::holds_alternative<GridBatchRun>(replay_application(explicit_parent)),"V4 dispatcher failed");
        auto changed_parent=body_of(explicit_parent);
        changed_parent.replace(changed_parent.find("stage 57\n"),9,"stage 6\n");
        rejected([&]{(void)replay_grid_batch(rehashed("BRUCE_GRID_BATCH_BUNDLE_V4",changed_parent));});
        const auto explicit_child=resume_grid_batch(explicit_parent,1);
        const auto explicit_child_bundle=pack_grid_resume(explicit_child);
        require(explicit_child_bundle.starts_with("BRUCE_GRID_RESUME_BUNDLE_V2\n"),"Explicit resume header wrong");
        require(body_of(explicit_child_bundle).starts_with("policy verified-prefix-cache-rebuild-v1\nstage_source explicit\nstage 57\nadditional_queries "),
            "Resume derived stage fields missing or out of order");
        require(explicit_child.batch.task.spec.stage==57 && explicit_child.batch.queries[1].cache_event=="HIT" &&
            explicit_child.new_evaluations==0,"Resume lost stage or prefix cache");
        require(pack_grid_resume(replay_grid_resume(explicit_child_bundle))==explicit_child_bundle,"Explicit resume replay differs");
        require(std::holds_alternative<GridResumeRun>(replay_application(explicit_child_bundle)),"Resume V2 dispatcher failed");
        require(replay_grid_source(explicit_child_bundle).task.spec.stage==57,"Resume V2 source not recognized");
        const auto explicit_grandchild=resume_grid_batch(explicit_child_bundle,2);
        require(explicit_grandchild.generation==2 && explicit_grandchild.batch.task.spec.stage==57 &&
            explicit_grandchild.batch.queries[2].run.value==14411518807585587100ULL &&
            explicit_grandchild.batch.queries[3].cache_event=="HIT","Chained resume lost stage/cache");
        require(pack_grid_resume(replay_grid_resume(pack_grid_resume(explicit_grandchild)))==pack_grid_resume(explicit_grandchild),
            "Chained explicit resume replay differs");
        for(const auto* mutation:{"stage_source leaf-y\nstage 57\n","stage 57\n", "stage_source explicit\nstage 057\n",
                "stage_source explicit\nstage 6\n", "stage 57\nstage_source explicit\n",
                "stage_source explicit\nstage 58\n", "stage_source explicit\nstage 18446744073709551616\n",
                "stage_source explicit\nstage 57\nstage 57\n", "stage_source explicit\n"}) {
            auto changed=body_of(explicit_child_bundle);
            changed.replace(changed.find("stage_source explicit\nstage 57\n"),std::string("stage_source explicit\nstage 57\n").size(),mutation);
            rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V2",changed));});
        }
        rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V1",body_of(explicit_child_bundle)));});
        rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V3",body_of(explicit_child_bundle)));});
        auto downgraded_resume=body_of(explicit_child_bundle);
        downgraded_resume.erase(downgraded_resume.find("stage_source explicit\nstage 57\n"),
            std::string("stage_source explicit\nstage 57\n").size());
        rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V1",downgraded_resume));});
        explicit_batch.spec.stage=0;
        const auto zero_parent=pack_grid_batch(evaluate_grid_batch(explicit_batch));
        const auto zero_resume=resume_grid_batch(zero_parent,3);
        require(zero_parent.starts_with("BRUCE_GRID_BATCH_BUNDLE_V4\n") && zero_resume.batch.task.spec.stage==0 &&
            zero_resume.batch.queries[2].run.value==0,"Explicit zero stage treated as absent");
        require(pack_grid_resume(zero_resume).starts_with("BRUCE_GRID_RESUME_BUNDLE_V2\n") &&
            pack_grid_resume(replay_grid_resume(pack_grid_resume(zero_resume)))==pack_grid_resume(zero_resume),
            "Explicit zero resume lost version or replay");
        GridTask t;t.path="2,6";
        GridCache rule_cache(65536);
        const auto leaf=rule_cache.evaluate(t);
        t.rule=GridSpec::scaled_exponential_rule;
        const auto scaled=rule_cache.evaluate(t);
        require(leaf.run.value==64 && scaled.run.value==6300,"Rule-specific cache values wrong");
        require(leaf.key_digest!=scaled.key_digest,"Rules share a cache identity");
        require(leaf.cache_event=="MISS_INSERT" && scaled.cache_event=="MISS_INSERT" &&
            rule_cache.stats().misses==2 && rule_cache.stats().entries==2,"Rules share a cache entry");
        require(rule_cache.evaluate(t).cache_event=="HIT","Scaled entry not retained");
        t.rule=GridSpec::leaf_local_power_rule;
        require(rule_cache.evaluate(t).run.value==64,"Scaled entry polluted legacy cache");
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
        require(bundle.starts_with("BRUCE_GRID_BATCH_BUNDLE_V1\n"),"Legacy complete batch header changed");
        require(render_grid_batch(replay_grid_batch(bundle))==render_grid_batch(run),"Batch replay differs");
        GridBatchTask scaled_batch;scaled_batch.spec.rule=GridSpec::scaled_exponential_rule;
        scaled_batch.paths={"2,6","02,06","4,3"};
        const auto scaled_run=evaluate_grid_batch(scaled_batch);
        const auto scaled_bundle=pack_grid_batch(scaled_run);
        require(scaled_bundle.find("value_rule SCALED_EXPONENTIAL_V1\n")!=std::string::npos,
            "Batch evidence lost selected rule");
        require(render_grid_batch(replay_grid_batch(scaled_bundle))==render_grid_batch(scaled_run),
            "Scaled batch replay differs");
        require(scaled_run.queries[0].run.value==6300 && scaled_run.queries[1].cache_event=="HIT" &&
            scaled_run.queries[2].run.value==700,"Scaled batch values or cache reuse wrong");
        const auto scaled_body=scaled_bundle.substr(scaled_bundle.find('\n',scaled_bundle.find('\n')+1)+1);
        for(const auto* replacement:{GridSpec::leaf_local_power_rule,"UNKNOWN_RULE"}) {
            auto changed=scaled_body;
            const auto at=changed.find("value_rule SCALED_EXPONENTIAL_V1\n");
            changed.replace(at,std::string("value_rule SCALED_EXPONENTIAL_V1").size(),
                "value_rule "+std::string(replacement));
            rejected([&]{(void)replay_grid_batch("BRUCE_GRID_BATCH_BUNDLE_V1\n"+sha256(changed)+"\n"+changed);});
        }
        scaled_batch.query_steps=1;
        const auto scaled_parent=pack_grid_batch(evaluate_grid_batch(scaled_batch));
        require(scaled_parent.starts_with("BRUCE_GRID_BATCH_BUNDLE_V2\n") &&
            scaled_parent.find("stage_source ")==std::string::npos,"Legacy partial batch changed");
        const auto scaled_child=resume_grid_batch(scaled_parent,2);
        require(pack_grid_resume(scaled_child).starts_with("BRUCE_GRID_RESUME_BUNDLE_V1\n"),"Legacy resume header changed");
        rejected([&]{(void)replay_grid_batch(rehashed("BRUCE_GRID_BATCH_BUNDLE_V4",body_of(scaled_parent)));});
        rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V2",body_of(pack_grid_resume(scaled_child))));});
        auto promoted_resume=body_of(pack_grid_resume(scaled_child));
        promoted_resume.insert(promoted_resume.find('\n')+1,"stage_source explicit\nstage 57\n");
        rejected([&]{(void)replay_grid_resume(rehashed("BRUCE_GRID_RESUME_BUNDLE_V2",promoted_resume));});
        require(scaled_child.batch.task.spec.rule==GridSpec::scaled_exponential_rule &&
            scaled_child.batch.queries[1].cache_event=="HIT" && scaled_child.batch.queries[2].run.value==700,
            "Resume lost selected rule or cache prefix");
        require(render_grid_resume(replay_grid_resume(pack_grid_resume(scaled_child)))==render_grid_resume(scaled_child),
            "Scaled resume replay differs");
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
