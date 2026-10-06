#include "bruce/evidence/chain.hpp"
#include "bruce/evidence/bundle.hpp"
#include <iostream>

namespace {
using namespace bruce;

void require(bool condition,const char* message) {
    if(!condition) throw Error(message);
}
template<class F> void rejects(F&& action,const char* message) {
    try { action(); } catch(const Error&) { return; }
    throw Error(message);
}
template<class F> void rejects_with(F&& action,std::string_view diagnostic) {
    try { action(); } catch(const Error& error) {
        require(std::string_view(error.what())==diagnostic,"Unexpected rejection path");
        return;
    }
    throw Error("Malformed chain was accepted");
}
ExactResearchTask rational_task() {
    ExactResearchTask task;
    task.domain="rational";
    task.minimum="0"; task.maximum="1";
    task.max_denominator=2;
    return task;
}
std::string rehash(std::string bundle) {
    const auto line=bundle.find('\n');
    require(line!=std::string::npos && bundle.size()>=line+66,"Malformed test envelope");
    bundle.replace(line+1,64,sha256(std::string_view(bundle).substr(line+66)));
    return bundle;
}
void replace_once(std::string& text,std::string_view old_text,std::string_view new_text) {
    const auto where=text.find(old_text);
    require(where!=std::string::npos,"Test mutation target absent");
    text.replace(where,old_text.size(),new_text);
}
std::string replace_native(std::string bundle,std::string_view native) {
    const auto field=bundle.find("native_bundle_bytes ");
    require(field!=std::string::npos,"Native size field absent");
    const auto end=bundle.find('\n',field);
    require(end!=std::string::npos,"Native size field truncated");
    const auto count=integer<std::size_t>(std::string_view(bundle).substr(field+20,end-field-20));
    bundle.replace(field,end+1+count-field,
                   "native_bundle_bytes "+std::to_string(native.size())+"\n"+std::string(native));
    return rehash(std::move(bundle));
}
}

