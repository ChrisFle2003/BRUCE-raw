#pragma once
#include "bruce/math/expression.hpp"
namespace bruce {
struct ExpressionNormalizationLimit : Error { ExpressionNormalizationLimit():Error("ExpressionNormalizationDegreeLimit") {} };
struct RationalExpressionForm {
    std::vector<ExactValue> numerator{0},denominator{1},excluded_polynomial{1};
    bool empty_domain=false;
    bool operator==(const RationalExpressionForm&) const=default;
};
// Sufficient equality certificate: reduced Q[x] fraction AND identical monic,
// square-free exclusion polynomial. No real/rational root filtering is performed.
RationalExpressionForm normalize_expression(const NumberDomain&,const Expression&,const ExpressionSpend&);
// Normalize an explicit polynomial fraction; all zeros of the original denominator remain excluded.
RationalExpressionForm normalize_fraction(const NumberDomain&,const std::vector<ExactValue>& numerator,
    const std::vector<ExactValue>& denominator,const ExpressionSpend&);
ExactValue evaluate_normal_form(const NumberDomain&,const RationalExpressionForm&,const ExactValue&,const ExpressionSpend&);
std::string normal_form_key(const RationalExpressionForm&);
struct RationalDomainComparison {
    std::string relation="UNKNOWN", reason;
    std::vector<ExactValue> left_remainder, right_remainder;
    std::string left_roots="NOT_CHECKED", right_roots="NOT_CHECKED";
    std::optional<ExactValue> witness;
    std::string excluded_side;
    std::uint64_t left_witness_candidates=0,right_witness_candidates=0;
    unsigned left_modular_prime=0,right_modular_prime=0;
    std::vector<unsigned> left_modular_values,right_modular_values;
};
// Compare complements of polynomial zero sets over Q. GCD removes common
// zeros; residual factors of degree <= 2 are decided exactly. Optional modular
// certificates prove absence of rational roots for some higher-degree factors.
// bounded_witness requires modular and probes reduced a/b, |a|<=16, 1<=b<=16;
// exhaustion is UNKNOWN, never a certificate of absence.
RationalDomainComparison compare_rational_domains(const NumberDomain&,
    const std::vector<ExactValue>& left_exclusion, const std::vector<ExactValue>& right_exclusion,
    const ExpressionSpend& spend,bool modular=false,bool bounded_witness=false);
}
