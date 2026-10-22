#include "bruce/application/grid_stage_sweep.hpp"
#include "bruce/application/application_task.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>
#include <set>

namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call,std::string_view reason) {
    try {call();} catch(const bruce::Error& error) {
        require(std::string_view(error.what()).starts_with(reason),"Wrong rejection reason");return;
    }
    throw bruce::Error("Invalid sweep accepted");
}
std::string mutate(const std::string& bundle,std::string_view before,std::string_view after) {
    const auto header_end=bundle.find('\n');
    auto body=bundle.substr(header_end+66);
    const auto at=body.find(before);
    require(at!=std::string::npos,"Mutation target missing");
    body.replace(at,before.size(),after);
    return bundle.substr(0,header_end+1)+bruce::sha256(body)+"\n"+body;
}
}
int main() {
    using namespace bruce;
    try {
        // Catches wrong formula, skipped stages, implicit leaf stage, shared keys and sentinel stops.
        GridStageSweepTask task;task.path="0,0";task.to_stage=7;
        const auto run=evaluate_grid_stage_sweep(task);
        const std::uint64_t expected[]={0,100,300,700,1500,3100,6300,12700};
        require(run.status=="COMPLETED" && run.stop_reason=="Completed" && !run.stopped_stage,"Complete status wrong");
        require(run.records.size()==8,"Sweep omitted records");
        require(run.spec.rule==GridSpec::scaled_exponential_rule && run.spec.zero_zero=="one","Sweep spec wrong");
        std::set<std::string> keys;
        for(std::size_t i=0;i<8;++i) {
            const auto& r=run.records[i];
            require(r.index==i && r.stage==i && r.run.task.stage==i,"Stage order/source wrong");
            require(r.run.value==expected[i] && !r.run.stopped && r.status=="EVALUATED","Sweep value wrong");
            require(r.run.trace.empty(),"Debug-off retained Grid trace");
            require(r.cache_event=="MISS_INSERT","Fresh sweep did not miss");keys.insert(r.key_digest);
        }
        require(keys.size()==8 && run.cache.evaluations==8 && run.cache.misses==8 && run.cache.hits==0,"Cache identity/stats wrong");
        const auto compact=pack_grid_stage_sweep(run);
        require(compact.starts_with("BRUCE_GRID_STAGE_SWEEP_BUNDLE_V1\n"),"Sweep bundle header wrong");
        require(render_grid_stage_sweep(run).find("\"cache_key_digest\":\""+run.records[0].key_digest+"\"")!=std::string::npos,
            "Per-record cache digest missing");
        require(compact.find("PATH_VALIDATED")==std::string::npos && render_grid_stage_sweep(run).find("\"trace\"")==std::string::npos,"Debug-off leaked traces");
        require(pack_grid_stage_sweep(replay_grid_stage_sweep(compact))==compact,"Complete replay changed bytes");
        // Catches a missing central header route and misclassified inspection/status.
        const auto generic=replay_application(compact);
        require(render_application(generic)==render_grid_stage_sweep(run),"Generic replay changed result");
        require(pack_application(generic)==compact,"Generic pack changed bundle");
        require(application_kind(generic)=="grid-stage-sweep","Generic kind wrong");
        const auto assessment=assess_application(generic);
        require(assessment.complete && assessment.claim_scope==ClaimScope::grid_evaluation &&
            assessment.status=="COMPLETED" && !assessment.proof_established,"Generic assessment wrong");
        require(inspect_application(compact).find("\"kind\":\"grid-stage-sweep\"")!=std::string::npos,"Generic inspection wrong");

        // Catches debug changing cache charging or values, or hiding trace in evidence.
        task.debug=true;const auto debug=evaluate_grid_stage_sweep(task);
        require(debug.cache.bytes==run.cache.bytes && debug.cache.peak_bytes==run.cache.peak_bytes,"Debug changed cache accounting");
        for(std::size_t i=0;i<8;++i) {
            require(!debug.records[i].run.trace.empty(),"Debug trace missing");
            require(debug.records[i].run.value==run.records[i].run.value && debug.records[i].key_digest==run.records[i].key_digest &&
                debug.records[i].cache_event==run.records[i].cache_event,"Debug changed compact result");
        }
        const auto verbose=pack_grid_stage_sweep(debug);
        require(verbose!=compact && verbose.find("PATH_VALIDATED")!=std::string::npos &&
            render_grid_stage_sweep(debug).find("\"trace\"")!=std::string::npos,"Debug did not expose traces");
        require(pack_grid_stage_sweep(replay_grid_stage_sweep(verbose))==verbose,"Debug replay changed bytes");

        // Catches swallowed errors, discarded prefixes, and wrong 63-bit overflow boundary.
        task.debug=false;task.to_stage=57;task.max_bits=63;
        const auto partial=evaluate_grid_stage_sweep(task);
        require(partial.status=="INCOMPLETE" && partial.stop_reason=="IntegerBitLimit" && partial.stopped_stage==57,"Partial stop wrong");
        require(partial.records.size()==57 && partial.records.back().stage==56 &&
            partial.records.back().run.value==7205759403792793500ULL && partial.cache.evaluations==57,"Partial prefix lost");
        const auto partial_bundle=pack_grid_stage_sweep(partial);
        const auto partial_assessment=assess_application(replay_application(partial_bundle));
        require(!partial_assessment.complete && partial_assessment.status=="INCOMPLETE" &&
            partial_assessment.stop_reason=="IntegerBitLimit","Generic partial assessment wrong");
        require(pack_grid_stage_sweep(replay_grid_stage_sweep(partial_bundle))==partial_bundle,"Partial replay changed bytes");
        task.from_stage=57;task.max_bits=1;
        const auto empty=evaluate_grid_stage_sweep(task);
        require(empty.records.empty() && empty.stopped_stage==57 && empty.cache.evaluations==0,"First-stage overflow not retained");

        // Catches Metric accidentally applying Exact budget, and modular zero-exponential handling.
        task.from_stage=0;task.mode="metric";
        const auto metric=evaluate_grid_stage_sweep(task);
        require(metric.records.size()==58 && metric.status=="COMPLETED" && metric.records.back().run.value==14411518807585587100ULL,"Metric stage 57 wrong");
        require(render_grid_stage_sweep(metric).find("\"bit_length\":64")!=std::string::npos,"Metric bit length missing");
        task.mode="modular";task.modulus=128;
        const auto modular=evaluate_grid_stage_sweep(task);
        require(modular.records.size()==58 && modular.records[0].run.value==0 && modular.records[6].run.value==28 &&
            modular.records[7].run.value==28 && modular.records.back().run.value==28,"Modular sweep wrong");
        require(pack_grid_stage_sweep(replay_grid_stage_sweep(pack_grid_stage_sweep(modular)))==pack_grid_stage_sweep(modular),"Modular replay wrong");

        // Catches validation being converted into an apparently publishable partial run.
        task=GridStageSweepTask{};task.path="2,6";task.from_stage=8;task.to_stage=7;
        rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidStageRange");
        task.from_stage=0;task.to_stage=58;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidStageRange");
        task.from_stage=UINT64_MAX;task.to_stage=UINT64_MAX;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidStageRange");
        task.from_stage=0;task.to_stage=0;task.mode="invalid";rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidEvaluationMode");
        task.mode="exact";task.max_bits=0;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidBitLimit");
        task.max_bits=65;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidBitLimit");
        task.max_bits=64;task.mode="modular";rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidModulus");
        task.modulus=0;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"InvalidModulus");
        task.mode="exact";task.modulus=128;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"ModulusRequiresModularMode");
        task.modulus.reset();task.cache_bytes=max_grid_cache_bytes+1;rejected([&]{(void)evaluate_grid_stage_sweep(task);},"GridCacheByteLimit");
        task.cache_bytes=0;task.to_stage=7;
        const auto bypass=evaluate_grid_stage_sweep(task);
        require(bypass.cache.bypasses==8 && bypass.cache.entries==0 && bypass.records[0].cache_event=="MISS_BYPASS","Zero cache budget wrong");

        // Catches digest-only replay and omission of any canonical diagnostic field.
        for(const auto& [before,after]:std::vector<std::pair<std::string,std::string>>{
                {"\"stage\":1,","\"stage\":2,"},{"\"value\":\"100\"","\"value\":\"101\""},
                {"MISS_INSERT","HIT"},{"\"misses\":8","\"misses\":9"},
                {"PATH_VALIDATED depth=1","PATH_VALIDATED depth=2"},
                {"\"cache_key_digest\":\""+debug.records[0].key_digest,"\"cache_key_digest\":\""+std::string(64,'0')}}) {
            rejected([&]{(void)replay_grid_stage_sweep(mutate(verbose,before,after));},"ReplayMismatch");
        }
        rejected([&]{(void)replay_grid_stage_sweep(mutate(partial_bundle,"\"stopped_stage\":57","\"stopped_stage\":56"));},"ReplayMismatch");
        rejected([&]{(void)replay_grid_stage_sweep(mutate(compact,"debug 0","debug 2"));},"InvalidBundleField");
        std::cout<<"grid stage sweep contracts passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