int main() {
    using namespace bruce;
    try {
        constexpr std::string_view train="x,y\n0,1/2\n1,3/2\n";
        constexpr std::string_view holdout="x,y\n2,5/2\n";
        auto run=create_exact_evidence_chain(rational_task(),std::string(train),std::string(holdout));
        require(run.research.status=="VERIFIED_ON_SAMPLES","Rational evidence did not verify on samples");
        const auto rendered=render_exact_evidence_chain(run);
        require(rendered.find("\"general_proof\":false")!=std::string::npos,"Nested result overclaims proof");
        const auto bundle=pack_exact_evidence_chain(run);
        require(bundle.starts_with("BRUCE_EVIDENCE_CHAIN_V1\n"),"Chain header changed");
        for(std::string_view literal:{"contract affine-evidence-chain-v1\n","independence declared-disjoint-v1\n",
              "countercheck_policy affine-inverse-v1\n",
              "countercheck_scope short-circuit-training-then-holdout-v1\n",
              "BRUCE_EXACT_BUNDLE_V1\n"})
            require(bundle.find(literal)!=std::string::npos,"Required policy or native bundle missing");
        require(bundle.find("contract affine-evidence-chain-v1\n") < bundle.find("independence declared-disjoint-v1\n") &&
                bundle.find("independence declared-disjoint-v1\n") < bundle.find("countercheck_policy affine-inverse-v1\n") &&
                bundle.find("countercheck_policy affine-inverse-v1\n") < bundle.find("countercheck_scope short-circuit-training-then-holdout-v1\n") &&
                bundle.find("countercheck_scope short-circuit-training-then-holdout-v1\n") < bundle.find("training_csv_bytes ") &&
                bundle.find("training_csv_bytes ") < bundle.find("holdout_csv_bytes ") &&
                bundle.find("holdout_csv_bytes ") < bundle.find("native_bundle_bytes "),
                "Policy field order changed");
        run.training_csv.clear();
        run.holdout_csv.clear();
        const auto replayed=replay_exact_evidence_chain(bundle);
        require(render_exact_evidence_chain(replayed)==rendered && pack_exact_evidence_chain(replayed)==bundle,
                "Rational chain did not replay byte-exactly after source strings expired");

        auto overlap=rational_task();
        rejects([&] { create_exact_evidence_chain(overlap,std::string(train),"x,y\n1,3/2\n"); },
                "Overlapping training and holdout input accepted");
        auto wrong_strategy=rational_task(); wrong_strategy.strategy="power-affine";
        rejects([&] { create_exact_evidence_chain(wrong_strategy,std::string(train),std::string(holdout)); },
                "Non-affine strategy accepted");
        auto wrong_planner=rational_task(); wrong_planner.planner="adaptive";
        rejects([&] { create_exact_evidence_chain(wrong_planner,std::string(train),std::string(holdout)); },
                "Non-enumeration planner accepted");
        auto wrong_domain=rational_task(); wrong_domain.domain="modular";
        rejects([&] { create_exact_evidence_chain(wrong_domain,std::string(train),std::string(holdout)); },
                "Unsupported domain accepted");
        auto mismatch=rational_task(); mismatch.training={{0,0}};
        rejects([&] { create_exact_evidence_chain(mismatch,std::string(train),std::string(holdout)); },
                "Task samples mismatching raw CSV accepted");

        auto changed=bundle;
        replace_once(changed,"countercheck_policy affine-inverse-v1","countercheck_policy affine-faster-v1");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"UnknownEvidenceChainPolicy");
        changed=bundle;
        replace_once(changed,"countercheck_scope short-circuit-training-then-holdout-v1",
                     "countercheck_scope short-circuit-training-then-holdout-v2");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"UnknownEvidenceChainPolicy");
        changed=bundle;
        replace_once(changed,"independence declared-disjoint-v1","independence externally-proven-v1");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"UnknownEvidenceChainPolicy");
        changed=bundle;
        replace_once(changed,"0,1/2","0,3/2");
        rejects([&] { replay_exact_evidence_chain(rehash(changed)); },"Mismatched embedded CSV accepted");
        changed=bundle;
        replace_once(changed,"\"general_proof\":false","\"general_proof\":true ");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },
                     "InvalidEvidence: digest mismatch"); // Inner digest is intentionally stale.

        auto native=pack_exact_research(replayed.research);
        replace_once(native,"\"general_proof\":false","\"general_proof\":true ");
        native=rehash(std::move(native));
        changed=replace_native(bundle,native); // Valid inner and outer digests.
        rejects_with([&] { replay_exact_evidence_chain(changed); },
                     "ReplayMismatch: exact input, result or trace differs");

        auto non_affine=rational_task();
        non_affine.strategy="power-affine";
        non_affine.max_power=2;
        non_affine.training=replayed.research.task.training;
        non_affine.holdout=replayed.research.task.holdout;
        const auto power_run=exact_research(non_affine);
        const auto power_bundle=pack_exact_research(power_run);
        require(render_exact_research(replay_exact_research(power_bundle))==render_exact_research(power_run),
                "Non-affine fixture is not a valid replayable native envelope");
        changed=replace_native(bundle,power_bundle);
        rejects_with([&] { replay_exact_evidence_chain(changed); },
                     "EvidenceChainRequiresExactNativeBundle");

        // Rehashed framing cases must reach the Chain parser, not fail its digest.
        const std::string training_field="training_csv_bytes "+std::to_string(train.size())+"\n";
        changed=bundle;
        replace_once(changed,training_field,"training_csv_bytes not-a-number\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidInteger: not-a-number");
        changed=bundle;
        replace_once(changed,training_field,"training_csv_bytes "+std::to_string(train.size()-1)+"\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainField");
        changed=bundle;
        replace_once(changed,"independence declared-disjoint-v1\n","");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainField");
        changed=bundle;
        replace_once(changed,"independence declared-disjoint-v1\n",
                     "independence declared-disjoint-v1\nindependence declared-disjoint-v1\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainField");
        changed=bundle;
        replace_once(changed,"independence declared-disjoint-v1\ncountercheck_policy affine-inverse-v1\n",
                     "countercheck_policy affine-inverse-v1\nindependence declared-disjoint-v1\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainField");
        changed=bundle;
        replace_once(changed,training_field,"training_csv_bytes 1048577\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainBlockSize");
        changed=bundle;
        replace_once(changed,"native_bundle_bytes "+std::to_string(pack_exact_research(replayed.research).size())+"\n",
                     "native_bundle_bytes "+std::to_string(max_file_bytes+1)+"\n");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },"InvalidEvidenceChainBlockSize");
        changed=bundle;
        changed.resize(max_file_bytes+1,'x');
        changed=rehash(std::move(changed));
        rejects_with([&] { replay_exact_evidence_chain(changed); },"BundleByteLimit");
        changed=bundle;
        const auto last_proof=changed.rfind("\"general_proof\":false");
        require(last_proof!=std::string::npos,"Outer result mutation target absent");
        changed.replace(last_proof,sizeof("\"general_proof\":false")-1,"\"general_proof\":true ");
        rejects_with([&] { replay_exact_evidence_chain(rehash(changed)); },
                     "ReplayMismatch: evidence chain differs");
        rejects([&] { replay_exact_evidence_chain(bundle+"trailing"); },"Trailing bytes accepted");
        rejects_with([&] { replay_exact_evidence_chain(rehash(bundle+"trailing")); },
                     "ReplayMismatch: evidence chain differs");

        auto short_budget=rational_task(); short_budget.budget.operations=1;
        const auto incomplete=create_exact_evidence_chain(short_budget,std::string(train),std::string(holdout));
        require(incomplete.research.status=="INCOMPLETE","Short budget did not retain incomplete status");
        const auto incomplete_replayed=replay_exact_evidence_chain(pack_exact_evidence_chain(incomplete));
        require(incomplete_replayed.research.status=="INCOMPLETE" &&
                incomplete_replayed.research.stop_reason==incomplete.research.stop_reason,
                "Incomplete budget status or reason changed on replay");
        std::cout << "Evidence chain contracts passed\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
