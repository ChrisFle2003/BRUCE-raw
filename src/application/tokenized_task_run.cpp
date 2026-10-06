#include "bruce/application/tokenized_task_run.hpp"
#include "bruce/evidence/bundle.hpp"
#include <array>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_TOKENIZED_TASK_RUN_V1\n";
constexpr std::size_t max_run_bytes=256*1024*1024;
constexpr std::array<std::string_view,7> expected_phases={
    "RECEIVED","SPEC_VALIDATED","COMPILED","CORE_EXECUTED","CORE_REPLAYED","EXPLAINED","COMPLETED"};

std::vector<std::string> complete_phases() {
    return {expected_phases.begin(),expected_phases.end()};
}
void check_phases(const std::vector<std::string>& phases) {
    if(phases!=complete_phases()) throw Error("InvalidTokenizedTaskRunPhases");
}
bool same_samples(const std::vector<ExactSample>& left,const std::vector<ExactSample>& right) {
    if(left.size()!=right.size()) return false;
    for(std::size_t i=0;i<left.size();++i)
        if(left[i].x!=right[i].x || left[i].y!=right[i].y) return false;
    return true;
}
void check_task(const ExactResearchTask& task,const ExactResearchTask& expected) {
    if(task.domain!=expected.domain || task.strategy!=expected.strategy || task.planner!=expected.planner ||
       task.minimum!=expected.minimum || task.maximum!=expected.maximum ||
       task.max_power!=expected.max_power || task.max_degree!=expected.max_degree ||
       task.max_bits!=expected.max_bits || task.max_denominator!=expected.max_denominator ||
       task.max_trace_bytes!=expected.max_trace_bytes ||
       task.budget.candidates!=expected.budget.candidates || task.budget.operations!=expected.budget.operations ||
       task.budget.trace_events!=expected.budget.trace_events || task.budget.results!=expected.budget.results ||
       !same_samples(task.training,expected.training) || !same_samples(task.holdout,expected.holdout) ||
       task.cancel_after_candidates!=expected.cancel_after_candidates ||
       task.cancel_after_checkpoints!=expected.cancel_after_checkpoints ||
       task.experience_snapshot!=expected.experience_snapshot || task.max_nodes!=expected.max_nodes ||
       task.expression_equivalence!=expected.expression_equivalence || task.domain_cache!=expected.domain_cache ||
       task.expression_planner!=expected.expression_planner || task.expression_operators!=expected.expression_operators)
        throw Error("TokenizedTaskCompiledTaskMismatch");
}
void check_native_task(const ApplicationRun& native,const ExactResearchTask& expected) {
    const auto* run=std::get_if<ExactResearchRun>(&native);
    if(!run) throw Error("TokenizedTaskCompiledTaskMismatch");
    check_task(run->task,expected);
}
ExecutionPhases ready_phases() {
    ExecutionPhases phases;
    for(const auto phase:{ExecutionPhase::executing,ExecutionPhase::serializing,ExecutionPhase::replaying,ExecutionPhase::ready})
        phases.advance(phase);
    return phases;
}
ExecutionSession replay_native(std::string bundle) {
    if(bundle.size()>max_file_bytes) throw Error("TokenizedTaskNativeByteLimit");
    ExecutionSession session;
    session.run=replay_application(bundle);
    session.bundle=std::move(bundle);
    session.phases=ready_phases();
    session.serialization_usage.serialized_bytes=session.bundle.size();
    return session;
}
std::string explain(const TokenizedTaskSpecV1& spec,std::string_view digest) {
    return "TaskSpec: "+spec.task_spec_version+" (SHA-256 "+std::string(digest)+
        "). Token table: "+spec.table.version+". Route: "+spec.route_id+
        ". Explicit token and asset role bindings compile to the native rational portfolio task. "
        "The nested core field retains native status, evidence scope and gates unchanged.";
}
TokenizedTaskSpecV1 validate_run(const TokenizedTaskRunV1& run) {
    if(run.spec_digest!=sha256(run.spec_bytes)) throw Error("TokenizedTaskSpecDigestMismatch");
    const auto spec=unpack_tokenized_task_spec(run.spec_bytes);
    const auto expected=compile_tokenized_task_spec(spec);
    check_task(run.compiled_task,expected);
    if(run.native_bundle_digest!=sha256(run.session.bundle)) throw Error("TokenizedTaskNativeDigestMismatch");
    check_phases(run.phases);
    if(run.explanation!=explain(spec,run.spec_digest)) throw Error("TokenizedTaskExplanationMismatch");
    if(run.session.phases.history()!=ready_phases().history()) throw Error("InvalidTokenizedTaskNativePhases");
    const auto native=replay_native(run.session.bundle);
    check_native_task(native.run,expected);
    check_native_task(run.session.run,expected);
    if(render_execution_session(native)!=render_execution_session(run.session)) throw Error("TokenizedTaskNativeEvidenceMismatch");
    return spec;
}
void put(std::string& out,std::string_view value) {
    out+=std::to_string(value.size()); out+=':'; out.append(value);
}
std::string_view take(std::string_view body,std::size_t& pos) {
    const auto start=pos;
    while(pos<body.size() && body[pos]>='0' && body[pos]<='9') ++pos;
    if(start==pos || pos==body.size() || body[pos]!=':' || (pos-start>1 && body[start]=='0'))
        throw Error("InvalidTokenizedTaskRunField");
    const auto size=integer<std::size_t>(body.substr(start,pos-start));
    ++pos;
    if(size>body.size()-pos) throw Error("InvalidTokenizedTaskRunField");
    const auto value=body.substr(pos,size); pos+=size; return value;
}
}

