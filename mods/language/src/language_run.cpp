#include "bruce/mods/language/language_run.hpp"
#include "bruce/mods/language/mod.hpp"
#include "bruce/mod/mod_api.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/evidence/bundle.hpp"
#include <array>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_LANGUAGE_RUN_V1\n";
constexpr std::size_t max_run_bytes=128*1024*1024;
constexpr std::array<std::string_view,7> expected_phases={
    "RECEIVED","INTERPRETED","CONFIRMED","CORE_EXECUTED","CORE_REPLAYED","EXPLAINED","COMPLETED"};

struct Roles {
    const LanguageInputV1 *train,*holdout;
    std::string interpretation;
};
const LanguageInputV1& find_input(const LanguageDraftV1& draft,std::string_view alias) {
    for(const auto& input:draft.inputs) if(input.alias==alias) return input;
    throw Error("MissingLanguageAlias");
}
Roles resolve(const LanguageDraftV1& draft) {
    if(draft.grammar_version!=language_grammar_version_v1) throw Error("UnsupportedLanguageGrammar");
    if(draft.status==LanguageInterpretationStatus::unsupported) throw Error("UnsupportedLanguageDraft");
    if(draft.status==LanguageInterpretationStatus::understood) {
        if(!draft.selected_id.empty() || draft.alternatives.size()!=1 ||
           draft.alternatives.front().id!="rational-portfolio-v1") throw Error("InvalidLanguageSelection");
        return {&find_input(draft,"train"),&find_input(draft,"holdout"),"rational-portfolio-v1"};
    }
    if(draft.status!=LanguageInterpretationStatus::ambiguous) throw Error("UnsupportedLanguageDraft");
    if(draft.selected_id.empty()) throw Error("UnselectedLanguageAlternative");
    if(draft.selected_id=="train=left;holdout=right")
        return {&find_input(draft,"left"),&find_input(draft,"right"),draft.selected_id};
    if(draft.selected_id=="train=right;holdout=left")
        return {&find_input(draft,"right"),&find_input(draft,"left"),draft.selected_id};
    throw Error("UnknownLanguageAlternative");
}
ExactResearchTask core_task(const LanguageDraftV1& draft,const Roles& roles) {
    ExactResearchTask task;
    task.domain="rational";
    task.strategy="portfolio-rational";
    task.planner=std::string(research_strategy("portfolio-rational").default_planner);
    task.minimum="-8";
    task.maximum="8";
    task.max_power=4;
    task.max_degree=3;
    task.max_bits=draft.max_bits;
    task.max_denominator=draft.max_denominator;
    task.max_trace_bytes=draft.max_trace_bytes;
    task.budget=draft.budget;
    const NumberDomain domain(task.domain,task.max_bits);
    task.training=parse_exact_samples(roles.train->bytes,domain);
    task.holdout=parse_exact_samples(roles.holdout->bytes,domain);
    return task;
}
bool same_samples(const std::vector<ExactSample>& left,const std::vector<ExactSample>& right) {
    if(left.size()!=right.size()) return false;
    for(std::size_t i=0;i<left.size();++i)
        if(left[i].x!=right[i].x || left[i].y!=right[i].y) return false;
    return true;
}
void check_core_task(const ApplicationRun& native,const ExactResearchTask& expected) {
    const auto* run=std::get_if<ExactResearchRun>(&native);
    if(!run) throw Error("LanguageCoreTaskMismatch");
    const auto& task=run->task;
    if(task.domain!=expected.domain || task.strategy!=expected.strategy || task.planner!=expected.planner ||
       task.minimum!=expected.minimum || task.maximum!=expected.maximum ||
       task.max_power!=expected.max_power || task.max_degree!=expected.max_degree ||
       task.max_bits!=expected.max_bits || task.max_denominator!=expected.max_denominator ||
       task.max_trace_bytes!=expected.max_trace_bytes ||
       task.budget.candidates!=expected.budget.candidates || task.budget.operations!=expected.budget.operations ||
       task.budget.trace_events!=expected.budget.trace_events || task.budget.results!=expected.budget.results ||
       !same_samples(task.training,expected.training) || !same_samples(task.holdout,expected.holdout) ||
       task.cancel_after_candidates || task.cancel_after_checkpoints || !task.experience_snapshot.empty() ||
       task.max_nodes!=expected.max_nodes || task.expression_equivalence!=expected.expression_equivalence ||
       task.domain_cache!=expected.domain_cache || task.expression_planner!=expected.expression_planner ||
       task.expression_operators!=expected.expression_operators)
        throw Error("LanguageCoreTaskMismatch");
}
std::vector<std::string> complete_phases() {
    std::vector<std::string> result;
    for(const auto phase:expected_phases) result.emplace_back(phase);
    return result;
}
void check_phases(const std::vector<std::string>& phases) {
    if(phases!=complete_phases()) throw Error("InvalidLanguageRunPhases");
}
std::string explain(const LanguageDraftV1& draft,const Roles& roles) {
    return "Sprache: "+draft.grammar_version+". Interpretation: "+roles.interpretation+
        ". Core-Eingaben: train=@"+roles.train->alias+" (SHA-256 "+roles.train->digest+
        "), holdout=@"+roles.holdout->alias+" (SHA-256 "+roles.holdout->digest+
        "). Der verschachtelte Core-Befund steht im Feld core; seine Bewertung bleibt unverändert.";
}
ExecutionPhases ready_native_phases() {
    ExecutionPhases phases;
    phases.advance(ExecutionPhase::executing);
    phases.advance(ExecutionPhase::serializing);
    phases.advance(ExecutionPhase::replaying);
    phases.advance(ExecutionPhase::ready);
    return phases;
}
ExecutionSession replay_native(std::string bundle) {
    if(bundle.size()>max_file_bytes) throw Error("LanguageCoreByteLimit");
    ExecutionSession session;
    session.run=replay_application(bundle);
    session.bundle=std::move(bundle);
    session.phases=ready_native_phases();
    return session;
}
void validate_run(const LanguageRunV1& run,const Roles& roles) {
    check_phases(run.phases);
    if(run.explanation!=explain(run.draft,roles)) throw Error("LanguageExplanationMismatch");
    if(run.session.phases.history()!=ready_native_phases().history()) throw Error("InvalidLanguageCorePhases");
    if(run.session.bundle.size()>max_file_bytes) throw Error("LanguageCoreByteLimit");
    const auto native=replay_native(run.session.bundle);
    check_core_task(native.run,core_task(run.draft,roles));
    if(render_execution_session(native)!=render_execution_session(run.session)) throw Error("LanguageCoreEvidenceMismatch");
}
void put_field(std::string& out,std::string_view value) {
    out+=std::to_string(value.size()); out+=':'; out.append(value);
}
std::string_view read_field(std::string_view body,std::size_t& offset) {
    const auto start=offset;
    while(offset<body.size() && body[offset]>='0' && body[offset]<='9') ++offset;
    if(start==offset || offset>=body.size() || body[offset]!=':') throw Error("InvalidLanguageRunField");
    const auto digits=body.substr(start,offset-start);
    if(digits.size()>1 && digits.front()=='0') throw Error("NoncanonicalLanguageRun");
    const auto count=integer<std::size_t>(digits);
    ++offset;
    if(count>body.size()-offset) throw Error("InvalidLanguageRunField");
    const auto value=body.substr(offset,count);
    offset+=count;
    return value;
}
}

