#pragma once
#include "bruce/math/numbers.hpp"
#include "bruce/math/expression.hpp"
#include "bruce/math/expression_normal_form.hpp"
#include "bruce/math/domain_cache.hpp"
#include <functional>
#include "bruce/core/replay_control.hpp"
#include <array>

namespace bruce {
struct ExactSample { ExactValue x,y; };
struct ExactResearchTask {
    std::string domain="bigint", minimum="-8", maximum="8";
    unsigned max_bits=4096, max_denominator=1;
    std::uint64_t max_trace_bytes=1048576;
    std::vector<ExactSample> training,holdout;
    Budget budget;
    std::optional<std::uint64_t> cancel_after_candidates;
    std::string strategy="affine";
    unsigned max_power=1;
    std::string planner="enumerate";
    std::optional<std::uint64_t> cancel_after_checkpoints;
    unsigned max_degree=3;
    std::string experience_snapshot;
    unsigned max_nodes=7;
    std::string expression_equivalence="syntax";
    std::string domain_cache="none";
    std::string expression_planner="grammar";
    std::string expression_operators="+-*/";
};
struct HypothesisOrigin { std::string strategy; std::uint64_t id; std::string status; };
struct ExpressionOrigin {std::uint64_t id;std::string status;Expression expression;};
struct ExactHypothesis {
    std::uint64_t id,a_index,b_index;
    ExactValue a,b;
    std::string status;
    unsigned power=1;
    std::vector<ExactValue> coefficients={};
    std::vector<HypothesisOrigin> origins={};
    std::vector<ExactValue> denominator={};
    Expression expression={};
    std::optional<RationalExpressionForm> normal_form={};
    std::vector<ExpressionOrigin> expression_origins={};
};
struct ExactResearchRun {
    ExactResearchTask task;
    std::string status,stop_reason="Completed";
    std::uint64_t candidates=0,operations=0,trace_bytes=0,raw_combinations=0;
    std::vector<ExactHypothesis> hypotheses;
    std::vector<std::string> trace;
    std::uint64_t plans_completed=0,planning_operations=0,checkpoints=0;
    std::vector<ExactResearchRun> strategy_runs={};
    std::vector<unsigned> strategy_order={};
    std::string ordering_reason="not_started";
    std::uint64_t ordering_operations=0;
    std::vector<std::uint64_t> experience_votes{0,0,0};
    std::uint64_t experience_records_used=0,experience_audit_operations=0;
    std::string experience_digest;
    ExpressionCacheStats expression_cache;
    DomainCacheStats domain_cache_stats,union_domain_cache_stats;
    std::string union_witness_summary;
    std::uint64_t domain_witness_candidates=0,domain_bounded_witnesses=0,domain_different=0;
    std::uint64_t domain_comparison_operations=0,domain_comparisons=0,domain_unknown=0,domain_modular_certificates=0;
    std::uint64_t normalization_operations=0,expression_sources=0,union_operations=0,index_entries=0;
};
std::vector<ExactSample> parse_exact_samples(std::string_view bytes,const NumberDomain& domain);
std::vector<ExactSample> read_exact_samples(const std::string& path,const NumberDomain& domain);
ExactResearchRun exact_research(ExactResearchTask task,const std::function<bool(std::uint64_t)>& cancelled={},const ReplayCheckpoint& experience_checkpoint={});
std::string render_exact_research(const ExactResearchRun& run);
std::string pack_exact_research(const ExactResearchRun& run,const ReplayCheckpoint& checkpoint={});
std::string pack_exact_research_records(const ExactResearchRun& run,const ReplayCheckpoint& checkpoint,const ReplayCheckpoint& records);
ExactResearchRun replay_exact_research(std::string_view bundle,std::uint64_t operation_limit=10000000,
    const std::function<void(const ExactResearchTask&)>& admit_task={},const ReplayCheckpoint& checkpoint={});
std::string calculate_exact(const NumberDomain& domain,std::string_view op,std::string_view left,std::string_view right,
    std::string_view modulus={});
}
