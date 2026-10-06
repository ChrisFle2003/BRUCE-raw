#include "bruce/application/portfolio_version.hpp"
#include "bruce/application/exact_research.hpp"
#include "bruce/evidence/bundle.hpp"
#include <sstream>

namespace bruce {
namespace {
constexpr std::size_t serialization_block_size=64*1024;
void append_chunked(std::ostringstream& out,std::string_view text,const ReplayCheckpoint& records) {
    for(std::size_t offset=0;offset<text.size();) {
        if(records) records();
        const auto count=std::min(serialization_block_size,text.size()-offset);
        out.write(text.data()+offset,static_cast<std::streamsize>(count));
        offset+=count;
    }
}
std::string payload(const ExactResearchRun& run,const ReplayCheckpoint& records={}) {
    const auto& t=run.task;
    std::ostringstream out;
    if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q-witness") out << "engine " << portfolio_version(t).engine
        << "\nrule q-domain-union-v1\npolicy shared-budget-v4\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="cached-grammar" ? "subtree-cache-v1" : t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\nexpression_operators " << t.expression_operators << "\ncache_policy " << (t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-witness-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-witness16-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.domain_cache=="equal-64") out << "engine " << portfolio_version(t).engine
        << "\nrule q-domain-union-v1\npolicy shared-budget-v3\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="cached-grammar" ? "subtree-cache-v1" : t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\nexpression_operators " << t.expression_operators << "\ncache_policy " << (t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-modular-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-v1\ndomain_cache_policy equal-64-key4096-v1\ndomain_cache_scope expression-and-union-separate-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q-modular") out << "engine " << portfolio_version(t).engine
        << "\nrule q-domain-union-v1\npolicy shared-budget-v3\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="cached-grammar" ? "subtree-cache-v1" : t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\nexpression_operators " << t.expression_operators << "\ncache_policy " << (t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-modular-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.domain_cache=="equal-64") out << "engine bruce-expression-v9\nrule q-domain-binary-v1\npolicy defined-expression-samples-v9\nplanner "
        << (t.planner=="cached-grammar" ? "subtree-cache-v1" : t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\ncache_policy " << (t.planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none") << "\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-modular-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-v1\ndomain_cache_policy equal-64-key4096-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_equivalence=="rational-q-witness") out << "engine bruce-expression-v10\nrule q-domain-binary-v1\npolicy defined-expression-samples-v10\nplanner "
        << (t.planner=="cached-grammar" ? "subtree-cache-v1" : t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\ncache_policy " << (t.planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none") << "\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-witness-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-witness16-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_equivalence=="rational-q-modular") out << "engine bruce-expression-v8\nrule q-domain-binary-v1\npolicy defined-expression-samples-v8\nplanner "
        << (t.planner=="cached-grammar" ? "subtree-cache-v1" : t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\ncache_policy " << (t.planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none") << "\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-modular-domain-v1\ndomain_comparison_policy rational-domain-gcd-modular-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q") out << "engine " << portfolio_version(t).engine
        << "\nrule q-domain-union-v1\npolicy shared-budget-v3\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="cached-grammar" ? "subtree-cache-v1" : t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\nexpression_operators " << t.expression_operators << "\ncache_policy " << (t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-domain-v1\ndomain_comparison_policy rational-domain-gcd-quadratic-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_equivalence=="rational-q") out << "engine bruce-expression-v7\nrule q-domain-binary-v1\npolicy defined-expression-samples-v7\nplanner "
        << (t.planner=="cached-grammar" ? "subtree-cache-v1" : t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")
        << "\ncache_policy " << (t.planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none") << "\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-q-domain-v1\ndomain_comparison_policy rational-domain-gcd-quadratic-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.planner=="cached-grammar") out << "engine bruce-expression-v6\nrule cached-binary-v1\npolicy defined-expression-samples-v6\nplanner subtree-cache-v1\ncache_policy direct-map-64-size3-v1\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence " << (t.expression_equivalence=="rational" ? "rational-domain-v1" : "syntax-v1") << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_operators.find('^')!=std::string::npos) out << "engine bruce-expression-v5\nrule bounded-power-v1\npolicy defined-expression-samples-v5\nplanner "
        << (t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1") << "\noperators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence " << (t.expression_equivalence=="rational" ? "rational-domain-v1" : "syntax-v1") << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_planner=="cached-grammar") out << "engine " << portfolio_version(t).engine
        << "\nrule normal-form-union-v1\npolicy shared-budget-v2\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << "subtree-cache-v1" << "\nexpression_operators " << t.expression_operators
        << "\ncache_policy direct-map-64-size3-v1\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-domain-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_operators.find('^')!=std::string::npos) out << "engine " << portfolio_version(t).engine
        << "\nrule normal-form-union-v1\npolicy shared-budget-v2\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1") << "\nexpression_operators " << t.expression_operators
        << "\npower_exponent_policy literal-2-to-8-v1\nequivalence rational-domain-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_operators!="+-*/") out << "engine " << portfolio_version(t).engine
        << "\nrule normal-form-union-v1\npolicy shared-budget-v2\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner " << (t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1") << "\nexpression_operators " << t.expression_operators
        << "\nequivalence rational-domain-v1\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_operators!="+-*/") out << "engine bruce-expression-v4\nrule selected-binary-v1\npolicy defined-expression-samples-v4\nplanner "
        << (t.planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1") << "\noperators " << t.expression_operators
        << "\nequivalence " << (t.expression_equivalence=="rational" ? "rational-domain-v1" : "syntax-v1") << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression" && t.expression_planner=="indexed-grammar") out << "engine " << portfolio_version(t).engine
        << "\nrule normal-form-union-v1\npolicy shared-budget-v2\nplanner " << portfolio_version(t).planner
        << "\nexpression_planner block-index-v1\nequivalence rational-domain-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-expression") out << "engine " << portfolio_version(t).engine
        << "\nrule normal-form-union-v1\npolicy shared-budget-v2\nplanner " << portfolio_version(t).planner << "\nequivalence rational-domain-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.planner=="indexed-grammar") out << "engine bruce-expression-v3\nrule binary-arithmetic-v1\npolicy defined-expression-samples-v3\nplanner block-index-v1\nequivalence "
        << (t.expression_equivalence=="rational" ? "rational-domain-v1" : "syntax-v1") << "\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression" && t.expression_equivalence=="rational") out << "engine bruce-expression-v2\nrule binary-arithmetic-v1\npolicy defined-expression-samples-v2\nplanner size-ranked-v1\nequivalence rational-domain-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="expression") out << "engine bruce-expression-v1\nrule binary-arithmetic-v1\npolicy defined-expression-samples-v1\nplanner size-ranked-v1\nmax_nodes " << t.max_nodes
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-quadratic") out << "engine " << (t.planner=="experience-order" ? "bruce-portfolio-v7" : t.planner=="adaptive-order" ? "bruce-portfolio-v6" : "bruce-portfolio-v5")
        << "\nrule domain-union-v2\npolicy shared-budget-v1\nplanner " << (t.planner=="experience-order" ? "experience-order-v2" : t.planner=="adaptive-order" ? "sample-shape-v2" : "four-family-v1")
        << "\nmax_power " << t.max_power << "\nmax_degree " << t.max_degree << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="rational-quadratic") out << "engine bruce-rational-quadratic-v1\nrule monic-quadratic-fraction-v1\npolicy defined-samples-v1\nplanner quadratic-system-v1\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.planner=="experience-order") out << "engine bruce-portfolio-v4\nrule domain-union-v1\npolicy shared-budget-v1\nplanner experience-order-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.planner=="adaptive-order") out << "engine bruce-portfolio-v3\nrule domain-union-v1\npolicy shared-budget-v1\nplanner sample-shape-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio-rational") out << "engine bruce-portfolio-v2\nrule domain-union-v1\npolicy shared-budget-v1\nplanner power-polynomial-rational-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="rational-linear") out << "engine bruce-rational-linear-v1\nrule monic-linear-fraction-v1\npolicy defined-samples-v1\nplanner linear-system-v1\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="portfolio") out << "engine bruce-portfolio-v1\nrule portfolio-union-v1\npolicy shared-budget-v1\nplanner power-then-polynomial-v1\nmax_power " << t.max_power
        << "\nmax_degree " << t.max_degree << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="polynomial") out << "engine bruce-polynomial-v1\nrule polynomial-v1\npolicy polynomial-samples-v1\nplanner newton-v1\nmax_degree " << t.max_degree
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.planner=="adaptive") out << "engine bruce-adaptive-v1\nrule power-affine-v1\npolicy adaptive-samples-v1\nplanner two-point-v1\nmax_power " << t.max_power
        << "\ncheckpoint " << (t.cancel_after_checkpoints ? std::to_string(*t.cancel_after_checkpoints) : "none") << '\n';
    else if(t.strategy=="power-affine") out << "engine bruce-power-v1\nrule power-affine-v1\npolicy power-samples-v1\nmax_power " << t.max_power << '\n';
    else out << "engine bruce-exact-v1\nrule affine-exact-v1\npolicy exact-samples-v1\n";
    out << "domain " << t.domain
        << "\nmax_bits " << t.max_bits << "\nmax_denominator " << t.max_denominator
        << "\nminimum " << t.minimum << "\nmaximum " << t.maximum
        << "\ncandidates " << t.budget.candidates << "\noperations " << t.budget.operations
        << "\ntrace " << t.budget.trace_events << "\nresults " << t.budget.results
        << "\ntrace_bytes " << t.max_trace_bytes << "\ncancel "
        << (t.cancel_after_candidates ? std::to_string(*t.cancel_after_candidates) : "none") << '\n';
    auto samples=[&](std::string_view key,const std::vector<ExactSample>& data) {
        out << key << ' ' << data.size() << '\n';
        for(const auto& s:data) out << exact_text(s.x) << ' ' << exact_text(s.y) << '\n';
    };
    samples("training",t.training); samples("holdout",t.holdout);
    if(t.planner=="experience-order") {
        out << "experience_bytes " << t.experience_snapshot.size() << '\n';
        append_chunked(out,t.experience_snapshot,records);
    }
    out << "RESULT\n" << render_exact_research(run) << '\n';
    return out.str();
}
std::string field(std::istream& in,std::string_view name) {
    std::string line,key,value,extra;
    if(!std::getline(in,line)) throw Error("TruncatedExactBundle");
    std::istringstream row(line);
    if(!(row>>key>>value) || key!=name || (row>>extra)) throw Error("InvalidExactBundleField");
    return value;
}
std::vector<ExactSample> samples(std::istream& in,std::string_view key,const NumberDomain& domain,const ReplayCheckpoint& checkpoint) {
    const auto count=integer<std::size_t>(field(in,key));
    if(count>4096) throw Error("SampleBudgetExceeded");
    std::vector<ExactSample> result;
    for(std::size_t i=0;i<count;++i) {
        if(checkpoint) checkpoint();
        std::string line,x,y,extra;
        if(!std::getline(in,line)) throw Error("TruncatedExactSamples");
        std::istringstream row(line);
        if(!(row>>x>>y) || (row>>extra)) throw Error("InvalidExactSample");
        result.push_back({domain.parse(x),domain.parse(y)});
    }
    return result;
}
}
std::string pack_exact_research_records(const ExactResearchRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records) {
    const auto body=payload(run,records);
    const auto result=std::string(run.task.strategy=="portfolio-expression" ? portfolio_version(run.task).header : run.task.strategy=="expression" && run.task.expression_equivalence=="rational-q-witness" ? "BRUCE_EXPRESSION_BUNDLE_V10\n" : run.task.domain_cache=="equal-64" ? "BRUCE_EXPRESSION_BUNDLE_V9\n" : run.task.strategy=="expression" && run.task.expression_equivalence=="rational-q-modular" ? "BRUCE_EXPRESSION_BUNDLE_V8\n" : run.task.strategy=="expression" && run.task.expression_equivalence=="rational-q" ? "BRUCE_EXPRESSION_BUNDLE_V7\n" : run.task.strategy=="expression" && run.task.planner=="cached-grammar" ? "BRUCE_EXPRESSION_BUNDLE_V6\n" : run.task.strategy=="expression" && run.task.expression_operators.find('^')!=std::string::npos ? "BRUCE_EXPRESSION_BUNDLE_V5\n" : run.task.strategy=="expression" && run.task.expression_operators!="+-*/" ? "BRUCE_EXPRESSION_BUNDLE_V4\n" : run.task.strategy=="expression" ? (run.task.planner=="indexed-grammar" ? "BRUCE_EXPRESSION_BUNDLE_V3\n" : run.task.expression_equivalence=="rational" ? "BRUCE_EXPRESSION_BUNDLE_V2\n" : "BRUCE_EXPRESSION_BUNDLE_V1\n") : run.task.strategy=="portfolio-quadratic" ? (run.task.planner=="experience-order" ? "BRUCE_PORTFOLIO_BUNDLE_V7\n" : run.task.planner=="adaptive-order" ? "BRUCE_PORTFOLIO_BUNDLE_V6\n" : "BRUCE_PORTFOLIO_BUNDLE_V5\n") : run.task.strategy=="rational-quadratic" ? "BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1\n" : run.task.planner=="experience-order" ? "BRUCE_PORTFOLIO_BUNDLE_V4\n" : run.task.planner=="adaptive-order" ? "BRUCE_PORTFOLIO_BUNDLE_V3\n" : run.task.strategy=="portfolio-rational" ? "BRUCE_PORTFOLIO_BUNDLE_V2\n" : run.task.strategy=="rational-linear" ? "BRUCE_RATIONAL_LINEAR_BUNDLE_V1\n" : run.task.strategy=="portfolio" ? "BRUCE_PORTFOLIO_BUNDLE_V1\n" : run.task.strategy=="polynomial" ? "BRUCE_POLYNOMIAL_BUNDLE_V1\n" : run.task.planner=="adaptive" ? "BRUCE_ADAPTIVE_BUNDLE_V1\n" :
        run.task.strategy=="power-affine" ? "BRUCE_POWER_BUNDLE_V1\n" : "BRUCE_EXACT_BUNDLE_V1\n")+sha256(body,checkpoint)+"\n"+body;
    if(result.size()>max_file_bytes) throw Error("BundleByteLimit");
    return result;
}
std::string pack_exact_research(const ExactResearchRun& run,const ReplayCheckpoint& checkpoint) {
    return pack_exact_research_records(run,checkpoint,{});
}
ExactResearchRun replay_exact_research(std::string_view bundle,std::uint64_t operation_limit,
    const std::function<void(const ExactResearchTask&)>& admit_task,const ReplayCheckpoint& checkpoint) {
    const auto portfolio_format=portfolio_version_from_header(bundle);
    const bool witness_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V32\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V33\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V34\n");
    const bool domain_cache_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V29\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V30\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V31\n");
    const bool witnessed=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V10\n");
    const bool domain_cached=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V9\n");
    const bool modular_five=witness_five || domain_cache_five || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V26\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V27\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V28\n");
    const bool modular_expression=witnessed || domain_cached || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V8\n");
    const bool modular=modular_five || modular_expression;
    const bool q_five=modular_five || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V23\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V24\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V25\n");
    const bool rational_q=modular_expression || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V7\n");
    const bool cached=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V6\n");
    const bool powered=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V5\n");
    const bool selected=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V4\n");
    const bool cached_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V20\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V21\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V22\n");
    const bool powered_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V17\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V18\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V19\n");
    const bool selected_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V14\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V15\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V16\n");
    const bool indexed_five=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V11\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V12\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V13\n");
    const bool five=q_five || cached_five || powered_five || selected_five || indexed_five || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V8\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V9\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V10\n");
    const bool indexed=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V3\n");
    const bool grouped=bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V2\n");
    const bool expression=rational_q || cached || powered || selected || indexed || grouped || bundle.starts_with("BRUCE_EXPRESSION_BUNDLE_V1\n");
    const bool quadratic=bundle.starts_with("BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1\n");
    const bool four=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V5\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V6\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V7\n");
    const bool learned=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V34\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V31\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V28\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V25\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V22\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V19\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V16\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V13\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V10\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V7\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V4\n");
    const bool ordered=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V33\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V30\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V27\n") || learned || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V24\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V21\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V18\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V15\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V12\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V9\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V6\n") || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V3\n");
    const bool extended=!five && (four || ordered || bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V2\n"));
    const bool fraction=quadratic || bundle.starts_with("BRUCE_RATIONAL_LINEAR_BUNDLE_V1\n");
    const bool adaptive=bundle.starts_with("BRUCE_ADAPTIVE_BUNDLE_V1\n");
    const bool polynomial=bundle.starts_with("BRUCE_POLYNOMIAL_BUNDLE_V1\n");
    const bool portfolio=bundle.starts_with("BRUCE_PORTFOLIO_BUNDLE_V1\n");
    const bool nonlinear=adaptive || bundle.starts_with("BRUCE_POWER_BUNDLE_V1\n");
    const std::string_view prefix=portfolio_format ? std::string_view(portfolio_format->header) : witnessed ? "BRUCE_EXPRESSION_BUNDLE_V10\n" : domain_cached ? "BRUCE_EXPRESSION_BUNDLE_V9\n" : modular_expression ? "BRUCE_EXPRESSION_BUNDLE_V8\n" : rational_q ? "BRUCE_EXPRESSION_BUNDLE_V7\n" : cached ? "BRUCE_EXPRESSION_BUNDLE_V6\n" : powered ? "BRUCE_EXPRESSION_BUNDLE_V5\n" : selected ? "BRUCE_EXPRESSION_BUNDLE_V4\n" : indexed ? "BRUCE_EXPRESSION_BUNDLE_V3\n" : grouped ? "BRUCE_EXPRESSION_BUNDLE_V2\n" : expression ? "BRUCE_EXPRESSION_BUNDLE_V1\n" : four ? (learned ? "BRUCE_PORTFOLIO_BUNDLE_V7\n" : ordered ? "BRUCE_PORTFOLIO_BUNDLE_V6\n" : "BRUCE_PORTFOLIO_BUNDLE_V5\n") : quadratic ? "BRUCE_RATIONAL_QUADRATIC_BUNDLE_V1\n" : learned ? "BRUCE_PORTFOLIO_BUNDLE_V4\n" : ordered ? "BRUCE_PORTFOLIO_BUNDLE_V3\n" : extended ? "BRUCE_PORTFOLIO_BUNDLE_V2\n" : fraction ? "BRUCE_RATIONAL_LINEAR_BUNDLE_V1\n" : portfolio ? "BRUCE_PORTFOLIO_BUNDLE_V1\n" : polynomial ? "BRUCE_POLYNOMIAL_BUNDLE_V1\n" : adaptive ? "BRUCE_ADAPTIVE_BUNDLE_V1\n" : nonlinear ? "BRUCE_POWER_BUNDLE_V1\n" : "BRUCE_EXACT_BUNDLE_V1\n";
    if(bundle.size()>max_file_bytes) throw Error("BundleByteLimit");
    if(!bundle.starts_with(prefix)) throw Error("UnknownBundleVersion");
    bundle.remove_prefix(prefix.size());
    if(bundle.size()<65 || bundle[64]!='\n') throw Error("InvalidDigestHeader");
    const auto body=bundle.substr(65);
    if(sha256(body)!=bundle.substr(0,64)) throw Error("InvalidEvidence: digest mismatch");
    if(checkpoint) checkpoint();
    std::istringstream in{std::string(body)};
    if(field(in,"engine")!=(portfolio_format ? portfolio_format->engine : witnessed ? "bruce-expression-v10" : domain_cached ? "bruce-expression-v9" : modular_expression ? "bruce-expression-v8" : rational_q ? "bruce-expression-v7" : cached ? "bruce-expression-v6" : powered ? "bruce-expression-v5" : selected ? "bruce-expression-v4" : indexed ? "bruce-expression-v3" : grouped ? "bruce-expression-v2" : expression ? "bruce-expression-v1" : four ? (learned ? "bruce-portfolio-v7" : ordered ? "bruce-portfolio-v6" : "bruce-portfolio-v5") : quadratic ? "bruce-rational-quadratic-v1" : learned ? "bruce-portfolio-v4" : ordered ? "bruce-portfolio-v3" : extended ? "bruce-portfolio-v2" : fraction ? "bruce-rational-linear-v1" : portfolio ? "bruce-portfolio-v1" : polynomial ? "bruce-polynomial-v1" : adaptive ? "bruce-adaptive-v1" : nonlinear ? "bruce-power-v1" : "bruce-exact-v1") ||
       field(in,"rule")!=(q_five ? "q-domain-union-v1" : rational_q ? "q-domain-binary-v1" : cached ? "cached-binary-v1" : powered ? "bounded-power-v1" : selected ? "selected-binary-v1" : five ? "normal-form-union-v1" : expression ? "binary-arithmetic-v1" : four ? "domain-union-v2" : quadratic ? "monic-quadratic-fraction-v1" : extended ? "domain-union-v1" : fraction ? "monic-linear-fraction-v1" : portfolio ? "portfolio-union-v1" : polynomial ? "polynomial-v1" : nonlinear ? "power-affine-v1" : "affine-exact-v1") ||
       field(in,"policy")!=(witness_five ? "shared-budget-v4" : witnessed ? "defined-expression-samples-v10" : domain_cached ? "defined-expression-samples-v9" : modular_expression ? "defined-expression-samples-v8" : q_five ? "shared-budget-v3" : rational_q ? "defined-expression-samples-v7" : cached ? "defined-expression-samples-v6" : powered ? "defined-expression-samples-v5" : selected ? "defined-expression-samples-v4" : five ? "shared-budget-v2" : indexed ? "defined-expression-samples-v3" : grouped ? "defined-expression-samples-v2" : expression ? "defined-expression-samples-v1" : extended ? "shared-budget-v1" : fraction ? "defined-samples-v1" : portfolio ? "shared-budget-v1" : polynomial ? "polynomial-samples-v1" : adaptive ? "adaptive-samples-v1" : nonlinear ? "power-samples-v1" : "exact-samples-v1"))
        throw Error("UnknownExactVersion");
    ExactResearchTask task;
    if(five) {
        if(!portfolio_format || field(in,"planner")!=portfolio_format->planner) throw Error("UnknownPlannerVersion");
        if(q_five) {
            const auto planner=field(in,"expression_planner");
            if(planner!="subtree-cache-v1" && planner!="block-index-v1" && planner!="size-ranked-v1") throw Error("UnknownExpressionPlannerVersion");
            task.expression_planner=planner=="subtree-cache-v1" ? "cached-grammar" : planner=="block-index-v1" ? "indexed-grammar" : "grammar";
            task.expression_operators=field(in,"expression_operators");
            if(canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
            if(field(in,"cache_policy")!=(task.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")) throw Error("UnknownExpressionCachePolicy");
        } else if(cached_five) {
            if(field(in,"expression_planner")!="subtree-cache-v1") throw Error("UnknownExpressionPlannerVersion");
            task.expression_planner="cached-grammar";
            task.expression_operators=field(in,"expression_operators");
            if(canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
            if(field(in,"cache_policy")!="direct-map-64-size3-v1") throw Error("UnknownExpressionCachePolicy");
        } else if(powered_five || selected_five) {
            const auto planner=field(in,"expression_planner");
            if(planner!="block-index-v1" && planner!="size-ranked-v1") throw Error("UnknownExpressionPlannerVersion");
            task.expression_planner=planner=="block-index-v1" ? "indexed-grammar" : "grammar";
            task.expression_operators=field(in,"expression_operators");
            if(powered_five!=(task.expression_operators.find('^')!=std::string::npos) || task.expression_operators=="+-*/" || canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
        } else if(indexed_five) {
            if(field(in,"expression_planner")!="block-index-v1") throw Error("UnknownExpressionPlannerVersion");
            task.expression_planner="indexed-grammar";
        }
        if((q_five || cached_five || powered_five) && field(in,"power_exponent_policy")!="literal-2-to-8-v1") throw Error("UnknownExpressionExponentPolicy");
        if(field(in,"equivalence")!=(witness_five ? "rational-q-witness-domain-v1" : modular ? "rational-q-modular-domain-v1" : q_five ? "rational-q-domain-v1" : "rational-domain-v1")) throw Error("UnknownEquivalenceVersion");
        task.strategy="portfolio-expression";task.planner=learned ? "experience-order" : ordered ? "adaptive-order" : "portfolio";task.expression_equivalence=witness_five ? "rational-q-witness" : modular ? "rational-q-modular" : q_five ? "rational-q" : "rational";
        if(q_five && field(in,"domain_comparison_policy")!=(witness_five ? "rational-domain-gcd-modular-witness16-v1" : modular ? "rational-domain-gcd-modular-v1" : "rational-domain-gcd-quadratic-v1")) throw Error("UnknownEquivalenceVersion");
        if(domain_cache_five) {
            if(field(in,"domain_cache_policy")!="equal-64-key4096-v1" ||
               field(in,"domain_cache_scope")!="expression-and-union-separate-v1") throw Error("UnknownDomainCachePolicy");
            task.domain_cache="equal-64";
        }
        task.max_power=integer<unsigned>(field(in,"max_power"));
        task.max_degree=integer<unsigned>(field(in,"max_degree"));
        task.max_nodes=integer<unsigned>(field(in,"max_nodes"));
    }
    if(expression) {
        const auto planner=field(in,"planner");
        if(rational_q ? (planner!="subtree-cache-v1" && planner!="block-index-v1" && planner!="size-ranked-v1") : cached ? planner!="subtree-cache-v1" : (powered || selected) ? (planner!="block-index-v1" && planner!="size-ranked-v1") : planner!=(indexed ? "block-index-v1" : "size-ranked-v1")) throw Error("UnknownPlannerVersion");
        task.strategy="expression";task.planner=(cached || (rational_q && planner=="subtree-cache-v1")) ? "cached-grammar" : planner=="block-index-v1" ? "indexed-grammar" : "grammar";
        if(rational_q) {
            if(field(in,"cache_policy")!=(task.planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")) throw Error("UnknownExpressionCachePolicy");
            task.expression_operators=field(in,"operators");
            if(canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
            if(field(in,"power_exponent_policy")!="literal-2-to-8-v1" || field(in,"equivalence")!=(witnessed ? "rational-q-witness-domain-v1" : modular ? "rational-q-modular-domain-v1" : "rational-q-domain-v1") ||
               field(in,"domain_comparison_policy")!=(witnessed ? "rational-domain-gcd-modular-witness16-v1" : modular ? "rational-domain-gcd-modular-v1" : "rational-domain-gcd-quadratic-v1")) throw Error("UnknownEquivalenceVersion");
            task.expression_equivalence=witnessed ? "rational-q-witness" : modular ? "rational-q-modular" : "rational-q";
        } else if(cached) {
            if(field(in,"cache_policy")!="direct-map-64-size3-v1") throw Error("UnknownExpressionCachePolicy");
            task.expression_operators=field(in,"operators");
            if(canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
            if(field(in,"power_exponent_policy")!="literal-2-to-8-v1") throw Error("UnknownExpressionExponentPolicy");
        } else if(powered || selected) {
            task.expression_operators=field(in,"operators");
            const bool has_power=task.expression_operators.find('^')!=std::string::npos;
            if(powered!=has_power || task.expression_operators=="+-*/" || canonical_expression_operators(task.expression_operators)!=task.expression_operators) throw Error("NoncanonicalExpressionOperators");
            if(powered && field(in,"power_exponent_policy")!="literal-2-to-8-v1") throw Error("UnknownExpressionExponentPolicy");
        }
        if(cached || powered || selected || grouped || indexed) {
            const auto mode=field(in,"equivalence");
            if(mode!="rational-domain-v1" && !((cached || powered || selected || indexed) && mode=="syntax-v1")) throw Error("UnknownEquivalenceVersion");
            task.expression_equivalence=mode=="rational-domain-v1" ? "rational" : "syntax";
        }
        if(domain_cached) {
            if(field(in,"domain_cache_policy")!="equal-64-key4096-v1") throw Error("UnknownDomainCachePolicy");
            task.domain_cache="equal-64";
        }
        task.max_nodes=integer<unsigned>(field(in,"max_nodes"));
    }
    if(fraction) {
        if(field(in,"planner")!=(quadratic ? "quadratic-system-v1" : "linear-system-v1")) throw Error("UnknownPlannerVersion");
        task.strategy=quadratic ? "rational-quadratic" : "rational-linear"; task.planner="solve";
    }
    if(portfolio || extended) {
        if(field(in,"planner")!=(four ? (learned ? "experience-order-v2" : ordered ? "sample-shape-v2" : "four-family-v1") : learned ? "experience-order-v1" : ordered ? "sample-shape-v1" : extended ? "power-polynomial-rational-v1" : "power-then-polynomial-v1")) throw Error("UnknownPlannerVersion");
        task.strategy=four ? "portfolio-quadratic" : extended ? "portfolio-rational" : "portfolio"; task.planner=learned ? "experience-order" : ordered ? "adaptive-order" : "portfolio";
        task.max_power=integer<unsigned>(field(in,"max_power")); task.max_degree=integer<unsigned>(field(in,"max_degree"));
    }
    if(polynomial) {
        if(field(in,"planner")!="newton-v1") throw Error("UnknownPlannerVersion");
        task.strategy="polynomial"; task.planner="interpolate";
        task.max_degree=integer<unsigned>(field(in,"max_degree"));
    }
    if(adaptive) {
        if(field(in,"planner")!="two-point-v1") throw Error("UnknownPlannerVersion");
        task.planner="adaptive";
    }
    if(nonlinear) {
        task.strategy="power-affine";
        task.max_power=integer<unsigned>(field(in,"max_power"));
    }
    if(five || expression || extended || fraction || adaptive || polynomial || portfolio) {
        const auto checkpoint=field(in,"checkpoint");
        if(checkpoint!="none") task.cancel_after_checkpoints=integer<std::uint64_t>(checkpoint);
    }
    task.domain=field(in,"domain");
    task.max_bits=integer<unsigned>(field(in,"max_bits"));
    task.max_denominator=integer<unsigned>(field(in,"max_denominator"));
    task.minimum=field(in,"minimum"); task.maximum=field(in,"maximum");
    task.budget.candidates=integer<std::uint64_t>(field(in,"candidates"));
    task.budget.operations=integer<std::uint64_t>(field(in,"operations"));
    task.budget.trace_events=integer<std::uint64_t>(field(in,"trace"));
    task.budget.results=integer<std::uint64_t>(field(in,"results"));
    task.max_trace_bytes=integer<std::uint64_t>(field(in,"trace_bytes"));
    const auto cancel=field(in,"cancel");
    if(cancel!="none") task.cancel_after_candidates=integer<std::uint64_t>(cancel);
    const NumberDomain domain(task.domain,task.max_bits);
    task.training=samples(in,"training",domain,checkpoint); task.holdout=samples(in,"holdout",domain,checkpoint);
    if(learned) {
        const auto size=integer<std::size_t>(field(in,"experience_bytes"));
        if(size>1024*1024) throw Error("ExperienceByteLimit");
        task.experience_snapshot.resize(size);
        if(!in.read(task.experience_snapshot.data(),size)) throw Error("TruncatedExperienceSnapshot");
    }
    std::string marker;
    if(!std::getline(in,marker) || marker!="RESULT") throw Error("MissingResult");
    if(task.budget.operations>operation_limit) throw Error("ReplayOperationLimit");
    if(admit_task) admit_task(task);
    if(checkpoint) checkpoint();
    std::function<bool(std::uint64_t)> observe;
    if(checkpoint) observe=[&](std::uint64_t){return detail::observe_replay_checkpoint(checkpoint);};
    ExactResearchRun run;
    try{run=exact_research(std::move(task),observe,checkpoint);}catch(const detail::ReplayCheckpointRaised& stop){std::rethrow_exception(stop.original);}
    if(checkpoint) checkpoint();
    if(payload(run)!=body) throw Error("ReplayMismatch: exact input, result or trace differs");
    return run;
}
}
