#include "bruce/application/application_task.hpp"
#include "bruce/application/research_gates.hpp"
#include "bruce/application/grid.hpp"
#include "bruce/application/grid_batch.hpp"
#include "bruce/application/grid_resume.hpp"
#include "bruce/application/grid_research.hpp"
#include "bruce/application/fraction_compare.hpp"
#include "bruce/application/compatibility.hpp"
#include "bruce/application/research.hpp"
#include "bruce/application/exact_research.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/math/exact.hpp"
#include "bruce/math/power_affine.hpp"
#include "bruce/math/polynomial.hpp"
#include "bruce/math/rational_linear.hpp"
#include <iomanip>
#include <sstream>

namespace bruce {
namespace {
constexpr std::string_view version="z-compatibility-v1";
constexpr std::size_t serialization_block_size=64*1024;
void append_chunked(std::ostringstream& out,std::string_view text,const ReplayCheckpoint& records) {
    for(std::size_t offset=0;offset<text.size();) {
        if(records) records();
        const auto count=std::min(serialization_block_size,text.size()-offset);
        out.write(text.data()+offset,static_cast<std::streamsize>(count));
        offset+=count;
    }
}
std::string hex_mask(std::uint64_t mask) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setfill('0') << std::setw(16) << mask;
    return out.str();
}
std::string task_payload(const CompatibilityRun& run,const ReplayCheckpoint& records={}) {
    const auto& t=run.task;
    std::ostringstream out;
    out << "engine " << version << "\nsnapshot_digest " << run.snapshot_digest
        << "\ncandidates_budget " << t.budget.candidates << "\nchecks_budget " << t.budget.checks
        << "\ntrace_budget " << t.budget.trace << "\ncancel "
        << (t.budget.cancel_after_candidates ? std::to_string(*t.budget.cancel_after_candidates) : "none")
        << "\nat " << t.at << '\n';
    auto block=[&](std::string_view label,const std::string& text) {
        out << label << ' ' << text.size() << '\n';
        append_chunked(out,text,records);
    };
    block("rules_bytes",t.rules); block("candidate_bytes",t.candidates); block("origin_bytes",t.origin);
    out << "RESULT\n" << render_compatibility(run) << '\n';
    return out.str();
}
class BundleReader {
public:
    explicit BundleReader(std::string_view data):data_(data) {}
    std::string field(std::string_view key) {
        const auto newline=data_.find('\n');
        if(newline==std::string_view::npos) throw Error("TruncatedCompatibilityBundle");
        const auto line=data_.substr(0,newline);
        const std::string prefix=std::string(key)+" ";
        if(!line.starts_with(prefix)) throw Error("InvalidCompatibilityBundleField");
        data_.remove_prefix(newline+1);
        return std::string(line.substr(prefix.size()));
    }
    std::string block(std::string_view key,std::size_t limit) {
        const auto count=integer<std::size_t>(field(key));
        if(count>limit || count>data_.size()) throw Error("InvalidBundleBlockSize");
        const auto result=std::string(data_.substr(0,count));
        data_.remove_prefix(count);
        return result;
    }
private:
    std::string_view data_;
};
}

