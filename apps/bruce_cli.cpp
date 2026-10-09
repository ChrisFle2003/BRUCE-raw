#include "bruce/application/transfer_control.hpp"
#include "bruce/io/local_files.hpp"
#include "bruce/application/compatibility_transfer.hpp"
#include "bruce/application/experience_transfer.hpp"
#include "rule_store.hpp"
#include "runtime_mod_cli.hpp"
#include "rule_review.hpp"
#include "bruce/application/research.hpp"
#include "bruce/application/experience_task.hpp"
#include "bruce/application/application_task.hpp"
#if BRUCE_BUILD_APPLICATION_DISPATCH
#include "bruce/application/application_dispatch.hpp"
#endif
#if BRUCE_BUILD_PHASE_DAG
#include "bruce/application/phase_dag_replay.hpp"
#include "bruce/application/phase_dag_session.hpp"
#endif
#include "bruce/application/execution_session.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/application/research_gates.hpp"
#include "bruce/application/fraction_compare.hpp"
#include "bruce/application/compatibility.hpp"
#include "bruce/application/exact_research.hpp"
#include "bruce/coordinate/address.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/evidence/chain.hpp"
#include "bruce/math/exact.hpp"
#include "bruce/experience/store.hpp"
#include "bruce/application/grid.hpp"
#include "bruce/application/grid_batch.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/application/grid_research.hpp"
#if BRUCE_BUILD_MOD_LANGUAGE
#include "bruce/mod/mod_api.hpp"
#include "bruce/mods/language/mod.hpp"
#include "bruce/mods/language/language_run.hpp"
#endif
#if BRUCE_BUILD_MOD_DIALOGUE
#include "bruce/mods/dialogue/dialogue.hpp"
#endif
#if BRUCE_BUILD_MOD_NANOCELL
#include "nanocell_cli.hpp"
#endif
#if BRUCE_BUILD_MOD_COCKPIT
#include "bruce/mods/cockpit/mod.hpp"
#endif
#include "bruce/token/task_spec.hpp"
#include "bruce/application/tokenized_task_run.hpp"
#include "management_inspection_cli.hpp"
#include <csignal>
#include "bruce/version.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

