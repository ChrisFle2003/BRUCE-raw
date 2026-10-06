#include "bruce/application/tokenized_task_run.hpp"
#include "bruce/coordinate/address.hpp"
#include "bruce/evidence/bundle.hpp"
#include <functional>
#include <iostream>

namespace {
void require(bool ok,const char* message) { if(!ok) throw bruce::Error(message); }
bool rejects(const std::function<void()>& action,std::string_view type={}) {
    try { action(); } catch(const bruce::Error& error) {
        return std::string_view(error.what()).find(type)!=std::string_view::npos;
    }
    return false;
}
bruce::TokenizedTaskSpecV1 fixture(bool collision=false) {
    bruce::TokenizedTaskSpecV1 spec;
    spec.table.version="v1";
    spec.table.entries={{"","Train","",0},{"","Holdout","",1},{"","Rational Portfolio","",2}};
    const std::string train="x,y\n0,1\n1,1/2\n";
    const std::string holdout=collision ? "x,y\n2,1\n" : "x,y\n2,1/3\n";
    spec.assets={{"train",train,bruce::sha256(train)},{"holdout",holdout,bruce::sha256(holdout)}};
    spec.route_id="rational-portfolio-v1";
    spec.domain="rational";
    spec.budget.candidates=3; spec.budget.operations=200;
    spec.budget.trace_events=24; spec.budget.results=2;
    spec=bruce::make_tokenized_task_spec(std::move(spec));
    spec.ordered_token_ids={spec.table.entries[0].id,spec.table.entries[1].id,spec.table.entries[2].id};
    spec.roles={{"train",spec.table.entries[0].id,"train"},{"holdout",spec.table.entries[1].id,"holdout"},
                {"route",spec.table.entries[2].id,""}};
    return bruce::make_tokenized_task_spec(std::move(spec));
}
// Rebuild only the outer envelope: hostile inputs must still pass inner validation.
std::string envelope(std::string body) {
    return "BRUCE_TOKENIZED_TASK_RUN_V1\n"+bruce::sha256(body)+"\n"+body;
}
std::vector<std::string> fields(std::string_view bytes) {
    const auto start=bytes.find('\n')+66;
    std::vector<std::string> result;
    for(std::size_t pos=start;pos<bytes.size();) {
        const auto colon=bytes.find(':',pos);
        const auto size=std::stoull(std::string(bytes.substr(pos,colon-pos)));
        result.emplace_back(bytes.substr(colon+1,size)); pos=colon+1+size;
    }
    return result;
}
std::string packed_fields(const std::vector<std::string>& values) {
    std::string body;
    for(const auto& value:values) body+=std::to_string(value.size())+":"+value;
    return envelope(std::move(body));
}
void outer_replay_preserves_native_status_and_gates() {
    const auto run=bruce::execute_tokenized_task_spec(fixture());
    const auto bytes=bruce::pack_tokenized_task_run(run);
    const auto replayed=bruce::replay_tokenized_task_run(bytes);
    const auto rendered=bruce::render_tokenized_task_run(replayed);
    require(bruce::pack_tokenized_task_run(replayed)==bytes,"canonical outer replay changed bytes");
    require(rendered==bruce::render_tokenized_task_run(run),"outer replay changed rendered evidence");
    require(rendered.find("\"contract\":\"tokenized-task-run-v1\"")!=std::string::npos,"outer contract absent");
    for(const auto* field:{"\"status\":\"INCOMPLETE\"","\"general_proof\":false","\"learn_gate\":false","\"publish_gate\":false"})
        require(rendered.find(field)!=std::string::npos,"native incomplete/gate field lost");
    require(rendered.find(bruce::render_execution_session(run.session))!=std::string::npos,"native session evidence lost");
    require(run.spec_digest==bruce::sha256(run.spec_bytes) && run.native_bundle_digest==bruce::sha256(run.session.bundle),"digest binding lost");
    require(std::variant_size_v<bruce::ApplicationTask> == 8,"application variants changed");
}
void outer_round_trip_preserves_refuted_and_verified_samples() {
    for(const auto* expected:{"REFUTED","VERIFIED_ON_SAMPLES"}) {
        auto spec=fixture();
        const std::string train="x,y\n0,0\n1,1\n2,4\n3,9\n";
        const std::string holdout=std::string_view(expected)=="REFUTED" ? "x,y\n4,17\n" : "x,y\n4,16\n";
        spec.assets={{"holdout",holdout,bruce::sha256(holdout)},{"train",train,bruce::sha256(train)}};
        spec.minimum="-2"; spec.maximum="2"; spec.max_denominator=1;
        spec.max_power=2; spec.max_degree=2; spec.budget=bruce::Budget{};
        const auto run=bruce::execute_tokenized_task_spec(spec);
        const auto& native=std::get<bruce::ExactResearchRun>(run.session.run);
        require(native.status==expected && native.stop_reason=="Completed","unexpected direct native status");
        const auto bytes=bruce::pack_tokenized_task_run(run);
        const auto replayed=bruce::replay_tokenized_task_run(bytes);
        require(std::get<bruce::ExactResearchRun>(replayed.session.run).status==expected,"outer replay changed native status");
        require(bruce::pack_tokenized_task_run(replayed)==bytes,"completed-status bundle changed on replay");
        const auto rendered=bruce::render_tokenized_task_run(replayed);
        require(rendered==bruce::render_tokenized_task_run(run),"completed-status render changed on replay");
        for(const auto* field:{"\"general_proof\":false","\"learn_gate\":false","\"publish_gate\":false"})
            require(rendered.find(field)!=std::string::npos,"completed status granted authority");
        for(const auto* field:{"\"general_proof\":true","\"learn_gate\":true","\"publish_gate\":true"})
            require(rendered.find(field)==std::string::npos,"completed status promoted a gate");
    }
}
void tampered_spec_digest_is_rejected() {
    const auto run=bruce::execute_tokenized_task_spec(fixture());
    auto values=fields(bruce::pack_tokenized_task_run(run));
    values[1][0]=values[1][0]=='0' ? '1' : '0';
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"TokenizedTaskSpecDigestMismatch"),"tampered_spec_digest_is_rejected");
}
void tampered_inner_bundle_is_rejected() {
    const auto run=bruce::execute_tokenized_task_spec(fixture());
    auto values=fields(bruce::pack_tokenized_task_run(run));
    values[2].back()^=1;
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"TokenizedTaskNativeDigestMismatch"),"native digest ignored");
    values[3]=bruce::sha256(values[2]);
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));}),"tampered_inner_bundle_is_rejected");
}
void compiled_native_task_mismatch_is_rejected() {
    const auto run=bruce::execute_tokenized_task_spec(fixture());
    auto other_spec=fixture(); other_spec.budget.candidates=4;
    const auto other=bruce::execute_tokenized_task_spec(other_spec);
    auto values=fields(bruce::pack_tokenized_task_run(run));
    values[2]=other.session.bundle; values[3]=other.native_bundle_digest;
    values.back()=bruce::render_execution_session(other.session);
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"TokenizedTaskCompiledTaskMismatch"),"compiled_native_task_mismatch_is_rejected");
    auto forged=run; forged.compiled_task.max_nodes++;
    require(rejects([&]{(void)bruce::pack_tokenized_task_run(forged);},"TokenizedTaskCompiledTaskMismatch"),"in-memory compiled task ignored");
}
void value_collision_does_not_merge_asset_or_address_identity() {
    const auto run=bruce::replay_tokenized_task_run(bruce::pack_tokenized_task_run(bruce::execute_tokenized_task_spec(fixture(true))));
    const auto spec=bruce::unpack_tokenized_task_spec(run.spec_bytes);
    require(spec.assets[0].digest!=spec.assets[1].digest,"distinct sample inputs lost evidence identity");
    require(spec.assets[0].alias!=spec.assets[1].alias,"equal values merged asset identity");
    require(spec.table.entries[0].normalized_text!=spec.table.entries[1].normalized_text &&
            spec.table.entries[0].id!=spec.table.entries[1].id,"canonical token identity changed");
    require(spec.ordered_token_ids.size()==3 && spec.roles[0].token_id!=spec.roles[2].token_id,"role identities collapsed");
    require(bruce::exact_text(run.compiled_task.training[0].y)=="1" && bruce::exact_text(run.compiled_task.holdout[0].y)=="1","math values changed");
    const auto first=bruce::pair(0,0),second=bruce::pair(1,0);
    require(first!=second,"address fixture broken");
    require(spec.table.entries[0].id!=std::to_string(second),"numeric address substituted for token identity");
}
void noncanonical_phases_explanation_and_session_are_rejected() {
    const auto run=bruce::execute_tokenized_task_spec(fixture());
    const auto bytes=bruce::pack_tokenized_task_run(run);
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(bytes+"x");}),"trailing bytes accepted");
    auto values=fields(bytes);
    values[5]="FORGED";
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"InvalidTokenizedTaskRunPhases"),"phase tamper accepted");
    values=fields(bytes); values[values.size()-2]="Proof granted";
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"TokenizedTaskExplanationMismatch"),"explanation tamper accepted");
    values=fields(bytes); values.back()="{}";
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(packed_fields(values));},"TokenizedTaskNativeEvidenceMismatch"),"session tamper accepted");
    const auto body_start=bytes.find('\n')+66;
    require(rejects([&]{(void)bruce::replay_tokenized_task_run(envelope("0"+bytes.substr(body_start)));}),"noncanonical length accepted");
}
}
int main() {
    try {
        outer_replay_preserves_native_status_and_gates();
        outer_round_trip_preserves_refuted_and_verified_samples();
        tampered_spec_digest_is_rejected();
        tampered_inner_bundle_is_rejected();
        compiled_native_task_mismatch_is_rejected();
        value_collision_does_not_merge_asset_or_address_identity();
        noncanonical_phases_explanation_and_session_are_rejected();
        std::cout<<"Tokenized task run contracts passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
