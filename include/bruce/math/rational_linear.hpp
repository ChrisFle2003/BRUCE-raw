#pragma once
#include "bruce/math/numbers.hpp"
#include <array>
#include <functional>

namespace bruce {
struct RationalPole : Error { RationalPole():Error("RationalFunctionPole") {} };
ExactValue rational_linear(const NumberDomain&,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& x);
bool rational_linear_countercheck(const NumberDomain&,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& x,const ExactValue& y);
template<std::size_t N> struct RationalFractionPlan {
    std::string decision="fallback";
    unsigned rank=0;
    std::size_t rows=0;
    std::array<ExactValue,N> coefficients{};
};
using RationalLinearPlan=RationalFractionPlan<3>;
using RationalQuadraticPlan=RationalFractionPlan<4>;
ExactValue rational_quadratic(const NumberDomain&,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& d,const ExactValue& x);
bool rational_quadratic_countercheck(const NumberDomain&,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& d,const ExactValue& x,const ExactValue& y);
RationalQuadraticPlan solve_rational_quadratic(const NumberDomain&,const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend);
RationalLinearPlan solve_rational_linear(const NumberDomain&,const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend);
}