namespace {
volatile std::sig_atomic_t interrupted=0;
void on_interrupt(int) { interrupted=1; }
using Options=std::map<std::string,std::string>;
Options options(int argc,char** argv,int start,const std::set<std::string>& allowed) {
    Options result;
    for(int i=start;i<argc;i+=2) {
        const std::string key=argv[i];
        if(!allowed.contains(key)) throw bruce::Error("UnknownOption: "+key);
        if(i+1>=argc) throw bruce::Error("MissingOptionValue: "+key);
        if(!result.emplace(key,argv[i+1]).second) throw bruce::Error("DuplicateOption: "+key);
    }
    return result;
}
std::string get(const Options& values,const std::string& key,const std::string& fallback="") {
    const auto it=values.find(key);
    return it==values.end() ? fallback : it->second;
}
std::string required(const Options& values,const std::string& key) {
    const auto value=get(values,key);
    if(value.empty()) throw bruce::Error("MissingOption: "+key);
    return value;
}
bruce::ReplayCheckpoint transfer_checkpoint(const Options& args,bruce::TransferControl& control) {
    if(args.contains("--max-transfer-checkpoints"))control.maximum=bruce::integer<std::uint64_t>(required(args,"--max-transfer-checkpoints"));
    const auto cancellation=get(args,"--transfer-cancellation","none");
    if(cancellation!="none" && cancellation!="checkpoints")throw bruce::Error("UnknownTransferCancellation");
    if(cancellation=="checkpoints") {
        std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
        control.cancelled=[]{return interrupted!=0;};
    }
    if(args.contains("--max-transfer-checkpoints") || args.contains("--transfer-cancellation"))return control.callback();
    return {};
}
std::string controlled_transfer_report(const std::string& result,const bruce::TransferControl& control,bool enabled) {
    return enabled ? "{\"contract\":\"controlled-transfer-v1\",\"result\":"+result+",\"usage\":"+control.report()+"}" : result;
}
#if BRUCE_BUILD_MOD_LANGUAGE
std::string language_status(bruce::LanguageInterpretationStatus status) {
    switch(status) {
        case bruce::LanguageInterpretationStatus::understood: return "UNDERSTOOD";
        case bruce::LanguageInterpretationStatus::ambiguous: return "AMBIGUOUS";
        case bruce::LanguageInterpretationStatus::unsupported: return "UNSUPPORTED";
    }
    throw bruce::Error("InvalidLanguageStatus");
}
std::string render_language_draft_cli(const bruce::LanguageDraftV1& draft,std::string_view contract) {
    std::string result="{\"contract\":"+bruce::json_string(contract)+
        ",\"status\":"+bruce::json_string(language_status(draft.status))+
        ",\"grammar_version\":"+bruce::json_string(draft.grammar_version)+
        ",\"raw_text\":"+bruce::json_string(draft.raw_text)+
        ",\"normalized_text\":"+bruce::json_string(draft.normalized_text)+
        ",\"alternatives\":[";
    for(std::size_t i=0;i<draft.alternatives.size();++i) {
        if(i) result+=',';
        result+="{\"id\":"+bruce::json_string(draft.alternatives[i].id)+
            ",\"rule\":"+bruce::json_string(draft.alternatives[i].rule)+"}";
    }
    result+="],\"selected_id\":"+(draft.selected_id.empty() ? "null" : bruce::json_string(draft.selected_id))+
        ",\"inputs\":[";
    for(std::size_t i=0;i<draft.inputs.size();++i) {
        if(i) result+=',';
        result+="{\"alias\":"+bruce::json_string(draft.inputs[i].alias)+
            ",\"digest\":"+bruce::json_string(draft.inputs[i].digest)+"}";
    }
    result+=']';
    if(contract=="language-selection-v1") {
        const auto chosen=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if(chosen==draft.alternatives.end()) throw bruce::Error("UnknownLanguageAlternative");
        result+=",\"selected_alternative\":{\"id\":"+bruce::json_string(chosen->id)+
            ",\"rule\":"+bruce::json_string(chosen->rule)+"}";
    }
    return result+'}';
}
std::string render_language_draft_cli(const bruce::LanguageDraftV2& draft,std::string_view contract) {
    std::string result="{\"contract\":"+bruce::json_string(contract)+
        ",\"status\":"+bruce::json_string(language_status(draft.status))+
        ",\"grammar_version\":"+bruce::json_string(draft.grammar_version)+
        ",\"raw_text\":"+bruce::json_string(draft.raw_text)+
        ",\"normalized_text\":"+bruce::json_string(draft.normalized_text)+
        ",\"alternatives\":[";
    for(std::size_t i=0;i<draft.alternatives.size();++i) {
        if(i) result+=',';
        result+="{\"id\":"+bruce::json_string(draft.alternatives[i].id)+
            ",\"rule\":"+bruce::json_string(draft.alternatives[i].rule)+"}";
    }
    result+="],\"selected_id\":"+(draft.selected_id.empty() ? "null" : bruce::json_string(draft.selected_id))+
        ",\"inputs\":[";
    for(std::size_t i=0;i<draft.inputs.size();++i) {
        if(i) result+=',';
        result+="{\"alias\":"+bruce::json_string(draft.inputs[i].alias)+
            ",\"digest\":"+bruce::json_string(draft.inputs[i].digest)+"}";
    }
    result+=']';
    if(contract=="language-selection-v2") {
        const auto chosen=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if(chosen==draft.alternatives.end()) throw bruce::Error("UnknownLanguageAlternative");
        result+=",\"selected_alternative\":{\"id\":"+bruce::json_string(chosen->id)+
            ",\"rule\":"+bruce::json_string(chosen->rule)+"}";
    }
    return result+'}';
}
std::string render_language_draft_cli(const bruce::LanguageDraftV3& draft,std::string_view contract) {
    std::string result="{\"contract\":"+bruce::json_string(contract)+
        ",\"status\":"+bruce::json_string(language_status(draft.status))+
        ",\"grammar_version\":"+bruce::json_string(draft.grammar_version)+
        ",\"raw_text\":"+bruce::json_string(draft.raw_text)+
        ",\"normalized_text\":"+bruce::json_string(draft.normalized_text)+
        ",\"alternatives\":[";
    for(std::size_t i=0;i<draft.alternatives.size();++i) {
        if(i) result+=',';
        result+="{\"id\":"+bruce::json_string(draft.alternatives[i].id)+
            ",\"rule\":"+bruce::json_string(draft.alternatives[i].rule)+"}";
    }
    result+="],\"selected_id\":"+(draft.selected_id.empty() ? "null" : bruce::json_string(draft.selected_id))+
        ",\"inputs\":[";
    for(std::size_t i=0;i<draft.inputs.size();++i) {
        if(i) result+=',';
        result+="{\"alias\":"+bruce::json_string(draft.inputs[i].alias)+
            ",\"digest\":"+bruce::json_string(draft.inputs[i].digest)+"}";
    }
    result+=']';
    if(contract=="language-selection-v3") {
        const auto chosen=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if(chosen==draft.alternatives.end()) throw bruce::Error("UnknownLanguageAlternative");
        result+=",\"selected_alternative\":{\"id\":"+bruce::json_string(chosen->id)+
            ",\"rule\":"+bruce::json_string(chosen->rule)+"}";
    }
    return result+'}';
}
std::string pack_language_mod_run(const bruce::LanguageRunV1& run) {
    const auto draft_bytes=bruce::pack_language_draft(run.draft);
    const auto run_bytes=bruce::pack_language_run(run);
    return bruce::pack_mod_run_envelope({
        .manifest = bruce::language_mod_manifest_v1(),
        .input_codec = "bruce-language-draft-v1",
        .input_bytes = draft_bytes,
        .input_digest = bruce::sha256(draft_bytes),
        .payload_codec = "bruce-language-run-v1",
        .payload_bytes = run_bytes,
        .payload_digest = bruce::sha256(run_bytes),
    });
}
bruce::LanguageRunV1 replay_language_cli(const std::string_view bytes) {
    if(!bytes.starts_with("BRUCE_MOD_RUN_V1\n")) return bruce::replay_language_run(bytes);
    const auto envelope=bruce::unpack_mod_run_envelope(bytes);
    bruce::require_matching_mod_manifest(envelope.manifest,bruce::language_mod_manifest_v1());
    if(envelope.input_codec!="bruce-language-draft-v1" || envelope.payload_codec!="bruce-language-run-v1")
        throw bruce::Error("InvalidLanguageModCodec");
    const auto draft=bruce::unpack_language_draft(envelope.input_bytes);
    const auto run=bruce::replay_language_run(envelope.payload_bytes);
    if(bruce::pack_language_draft(run.draft)!=bruce::pack_language_draft(draft))
        throw bruce::Error("LanguageModDraftRunMismatch");
    return run;
}
std::string pack_language_mod_run_v2(const bruce::LanguageRunV2& run) {
    const auto draft_bytes=bruce::pack_language_draft_v2(run.draft);
    const auto run_bytes=bruce::pack_language_run_v2(run);
    return bruce::pack_mod_run_envelope({
        .manifest = bruce::language_mod_manifest_v1(),
        .input_codec = "bruce-language-draft-v2",
        .input_bytes = draft_bytes,
        .input_digest = bruce::sha256(draft_bytes),
        .payload_codec = "bruce-language-run-v2",
        .payload_bytes = run_bytes,
        .payload_digest = bruce::sha256(run_bytes),
    });
}
bruce::LanguageRunV2 replay_language_cli_v2(const std::string_view bytes) {
    if(!bytes.starts_with("BRUCE_MOD_RUN_V1\n")) return bruce::replay_language_run_v2(bytes);
    const auto envelope=bruce::unpack_mod_run_envelope(bytes);
    bruce::require_matching_mod_manifest(envelope.manifest,bruce::language_mod_manifest_v1());
    if(envelope.input_codec!="bruce-language-draft-v2" || envelope.payload_codec!="bruce-language-run-v2")
        throw bruce::Error("InvalidLanguageModCodec");
    const auto draft=bruce::unpack_language_draft_v2(envelope.input_bytes);
    const auto run=bruce::replay_language_run_v2(envelope.payload_bytes);
    if(bruce::pack_language_draft_v2(run.draft)!=bruce::pack_language_draft_v2(draft))
        throw bruce::Error("LanguageModDraftRunMismatch");
    return run;
}
std::string pack_language_mod_run_v3(const bruce::LanguageRunV3& run) {
    const auto draft_bytes=bruce::pack_language_draft_v3(run.draft);
    const auto run_bytes=bruce::pack_language_run_v3(run);
    return bruce::pack_mod_run_envelope({
        .manifest = bruce::language_mod_manifest_v1(),
        .input_codec = "bruce-language-draft-v3",
        .input_bytes = draft_bytes,
        .input_digest = bruce::sha256(draft_bytes),
        .payload_codec = "bruce-language-run-v3",
        .payload_bytes = run_bytes,
        .payload_digest = bruce::sha256(run_bytes),
    });
}
bruce::LanguageRunV3 replay_language_cli_v3(const std::string_view bytes) {
    if(!bytes.starts_with("BRUCE_MOD_RUN_V1\n")) return bruce::replay_language_run_v3(bytes);
    const auto envelope=bruce::unpack_mod_run_envelope(bytes);
    bruce::require_matching_mod_manifest(envelope.manifest,bruce::language_mod_manifest_v1());
    if(envelope.input_codec!="bruce-language-draft-v3" || envelope.payload_codec!="bruce-language-run-v3")
        throw bruce::Error("InvalidLanguageModCodec");
    const auto draft=bruce::unpack_language_draft_v3(envelope.input_bytes);
    const auto run=bruce::replay_language_run_v3(envelope.payload_bytes);
    if(bruce::pack_language_draft_v3(run.draft)!=bruce::pack_language_draft_v3(draft))
        throw bruce::Error("LanguageModDraftRunMismatch");
    return run;
}
bool language_run_is_v3(const std::string_view bytes) {
    if(bytes.starts_with("BRUCE_LANGUAGE_RUN_V3\n")) return true;
    if(!bytes.starts_with("BRUCE_MOD_RUN_V1\n")) return false;
    return bruce::unpack_mod_run_envelope(bytes).payload_codec=="bruce-language-run-v3";
}
bool language_run_is_v2(const std::string_view bytes) {
    if(bytes.starts_with("BRUCE_LANGUAGE_RUN_V2\n")) return true;
    if(!bytes.starts_with("BRUCE_MOD_RUN_V1\n")) return false;
    return bruce::unpack_mod_run_envelope(bytes).payload_codec=="bruce-language-run-v2";
}
#endif
#if BRUCE_BUILD_MOD_DIALOGUE
std::string dialogue_status(bruce::DialogueStatus status) {
    switch(status) {
        case bruce::DialogueStatus::understood: return "UNDERSTOOD";
        case bruce::DialogueStatus::ambiguous: return "AMBIGUOUS";
        case bruce::DialogueStatus::unsupported: return "UNSUPPORTED";
    }
    throw bruce::Error("InvalidDialogueStatus");
}
std::string render_dialogue_cli(const bruce::DialogueDraftV1& draft,std::string_view contract) {
    std::string result="{\"contract\":"+bruce::json_string(contract)+
        ",\"status\":"+bruce::json_string(dialogue_status(draft.status))+
        ",\"normalized_text\":"+bruce::json_string(draft.normalized_text)+
        ",\"selected_id\":"+(draft.selected_id.empty() ? "null" : bruce::json_string(draft.selected_id))+
        ",\"candidates\":[";
    for(std::size_t i=0;i<draft.candidates.size();++i) {
        if(i) result+=',';
        const auto& candidate=draft.candidates[i];
        result+="{\"id\":"+bruce::json_string(candidate.id)+
            ",\"intent\":"+bruce::json_string(candidate.intent)+
            ",\"rule\":"+bruce::json_string(candidate.rule)+
            ",\"requires_confirmation\":"+(candidate.requires_confirmation ? "true" : "false")+"}";
    }
    return result+"]}";
}
std::string render_dialogue_run_cli(const bruce::DialogueRunV1& run,std::string_view contract) {
    return "{\"contract\":"+bruce::json_string(contract)+
        ",\"decision_intent\":"+bruce::json_string(run.decision_intent)+
        ",\"explanation\":"+bruce::json_string(run.explanation)+
        ",\"draft\":"+render_dialogue_cli(run.draft,"dialogue-draft-v1")+"}";
}
#endif
void append_task_spec_roles(std::string& result,const std::vector<bruce::TokenizedRoleV1>& roles) {
    result+='[';
    for(std::size_t i=0;i<roles.size();++i) {
        if(i) result+=',';
        const auto& role=roles[i];
        result+="{\"role\":"+bruce::json_string(role.role)+
            ",\"token_id\":"+bruce::json_string(role.token_id)+
            ",\"asset_alias\":"+(role.asset_alias.empty() ? "null" : bruce::json_string(role.asset_alias))+"}";
    }
    result+=']';
}
std::string render_task_spec_cli(const bruce::TokenizedTaskSpecV1& spec) {
    std::string result="{\"contract\":\"tokenized-task-spec-v1\",\"task_spec_version\":"+
        bruce::json_string(spec.task_spec_version)+
        ",\"normalization_version\":"+bruce::json_string(spec.normalization_version)+
        ",\"token_table_version\":"+bruce::json_string(spec.table.version)+
        ",\"token_table_digest\":"+bruce::json_string(spec.table_digest)+
        ",\"ordered_token_ids\":[";
    for(std::size_t i=0;i<spec.ordered_token_ids.size();++i) {
        if(i) result+=',';
        result+=bruce::json_string(spec.ordered_token_ids[i]);
    }
    result+="],\"assets\":[";
    for(std::size_t i=0;i<spec.assets.size();++i) {
        if(i) result+=',';
        result+="{\"alias\":"+bruce::json_string(spec.assets[i].alias)+
            ",\"digest\":"+bruce::json_string(spec.assets[i].digest)+"}";
    }
    result+="],\"roles\":";
    append_task_spec_roles(result,spec.roles);
    result+=",\"alternatives\":[";
    for(std::size_t i=0;i<spec.alternatives.size();++i) {
        if(i) result+=',';
        result+="{\"id\":"+bruce::json_string(spec.alternatives[i].id)+",\"roles\":";
        append_task_spec_roles(result,spec.alternatives[i].roles);
        result+='}';
    }
    result+="],\"selected_id\":"+(spec.selected_id.empty() ? "null" : bruce::json_string(spec.selected_id))+
        ",\"route\":"+bruce::json_string(spec.route_id)+
        ",\"domain\":"+bruce::json_string(spec.domain)+
        ",\"budget\":{\"max_candidates\":"+std::to_string(spec.budget.candidates)+
        ",\"max_operations\":"+std::to_string(spec.budget.operations)+
        ",\"max_trace\":"+std::to_string(spec.budget.trace_events)+
        ",\"max_results\":"+std::to_string(spec.budget.results)+
        "},\"limits\":{\"min\":"+bruce::json_string(spec.minimum)+
        ",\"max\":"+bruce::json_string(spec.maximum)+
        ",\"max_bits\":"+std::to_string(spec.max_bits)+
        ",\"max_denominator\":"+std::to_string(spec.max_denominator)+
        ",\"max_power\":"+std::to_string(spec.max_power)+
        ",\"max_degree\":"+std::to_string(spec.max_degree)+
        ",\"max_trace_bytes\":"+std::to_string(spec.max_trace_bytes)+"}}";
    return result;
}
void execute_cli(const Options& args,bruce::ApplicationTask task,const bruce::ExecutionControl& control={}) {
    const auto verification=get(args,"--verification","none");
    if(verification!="none" && verification!="replay") throw bruce::Error("UnknownExecutionVerification");
    auto execution_control=control;
    const auto execution=get(args,"--execution-cancellation","none");
    if(execution!="none" && execution!="experience-checkpoints" && execution!="grid-source-checkpoints" && execution!="grid-and-experience-checkpoints" && execution!="compatibility-source-checkpoints" && execution!="resume-import-checkpoints")throw bruce::Error("UnknownExecutionCancellation");
    if(execution!="none"){
        if(verification!="replay")throw bruce::Error("ExecutionCancellationRequiresReplay");
        std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
        if(execution=="resume-import-checkpoints")execution_control.resume_import_cancelled=[]{return interrupted!=0;};
        if(execution=="compatibility-source-checkpoints")execution_control.compatibility_source_cancelled=[]{return interrupted!=0;};
        if(execution=="experience-checkpoints" || execution=="grid-and-experience-checkpoints")execution_control.experience_cancelled=[]{return interrupted!=0;};
        if(execution=="grid-source-checkpoints" || execution=="grid-and-experience-checkpoints")execution_control.grid_source_cancelled=[]{return interrupted!=0;};
    }
    const auto cancellation=get(args,"--verification-cancellation","none");
    if(cancellation!="none" && cancellation!="phase-boundaries" && cancellation!="grid-query-boundaries" && cancellation!="research-checkpoints" && cancellation!="comparison-checkpoints" && cancellation!="compatibility-checkpoints") throw bruce::Error("UnknownVerificationCancellation");
    const auto serialization=get(args,"--serialization-cancellation","none");
    if(serialization!="none" && serialization!="digest-checkpoints" && serialization!="grid-record-checkpoints") throw bruce::Error("UnknownSerializationCancellation");
    if(serialization!="none" && verification!="replay") throw bruce::Error("SerializationCancellationRequiresReplay");
    bruce::VerificationControl verification_control;
    verification_control.serialization_digest_checkpoints=serialization=="digest-checkpoints";
    verification_control.serialization_grid_records=serialization=="grid-record-checkpoints";
    if(cancellation!="none" || serialization!="none") {
        if(verification!="replay") throw bruce::Error("VerificationCancellationRequiresReplay");
        std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
        verification_control.cancelled=[]{return interrupted!=0;};
        verification_control.grid_replay_checkpoints=cancellation=="grid-query-boundaries";
        verification_control.research_replay_checkpoints=cancellation=="research-checkpoints";
        verification_control.comparison_replay_checkpoints=cancellation=="comparison-checkpoints";
        verification_control.compatibility_replay_checkpoints=cancellation=="compatibility-checkpoints";
    }
    bruce::VerificationBudget budget;
    bruce::SerializationBudget serialization_budget;
    if(args.contains("--max-replay-bytes")) {
        if(verification!="replay") throw bruce::Error("ReplayBudgetRequiresVerification");
        budget.max_replay_bytes=bruce::integer<std::uint64_t>(required(args,"--max-replay-bytes"));
    }
    if(args.contains("--max-serialized-bytes")) {
        if(verification!="replay") throw bruce::Error("SerializationBudgetRequiresVerification");
        serialization_budget.max_serialized_bytes=bruce::integer<std::uint64_t>(required(args,"--max-serialized-bytes"));
    }
    bruce::SessionBudget session_budget;
    if(args.contains("--max-session-checkpoints")){
        if(verification!="replay")throw bruce::Error("SessionBudgetRequiresVerification");
        session_budget.max_checkpoints=bruce::integer<std::uint64_t>(required(args,"--max-session-checkpoints"));
    }
    if(verification=="replay") {
        const auto session=bruce::execute_checked_application(std::move(task),execution_control,budget,verification_control,session_budget,serialization_budget);
        const auto output=bruce::render_execution_session(session);
        if(args.contains("--output")) bruce::export_exclusive(required(args,"--output"),session.bundle);
        std::cout<<output<<'\n';
    } else {
        const auto run=bruce::execute_application(std::move(task),execution_control);
        if(args.contains("--output")) bruce::export_exclusive(required(args,"--output"),bruce::pack_application(run));
        std::cout<<bruce::render_application(run)<<'\n';
    }
}
void help() {
    std::cout << "BRUCE Core " << BRUCE_VERSION << R"( — Rational equality with preserved domains of definition

bruce --version
bruce contracts
bruce task-contracts
bruce gate-contracts
bruce mods verify PACKAGE --trust-store PATH
bruce mods install PACKAGE --mod-root PATH --trust-store PATH
bruce mods list --mod-root PATH
bruce mods activate MOD_ID VERSION --mod-root PATH
bruce mods disable MOD_ID --mod-root PATH
bruce route --text TEXT --mod-root PATH [--output REPORT]
bruce route replay REPORT --mod-root PATH
bruce compare --left-numerator A0,A1,... --left-denominator B0,B1,...
              --right-numerator C0,C1,... --right-denominator D0,D1,...
              [--domain-policy quadratic|modular|modular-witness]
              [--max-bits 4096] [--max-operations 1000000] [--output RUN.bruce]
              Exact rational-function comparison over Q; coefficients in ascending order.
bruce coordinate --x N --y N
bruce coordinate --z N
bruce grid --path 2,6/4,3 [--mode exact|modular|metric]
           [--modulus N] [--max-bits 1..64] [--zero-zero one|stop] [--output RUN.bruce]
bruce grid-resume --from-run PARTIAL --additional-queries 1..1024 [--output FILE]
bruce grid-batch --paths "2,6;4,3;2,6" [--cache-bytes 0..1048576] [--max-query-steps 0..1024]
                 [the same grid modes, limits, and export options]
bruce research --train FILE [--holdout FILE] [--min -8] [--max 8]
               [--max-candidates 100000] [--max-operations 1000000]
               [--max-trace 100002] [--max-results 128] [--output RUN.bruce]
bruce language interpret --text TEXT --train TRAIN.csv --holdout HOLDOUT.csv
               [--grammar v1|v2|v3]
               [V2: --train-alias ALIAS --holdout-alias ALIAS]
               [--max-bits N] [--max-denominator N] [--max-candidates N]
               [--max-operations N] [--max-trace N] [--max-results N] --output DRAFT
bruce language select DRAFT --alternative ID --output SELECTED_DRAFT
bruce language execute DRAFT --output LANGUAGE_RUN
bruce language replay LANGUAGE_RUN
)";
#if BRUCE_BUILD_MOD_DIALOGUE
    std::cout << R"(bruce dialogue interpret --text TEXT --output DIALOGUE_DRAFT
bruce dialogue select DIALOGUE_DRAFT --alternative ID --output SELECTED_DRAFT
bruce dialogue confirm DIALOGUE_DRAFT --output DIALOGUE_RUN
bruce dialogue replay DIALOGUE_DRAFT|DIALOGUE_RUN
)";
#endif
    std::cout << R"(