static CompatibilityRun run_compatibility_impl(CompatibilityTask task,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& rules_checkpoint={}) {
    if(checkpoint) checkpoint();
    const CompatibilitySnapshot snapshot(task.rules,rules_checkpoint);
    task.rules=snapshot.canonical(rules_checkpoint);
    std::string origin_status;
    std::vector<ZCandidate> candidates;
    if(!task.origin.empty()) {
        const bool grid_origin=task.origin.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V2\n") || (task.origin.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V3\n") || task.origin.starts_with("BRUCE_GRID_RESEARCH_BUNDLE_V4\n"));
        if(grid_origin && (task.at<0 || task.at>7)) throw Error("GridProjectionInputOutOfRange");
        if(grid_origin || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V32\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V33\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V34\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V10\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V29\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V30\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V31\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V9\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V26\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V27\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V28\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V8\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V23\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V24\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V25\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V7\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V20\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V21\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V22\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V17\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V18\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V19\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V14\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V15\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V16\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V11\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V12\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V13\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V6\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V5\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V4\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V3\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V10\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V9\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V8\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_EXPRESSION_BUNDLE_V2\n") || task.origin.starts_with("BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_RATIONAL_LINEAR_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_EXACT_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_POWER_BUNDLE_V1\n") ||
           task.origin.starts_with("BRUCE_ADAPTIVE_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_POLYNOMIAL_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V1\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V2\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V3\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V4\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V5\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V6\n") || task.origin.starts_with("BRUCE_PORTFOLIO_BUNDLE_V7\n")) {
            const auto origin=grid_origin ? replay_grid_research(task.origin,10000000,{},checkpoint).research : replay_exact_research(task.origin,10000000,{},checkpoint);
            origin_status=origin.status;
            if(!research_gates({origin.status,origin.stop_reason}).selection) throw Error("IncompleteResearchCannotBeRanked");
            const NumberDomain domain(origin.task.domain,origin.task.max_bits);
            const auto x=domain.parse(std::to_string(task.at));
            unsigned operator_mask=0;
            for(char op:origin.task.expression_operators) operator_mask|=1u<<std::string_view("+-*/^").find(op);
            for(const auto& h:origin.hypotheses) {
                if(checkpoint) checkpoint();
                if(h.status=="REFUTED" || h.status=="INCOMPLETE") continue;
                const bool five=origin.task.strategy=="portfolio-expression";
                const bool nonlinear=origin.task.strategy=="power-affine";
                const bool four=origin.task.strategy=="portfolio-quadratic";
                const bool extended=four || origin.task.strategy=="portfolio-rational";
                const bool portfolio=extended || origin.task.strategy=="portfolio";
                const bool polynomial=portfolio || origin.task.strategy=="polynomial";
                const bool fraction=!h.denominator.empty();
                const bool quadratic=h.denominator.size()==3;
                const bool expression=origin.task.strategy=="expression";
                const auto value=five ? evaluate_normal_form(domain,*h.normal_form,x,[](std::uint64_t){}) : expression ? evaluate_expression(domain,h.expression,x,[](std::uint64_t){}) : quadratic ? rational_quadratic(domain,h.a,h.b,h.denominator[1],h.denominator[0],x) : fraction ? rational_linear(domain,h.a,h.b,h.denominator[0],x) : polynomial ? polynomial_horner(domain,h.coefficients,x) :
                    nonlinear ? power_affine(domain,h.a,h.b,x,h.power) : exact_affine(domain,h.a,h.b,x);
                if(value.get_den()!=1) throw Error("CompatibilityProjectionRequiresInt64: fractional value");
                std::int64_t narrow;
                try { narrow=integer<std::int64_t>(exact_text(value)); }
                catch(const Error&) { throw Error("CompatibilityProjectionRequiresInt64: out of range"); }
                candidates.push_back({h.id,0,h.a_index,h.b_index,0,narrow,(five && origin.task.expression_equivalence=="rational-q-witness" ? std::string(origin.task.planner=="experience-order" ? "portfolio-q-witness-v34-ops" : origin.task.planner=="adaptive-order" ? "portfolio-q-witness-v33-ops" : "portfolio-q-witness-v32-ops")+std::to_string(operator_mask)+"-"+origin.task.expression_planner+"-" : five && origin.task.domain_cache=="equal-64" ? std::string(origin.task.planner=="experience-order" ? "portfolio-q-cached-v31-ops" : origin.task.planner=="adaptive-order" ? "portfolio-q-cached-v30-ops" : "portfolio-q-cached-v29-ops")+std::to_string(operator_mask)+"-"+origin.task.expression_planner+"-" : five && origin.task.expression_equivalence=="rational-q-modular" ? std::string(origin.task.planner=="experience-order" ? "portfolio-q-modular-v28-ops" : origin.task.planner=="adaptive-order" ? "portfolio-q-modular-v27-ops" : "portfolio-q-modular-v26-ops")+std::to_string(operator_mask)+"-"+origin.task.expression_planner+"-" : five && origin.task.expression_equivalence=="rational-q" ? std::string(origin.task.planner=="experience-order" ? "portfolio-q-v25-ops" : origin.task.planner=="adaptive-order" ? "portfolio-q-v24-ops" : "portfolio-q-v23-ops")+std::to_string(operator_mask)+"-"+origin.task.expression_planner+"-" : five && origin.task.expression_planner=="cached-grammar" ? std::string(origin.task.planner=="experience-order" ? "portfolio-v22-ops" : origin.task.planner=="adaptive-order" ? "portfolio-v21-ops" : "portfolio-v20-ops")+std::to_string(operator_mask)+"-cached-" : five && origin.task.expression_operators.find('^')!=std::string::npos ? std::string(origin.task.planner=="experience-order" ? "portfolio-v19-ops" : origin.task.planner=="adaptive-order" ? "portfolio-v18-ops" : "portfolio-v17-ops")+std::to_string(operator_mask)+"-"+(origin.task.expression_planner=="indexed-grammar" ? "indexed-" : "grammar-") : five && origin.task.expression_operators!="+-*/" ? std::string(origin.task.planner=="experience-order" ? "portfolio-v16-ops" : origin.task.planner=="adaptive-order" ? "portfolio-v15-ops" : "portfolio-v14-ops")+std::to_string(operator_mask)+"-"+(origin.task.expression_planner=="indexed-grammar" ? "indexed-" : "grammar-") : five && origin.task.expression_planner=="indexed-grammar" ? (origin.task.planner=="experience-order" ? "portfolio-v13-" : origin.task.planner=="adaptive-order" ? "portfolio-v12-" : "portfolio-v11-") : five ? (origin.task.planner=="experience-order" ? "portfolio-v10-" : origin.task.planner=="adaptive-order" ? "portfolio-v9-" : "portfolio-v8-") : expression && origin.task.expression_equivalence=="rational-q-witness" ? std::string("expression-q-witness-v10-")+std::to_string(operator_mask)+"-"+origin.task.planner+"-" : expression && origin.task.domain_cache=="equal-64" ? std::string("expression-q-cached-v9-")+std::to_string(operator_mask)+"-"+origin.task.planner+"-" : expression && origin.task.expression_equivalence=="rational-q-modular" ? std::string("expression-q-modular-")+std::to_string(operator_mask)+"-"+origin.task.planner+"-" : expression && origin.task.expression_equivalence=="rational-q" ? std::string("expression-q-")+std::to_string(operator_mask)+"-"+origin.task.planner+"-" : expression && origin.task.planner=="cached-grammar" ? std::string("expression-cached-")+std::to_string(operator_mask)+"-"+(origin.task.expression_equivalence=="rational" ? "normalized-" : "") : expression && origin.task.expression_operators!="+-*/" ? std::string(origin.task.expression_operators.find('^')!=std::string::npos ? "expression-power-" : "expression-selected-")+std::to_string(operator_mask)+"-"+(origin.task.planner=="indexed-grammar" ? "indexed-" : "")+(origin.task.expression_equivalence=="rational" ? "normalized-" : "") : expression ? (origin.task.planner=="indexed-grammar" ? (origin.task.expression_equivalence=="rational" ? "expression-indexed-normalized-" : "expression-indexed-") : origin.task.expression_equivalence=="rational" ? "expression-normalized-" : "expression-") : four ? (origin.task.planner=="experience-order" ? "portfolio-v7-" : origin.task.planner=="adaptive-order" ? "portfolio-v6-" : "portfolio-v5-") : quadratic ? "quadratic-fraction-" : origin.task.planner=="experience-order" ? "portfolio-v4-" : origin.task.planner=="adaptive-order" ? "portfolio-v3-" : extended ? "portfolio-v2-" : fraction ? "fraction-" : portfolio ? "portfolio-" : polynomial ? "polynomial-" : nonlinear ? "power-" : "affine-")+origin.task.domain+"-v1"});
            }
        } else {
            const auto origin=replay(task.origin,checkpoint);
            origin_status=origin.status;
            if(!research_gates({origin.status,origin.stop_reason}).selection) throw Error("IncompleteResearchCannotBeRanked");
            for(const auto& h:origin.hypotheses) {
                if(checkpoint) checkpoint();
                if(h.status=="REFUTED" || h.status=="INCOMPLETE") continue;
                candidates.push_back({h.id,0,static_cast<std::uint64_t>(h.a-origin.task.minimum),
                    static_cast<std::uint64_t>(h.b-origin.task.minimum),0,affine(h.a,h.b,task.at),"affine-v1"});
            }
        }
        if(candidates.empty()) throw Error("NoResearchCandidatesToRank");
        const auto projected=canonical_candidates(candidates);
        if(!task.candidates.empty() && task.candidates!=projected) throw Error("ResearchProjectionMismatch");
        task.candidates=projected;
    } else {
        if(task.at!=0) throw Error("ProjectionRequiresResearchOrigin");
        candidates=parse_candidates(task.candidates);
        task.candidates=canonical_candidates(candidates);
    }
    if(checkpoint) checkpoint();
    auto result=evaluate_compatibility(snapshot,std::move(candidates),task.budget,cancelled,rules_checkpoint);
    task.budget=result.budget;
    return {std::move(task),sha256(snapshot.canonical(rules_checkpoint),rules_checkpoint),origin_status,std::move(result)};
}

