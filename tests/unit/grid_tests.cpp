#include "bruce/application/grid.hpp"
#include "bruce/application/grid_spec.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

namespace {
void require(bool value,const char* message) {if(!value) throw bruce::Error(message);}
template<class F> void rejected(F call) {
    bool failed=false;try {call();} catch(const bruce::Error&) {failed=true;}
    require(failed,"Invalid grid evidence or task accepted");
}
template<class F> void rejected_as(F call,std::string_view message) {
    try {call();} catch(const bruce::Error& error) {
        require(error.what()==message,"Wrong grid error");return;
    }
    throw bruce::Error("Invalid grid task accepted");
}
void explicit_stage_contracts() {
    using namespace bruce;
    // These contracts catch accidental leaf-y fallback, lost evidence binding,
    // expanded numeric/address domains, and changes to historical V1 bytes.
    GridTask task{.path="2,6", .rule=GridSpec::scaled_exponential_rule};
    const auto legacy_json=std::string("{\"rule\":\"SCALED_EXPONENTIAL_V1\",\"path\":\"2,6\",\"depth\":1,\"theoretical_space\":\"64^1\",\"evaluations\":1,\"mode\":\"exact\",\"status\":\"EVALUATED\",\"stop_scope\":\"NONE\",\"stop_reason\":\"Completed\",\"stage\":6,\"base\":2,\"scale\":100,\"formula\":\"100*(2^stage-1)\",\"value\":\"6300\"}");
    const auto legacy_bundle=std::string("BRUCE_GRID_BUNDLE_V1\n5397e9f7e5d96aac64d205ad45982126b75abd37e1a1266f79b5fd56cf7210a3\n")+
        "engine bruce-grid-v1\nrule SCALED_EXPONENTIAL_V1\npolicy local-u64-single-leaf-v1\npath 2,6\nmode exact\nmax_bits 64\nmodulus none\nzero_zero one\nTRACE\nPATH_VALIDATED depth=1\nLEAF x=2 y=6\nCONFIG mode=exact max_bits=64 modulus=none zero_zero=one\nRULE rule=SCALED_EXPONENTIAL_V1 stage=6 base=2 scale=100 formula=100*(2^stage-1)\nEVALUATED evaluations=1\nRESULT\n"+legacy_json+"\n";
    require(render_grid(evaluate_grid(task))==legacy_json,"Legacy V1 rendering changed");
    require(pack_grid(evaluate_grid(task))==legacy_bundle,"Legacy V1 packing changed");
    require(pack_grid(replay_grid(legacy_bundle))==legacy_bundle,"Legacy V1 replay changed");
    require(render_grid_spec(grid_spec(task)).find("\"version\":\"grid-spec-v1\"")!=std::string::npos,"Legacy spec changed");
    const auto legacy_key=grid_cache_key(task);
    const std::uint64_t stages[]={0,7,57};
    const std::uint64_t values[]={0,12700,14411518807585587100ULL};
    for(unsigned i=0;i<3;++i) {
        task.stage=stages[i];
        const auto run=evaluate_grid(task);
        require(run.value==values[i] && !run.stopped,"Explicit stage used leaf y");
        require(grid_spec(task).stage==task.stage && grid_task(grid_spec(task),task.path).stage==task.stage,"Spec lost stage");
        const auto spec=render_grid_spec(grid_spec(task));
        require(spec.find("\"version\":\"grid-spec-v2\"")!=std::string::npos &&
            spec.find("\"stage_source\":\"explicit\",\"stage\":"+std::to_string(stages[i]))!=std::string::npos,"Explicit spec metadata missing");
        require(grid_cache_key(task)!=legacy_key,"Explicit stage cache identity lost");
        const auto json=render_grid(run);
        require(json.find("\"stage_source\":\"explicit\",\"stage\":"+std::to_string(stages[i]))!=std::string::npos,"Explicit JSON stage lost");
        require(run.trace[3]=="RULE rule=SCALED_EXPONENTIAL_V1 stage_source=explicit stage="+std::to_string(stages[i])+" base=2 scale=100 formula=100*(2^stage-1)","Explicit trace stage lost");
        const auto bundle=pack_grid(run);
        require(bundle.starts_with("BRUCE_GRID_BUNDLE_V2\n"),"Explicit bundle did not select V2");
        require(bundle.find("\nstage_source explicit\nstage "+std::to_string(stages[i])+"\n")!=std::string::npos,"Explicit payload stage lost");
        require(pack_grid(replay_grid(bundle))==bundle,"Explicit V2 replay differs");
    }
    task.stage=0;task.mode="metric";task.max_bits=1;
    require(evaluate_grid(task).value==0 && render_grid(evaluate_grid(task)).find("\"bit_length\":0")!=std::string::npos,"Explicit stage 0 metric differs");
    task.stage=7;
    require(evaluate_grid(task).value==12700 && render_grid(evaluate_grid(task)).find("\"bit_length\":14")!=std::string::npos,"Explicit stage 7 metric differs");
    task.stage=57;
    require(evaluate_grid(task).value==values[2] && render_grid(evaluate_grid(task)).find("\"bit_length\":64")!=std::string::npos,"Stage 57 metric limited by exact bits");
    task.mode="modular";task.modulus=128;
    task.stage=0;require(evaluate_grid(task).value==0,"Explicit stage 0 modular value differs");
    task.stage=7;require(evaluate_grid(task).value==28,"Explicit stage 7 modular value differs");
    task.stage=57;
    require(evaluate_grid(task).value==28,"Stage 57 modular value differs");
    task.modulus=0;rejected_as([&]{(void)evaluate_grid(task);},"InvalidModulus");
    task.modulus.reset();task.mode="exact";task.max_bits=63;
    rejected_as([&]{(void)evaluate_grid(task);},"IntegerBitLimit");
    task.max_bits=64;task.stage=58;
    rejected_as([&]{(void)evaluate_grid(task);},"InvalidGridStage");
    task.stage=7;task.rule=GridSpec::leaf_local_power_rule;
    rejected_as([&]{(void)evaluate_grid(task);},"StageRequiresScaledExponentialRule");
    task.rule=GridSpec::scaled_exponential_rule;task.stage=57;task.path="0,0";task.zero_zero="stop";task.max_bits=1;
    require(evaluate_grid(task).stopped && evaluate_grid(task).value==0,"Explicit stage bypassed address sentinel");
    require(replay_grid(pack_grid(evaluate_grid(task))).stopped,"Explicit stopped replay differs");
    task.path="2,6";task.stage=0;
    require(!evaluate_grid(task).stopped && evaluate_grid(task).value==0,"Explicit zero stage incorrectly stopped");
    task.max_bits=64;task.stage=7;
    const auto bundle=pack_grid(evaluate_grid(task));
    const auto body=bundle.substr(bundle.find('\n',bundle.find('\n')+1)+1);
    const auto rehashed=[&](std::string changed) {return "BRUCE_GRID_BUNDLE_V2\n"+sha256(changed)+"\n"+changed;};
    for(const auto replacement:{"stage 6", "stage 07", "stage +7", "stage 58", "stage seven", "stage 18446744073709551616"}) {
        auto changed=body;changed.replace(changed.find("stage 7"),7,replacement);
        rejected([&]{(void)replay_grid(rehashed(changed));});
    }
    for(const auto replacement:{"stage_source leaf-y", "stage_source EXPLICIT", "stage_source  explicit", "stage 7"}) {
        auto changed=body;changed.replace(changed.find("stage_source explicit"),21,replacement);
        rejected([&]{(void)replay_grid(rehashed(changed));});
    }
    for(const auto missing:{"stage_source explicit\n", "stage 7\n"}) {
        auto changed=body;changed.erase(changed.find(missing),std::string(missing).size());
        rejected([&]{(void)replay_grid(rehashed(changed));});
    }
    rejected([&]{(void)replay_grid("BRUCE_GRID_BUNDLE_V1\n"+sha256(body)+"\n"+body);});
    task.path="2,8";rejected([&]{(void)evaluate_grid(task);});
}
}
int main() {
    using namespace bruce;
    try {
        explicit_stage_contracts();
        GridTask t;t.path="2,6/4,3";
        const auto exact=evaluate_grid(t);
        require(exact.value==64 && exact.path.steps.size()==2 && !exact.stopped,"Wrong leaf evaluation");
        require(render_grid(exact)=="{\"rule\":\"LEAF_LOCAL_POWER_V1\",\"path\":\"2,6/4,3\",\"depth\":2,\"theoretical_space\":\"64^2\",\"evaluations\":1,\"mode\":\"exact\",\"status\":\"EVALUATED\",\"stop_scope\":\"NONE\",\"stop_reason\":\"Completed\",\"value\":\"64\"}","Default grid rendering changed");
        GridTask scaled{.path="2,3", .rule=GridSpec::scaled_exponential_rule};
        require(evaluate_grid(scaled).value==700,"Scaled exponential stage 3 differs");
        const std::uint64_t values[]={0,100,300,700,1500,3100,6300,12700};
        for(unsigned stage=0;stage<8;++stage) {
            scaled.path="2,"+std::to_string(stage);
            require(evaluate_grid(scaled).value==values[stage],"Scaled exponential stage differs");
        }
        scaled.path="2,3";
        const auto scaled_run=evaluate_grid(scaled);
        const auto scaled_json=render_grid(scaled_run);
        require(scaled_json.find("\"rule\":\"SCALED_EXPONENTIAL_V1\"")!=std::string::npos &&
            scaled_json.find("\"stage\":3")!=std::string::npos &&
            scaled_json.find("\"base\":2")!=std::string::npos &&
            scaled_json.find("\"scale\":100")!=std::string::npos &&
            scaled_json.find("\"formula\":\"100*(2^stage-1)\"")!=std::string::npos,"Scaled rule metadata missing");
        require(scaled_run.trace.size()==5 && scaled_run.trace[3]==
            "RULE rule=SCALED_EXPONENTIAL_V1 stage=3 base=2 scale=100 formula=100*(2^stage-1)","Scaled trace missing");
        require(grid_spec(scaled).rule==GridSpec::scaled_exponential_rule &&
            grid_task(grid_spec(scaled),scaled.path).rule==scaled.rule,"Grid spec lost selected rule");
        require(render_grid_spec(grid_spec(scaled)).find("\"value_rule\":\"SCALED_EXPONENTIAL_V1\"")!=std::string::npos,"Grid spec rendered default rule");
        auto legacy=scaled;legacy.rule=GridSpec::leaf_local_power_rule;
        require(grid_cache_key(scaled)!=grid_cache_key(legacy),"Grid cache key ignored rule");
        const auto scaled_bundle=pack_grid(scaled_run);
        require(render_grid(replay_grid(scaled_bundle))==scaled_json,"Scaled grid replay differs");
        const auto scaled_body_start=scaled_bundle.find('\n',scaled_bundle.find('\n')+1)+1;
        auto changed_body=scaled_bundle.substr(scaled_body_start);
        const auto rule_position=changed_body.find("rule SCALED_EXPONENTIAL_V1");
        require(rule_position!=std::string::npos,"Scaled bundle omitted rule");
        changed_body.replace(rule_position,std::string("rule SCALED_EXPONENTIAL_V1").size(),"rule LEAF_LOCAL_POWER_V1");
        rejected([&]{(void)replay_grid("BRUCE_GRID_BUNDLE_V1\n"+sha256(changed_body)+"\n"+changed_body);});
        scaled.mode="metric";scaled.max_bits=1;
        require(evaluate_grid(scaled).value==700 &&
            render_grid(evaluate_grid(scaled)).find("\"bit_length\":10")!=std::string::npos,"Scaled metric bit length differs");
        scaled.path="2,0";
        require(render_grid(evaluate_grid(scaled)).find("\"bit_length\":0")!=std::string::npos,"Zero scaled metric bit length differs");
        scaled.path="2,7";
        require(render_grid(evaluate_grid(scaled)).find("\"bit_length\":14")!=std::string::npos,"Stage 7 scaled metric bit length differs");
        scaled.mode="modular";scaled.modulus=17;scaled.path="2,3";
        require(evaluate_grid(scaled).value==3,"Scaled modular stage 3 differs");
        scaled.path="2,7";require(evaluate_grid(scaled).value==1,"Scaled modular stage 7 differs");
        scaled.path="2,0";require(evaluate_grid(scaled).value==0,"Scaled modular zero differs");
        scaled.path="2,3";scaled.modulus=1;
        require(evaluate_grid(scaled).value==0,"Scaled modulus one differs");
        scaled.modulus=UINT64_MAX;
        require(evaluate_grid(scaled).value==700,"Scaled maximum modulus differs");
        scaled.mode="exact";scaled.modulus.reset();scaled.max_bits=9;
        rejected_as([&]{(void)evaluate_grid(scaled);},"IntegerBitLimit");
        scaled.max_bits=10;require(evaluate_grid(scaled).value==700,"Scaled exact bit boundary rejected");
        scaled.path="0,0";scaled.zero_zero="stop";
        require(evaluate_grid(scaled).stopped,"Scaled sentinel did not stop");
        require(replay_grid(pack_grid(evaluate_grid(scaled))).stopped,"Scaled stopped replay differs");
        scaled.path="2,0";
        require(!evaluate_grid(scaled).stopped && evaluate_grid(scaled).value==0,"Scaled zero stage incorrectly stopped");
        scaled.rule="UNKNOWN_RULE";
        rejected_as([&]{(void)evaluate_grid(scaled);},"InvalidGridRule");
        rejected_as([&]{(void)render_grid_spec(grid_spec(scaled));},"InvalidGridRule");
        const auto bundle=pack_grid(exact);
        require(render_grid(replay_grid(bundle))==render_grid(exact),"Grid replay differs");
        for(std::size_t n=0;n<bundle.size();++n) rejected([&]{(void)replay_grid(std::string_view(bundle).substr(0,n));});
        const auto digest_end=bundle.find('\n',bundle.find('\n')+1);
        const auto body=bundle.substr(digest_end+1);
        for(std::size_t n=0;n<body.size();++n) {
            const auto shortened=body.substr(0,n);
            rejected([&]{(void)replay_grid("BRUCE_GRID_BUNDLE_V1\n"+sha256(shortened)+"\n"+shortened);});
        }
        t.mode="modular";rejected([&]{(void)evaluate_grid(t);});
        t.modulus=0;rejected([&]{(void)evaluate_grid(t);});
        t.modulus=UINT64_MAX;require(evaluate_grid(t).value==64,"Maximum modulus corrupted");
        t.mode="exact";rejected([&]{(void)evaluate_grid(t);});
        t.modulus.reset();t.max_bits=6;rejected([&]{(void)evaluate_grid(t);});
        t.mode="metric";require(evaluate_grid(t).value==64,"Metric inherited exact bit cutoff");
        t.max_bits=65;rejected([&]{(void)evaluate_grid(t);});
        t.max_bits=64;t.path="0,0";t.zero_zero="stop";
        const auto stopped=evaluate_grid(t);
        require(stopped.stopped && stopped.trace.size()==4,"Sentinel did not stop");
        require(render_grid(replay_grid(pack_grid(stopped)))==render_grid(stopped),"Stopped replay differs");
        t.path="0,0/2,6";require(!evaluate_grid(t).stopped,"Ancestor sentinel stopped leaf-local query");
        t.path=std::string(max_file_bytes+1,'0');rejected([&]{(void)evaluate_grid(t);});
        rejected([&]{(void)replay_grid(std::string(max_file_bytes+1,'0'));});
        std::cout<<"Grid application, bounded input, all truncations and canonical replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