bruce task-spec create --train FILE --holdout FILE --route rational-portfolio-v1
               --token-table-version v1 --output SPEC
               [--min N] [--max N] [--max-bits N] [--max-denominator N]
               [--max-power N] [--max-degree N] [--max-trace-bytes N]
               [--max-candidates N] [--max-operations N] [--max-trace N] [--max-results N]
bruce task-spec inspect SPEC
bruce task-spec execute SPEC --output RUN
bruce task-spec replay RUN
bruce evidence create --train FILE --holdout FILE --output FILE
    [--domain bigint|rational] [--min N] [--max N] [--max-bits N]
    [--max-denominator N] [--max-trace-bytes N] [--max-candidates N]
    [--max-operations N] [--max-trace N] [--max-results N]
bruce evidence replay CHAIN
)";
#if BRUCE_BUILD_MOD_NANOCELL
    std::cout << R"(bruce nanocell run --core HEX16 --active-index 0..7 --context-id ID --context-file FILE
                   --technique masked-match-v1|rotate-match-v1|context-conflict-v1
                   [--max-operations 0..16] [--max-trace-entries 2..18] [--output RUN]
bruce nanocell replay RUN
)";
#endif
#if BRUCE_BUILD_MOD_COCKPIT
    std::cout << "bruce cockpit [--port N]\n";
#endif
#if BRUCE_BUILD_APPLICATION_DISPATCH
    std::cout << R"(bruce dispatch inspect BUNDLE
bruce dispatch replay BUNDLE
)";
#endif
    std::cout << R"(bruce research --grid-train GRID.bruce [--grid-holdout GRID.bruce] [--grid-axis x|y] [--max-total-operations N]
               [existing exact strategies, budgets, and export options]