CompatibilityRun run_compatibility(CompatibilityTask task,const std::function<bool(std::uint64_t)>& cancelled,const ReplayCheckpoint& source_checkpoint,const ReplayCheckpoint& rules_checkpoint) {
    if(source_checkpoint && task.origin.empty())throw Error("ExecutionCompatibilitySourceRequiresOrigin");
    return run_compatibility_impl(std::move(task),cancelled,source_checkpoint,rules_checkpoint);
}

std::string render_compatibility(const CompatibilityRun& run) {
    const auto& r=run.result;
    std::ostringstream out;
    out << "{\"engine\":" << json_string(version) << ",\"snapshot_digest\":" << json_string(run.snapshot_digest)
        << ",\"snapshot_id\":" << json_string(CompatibilitySnapshot(run.task.rules).id())
        << ",\"input_fingerprint\":" << json_string(sha256(run.task.candidates))
        << ",\"origin_digest\":" << json_string(run.task.origin.empty() ? "" : sha256(run.task.origin))
        << ",\"origin_status\":" << json_string(run.origin_status)
        << ",\"projection_at\":" << json_string(std::to_string(run.task.at))
        << ",\"status\":" << json_string(r.status) << ",\"stop_reason\":" << json_string(r.stop_reason)
        << ",\"general_proof\":false,\"learn_gate\":false,\"publish_gate\":false"
        << ",\"explanation\":" << json_string(r.status=="INCOMPLETE" ? "Pruefung unvollstaendig; keine abschliessende Auswahl." :
            r.status=="AMBIGUOUS" ? "Mehrere zulaessige Kandidaten haben denselben besten Score. IDs ordnen nur die Anzeige." :
            r.status=="SELECTED" ? "Ein Kandidat hat den besten Score unter den zulaessigen Kandidaten. Auswahl ist kein Wahrheitsbeweis." :
            "Alle Kandidaten sind durch die angegebenen Regeln ausgeschlossen.")
        << ",\"candidates_started\":" << r.started << ",\"checks_used\":" << r.checks << ",\"rules\":[";
    bool first=true;
    const CompatibilitySnapshot snapshot(run.task.rules);
    for(const auto& rule:snapshot.rules()) {
        if(!first) out<<',';
        first=false;
        out << "{\"id\":" << json_string(rule.id) << ",\"kind\":" << json_string(rule.kind)
            << ",\"field\":" << json_string(rule.field) << ",\"operator\":" << json_string(rule.operation)
            << ",\"argument\":" << json_string(std::to_string(rule.argument))
            << ",\"remainder\":" << json_string(std::to_string(rule.remainder))
            << ",\"weight\":" << rule.weight << ",\"source\":\"manual\",\"evidence_ref\":" << json_string(rule.evidence) << '}';
    }
    out << "],\"selected_ids\":[";
    first=true;
    for(auto id:r.selection.ids) { if(!first) out<<','; first=false; out<<json_string(std::to_string(id)); }
    out << "],\"candidates\":[";
    first=true;
    for(const auto& a:r.candidates) {
        if(!first) out<<',';
        first=false;
        const auto& c=a.candidate;
        out << "{\"id\":" << json_string(std::to_string(c.id)) << ",\"domain\":" << json_string(c.domain)
            << ",\"layer\":" << json_string(std::to_string(c.layer)) << ",\"x\":" << json_string(std::to_string(c.x))
            << ",\"y\":" << json_string(std::to_string(c.y)) << ",\"z\":" << json_string(std::to_string(c.z))
            << ",\"value\":" << json_string(std::to_string(c.value)) << ",\"score\":" << a.score
            << ",\"rules_checked\":" << a.rules_checked << ",\"complete\":" << (a.complete ? "true" : "false")
            << ",\"hard_rejected\":" << (a.hard_rejected ? "true" : "false")
            << ",\"negative_evidence\":" << (a.negative ? "true" : "false")
            << ",\"context_matches\":" << (a.complete ? (a.context_matches ? "true" : "false") : "null") << '}';
    }
    out << "],\"groups\":[";
    first=true;
    for(std::size_t i=0;i<r.groups.size();++i) {
        if(!first) out<<',';
        first=false;
        const auto& g=r.groups[i];
        out << "{\"group\":" << i << ",\"candidate_ids\":[";
        for(std::size_t j=0;j<g.candidate_ids.size();++j) {
            if(j) out<<',';
            out << json_string(std::to_string(g.candidate_ids[j]));
        }
        out << "],\"positive_match_mask\":" << json_string(hex_mask(g.positive_match_mask))
            << ",\"hard_reject_mask\":" << json_string(hex_mask(g.hard_reject_mask))
            << ",\"negative_evidence_mask\":" << json_string(hex_mask(g.negative_evidence_mask))
            << ",\"context_role_mask\":" << json_string(hex_mask(g.context_role_mask))
            << ",\"complete_mask\":" << json_string(hex_mask(g.complete_mask))
            << ",\"eligible_mask\":" << json_string(hex_mask(g.eligible_mask)) << '}';
    }
    out << "],\"trace\":[";
    first=true;
    for(const auto& entry:r.trace) {
        if(!first) out<<',';
        first=false;
        out << "{\"candidate_id\":" << json_string(std::to_string(entry.candidate_id))
            << ",\"rule_id\":" << json_string(entry.rule_id) << ",\"matched\":" << (entry.matched ? "true" : "false")
            << ",\"effect\":" << json_string(entry.effect) << '}';
    }
    out << "]}";
    return out.str();
}

