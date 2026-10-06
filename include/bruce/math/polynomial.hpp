#pragma once
#include "bruce/math/numbers.hpp"
#include <functional>

namespace bruce {
std::vector<ExactValue> interpolate_polynomial(const NumberDomain& rational,
    const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend);
ExactValue polynomial_horner(const NumberDomain& domain,const std::vector<ExactValue>& coefficients,const ExactValue& x);
ExactValue polynomial_sum(const NumberDomain& domain,const std::vector<ExactValue>& coefficients,const ExactValue& x);
std::string polynomial_formula(const std::vector<ExactValue>& coefficients);
}