bruce experience add --from-run RUN.bruce [--store OLD.experience] --output NEW.experience
                  [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience revoke --store OLD.experience --id ID --reason TEXT --output NEW.experience
                     [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience replace --store STORE --id ID --reason TEXT --from-run RUN --output STORE
                      [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience plan-add --from-run RUN [--store STORE] [--max-source-bytes N] [--max-store-bytes N] --output PLAN
bruce experience apply-add PLAN --from-run RUN [--store STORE] --output NEW
               [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce experience inspect SNAPSHOT [--max-management-checkpoints N]
                                  [--management-cancellation none|checkpoints]
bruce rules-stage STORE NEW PROPOSAL
bruce rules-activate STORE PROPOSAL
bruce rules-status STORE [--max-management-checkpoints N]
                         [--management-cancellation none|checkpoints]
bruce rules-propose OLD NEW PROPOSAL
bruce rules-approve PROPOSAL CURRENT OUTPUT
bruce inspect-run RUN.bruce
)";
#if BRUCE_BUILD_PHASE_DAG
    std::cout << R"(bruce phase-dag inspect BUNDLE
bruce phase-dag replay BUNDLE
)";
#endif
    std::cout << R"(bruce gate-run RUN.bruce [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none|checkpoints]
bruce assess-run RUN.bruce
bruce replay RUN.bruce
bruce calculate --op add|sub|mul|div|divmod|pow|modpow --left NUMBER --right NUMBER
                [--domain bigint|rational] [--max-bits 4096] [--modulus NUMBER]
bruce research --domain bigint|rational --train FILE [--holdout FILE]
               [--min -8] [--max 8] [--max-bits 4096] [--max-denominator N]
               [--max-trace-bytes 1048576] [the same research budgets/export options]
bruce research --strategy power-affine --train FILE [--max-power 4]
               [--domain bigint|rational] [--planner enumerate|adaptive]
               [the same research budgets/export options]
bruce research --strategy polynomial --train FILE [--max-degree 3]
               [--domain bigint|rational] [--planner interpolate]
               [the same research budgets/export options]
bruce research --strategy expression --train FILE [--holdout FILE]
               [--max-nodes 7] [--min 0] [--max 1] [--max-denominator 1]
               [--domain rational] [--planner grammar|indexed-grammar|cached-grammar] [--equivalence syntax|rational|rational-q|rational-q-modular|rational-q-witness]
               [--domain-cache none|equal-64 (rational-q-modular only)]
               [--max-results 1..256 for rational-q-witness; default 128]
               [--operators "+-*/" (optional ^ with literal exponents 2..8)]
               [research budgets/export options]
bruce research --strategy rational-quadratic --train FILE [--holdout FILE]
               [--domain rational] [--planner solve] [the same research budgets/export options]
bruce research --strategy rational-linear --train FILE [--holdout FILE]
               [--domain rational] [--planner solve] [--max-denominator 8]
               [--min -8] [--max 8] [--output RUN.bruce]
bruce research --strategy portfolio-expression --train FILE [--holdout FILE]
               [--max-power 4] [--max-degree 3] [--max-nodes 7]
               [--min 0] [--max 1] [--max-denominator 1]
               [--domain rational] [--planner portfolio|adaptive-order|experience-order]
               [--equivalence rational|rational-q|rational-q-modular|rational-q-witness] [--domain-cache none|equal-64]
               [--expression-planner grammar|indexed-grammar|cached-grammar]
               [--operators "+-*/" (optional ^, literal exponents 2..8)] [--experience SNAPSHOT]
bruce research --strategy portfolio-quadratic --train FILE [--holdout FILE]
               [--planner portfolio|adaptive-order|experience-order] [--experience SNAPSHOT]
               [--max-power 4] [--max-degree 3] [shared research budgets]
bruce research --strategy portfolio-rational --train FILE [--holdout FILE]
               [--domain rational] [--max-power 4] [--max-degree 3]
               [--planner portfolio|adaptive-order|experience-order] [--experience SNAPSHOT]
               [shared research budgets and export options]
bruce research --strategy portfolio --train FILE [--max-power 4] [--max-degree 3]
               [--domain bigint|rational] [--planner portfolio]
               [shared research budgets and export options]
bruce compatibility-plan --from-run RUN --rules RULES --at X --output PLAN
                  [--rule-store STORE instead of --rules; --max-source-bytes N; selection budgets]
                  [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
bruce compatibility-apply PLAN --from-run RUN --rules RULES [--output RUN]
                  [--max-transfer-checkpoints N] [--transfer-cancellation none|checkpoints]
                  [--rule-store STORE instead of --rules]
bruce compatibility --rule-store STORE --candidates CANDIDATE_FILE
                  [alternative to --rules; existing selection/session options]
bruce compatibility --rules RULE_FILE --candidates CANDIDATE_FILE
                    [--max-candidates 4096] [--max-checks 1048576]
                    [--max-trace 65536] [--output RUN.bruce]
bruce compatibility --rules RULE_FILE --from-run RESEARCH.bruce --at X
                    [the same budgets and export options]

Pair files: header x,y; input on the left, expected output on the right.
Research family: a*x+b; bigint/int64 with integer parameters in [min,max].
Rational: reduced parameters n/d, n in [min,max], d up to max-denominator (default 8).
power-affine: a*x^p+b with p=1..max-power (at most 8); bigint is the default domain.
New input is processed at runtime; no knowledge base is required.
Optional for run commands: --verification replay (an additional complete replay check).
Shared checkpoint budget: --max-session-checkpoints N (with --verification replay).
Optional: --verification-cancellation phase-boundaries (honor signals at verification boundaries).
For resume imports during initial execution: --execution-cancellation resume-import-checkpoints.
For compatibility-selection research sources during initial execution: --execution-cancellation compatibility-source-checkpoints.
For grid sources during initial execution: --execution-cancellation grid-source-checkpoints.
For both imports in grid research: --execution-cancellation grid-and-experience-checkpoints.
For experience imports during initial execution: --execution-cancellation experience-checkpoints (with --verification replay).
For grid batches/resumes: --verification-cancellation grid-query-boundaries (also during replay).
For Int64/exact research, including grid research: --verification-cancellation research-checkpoints (also during replay).
For grid batches/resumes: --serialization-cancellation grid-record-checkpoints.
For bundle digests: --serialization-cancellation digest-checkpoints (with --verification replay).
For Z compatibility: --verification-cancellation compatibility-checkpoints (also during replay).
For rational-function comparison: --verification-cancellation comparison-checkpoints (also during replay).
Optional: --max-replay-bytes N (limit bundle bytes before replay; 0 disables replay).
Additionally: --max-serialized-bytes N (limit bundle size after SERIALIZING).
Run exports require --output and are never overwritten. Mod lifecycle commands update --mod-root.
Ctrl+C stops the search at a logical cancellation point (replayable).
VERIFIED_ON_SAMPLES confirms the checked pairs, not a general proof.
Output: JSON, integers without floating-point rounding. Errors: JSON on stderr, exit 2.
)";
}
}