std::string pack_compatibility_records(const CompatibilityRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    const auto payload=task_payload(run,records);
    const auto result="BRUCE_COMPATIBILITY_BUNDLE_V1\n"+sha256(payload,checkpoint)+"\n"+payload;
    if(result.size()>max_file_bytes) throw Error("BundleByteLimit");
    return result;
}
std::string pack_compatibility(const CompatibilityRun& run,const ReplayCheckpoint& checkpoint) {
    return pack_compatibility_records(run,checkpoint,{});
}
CompatibilityRun replay_compatibility(std::string_view bundle,const ReplayCheckpoint& checkpoint) {
    constexpr std::string_view prefix="BRUCE_COMPATIBILITY_BUNDLE_V1\n";
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(prefix)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(prefix.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto payload=bundle.substr(65);
    if(sha256(payload)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    BundleReader reader(payload);
    if(reader.field("engine")!=version) throw Error("UnknownCompatibilityVersion");
    const auto digest=reader.field("snapshot_digest");
    CompatibilityTask task;
    task.budget.candidates=integer<std::uint64_t>(reader.field("candidates_budget"));
    task.budget.checks=integer<std::uint64_t>(reader.field("checks_budget"));
    task.budget.trace=integer<std::uint64_t>(reader.field("trace_budget"));
    const auto cancel=reader.field("cancel");
    if(cancel!="none") task.budget.cancel_after_candidates=integer<std::uint64_t>(cancel);
    task.at=integer<std::int64_t>(reader.field("at"));
    task.rules=reader.block("rules_bytes",1024*1024);
    task.candidates=reader.block("candidate_bytes",1024*1024);
    task.origin=reader.block("origin_bytes",max_file_bytes);
    if(sha256(task.rules)!=digest) throw Error("InvalidSnapshotDigest");
    std::function<bool(std::uint64_t)> observe;
    if(checkpoint) observe=[&](std::uint64_t){checkpoint();return false;};
    const auto run=run_compatibility_impl(std::move(task),observe,checkpoint);
    if(checkpoint) checkpoint();
    if(task_payload(run)!=payload) throw Error("ReplayMismatch: compatibility trace or result differs");
    return run;
}
std::string replay_output(std::string_view bundle) {return render_application(replay_application(bundle));}
}
