#include "bruce/mods/language/language_run.hpp"
#include "bruce/evidence/bundle.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <tuple>
#include <unistd.h>

namespace {
void require(bool condition,const char* message) {
    if (!condition) throw bruce::Error(message);
}
bool rejects(const std::function<void()>& action,std::string_view message={}) {
    try { action(); } catch (const bruce::Error& error) {
        return message.empty() || std::string_view(error.what()).find(message)!=std::string_view::npos;
    }
    return false;
}
bruce::LanguageInputV1 input(std::string alias,std::string bytes) {
    return {std::move(alias),bytes,bruce::sha256(bytes)};
}
bruce::LanguageInputV2 input_v2(std::string alias,std::string bytes) {
    return {std::move(alias),bytes,bruce::sha256(bytes)};
}
bruce::LanguageInputV3 input_v3(std::string alias,std::string bytes) {
    return {std::move(alias),bytes,bruce::sha256(bytes)};
}
bruce::LanguageDraftV1 draft(std::string phrase,std::string first_alias="train",std::string second_alias="holdout") {
    bruce::LanguageRequestV1 request;
    request.text=std::move(phrase);
    request.inputs={input(std::move(first_alias),"x,y\n0,1\n1,1/2\n"),
                    input(std::move(second_alias),"x,y\n2,1/3\n")};
    request.budget.candidates=3;
    request.budget.operations=200;
    request.budget.trace_events=24;
    request.budget.results=2;
    return bruce::interpret_language(std::move(request));
}
bruce::LanguageDraftV2 draft_v2(std::string phrase,std::string first_alias="train",std::string second_alias="holdout") {
    bruce::LanguageRequestV2 request;
    request.text=std::move(phrase);
    request.inputs={input_v2(std::move(first_alias),"x,y\n0,1\n1,1/2\n"),
                    input_v2(std::move(second_alias),"x,y\n2,1/3\n")};
    request.budget.candidates=3;
    request.budget.operations=200;
    request.budget.trace_events=24;
    request.budget.results=2;
    return bruce::interpret_language_v2(std::move(request));
}
bruce::LanguageDraftV3 draft_v3(std::string phrase,std::string first_alias="train",std::string second_alias="holdout") {
    bruce::LanguageRequestV3 request;
    request.text=std::move(phrase);
    request.inputs={input_v3(std::move(first_alias),"x,y\n0,1\n1,1/2\n"),
                    input_v3(std::move(second_alias),"x,y\n2,1/3\n")};
    request.budget.candidates=3;
    request.budget.operations=200;
    request.budget.trace_events=24;
    request.budget.results=2;
    return bruce::interpret_language_v3(std::move(request));
}
const bruce::ExactResearchRun& exact(const bruce::LanguageRunV1& run) {
    return std::get<bruce::ExactResearchRun>(run.session.run);
}
std::string corrupt_core_digest(std::string bytes,const std::string& digest) {
    const auto body_offset=std::string("BRUCE_LANGUAGE_RUN_V1\n").size()+65;
    const auto position=bytes.find(digest,body_offset);
    require(position!=std::string::npos,"core digest absent from run");
    bytes[position]=bytes[position]=='0' ? '1' : '0';
    bytes.replace(std::string("BRUCE_LANGUAGE_RUN_V1\n").size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
std::string corrupt_core_bundle(std::string bytes,const std::string& bundle) {
    const auto body_offset=std::string("BRUCE_LANGUAGE_RUN_V1\n").size()+65;
    const auto position=bytes.find(bundle,body_offset);
    require(position!=std::string::npos,"core bundle absent from run");
    bytes[position+bundle.size()-1]^=1;
    const auto digest=bruce::sha256(bundle);
    const auto digest_position=bytes.find(digest,position+bundle.size());
    require(digest_position!=std::string::npos,"core digest absent after bundle");
    bytes.replace(digest_position,digest.size(),bruce::sha256(std::string_view(bytes).substr(position,bundle.size())));
    bytes.replace(std::string("BRUCE_LANGUAGE_RUN_V1\n").size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
std::string corrupt_phase(std::string bytes) {
    const auto body_offset=std::string("BRUCE_LANGUAGE_RUN_V1\n").size()+65;
    const auto position=bytes.find("CORE_REPLAYED",body_offset);
    require(position!=std::string::npos,"outer replay phase absent from run");
    bytes[position]='X';
    bytes.replace(std::string("BRUCE_LANGUAGE_RUN_V1\n").size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
std::string swap_first_two_v3_run_fields(std::string bytes) {
    const auto header=std::string("BRUCE_LANGUAGE_RUN_V3\n");
    const auto body_offset=header.size()+65;
    const auto field_end=[&](std::size_t start) {
        const auto colon=bytes.find(':',start);
        require(colon!=std::string::npos,"V3 run field fixture missing colon");
        return colon+1+std::stoull(bytes.substr(start,colon-start));
    };
    const auto first_end=field_end(body_offset);
    const auto second_end=field_end(first_end);
    const auto first=bytes.substr(body_offset,first_end-body_offset);
    const auto second=bytes.substr(first_end,second_end-first_end);
    bytes.replace(body_offset,second_end-body_offset,second+first);
    bytes.replace(header.size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
std::string append_v3_run_field(std::string bytes,std::string_view field) {
    const auto header=std::string("BRUCE_LANGUAGE_RUN_V3\n");
    const auto body_offset=header.size()+65;
    bytes.append(field);
    bytes.replace(header.size(),64,bruce::sha256(std::string_view(bytes).substr(body_offset)));
    return bytes;
}
}

int main() {
    using namespace bruce;
    try {
        const auto understood=draft("Untersuche @train mit @holdout auf rationale Formeln.");
        const auto run=execute_language_draft(understood);
        require(run.session.phases.current()==ExecutionPhase::ready,"checked Core replay skipped");
        require(run.phases==std::vector<std::string>({"RECEIVED","INTERPRETED","CONFIRMED","CORE_EXECUTED","CORE_REPLAYED","EXPLAINED","COMPLETED"}),"outer phase sequence changed");
        require(exact(run).task.domain=="rational" && exact(run).task.strategy=="portfolio-rational" && exact(run).task.planner=="portfolio","wrong Core route");
        require(exact(run).task.max_power==4 && exact(run).task.max_degree==3 && exact(run).task.max_denominator==8,"CLI rational defaults changed");
        require(exact(run).task.budget.candidates==3 && exact(run).task.max_trace_bytes==understood.max_trace_bytes,"draft budget mapping changed");
        require(exact_text(exact(run).task.training[0].y)=="1" && exact_text(exact(run).task.holdout[0].y)=="1/3","Core sample roles changed");
        require(exact(run).status=="INCOMPLETE" && render_language_run(run).find("INCOMPLETE")!=std::string::npos,"bounded Core status was promoted");
        require(render_language_run(run).find(render_execution_session(run.session))!=std::string::npos,"Core evidence absent from language render");
        require(run.explanation.find("Sprache")!=std::string::npos && run.explanation.find("VERIFIED_ON_SAMPLES")==std::string::npos,"explanation is not descriptive");
        const auto packed=pack_language_run(run);
        const auto replayed=replay_language_run(packed);
        require(pack_language_run(replayed)==packed,"run replay changed canonical bytes");
        require(replayed.explanation==run.explanation && render_language_run(replayed)==render_language_run(run),"run replay changed rendered evidence");
        require(render_application(replayed.session.run)==render_application(run.session.run),"run replay changed Core result");
        const auto original_assessment=assess_application(run.session.run);
        const auto replayed_assessment=assess_application(replayed.session.run);
        require(original_assessment.claim_scope==replayed_assessment.claim_scope &&
                original_assessment.proof_established==replayed_assessment.proof_established,"language replay changed Core claim scope");
        const auto original_gates=evaluate_application_gates(run.session.bundle);
        const auto replayed_gates=evaluate_application_gates(replayed.session.bundle);
        require(original_gates.general_proof==replayed_gates.general_proof &&
                original_gates.learn_promotion==replayed_gates.learn_promotion &&
                original_gates.publish==replayed_gates.publish,"language replay changed Core gates");

        const auto ambiguous=draft("Untersuche @left und @right auf rationale Formeln.","left","right");
        require(rejects([&]{(void)execute_language_draft(ambiguous);},"UnselectedLanguageAlternative"),"unselected ambiguity executed");
        const auto unsupported=draft("Finde alles.");
        require(rejects([&]{(void)execute_language_draft(unsupported);},"UnsupportedLanguageDraft"),"unsupported draft executed");
        for(const auto& [selected,training_y,holdout_y]:{
            std::tuple{"train=left;holdout=right","1","1/3"},
            std::tuple{"train=right;holdout=left","1/3","1"}}) {
            const auto selected_run=execute_language_draft(select_language_alternative(ambiguous,selected));
            require(exact_text(exact(selected_run).task.training[0].y)==training_y &&
                    exact_text(exact(selected_run).task.holdout[0].y)==holdout_y,"selected role order not passed to Core");
        }
        auto forged=understood;
        forged.inputs[0].bytes.push_back('x');
        require(rejects([&]{(void)execute_language_draft(forged);},"LanguageInputDigestMismatch"),"changed input bytes executed");
        forged=understood; forged.grammar_version="future";
        require(rejects([&]{(void)execute_language_draft(forged);},"UnsupportedLanguageGrammar"),"changed grammar executed");
        forged=ambiguous; forged.selected_id="unknown";
        require(rejects([&]{(void)execute_language_draft(forged);},"UnknownLanguageAlternative"),"unknown selection executed");
        forged=understood; forged.inputs[1].bytes="wrong,header\n"; forged.inputs[1].digest=sha256(forged.inputs[1].bytes);
        require(rejects([&]{(void)execute_language_draft(forged);},"InvalidSampleHeader"),"bad CSV reached Core");

        auto tampered=packed; tampered.back()^=1;
        require(rejects([&]{(void)replay_language_run(tampered);},"LanguageRunDigestMismatch"),"tampered run accepted");
        require(rejects([&]{(void)replay_language_run(packed+"x");}),"trailing run bytes accepted");
        require(rejects([&]{(void)replay_language_run(corrupt_core_digest(packed,sha256(run.session.bundle)));},"LanguageCoreDigestMismatch"),"tampered Core digest accepted");
        require(rejects([&]{(void)replay_language_run(corrupt_core_bundle(packed,run.session.bundle));}),"tampered Core bundle accepted");
        require(rejects([&]{(void)replay_language_run(corrupt_phase(packed));},"InvalidLanguageRunPhases"),"incomplete outer phase trace accepted");

        const auto v2_understood=draft_v2("Analysiere @train anhand @holdout auf rationale Beziehungen.");
        const auto v2_run=execute_language_draft_v2(v2_understood);
        require(v2_run.draft.grammar_version==language_grammar_version_v2,"V2 run grammar changed");
        require(std::get<ExactResearchRun>(v2_run.session.run).task.strategy=="portfolio-rational",
                "V2 did not use existing Core route");
        require(render_language_run_v2(v2_run).find("language-run-v2")!=std::string::npos,
                "V2 render contract missing");
        const auto v2_packed=pack_language_run_v2(v2_run);
        require(v2_packed.starts_with("BRUCE_LANGUAGE_RUN_V2\n"),"V2 run header missing");
        const auto v2_replayed=replay_language_run_v2(v2_packed);
        require(pack_language_run_v2(v2_replayed)==v2_packed,"V2 run replay changed canonical bytes");
        const auto v2_ambiguous=draft_v2("Analysiere @left und @right auf rationale Formeln.","left","right");
        const auto v2_unsupported=draft_v2("Finde die beste Formel.");
        require(rejects([&]{(void)execute_language_draft_v2(v2_ambiguous);},"UnselectedLanguageAlternative"),
                "V2 ambiguity executed without selection");
        const auto v2_selected=select_language_alternative_v2(v2_ambiguous,"train=left;holdout=right");
        const auto v2_selected_run=execute_language_draft_v2(v2_selected);
        require(std::get<ExactResearchRun>(v2_selected_run.session.run).task.training.size()==2,
                "V2 selected training role not executed");
        const auto binding=bind_language_to_core_v1(v2_understood);
        const auto& bound_task=std::get<ExactResearchTask>(binding.task);
        require(binding.alternative_id=="rational-portfolio-v2" &&
                binding.train_alias=="train" && binding.holdout_alias=="holdout" &&
                binding.train_digest==sha256("x,y\n0,1\n1,1/2\n") &&
                binding.holdout_digest==sha256("x,y\n2,1/3\n"),
                "language Core binding metadata changed");
        const auto& native_task=std::get<ExactResearchRun>(v2_run.session.run).task;
        require(bound_task.domain==native_task.domain && bound_task.strategy==native_task.strategy &&
                bound_task.planner==native_task.planner && bound_task.training.size()==native_task.training.size() &&
                bound_task.holdout.size()==native_task.holdout.size() &&
                exact_text(bound_task.training[0].x)==exact_text(native_task.training[0].x) &&
                exact_text(bound_task.training[0].y)==exact_text(native_task.training[0].y) &&
                exact_text(bound_task.holdout[0].x)==exact_text(native_task.holdout[0].x) &&
                exact_text(bound_task.holdout[0].y)==exact_text(native_task.holdout[0].y) &&
                bound_task.max_bits==native_task.max_bits &&
                bound_task.max_denominator==native_task.max_denominator &&
                bound_task.max_trace_bytes==native_task.max_trace_bytes &&
                bound_task.budget.candidates==native_task.budget.candidates &&
                bound_task.budget.operations==native_task.budget.operations &&
                bound_task.budget.trace_events==native_task.budget.trace_events &&
                bound_task.budget.results==native_task.budget.results,
                "language Core binding changed task parity");
        const auto selected_binding=bind_language_to_core_v1(v2_selected);
        const auto& selected_task=std::get<ExactResearchTask>(selected_binding.task);
        require(selected_binding.train_alias=="left" && selected_binding.holdout_alias=="right" &&
                exact_text(selected_task.training[0].y)=="1" &&
                exact_text(selected_task.holdout[0].y)=="1/3",
                "selected language Core binding changed roles");
        require(rejects([&]{(void)bind_language_to_core_v1(v2_ambiguous);},
                        "UnselectedLanguageAlternative"),
                "language binding accepted unselected ambiguity");
        require(rejects([&]{(void)bind_language_to_core_v1(v2_unsupported);},
                        "UnsupportedLanguageDraft"),
                "language binding accepted unsupported draft");
        auto binding_unknown=v2_selected;
        binding_unknown.selected_id="train=unknown;holdout=right";
        require(rejects([&]{(void)bind_language_to_core_v1(binding_unknown);},
                        "UnknownLanguageAlternative"),
                "language binding accepted unknown selection");
        auto binding_bad_budget=v2_understood;
        binding_bad_budget.budget.candidates=0;
        require(rejects([&]{(void)bind_language_to_core_v1(binding_bad_budget);},
                        "InvalidExactBudget"),
                "native Core budget priority was not preserved");
        auto v2_forged=v2_packed;
        v2_forged.back()^=1;
        require(rejects([&]{(void)replay_language_run_v2(v2_forged);},"LanguageRunDigestMismatch"),
                "tampered V2 run accepted");

        const auto v3_draft=draft_v3("Prüfe @train gegen @holdout auf rationale Formeln.");
        const auto v3_run=execute_language_draft_v3(v3_draft);
        require(v3_run.draft.grammar_version==language_grammar_version_v3 &&
                v3_run.session.phases.current()==ExecutionPhase::ready &&
                std::get<ExactResearchRun>(v3_run.session.run).task.strategy=="portfolio-rational",
                "V3 run lost native Core evidence");
        require(render_language_run_v3(v3_run).find("language-run-v3")!=std::string::npos,
                "V3 run render contract missing");
        const auto v3_packed=pack_language_run_v3(v3_run);
        require(v3_packed.starts_with("BRUCE_LANGUAGE_RUN_V3\n"),"V3 run header missing");
        const auto v3_replayed=replay_language_run_v3(v3_packed);
        require(pack_language_run_v3(v3_replayed)==v3_packed &&
                render_language_run_v3(v3_replayed)==render_language_run_v3(v3_run),
                "V3 run replay changed canonical evidence");
        const auto v3_ambiguous=draft_v3("Vergleiche @left und @right auf rationale Formeln.","left","right");
        require(rejects([&]{(void)execute_language_draft_v3(v3_ambiguous);},"UnselectedLanguageAlternative"),
                "V3 unselected draft executed");
        const auto v3_selected=select_language_alternative_v3(v3_ambiguous,"train=right;holdout=left");
        const auto v3_selected_run=execute_language_draft_v3(v3_selected);
        require(exact_text(std::get<ExactResearchRun>(v3_selected_run.session.run).task.training[0].y)=="1/3",
                "V3 explicit selection lost Core role mapping");
        const auto v3_binding=bind_language_to_core_v3(v3_draft);
        const auto& v3_bound_task=std::get<ExactResearchTask>(v3_binding.task);
        const auto& v3_native_task=std::get<ExactResearchRun>(v3_run.session.run).task;
        require(v3_binding.alternative_id=="rational-portfolio-v2" &&
                v3_binding.train_alias=="train" && v3_binding.holdout_alias=="holdout" &&
                v3_binding.train_digest==sha256("x,y\n0,1\n1,1/2\n") &&
                v3_binding.holdout_digest==sha256("x,y\n2,1/3\n"),
                "V3 language Core binding metadata changed");
        require(v3_bound_task.domain==v3_native_task.domain &&
                v3_bound_task.strategy==v3_native_task.strategy &&
                v3_bound_task.planner==v3_native_task.planner &&
                v3_bound_task.training.size()==v3_native_task.training.size() &&
                v3_bound_task.holdout.size()==v3_native_task.holdout.size() &&
                exact_text(v3_bound_task.training[0].y)==exact_text(v3_native_task.training[0].y) &&
                exact_text(v3_bound_task.holdout[0].y)==exact_text(v3_native_task.holdout[0].y) &&
                v3_bound_task.budget.candidates==v3_native_task.budget.candidates &&
                v3_bound_task.max_bits==v3_native_task.max_bits,
                "V3 binding changed native Core task parity");
        const auto v3_selected_binding=bind_language_to_core_v3(v3_selected);
        const auto& v3_selected_task=std::get<ExactResearchTask>(v3_selected_binding.task);
        require(v3_selected_binding.train_alias=="right" && v3_selected_binding.holdout_alias=="left" &&
                exact_text(v3_selected_task.training[0].y)=="1/3" &&
                exact_text(v3_selected_task.holdout[0].y)=="1",
                "V3 binding changed selected role mapping");
        require(rejects([&]{(void)bind_language_to_core_v3(v3_ambiguous);},
                        "UnselectedLanguageAlternative"),
                "V3 binding accepted unselected ambiguity");
        const auto v3_unsupported=draft_v3("Finde die beste Formel.");
        require(rejects([&]{(void)bind_language_to_core_v3(v3_unsupported);},
                        "UnsupportedLanguageDraft"),
                "V3 binding accepted unsupported draft");
        auto v3_bad_budget=v3_draft;
        v3_bad_budget.budget.candidates=0;
        require(rejects([&]{(void)bind_language_to_core_v3(v3_bad_budget);},
                        "InvalidExactBudget"),
                "V3 native Core budget priority was not preserved");
        auto v3_tampered=v3_packed; v3_tampered.back()^=1;
        require(rejects([&]{(void)replay_language_run_v3(v3_tampered);},"LanguageRunDigestMismatch"),
                "V3 run digest tampering accepted");
        require(rejects([&]{(void)replay_language_run_v3(v3_packed+"x");}),
                "V3 run trailing bytes accepted");
        require(rejects([&]{(void)replay_language_run_v3(v2_packed);}),
                "V2 run accepted by V3 codec");
        require(rejects([&]{(void)replay_language_run_v2(v3_packed);}),
                "V3 run accepted by V2 codec");
        require(rejects([&]{(void)replay_language_run_v3(packed);}),
                "V1 run accepted by V3 codec");
        require(rejects([&]{(void)replay_language_run(v3_packed);}),
                "V3 run accepted by V1 codec");
        require(rejects([&]{(void)replay_language_run_v3(swap_first_two_v3_run_fields(v3_packed));}),
                "V3 rehashed run field-order mutation accepted");
        require(rejects([&]{(void)replay_language_run_v3(append_v3_run_field(v3_packed,"1:x"));},
                "NoncanonicalLanguageRun"),
                "V3 rehashed duplicate or unknown run field accepted");

        const NumberDomain domain("rational",4096);
        const std::string csv="x,y\n0,1\n1,1/2\n";
        const auto path=std::filesystem::temp_directory_path()/("bruce-language-samples-"+std::to_string(getpid())+".csv");
        { std::ofstream output(path,std::ios::binary); output<<csv; }
        const auto from_file=read_exact_samples(path.string(),domain);
        std::filesystem::remove(path);
        const auto from_bytes=parse_exact_samples(csv,domain);
        require(from_file.size()==from_bytes.size() && exact_text(from_file[1].y)==exact_text(from_bytes[1].y),"byte/file parser diverged");
        require(rejects([&]{(void)parse_exact_samples("bad\n0,1\n",domain);},"InvalidSampleHeader"),"bad header accepted");
        require(rejects([&]{(void)parse_exact_samples("x,y\n",domain);},"EmptySamples"),"empty CSV accepted");
        std::cout<<"Language run, replay, parser and role mapping contracts passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