int main(int argc,char** argv) {
    using namespace bruce;
    try {
        if(argc<2 || std::string(argv[1])=="--help" || std::string(argv[1])=="help") { help(); return 0; }
        const std::string command=argv[1];
        if(command=="mods" || command=="route") return bruce::runtime_mod_cli(argc,argv);
#if BRUCE_BUILD_MOD_NANOCELL
        if(command=="nanocell") return bruce::nanocell_cli(argc,argv);
#endif
#if BRUCE_BUILD_MOD_COCKPIT
        if(command=="cockpit") return bruce::cockpit_cli(argc,argv);
#endif
#if BRUCE_BUILD_APPLICATION_DISPATCH
        if(command=="dispatch") {
            if(argc!=4) throw Error("Usage: bruce dispatch inspect|replay BUNDLE");
            const auto bundle=read_bounded(argv[3],dispatch_max_payload_bytes+1024);
            if(std::string(argv[2])=="inspect") {
                std::cout << inspect_dispatch(bundle) << '\n';
                return 0;
            }
            if(std::string(argv[2])=="replay") {
                std::cout << render_dispatch(replay_dispatch(bundle)) << '\n';
                return 0;
            }
            throw Error("Usage: bruce dispatch inspect|replay BUNDLE");
        }
#endif
        if(command=="rules-stage" || command=="rules-activate" || command=="rules-status") return bruce::rule_store_cli(argc,argv);
        if(command=="rules-propose" || command=="rules-approve") return bruce::rule_review(argc,argv);
#if BRUCE_BUILD_PHASE_DAG
        if(command=="phase-dag") {
            if(argc!=4)
                throw Error("Usage: bruce phase-dag inspect BUNDLE");
            const std::string subcommand=argv[2];
            if(subcommand=="inspect") {
                std::cout << inspect_phase_dag_application_bundle(read_bounded(argv[3])) << '\n';
                return 0;
            }
            if(subcommand=="replay") {
                std::cout << replay_phase_dag_session(read_bounded(argv[3])).report() << '\n';
                return 0;
            }
            throw Error("Usage: bruce phase-dag inspect BUNDLE");
        }
#endif
        if(command=="--version") {
            if(argc!=2)throw Error("VersionTakesNoArguments");
            std::cout << "{\"contract\":\"bruce-version-v1\",\"version\":" << json_string(BRUCE_VERSION) << "}\n";
            return 0;
        }
        if(command=="compare") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--left-numerator","--left-denominator","--right-numerator","--right-denominator","--domain-policy","--max-bits","--max-operations","--output"});
            FractionComparisonTask task;
            task.domain_policy=get(args,"--domain-policy","quadratic");
            task.left_numerator=required(args,"--left-numerator");task.left_denominator=required(args,"--left-denominator");
            task.right_numerator=required(args,"--right-numerator");task.right_denominator=required(args,"--right-denominator");
            task.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
            task.max_operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
            std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
            execute_cli(args,std::move(task),{[]{return interrupted!=0;}});
        } else if(command=="coordinate") {
            const auto args=options(argc,argv,2,{"--x","--y","--z"});
            Coordinate coordinate;
            std::uint64_t z;
            if(args.contains("--z")) {
                if(args.size()!=1) throw Error("UseEitherZOrXY");
                z=integer<std::uint64_t>(required(args,"--z")); coordinate=unpair(z);
            } else {
                coordinate={integer<std::uint64_t>(required(args,"--x")),integer<std::uint64_t>(required(args,"--y"))};
                z=pair(coordinate.x,coordinate.y);
            }
            std::cout << "{\"x\":\"" << coordinate.x << "\",\"y\":\"" << coordinate.y << "\",\"z\":\"" << z << "\"";
            if(coordinate.x<8 && coordinate.y<8) std::cout << ",\"local_z\":" << unsigned(local_pair(coordinate.x,coordinate.y));
            std::cout << "}\n";
        } else if(command=="grid") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--path","--mode","--modulus","--max-bits","--zero-zero","--output"});
            GridTask task;task.path=required(args,"--path");task.mode=get(args,"--mode","exact");
            task.zero_zero=get(args,"--zero-zero","one");task.max_bits=integer<unsigned>(get(args,"--max-bits","64"));
            if(task.mode=="modular") task.modulus=integer<std::uint64_t>(required(args,"--modulus"));
            else if(args.contains("--modulus")) throw Error("ModulusRequiresModularMode");
            execute_cli(args,std::move(task));
        } else if(command=="grid-batch") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--paths","--mode","--modulus","--max-bits","--zero-zero","--cache-bytes","--max-query-steps","--output"});
            GridBatchTask task;
            if(args.contains("--max-query-steps")) task.query_steps=integer<std::size_t>(required(args,"--max-query-steps"));
            task.spec.mode=get(args,"--mode","exact");task.spec.zero_zero=get(args,"--zero-zero","one");
            task.spec.max_bits=integer<unsigned>(get(args,"--max-bits","64"));
            task.cache_bytes=integer<std::size_t>(get(args,"--cache-bytes","65536"));
            if(task.spec.mode=="modular") task.spec.modulus=integer<std::uint64_t>(required(args,"--modulus"));
            else if(args.contains("--modulus")) throw Error("ModulusRequiresModularMode");
            const auto paths=required(args,"--paths");
            if(paths.size()>max_grid_batch_input_bytes) throw Error("GridBatchInputByteLimit");
            std::string_view remaining=paths;
            while(true) {
                const auto separator=remaining.find(';');
                task.paths.emplace_back(remaining.substr(0,separator));
                if(task.paths.size()>max_grid_batch_queries) throw Error("GridQueryLimit");
                if(separator==std::string_view::npos) break;
                remaining.remove_prefix(separator+1);
            }
            std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
            execute_cli(args,std::move(task),{[]{return interrupted!=0;}});
        } else if(command=="grid-resume") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--from-run","--additional-queries","--output"});
            const auto parent=read_bounded(required(args,"--from-run"));
            const auto additional=integer<std::size_t>(required(args,"--additional-queries"));
            std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
            execute_cli(args,GridResumeTask{parent,additional},{[]{return interrupted!=0;}});
        } else if(command=="task-contracts") {
            if(argc!=2) throw Error("Usage: bruce task-contracts");
            std::cout<<render_application_contract()<<'\n';
        } else if(command=="gate-contracts") {
            if(argc!=2) throw Error("Usage: bruce gate-contracts");
            std::cout<<render_research_gate_contract()<<'\n';
        } else if(command=="contracts") {
            if(argc!=2) throw Error("Usage: bruce contracts");
            std::cout<<render_research_contract()<<'\n';
        } else if(command=="calculate") {
            const auto args=options(argc,argv,2,{"--domain","--op","--left","--right","--max-bits","--modulus"});
            const NumberDomain domain(get(args,"--domain","bigint"),integer<unsigned>(get(args,"--max-bits","4096")));
            std::cout << calculate_exact(domain,required(args,"--op"),required(args,"--left"),required(args,"--right"),get(args,"--modulus")) << '\n';
        } else if(command=="task-spec") {
            if(argc<3) throw Error("Usage: bruce task-spec create|inspect|execute|replay");
            const std::string action=argv[2];
            if(action=="create") {
                const auto args=options(argc,argv,3,{"--train","--holdout","--route","--token-table-version","--output",
                    "--min","--max","--max-bits","--max-denominator","--max-power","--max-degree",
                    "--max-trace-bytes","--max-candidates","--max-operations","--max-trace","--max-results"});
                const auto train_path=required(args,"--train");
                const auto holdout_path=required(args,"--holdout");
                const auto route=required(args,"--route");
                const auto table_version=required(args,"--token-table-version");
                const auto output=required(args,"--output");
                if(route!="rational-portfolio-v1") throw Error("UnsupportedTaskSpecRoute");
                if(table_version!="v1") throw Error("UnsupportedTokenTableVersion");
                if(std::filesystem::equivalent(train_path,holdout_path)) throw Error("DuplicateTaskSpecFileAlias");
                const auto training=read_bounded(train_path);
                const auto holdout=read_bounded(holdout_path);
                TokenizedTaskSpecV1 spec;
                spec.table.version=table_version;
                spec.table.entries={{"","Train","",0},{"","Holdout","",1},{"","Rational Portfolio","",2}};
                spec.assets={{"train",training,sha256(training)},{"holdout",holdout,sha256(holdout)}};
                spec.route_id=route;
                spec.domain="rational";
                spec.minimum=get(args,"--min","-8");
                spec.maximum=get(args,"--max","8");
                spec.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                spec.max_denominator=integer<unsigned>(get(args,"--max-denominator","8"));
                spec.max_power=integer<unsigned>(get(args,"--max-power","4"));
                spec.max_degree=integer<unsigned>(get(args,"--max-degree","3"));
                spec.max_trace_bytes=integer<std::uint64_t>(get(args,"--max-trace-bytes","1048576"));
                spec.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                spec.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                spec.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                spec.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                spec=make_tokenized_task_spec(std::move(spec));
                spec.ordered_token_ids={spec.table.entries[0].id,spec.table.entries[1].id,spec.table.entries[2].id};
                spec.roles={{"train",spec.table.entries[0].id,"train"},
                            {"holdout",spec.table.entries[1].id,"holdout"},
                            {"route",spec.table.entries[2].id,""}};
                spec=make_tokenized_task_spec(std::move(spec));
                export_exclusive(output,pack_tokenized_task_spec(spec));
                std::cout<<render_task_spec_cli(spec)<<'\n';
            } else if(action=="inspect") {
                if(argc!=4) throw Error("Usage: bruce task-spec inspect SPEC");
                const auto spec=unpack_tokenized_task_spec(read_bounded(argv[3]));
                std::cout<<render_task_spec_cli(spec)<<'\n';
            } else if(action=="execute") {
                if(argc<4) throw Error("Usage: bruce task-spec execute SPEC --output RUN");
                const auto args=options(argc,argv,4,{"--output"});
                const auto output=required(args,"--output");
                const auto spec=unpack_tokenized_task_spec(read_bounded(argv[3]));
                const auto run=execute_tokenized_task_spec(spec);
                const auto rendered=render_tokenized_task_run(run);
                export_exclusive(output,pack_tokenized_task_run(run));
                std::cout<<rendered<<'\n';
            } else if(action=="replay") {
                if(argc!=4) throw Error("Usage: bruce task-spec replay RUN");
                std::cout<<render_tokenized_task_run(replay_tokenized_task_run(read_bounded(argv[3])))<<'\n';
            } else throw Error("UnknownTaskSpecAction: "+action);
#if BRUCE_BUILD_MOD_LANGUAGE
        } else if(command=="language") {
            if(argc<3) throw Error("Usage: bruce language interpret|select|execute|replay");
            const std::string action=argv[2];
            if(action=="interpret") {
                const auto args=options(argc,argv,3,{"--text","--train","--holdout","--train-alias","--holdout-alias","--grammar","--max-bits","--max-denominator","--max-candidates","--max-operations","--max-trace","--max-results","--output"});
                const auto text=required(args,"--text");
                const auto training=read_bounded(required(args,"--train"),1024*1024);
                const auto holdout=read_bounded(required(args,"--holdout"),1024*1024);
                const auto grammar=get(args,"--grammar","v1");
                if(grammar=="v3") {
                    LanguageRequestV3 request;
                    request.text=text;
                    request.inputs={{get(args,"--train-alias","train"),training,sha256(training)},
                                    {get(args,"--holdout-alias","holdout"),holdout,sha256(holdout)}};
                    request.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                    request.max_denominator=integer<unsigned>(get(args,"--max-denominator","8"));
                    request.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                    request.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                    request.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                    request.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                    const auto draft=interpret_language_v3(std::move(request));
                    if(draft.status==LanguageInterpretationStatus::unsupported) throw Error("UnsupportedLanguageText");
                    export_exclusive(required(args,"--output"),pack_language_draft_v3(draft));
                    std::cout<<render_language_draft_cli(draft,"language-interpretation-v3")<<'\n';
                } else if(grammar=="v2") {
                    LanguageRequestV2 request;
                    request.text=text;
                    request.inputs={{get(args,"--train-alias","train"),training,sha256(training)},
                                    {get(args,"--holdout-alias","holdout"),holdout,sha256(holdout)}};
                    request.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                    request.max_denominator=integer<unsigned>(get(args,"--max-denominator","8"));
                    request.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                    request.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                    request.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                    request.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                    const auto draft=interpret_language_v2(std::move(request));
                    if(draft.status==LanguageInterpretationStatus::unsupported) throw Error("UnsupportedLanguageText");
                    export_exclusive(required(args,"--output"),pack_language_draft_v2(draft));
                    std::cout<<render_language_draft_cli(draft,"language-interpretation-v2")<<'\n';
                } else if(grammar=="v1") {
                    if(args.contains("--train-alias") || args.contains("--holdout-alias"))
                        throw Error("LanguageAliasOptionsRequireV2");
                    LanguageRequestV1 request;
                    request.text=text;
                    request.inputs={{"train",training,sha256(training)},{"holdout",holdout,sha256(holdout)}};
                    request.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                    request.max_denominator=integer<unsigned>(get(args,"--max-denominator","8"));
                    request.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                    request.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                    request.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                    request.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                    LanguageDraftV1 draft;
                    try { draft=interpret_language(request); }
                    catch(const Error& error) {
                        if(std::string_view(error.what())!="MissingLanguageAlias") throw;
                        request.inputs[0].alias="left";
                        request.inputs[1].alias="right";
                        draft=interpret_language(std::move(request));
                    }
                    if(draft.status==LanguageInterpretationStatus::unsupported) throw Error("UnsupportedLanguageText");
                    export_exclusive(required(args,"--output"),pack_language_draft(draft));
                    std::cout<<render_language_draft_cli(draft,"language-interpretation-v1")<<'\n';
                } else throw Error("UnsupportedLanguageGrammar");
            } else if(action=="select") {
                if(argc<4) throw Error("Usage: bruce language select DRAFT --alternative ID --output SELECTED_DRAFT");
                const auto args=options(argc,argv,4,{"--alternative","--output"});
                const auto bytes=read_bounded(argv[3]);
                if(bytes.starts_with("BRUCE_LANGUAGE_DRAFT_V3\n")) {
                    const auto draft=unpack_language_draft_v3(bytes);
                    const auto selected=select_language_alternative_v3(draft,required(args,"--alternative"));
                    export_exclusive(required(args,"--output"),pack_language_draft_v3(selected));
                    std::cout<<render_language_draft_cli(selected,"language-selection-v3")<<'\n';
                } else if(bytes.starts_with("BRUCE_LANGUAGE_DRAFT_V2\n")) {
                    const auto draft=unpack_language_draft_v2(bytes);
                    const auto selected=select_language_alternative_v2(draft,required(args,"--alternative"));
                    export_exclusive(required(args,"--output"),pack_language_draft_v2(selected));
                    std::cout<<render_language_draft_cli(selected,"language-selection-v2")<<'\n';
                } else {
                    const auto draft=unpack_language_draft(bytes);
                    const auto selected=select_language_alternative(draft,required(args,"--alternative"));
                    export_exclusive(required(args,"--output"),pack_language_draft(selected));
                    std::cout<<render_language_draft_cli(selected,"language-selection-v1")<<'\n';
                }
            } else if(action=="execute") {
                if(argc<4) throw Error("Usage: bruce language execute DRAFT --output LANGUAGE_RUN");
                const auto args=options(argc,argv,4,{"--output"});
                const auto bytes=read_bounded(argv[3]);
                if(bytes.starts_with("BRUCE_LANGUAGE_DRAFT_V3\n")) {
                    const auto draft=unpack_language_draft_v3(bytes);
                    const auto run=execute_language_draft_v3(draft);
                    export_exclusive(required(args,"--output"),pack_language_mod_run_v3(run));
                    std::cout<<"{\"contract\":\"language-execution-v3\",\"result\":"<<render_language_run_v3(run)<<"}\n";
                } else if(bytes.starts_with("BRUCE_LANGUAGE_DRAFT_V2\n")) {
                    const auto draft=unpack_language_draft_v2(bytes);
                    const auto run=execute_language_draft_v2(draft);
                    export_exclusive(required(args,"--output"),pack_language_mod_run_v2(run));
                    std::cout<<"{\"contract\":\"language-execution-v2\",\"result\":"<<render_language_run_v2(run)<<"}\n";
                } else {
                    const auto draft=unpack_language_draft(bytes);
                    const auto run=execute_language_draft(draft);
                    export_exclusive(required(args,"--output"),pack_language_mod_run(run));
                    std::cout<<"{\"contract\":\"language-execution-v1\",\"result\":"<<render_language_run(run)<<"}\n";
                }
            } else if(action=="replay") {
                if(argc!=4) throw Error("Usage: bruce language replay LANGUAGE_RUN");
                const auto bytes=read_bounded(argv[3]);
                if(language_run_is_v3(bytes)) {
                    const auto run=replay_language_cli_v3(bytes);
                    std::cout<<"{\"contract\":\"language-replay-v3\",\"result\":"<<render_language_run_v3(run)<<"}\n";
                } else if(language_run_is_v2(bytes)) {
                    const auto run=replay_language_cli_v2(bytes);
                    std::cout<<"{\"contract\":\"language-replay-v2\",\"result\":"<<render_language_run_v2(run)<<"}\n";
                } else {
                    const auto run=replay_language_cli(bytes);
                    std::cout<<"{\"contract\":\"language-replay-v1\",\"result\":"<<render_language_run(run)<<"}\n";
                }
            } else throw Error("UnknownLanguageAction: "+action);
        }