LanguageRunV1 execute_language_draft(const LanguageDraftV1& draft) {
    const auto roles=resolve(draft);
    const auto canonical=unpack_language_draft(pack_language_draft(draft));
    const auto task=core_task(canonical,roles);
    auto session=execute_mod_core_run({
        .manifest = language_mod_manifest_v1(),
        .task = ApplicationTask{task},
        .control = {},
        .verification_budget = {},
        .verification = {},
        .session_budget = {},
        .serialization_budget = {},
    });
    return {canonical,std::move(session),complete_phases(),explain(canonical,roles)};
}

std::string render_language_run(const LanguageRunV1& run) {
    const auto roles=resolve(run.draft);
    (void)unpack_language_draft(pack_language_draft(run.draft));
    validate_run(run,roles);
    std::string out="{\"contract\":\"language-run-v1\",\"grammar_version\":"+json_string(run.draft.grammar_version)+
        ",\"interpretation\":"+json_string(roles.interpretation)+
        ",\"inputs\":{\"train\":{\"alias\":"+json_string(roles.train->alias)+
        ",\"digest\":"+json_string(roles.train->digest)+"},\"holdout\":{\"alias\":"+
        json_string(roles.holdout->alias)+",\"digest\":"+json_string(roles.holdout->digest)+"}},\"phases\":[";
    bool first=true;
    for(const auto& phase:run.phases) { if(!first) out+=','; first=false; out+=json_string(phase); }
    return out+"],\"explanation\":"+json_string(run.explanation)+",\"core\":"+
        render_execution_session(run.session)+"}";
}

