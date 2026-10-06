#pragma once
#include "bruce/math/expression_normal_form.hpp"
#include <functional>
#include "bruce/core/replay_control.hpp"

namespace bruce {
struct FractionComparisonTask {
    std::string left_numerator="0",left_denominator="1",right_numerator="0",right_denominator="1";
    std::string domain_policy="quadratic";
    unsigned max_bits=4096;
    std::uint64_t max_operations=1000000;
    std::optional<std::uint64_t> cancel_after_operations;
};
struct FractionComparisonRun {
    FractionComparisonTask task;
    std::string status="INCOMPLETE",stop_reason="Completed",value_relation="UNKNOWN";
    std::uint64_t operations=0;
    std::optional<RationalExpressionForm> left,right;
    RationalDomainComparison domains;
    std::vector<std::string> trace;
};
FractionComparisonRun compare_fractions(FractionComparisonTask,
    const std::function<bool(std::uint64_t)>& cancelled={});
std::string render_fraction_comparison(const FractionComparisonRun&);
std::string pack_fraction_comparison(const FractionComparisonRun&,const ReplayCheckpoint& checkpoint={});
std::string pack_fraction_comparison_records(const FractionComparisonRun&,const ReplayCheckpoint&,const ReplayCheckpoint&);
FractionComparisonRun replay_fraction_comparison(std::string_view,const ReplayCheckpoint& checkpoint={});
}