#else
        }
#endif
#if BRUCE_BUILD_MOD_DIALOGUE
        else if(command=="dialogue") {
            if(argc<3) throw Error("Usage: bruce dialogue interpret|select|confirm|replay");
            const std::string action=argv[2];
            if(action=="interpret") {
                const auto args=options(argc,argv,3,{"--text","--output"});
                const auto draft=interpret_dialogue_v1(required(args,"--text"));
                if(draft.status==DialogueStatus::unsupported) throw Error("UnsupportedDialogueText");
                export_exclusive(required(args,"--output"),pack_dialogue_draft_v1(draft));
                std::cout<<render_dialogue_cli(draft,"dialogue-interpretation-v1")<<'\n';
            } else if(action=="select") {
                if(argc<4) throw Error("Usage: bruce dialogue select DRAFT --alternative ID --output SELECTED_DRAFT");
                const auto args=options(argc,argv,4,{"--alternative","--output"});
                const auto draft=unpack_dialogue_draft_v1(read_bounded(argv[3]));
                const auto selected=select_dialogue_alternative_v1(draft,required(args,"--alternative"));
                export_exclusive(required(args,"--output"),pack_dialogue_draft_v1(selected));
                std::cout<<render_dialogue_cli(selected,"dialogue-selection-v1")<<'\n';
            } else if(action=="confirm") {
                if(argc<4) throw Error("Usage: bruce dialogue confirm DRAFT --output DIALOGUE_RUN");
                const auto args=options(argc,argv,4,{"--output"});
                const auto draft=unpack_dialogue_draft_v1(read_bounded(argv[3]));
                const auto run=confirm_dialogue_draft_v1(draft);
                export_exclusive(required(args,"--output"),pack_dialogue_run_v1(run));
                std::cout<<render_dialogue_run_cli(run,"dialogue-confirmation-v1")<<'\n';
            } else if(action=="replay") {
                if(argc!=4) throw Error("Usage: bruce dialogue replay DIALOGUE_DRAFT");
                const auto bytes=read_bounded(argv[3]);
                if(bytes.starts_with("BRUCE_DIALOGUE_RUN_V1\n")) {
                    const auto run=unpack_dialogue_run_v1(bytes);
                    std::cout<<render_dialogue_run_cli(run,"dialogue-replay-v1")<<'\n';
                } else {
                    const auto draft=unpack_dialogue_draft_v1(bytes);
                    std::cout<<render_dialogue_cli(draft,"dialogue-replay-v1")<<'\n';
                }
            } else throw Error("UnknownDialogueAction: "+action);
        }