TokenizedTaskRunV1 execute_tokenized_task_spec(const TokenizedTaskSpecV1& spec) {
    auto bytes=pack_tokenized_task_spec(spec);
    const auto canonical=unpack_tokenized_task_spec(bytes);
    auto task=compile_tokenized_task_spec(canonical);
    auto session=execute_checked_application(task);
    const auto digest=sha256(bytes),native_digest=sha256(session.bundle);
    return {std::move(bytes),digest,std::move(task),std::move(session),native_digest,
            complete_phases(),explain(canonical,digest)};
}

std::string render_tokenized_task_run(const TokenizedTaskRunV1& run) {
    const auto spec=validate_run(run);
    std::string out="{\"contract\":\"tokenized-task-run-v1\",\"spec_digest\":"+json_string(run.spec_digest)+
        ",\"native_bundle_digest\":"+json_string(run.native_bundle_digest)+
        ",\"task_spec_version\":"+json_string(spec.task_spec_version)+
        ",\"token_table_version\":"+json_string(spec.table.version)+
        ",\"table_digest\":"+json_string(spec.table_digest)+
        ",\"route\":"+json_string(spec.route_id)+",\"selected_id\":"+
        (spec.selected_id.empty() ? "null" : json_string(spec.selected_id))+",\"phases\":[";
    bool first=true;
    for(const auto& phase:run.phases) { if(!first) out+=','; first=false; out+=json_string(phase); }
    return out+"],\"explanation\":"+json_string(run.explanation)+",\"core\":"+render_execution_session(run.session)+"}";
}

std::string pack_tokenized_task_run(const TokenizedTaskRunV1& run) {
    (void)validate_run(run);
    std::string body;
    // The compiled task is carried in the native bundle, checked field-by-field
    // against recompilation; no second task codec or authority surface is added.
    put(body,run.spec_bytes); put(body,run.spec_digest);
    put(body,run.session.bundle); put(body,run.native_bundle_digest);
    put(body,std::to_string(run.phases.size()));
    for(const auto& phase:run.phases) put(body,phase);
    put(body,run.explanation); put(body,render_execution_session(run.session));
    if(body.size()>max_run_bytes) throw Error("TokenizedTaskRunByteLimit");
    return std::string(header)+sha256(body)+"\n"+body;
}

TokenizedTaskRunV1 replay_tokenized_task_run(std::string_view bytes) {
    if(bytes.size()>max_run_bytes+header.size()+65) throw Error("TokenizedTaskRunByteLimit");
    if(!bytes.starts_with(header) || bytes.size()<header.size()+65 || bytes[header.size()+64]!='\n')
        throw Error("InvalidTokenizedTaskRunHeader");
    const auto body=bytes.substr(header.size()+65);
    if(bytes.substr(header.size(),64)!=sha256(body)) throw Error("TokenizedTaskRunDigestMismatch");
    std::size_t pos=0;
    const auto spec_bytes=take(body,pos),spec_digest=take(body,pos);
    if(spec_digest!=sha256(spec_bytes)) throw Error("TokenizedTaskSpecDigestMismatch");
    const auto spec=unpack_tokenized_task_spec(spec_bytes);
    auto task=compile_tokenized_task_spec(spec);
    const auto bundle=take(body,pos),native_digest=take(body,pos);
    if(native_digest!=sha256(bundle)) throw Error("TokenizedTaskNativeDigestMismatch");
    if(take(body,pos)!=std::to_string(expected_phases.size())) throw Error("InvalidTokenizedTaskRunPhases");
    std::vector<std::string> phases;
    for(std::size_t i=0;i<expected_phases.size();++i) phases.emplace_back(take(body,pos));
    check_phases(phases);
    const auto explanation=take(body,pos),native_render=take(body,pos);
    if(pos!=body.size()) throw Error("NoncanonicalTokenizedTaskRun");
    if(explanation!=explain(spec,spec_digest)) throw Error("TokenizedTaskExplanationMismatch");
    auto session=replay_native(std::string(bundle));
    check_native_task(session.run,task);
    if(native_render!=render_execution_session(session)) throw Error("TokenizedTaskNativeEvidenceMismatch");
    TokenizedTaskRunV1 run{std::string(spec_bytes),std::string(spec_digest),std::move(task),std::move(session),
        std::string(native_digest),std::move(phases),std::string(explanation)};
    if(pack_tokenized_task_run(run)!=bytes) throw Error("NoncanonicalTokenizedTaskRun");
    return run;
}
}