std::string pack_language_run(const LanguageRunV1& run) {
    const auto draft_bytes=pack_language_draft(run.draft);
    const auto roles=resolve(run.draft);
    validate_run(run,roles);
    std::string body;
    put_field(body,draft_bytes);
    put_field(body,roles.train->digest);
    put_field(body,roles.holdout->digest);
    put_field(body,run.session.bundle);
    put_field(body,sha256(run.session.bundle));
    put_field(body,std::to_string(run.phases.size()));
    for(const auto& phase:run.phases) put_field(body,phase);
    put_field(body,run.explanation);
    put_field(body,render_execution_session(run.session));
    if(body.size()>max_run_bytes) throw Error("LanguageRunByteLimit");
    return std::string(header)+sha256(body)+"\n"+body;
}

LanguageRunV1 replay_language_run(std::string_view bytes) {
    if(bytes.size()>max_run_bytes+header.size()+65) throw Error("LanguageRunByteLimit");
    if(!bytes.starts_with(header) || bytes.size()<header.size()+65 || bytes[header.size()+64]!='\n')
        throw Error("InvalidLanguageRunHeader");
    const auto body=bytes.substr(header.size()+65);
    if(bytes.substr(header.size(),64)!=sha256(body)) throw Error("LanguageRunDigestMismatch");
    std::size_t offset=0;
    const auto draft=unpack_language_draft(read_field(body,offset));
    const auto roles=resolve(draft);
    const auto train_digest=read_field(body,offset);
    const auto holdout_digest=read_field(body,offset);
    if(train_digest!=roles.train->digest || holdout_digest!=roles.holdout->digest)
        throw Error("LanguageInputDigestMismatch");
    const auto bundle=read_field(body,offset);
    const auto core_digest=read_field(body,offset);
    if(core_digest!=sha256(bundle)) throw Error("LanguageCoreDigestMismatch");
    if(read_field(body,offset)!="7") throw Error("InvalidLanguageRunPhases");
    std::vector<std::string> phases;
    for(std::size_t i=0;i<expected_phases.size();++i) phases.emplace_back(read_field(body,offset));
    check_phases(phases);
    const auto explanation=read_field(body,offset);
    const auto native_render=read_field(body,offset);
    if(offset!=body.size()) throw Error("NoncanonicalLanguageRun");
    auto session=replay_native(std::string(bundle));
    check_core_task(session.run,core_task(draft,roles));
    if(native_render!=render_execution_session(session)) throw Error("LanguageCoreEvidenceMismatch");
    if(explanation!=explain(draft,roles)) throw Error("LanguageExplanationMismatch");
    LanguageRunV1 run{draft,std::move(session),std::move(phases),std::string(explanation)};
    if(pack_language_run(run)!=bytes) throw Error("NoncanonicalLanguageRun");
    return run;
}
}