#endif
        else if(command=="evidence") {
            if(argc<3) throw Error("Usage: bruce evidence create|replay");
            const std::string action=argv[2];
            if(action=="create") {
                const auto args=options(argc,argv,3,{"--train","--holdout","--output","--domain","--min","--max","--max-bits","--max-denominator","--max-trace-bytes","--max-candidates","--max-operations","--max-trace","--max-results"});
                ExactResearchTask task;
                task.domain=get(args,"--domain","bigint");
                if(task.domain!="bigint" && task.domain!="rational") throw Error("UnsupportedEvidenceChainDomain");
                task.strategy="affine";
                task.planner="enumerate";
                task.minimum=get(args,"--min","-8");
                task.maximum=get(args,"--max","8");
                task.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                task.max_denominator=integer<unsigned>(get(args,"--max-denominator",task.domain=="rational" ? "8" : "1"));
                task.max_trace_bytes=integer<std::uint64_t>(get(args,"--max-trace-bytes","1048576"));
                task.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                task.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                task.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                task.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                const auto training=read_bounded(required(args,"--train"),1024*1024);
                const auto holdout=read_bounded(required(args,"--holdout"),1024*1024);
                const NumberDomain domain(task.domain,task.max_bits);
                task.training=parse_exact_samples(training,domain);
                task.holdout=parse_exact_samples(holdout,domain);
                const auto run=create_exact_evidence_chain(std::move(task),training,holdout);
                const auto bundle=pack_exact_evidence_chain(run);
                const auto output=render_exact_evidence_chain(run);
                export_exclusive(required(args,"--output"),bundle);
                std::cout<<output<<'\n';
            } else if(action=="replay") {
                if(argc!=4) throw Error("Usage: bruce evidence replay CHAIN");
                const auto run=replay_exact_evidence_chain(read_bounded(argv[3]));
                std::cout<<render_exact_evidence_chain(run)<<'\n';
            } else throw Error("UnknownEvidenceAction: "+action);
        } else if(command=="research") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--train","--holdout","--min","--max","--max-candidates","--max-operations","--max-trace","--max-results","--output","--domain","--max-bits","--max-denominator","--max-trace-bytes","--strategy","--max-power","--planner","--max-degree","--experience","--max-nodes","--equivalence","--domain-cache","--expression-planner","--operators","--grid-train","--grid-holdout","--grid-axis","--max-total-operations"});
            const bool grid_source=args.contains("--grid-train");
            if(args.contains("--max-total-operations") && !grid_source) throw Error("TotalBudgetRequiresGridTraining");
            if(args.contains("--grid-axis") && !grid_source) throw Error("GridAxisRequiresGridTraining");
            if(args.contains("--grid-holdout") && !grid_source) throw Error("GridHoldoutRequiresGridTraining");
            if(grid_source && (args.contains("--train") || args.contains("--holdout"))) throw Error("UseGridOrCsvSamples");
            const auto strategy=get(args,"--strategy","affine");
            if(args.contains("--domain-cache") && strategy!="expression" && strategy!="portfolio-expression") throw Error("DomainCacheRequiresModularExpression");
            const auto planner=get(args,"--planner",std::string(research_strategy(strategy).default_planner));
            validate_research_route(strategy,planner);
            if(strategy!="expression" && strategy!="portfolio-expression" && args.contains("--operators")) throw Error("OperatorsRequireExpressionStrategy");
            if(strategy!="portfolio-expression" && args.contains("--expression-planner")) throw Error("ExpressionPlannerRequiresFiveFamilyPortfolio");
            if(strategy!="expression" && strategy!="portfolio-expression" && args.contains("--equivalence")) throw Error("EquivalenceRequiresExpressionStrategy");
            if(strategy!="expression" && strategy!="portfolio-expression" && args.contains("--max-nodes")) throw Error("MaxNodesRequiresExpressionStrategy");
            if(strategy!="power-affine" && strategy!="portfolio" && strategy!="portfolio-rational" && strategy!="portfolio-quadratic" && strategy!="portfolio-expression" && args.contains("--max-power")) throw Error("MaxPowerRequiresPowerStrategy");
            if(strategy!="polynomial" && strategy!="portfolio" && strategy!="portfolio-rational" && strategy!="portfolio-quadratic" && strategy!="portfolio-expression" && args.contains("--max-degree")) throw Error("MaxDegreeRequiresPolynomialStrategy");
            const auto domain_name=get(args,"--domain",strategy=="expression" ? "rational" : (strategy=="rational-linear" || strategy=="rational-quadratic" || (strategy=="portfolio-rational" || strategy=="portfolio-quadratic" || strategy=="portfolio-expression")) ? "rational" : strategy!="affine" || grid_source ? "bigint" : "int64");
            if(grid_source && domain_name=="int64") throw Error("GridResearchRequiresExactDomain");
            if(strategy!="affine" && domain_name=="int64") throw Error("StrategyRequiresExactDomain");
            if(domain_name!="int64") {
                ExactResearchTask task;
                if(args.contains("--experience")) task.experience_snapshot=read_bounded(required(args,"--experience"),experience_byte_limit);
                task.domain=domain_name; task.strategy=strategy; task.planner=planner;
                task.max_nodes=integer<unsigned>(get(args,"--max-nodes","7"));
                task.expression_operators=get(args,"--operators","+-*/");
                task.expression_planner=get(args,"--expression-planner","grammar");
                task.domain_cache=get(args,"--domain-cache","none");
                task.expression_equivalence=get(args,"--equivalence",strategy=="portfolio-expression" ? "rational" : "syntax");
                task.max_degree=integer<unsigned>(get(args,"--max-degree","3"));
                task.max_power=integer<unsigned>(get(args,"--max-power",strategy=="power-affine" || strategy=="portfolio" || (strategy=="portfolio-rational" || strategy=="portfolio-quadratic" || strategy=="portfolio-expression") ? "4" : "1"));
                task.max_bits=integer<unsigned>(get(args,"--max-bits","4096"));
                task.max_denominator=integer<unsigned>(get(args,"--max-denominator",task.domain=="rational" && strategy!="expression" && strategy!="portfolio-expression" ? "8" : "1"));
                task.max_trace_bytes=integer<std::uint64_t>(get(args,"--max-trace-bytes","1048576"));
                const NumberDomain domain(task.domain,task.max_bits);
                if(!grid_source) {
                    task.training=read_exact_samples(required(args,"--train"),domain);
                    if(args.contains("--holdout")) task.holdout=read_exact_samples(required(args,"--holdout"),domain);
                }
                task.minimum=get(args,"--min",(strategy=="expression" || strategy=="portfolio-expression") ? "0" : "-8"); task.maximum=get(args,"--max",(strategy=="expression" || strategy=="portfolio-expression") ? "1" : "8");
                task.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
                task.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
                task.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
                task.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
                std::signal(SIGINT,on_interrupt); std::signal(SIGTERM,on_interrupt);
                if(grid_source) {
                    const auto training=read_bounded(required(args,"--grid-train"),grid_research_source_limit);
                    const auto holdout=args.contains("--grid-holdout") ? read_bounded(required(args,"--grid-holdout"),grid_research_source_limit) : "";
                    if(args.contains("--grid-holdout") && holdout.empty()) throw Error("EmptyGridHoldoutSource");
                    execute_cli(args,GridResearchTask{std::move(task),training,holdout,get(args,"--grid-axis","x"),
                        args.contains("--max-total-operations") ? std::optional<std::uint64_t>(integer<std::uint64_t>(required(args,"--max-total-operations"))) : std::nullopt},{[]{return interrupted!=0;}});
                    return 0;
                }
                execute_cli(args,std::move(task),{[]{return interrupted!=0;}});
                return 0;
            }
            if(args.contains("--experience")) throw Error("ExperienceRequiresRationalPortfolio");
            if(args.contains("--max-bits") || args.contains("--max-denominator") || args.contains("--max-trace-bytes"))
                throw Error("ExactOptionsRequireBigintOrRationalDomain");
            Task task;
            task.training=read_samples(required(args,"--train"));
            if(args.contains("--holdout")) task.holdout=read_samples(required(args,"--holdout"));
            task.minimum=integer<std::int64_t>(get(args,"--min","-8"));
            task.maximum=integer<std::int64_t>(get(args,"--max","8"));
            task.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","100000"));
            task.budget.operations=integer<std::uint64_t>(get(args,"--max-operations","1000000"));
            task.budget.trace_events=integer<std::uint64_t>(get(args,"--max-trace","100002"));
            task.budget.results=integer<std::uint64_t>(get(args,"--max-results","128"));
            std::signal(SIGINT,on_interrupt);
            std::signal(SIGTERM,on_interrupt);
            execute_cli(args,std::move(task),{[]{return interrupted!=0;}});
        } else if(command=="compatibility-plan" || command=="compatibility-apply") {
            if(command=="compatibility-apply" && argc<3)throw Error("Usage: compatibility-apply PLAN --from-run RUN --rules RULES [--output RUN]");
            const auto args=options(argc,argv,command=="compatibility-plan" ? 2:3,command=="compatibility-plan"
                ? std::set<std::string>{"--from-run","--rules","--rule-store","--at","--max-source-bytes","--max-candidates","--max-checks","--max-trace","--output","--max-transfer-checkpoints","--transfer-cancellation"}
                : std::set<std::string>{"--from-run","--rules","--rule-store","--output","--max-transfer-checkpoints","--transfer-cancellation"});
            TransferControl control;const auto checkpoint=transfer_checkpoint(args,control);
            if(checkpoint)checkpoint();
            if(args.contains("--rules") && args.contains("--rule-store"))throw Error("UseRulesOrRuleStore");
            const auto rules=args.contains("--rule-store") ? LocalFiles::active_rules(required(args,"--rule-store"),checkpoint):LocalFiles::read(required(args,"--rules"),1024*1024,checkpoint);
            if(command=="compatibility-plan") {
                const auto limit=integer<std::uint64_t>(get(args,"--max-source-bytes","33554432"));
                if(limit>max_file_bytes)throw Error("CompatibilityTransferInvalidBudget");
                ZBudget budget;
                budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","4096"));
                budget.checks=integer<std::uint64_t>(get(args,"--max-checks","1048576"));
                budget.trace=integer<std::uint64_t>(get(args,"--max-trace","65536"));
                const auto transfer=CompatibilityTransfer::plan(LocalFiles::read(required(args,"--from-run"),limit,checkpoint),rules,integer<std::int64_t>(required(args,"--at")),budget,limit,checkpoint);
                const auto bytes=transfer.bytes();const auto report=transfer.report();
                if(checkpoint)checkpoint();
                LocalFiles::publish_new(required(args,"--output"),bytes,checkpoint);std::cout<<controlled_transfer_report(report,control,bool(checkpoint))<<'\n';
            } else {
                const CompatibilityTransfer transfer(LocalFiles::read(argv[2],4096,checkpoint));
                const auto run=transfer.apply(LocalFiles::read(required(args,"--from-run"),transfer.max_source(),checkpoint),rules,checkpoint);
                const auto bundle=args.contains("--output") ? pack_compatibility(run,checkpoint):"";
                const auto report=transfer.result_report(run);
                if(checkpoint)checkpoint();
                if(args.contains("--output"))LocalFiles::publish_new(required(args,"--output"),bundle,checkpoint);
                std::cout<<controlled_transfer_report(report,control,bool(checkpoint))<<'\n';
            }
        } else if(command=="compatibility") {
            const auto args=options(argc,argv,2,{"--execution-cancellation","--serialization-cancellation","--verification","--verification-cancellation","--max-replay-bytes","--max-serialized-bytes","--max-session-checkpoints","--rules","--rule-store","--candidates","--from-run","--at","--max-candidates","--max-checks","--max-trace","--output"});
            if(args.contains("--candidates")==args.contains("--from-run")) throw Error("UseCandidatesOrResearchRun");
            CompatibilityTask task;
            if(args.contains("--rules") && args.contains("--rule-store"))throw Error("UseRulesOrRuleStore");
            task.rules=args.contains("--rule-store") ? active_rule_snapshot(required(args,"--rule-store")) : read_bounded(required(args,"--rules"),1024*1024);
            if(args.contains("--from-run")) {
                task.origin=read_bounded(required(args,"--from-run"));
                task.at=integer<std::int64_t>(required(args,"--at"));
            } else {
                if(args.contains("--at")) throw Error("AtRequiresResearchRun");
                task.candidates=read_bounded(required(args,"--candidates"),1024*1024);
            }
            task.budget.candidates=integer<std::uint64_t>(get(args,"--max-candidates","4096"));
            task.budget.checks=integer<std::uint64_t>(get(args,"--max-checks","1048576"));
            task.budget.trace=integer<std::uint64_t>(get(args,"--max-trace","65536"));
            std::signal(SIGINT,on_interrupt);
            std::signal(SIGTERM,on_interrupt);
            execute_cli(args,std::move(task),{[]{return interrupted!=0;}});
        } else if(command=="experience") {
            if(argc<3) throw Error("MissingExperienceAction");
            const std::string action=argv[2];
            if(action=="plan-add" || action=="apply-add") {
                if(action=="apply-add" && argc<4)throw Error("Usage: experience apply-add PLAN --from-run RUN [--store STORE] --output NEW");
                const auto args=options(argc,argv,action=="plan-add" ? 3:4,action=="plan-add"
                    ? std::set<std::string>{"--from-run","--store","--max-source-bytes","--max-store-bytes","--output","--max-transfer-checkpoints","--transfer-cancellation"}
                    : std::set<std::string>{"--from-run","--store","--output","--max-transfer-checkpoints","--transfer-cancellation"});
                if(action=="plan-add") {
                    TransferControl control;const auto checkpoint=transfer_checkpoint(args,control);
                    const auto source_limit=integer<std::uint64_t>(get(args,"--max-source-bytes","1048576"));
                    const auto store_limit=integer<std::uint64_t>(get(args,"--max-store-bytes","1048576"));
                    if(source_limit>1048576 || store_limit>1048576)throw Error("ExperienceTransferInvalidBudget");
                    const auto source=LocalFiles::read(required(args,"--from-run"),source_limit,checkpoint);
                    const auto previous=args.contains("--store") ? LocalFiles::read(required(args,"--store"),store_limit,checkpoint):"";
                    const auto transfer=ExperienceTransfer::plan(source,previous,source_limit,store_limit,checkpoint);
                    const auto bytes=transfer.bytes();const auto report=transfer.report();
                    if(checkpoint)checkpoint();
                    LocalFiles::publish_new(required(args,"--output"),bytes,checkpoint);
                    std::cout<<controlled_transfer_report(report,control,bool(checkpoint))<<'\n';
                } else {
                    TransferControl control;const auto checkpoint=transfer_checkpoint(args,control);
                    const ExperienceTransfer transfer(LocalFiles::read(argv[3],4096,checkpoint));
                    const auto source=LocalFiles::read(required(args,"--from-run"),transfer.max_source(),checkpoint);
                    const auto previous=args.contains("--store") ? LocalFiles::read(required(args,"--store"),transfer.max_previous(),checkpoint):"";
                    const auto result=transfer.apply(source,previous,checkpoint);
                    const auto report=transfer.result_report(result);
                    if(checkpoint)checkpoint();
                    LocalFiles::publish_new(required(args,"--output"),result.snapshot,checkpoint);
                    std::cout<<controlled_transfer_report(report,control,bool(checkpoint))<<'\n';
                }
            } else if(action=="inspect") {
                if(argc<4) throw Error("Usage: bruce experience inspect SNAPSHOT");
                auto management=parse_management_inspection_options(
                    argc,argv,4,"Usage: bruce experience inspect SNAPSHOT");
                const auto checkpoint=management.checkpoint();
                const auto snapshot=checkpoint ? LocalFiles::read(argv[3],experience_byte_limit,checkpoint) :
                    read_bounded(argv[3],experience_byte_limit);
                const auto result=execute_experience(ExperienceInspectTask{snapshot},checkpoint);
                std::cout << management.render(result.report) << '\n';
            } else if(action=="add") {
                const auto args=options(argc,argv,3,{"--store","--from-run","--output","--max-transfer-checkpoints","--transfer-cancellation"});
                TransferControl control;const auto checkpoint=transfer_checkpoint(args,control);
                if(checkpoint) {
                    const auto previous=args.contains("--store") ? LocalFiles::read(required(args,"--store"),experience_byte_limit,checkpoint) : "";
                    const auto source=LocalFiles::read(required(args,"--from-run"),experience_byte_limit,checkpoint);
                    const auto result=execute_experience(ExperienceTask{ExperienceAddTask{previous,source}},checkpoint);
                    LocalFiles::publish_new(required(args,"--output"),result.snapshot,checkpoint);
                    std::cout<<controlled_transfer_report(result.report,control,true)<<'\n';
                } else {
                    const auto previous=args.contains("--store") ? read_bounded(required(args,"--store"),experience_byte_limit) : "";
                    const auto result=execute_experience(ExperienceTask{ExperienceAddTask{previous,read_bounded(required(args,"--from-run"),experience_byte_limit)}});
                    export_exclusive(required(args,"--output"),result.snapshot);
                    std::cout<<result.report<<'\n';
                }
            } else if(action=="revoke" || action=="replace") {
                const auto args=options(argc,argv,3,action=="replace" ? std::set<std::string>{"--store","--id","--reason","--from-run","--output","--max-transfer-checkpoints","--transfer-cancellation"} :
                    std::set<std::string>{"--store","--id","--reason","--output","--max-transfer-checkpoints","--transfer-cancellation"});
                TransferControl control;const auto checkpoint=transfer_checkpoint(args,control);
                if(checkpoint) {
                    const auto previous=args.contains("--store") ? LocalFiles::read(required(args,"--store"),experience_byte_limit,checkpoint) : "";
                    const ExperienceTask task=action=="replace"
                        ? ExperienceTask{ExperienceReplaceTask{previous,required(args,"--id"),required(args,"--reason"),LocalFiles::read(required(args,"--from-run"),experience_byte_limit,checkpoint)}}
                        : ExperienceTask{ExperienceRevokeTask{previous,required(args,"--id"),required(args,"--reason")}};
                    const auto result=execute_experience(task,checkpoint);
                    LocalFiles::publish_new(required(args,"--output"),result.snapshot,checkpoint);
                    std::cout<<controlled_transfer_report(result.report,control,true)<<'\n';
                } else {
                    const auto previous=args.contains("--store") ? read_bounded(required(args,"--store"),experience_byte_limit) : "";
                    const ExperienceTask task=action=="replace"
                        ? ExperienceTask{ExperienceReplaceTask{previous,required(args,"--id"),required(args,"--reason"),read_bounded(required(args,"--from-run"),experience_byte_limit)}}
                        : ExperienceTask{ExperienceRevokeTask{previous,required(args,"--id"),required(args,"--reason")}};
                    const auto result=execute_experience(task);
                    export_exclusive(required(args,"--output"),result.snapshot);
                    std::cout << result.report << '\n';
                }
            } else throw Error("UnknownExperienceAction");
        } else if(command=="gate-run") {
            if(argc<3) throw Error("Usage: bruce gate-run RUN.bruce [--max-session-checkpoints N] [--max-replay-bytes N] [--cancellation none|checkpoints]");
            const auto args=options(argc,argv,3,{"--max-session-checkpoints","--max-replay-bytes","--cancellation"});
            SessionUsage usage;
            ReplayCheckpoint checkpoint;
            if(args.contains("--max-session-checkpoints")) {
                usage.max_checkpoints=integer<std::uint64_t>(required(args,"--max-session-checkpoints"));
            }
            const auto cancellation=get(args,"--cancellation","none");
            if(cancellation!="none" && cancellation!="checkpoints") throw Error("UnknownGateCancellation");
            if(cancellation=="checkpoints") {
                std::signal(SIGINT,on_interrupt);std::signal(SIGTERM,on_interrupt);
            }
            if(usage.max_checkpoints || cancellation=="checkpoints") {
                checkpoint=[&]{
                    if(cancellation=="checkpoints" && interrupted) throw Error("GateInspectionCancelled");
                    usage.consume(ExecutionPhase::replaying);
                };
            }
            std::size_t input_limit=max_file_bytes;
            if(args.contains("--max-replay-bytes")) {
                const auto requested=integer<std::uint64_t>(required(args,"--max-replay-bytes"));
                if(requested<input_limit) input_limit=static_cast<std::size_t>(requested);
            }
            const auto report=render_application_gates(evaluate_application_gates(read_bounded(argv[2],input_limit),checkpoint));
            if(usage.max_checkpoints)
                std::cout<<"{\"contract\":\"application-gates-budget-v1\",\"result\":"<<report<<",\"usage\":"<<render_session_usage(usage)<<"}\n";
            else std::cout<<report<<'\n';
        } else if(command=="assess-run") {
            if(argc!=3) throw Error("Usage: bruce assess-run RUN.bruce");
            std::cout<<assess_application_bundle(read_bounded(argv[2]))<<'\n';
        } else if(command=="inspect-run") {
            if(argc!=3) throw Error("Usage: bruce inspect-run RUN.bruce");
            std::cout<<inspect_application(read_bounded(argv[2]))<<'\n';
        } else if(command=="replay") {
            if(argc!=3) throw Error("Usage: bruce replay RUN.bruce");
            std::cout << replay_output(read_bounded(argv[2])) << '\n';
        } else throw Error("UnknownCommand: "+command);
    } catch(const bruce::ExecutionSessionError& error) {
        std::cerr << bruce::render_execution_failure(error) << '\n';
        return 2;
    } catch(const std::exception& error) {
        std::cerr << "{\"error\":" << json_string(error.what()) << "}\n";
        return 2;
    }
}
