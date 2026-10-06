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
