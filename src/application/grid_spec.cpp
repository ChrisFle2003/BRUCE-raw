#include "bruce/application/grid_spec.hpp"
#include "bruce/application/research.hpp"

namespace bruce {
GridSpec grid_spec(const GridTask& task) {
    if(task.rule!=GridSpec::leaf_local_power_rule && task.rule!=GridSpec::scaled_exponential_rule) throw Error("InvalidGridRule");
    if(task.stage && task.rule!=GridSpec::scaled_exponential_rule) throw Error("StageRequiresScaledExponentialRule");
    if(task.stage && *task.stage>57) throw Error("InvalidGridStage");
    if(task.mode!="exact" && task.mode!="modular" && task.mode!="metric") throw Error("InvalidEvaluationMode");
    if(task.zero_zero!="one" && task.zero_zero!="stop") throw Error("InvalidZeroZeroPolicy");
    if(task.max_bits<1 || task.max_bits>64) throw Error("InvalidBitLimit");
    if(task.mode=="modular") {
        if(!task.modulus || !*task.modulus) throw Error("InvalidModulus");
    } else if(task.modulus) throw Error("ModulusRequiresModularMode");
    return {task.mode,task.zero_zero,task.max_bits,task.modulus,task.rule,task.stage};
}
GridTask grid_task(const GridSpec& spec,std::string path) {
    return {std::move(path),spec.mode,spec.zero_zero,spec.max_bits,spec.modulus,spec.rule,spec.stage};
}
std::string canonical_grid_path(std::string_view text) {
    if(text.size()>max_file_bytes) throw Error("GridInputByteLimit");
    const auto path=parse_path(text);
    std::string out;
    for(const auto& step:path.steps) {
        if(!out.empty()) out+='/';
        out+=std::to_string(step.x)+","+std::to_string(step.y);
    }
    return out;
}
std::string render_grid_spec(const GridSpec& spec) {
    (void)grid_spec(grid_task(spec,""));
    return "{\"version\":"+json_string(spec.stage ? GridSpec::explicit_stage_version : GridSpec::version)+",\"engine\":"+json_string(GridSpec::engine)+
        ",\"address_rule\":"+json_string(GridSpec::address_rule)+",\"value_rule\":"+json_string(spec.rule)+
        ",\"policy\":"+json_string(GridSpec::policy)+",\"domain\":"+json_string(GridSpec::domain)+
        ",\"local_extent\":8,\"max_depth\":64,\"mode\":"+json_string(spec.mode)+
        ",\"max_bits\":"+std::to_string(spec.max_bits)+",\"modulus\":"+
        (spec.modulus ? json_string(std::to_string(*spec.modulus)) : "null")+
        ",\"zero_zero\":"+json_string(spec.zero_zero)+
        (spec.stage ? ",\"stage_source\":\"explicit\",\"stage\":"+std::to_string(*spec.stage) : "")+"}";
}
std::string grid_cache_key(const GridTask& task) {
    return "BRUCE_GRID_KEY_V1\n"+render_grid_spec(grid_spec(task))+"\npath "+canonical_grid_path(task.path)+"\n";
}
}
