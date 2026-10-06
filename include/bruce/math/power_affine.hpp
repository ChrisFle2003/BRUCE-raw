#pragma once
#include "bruce/math/numbers.hpp"

namespace bruce {
ExactValue power_affine(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,
    const ExactValue& x,unsigned power);
bool power_countercheck(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,
    const ExactValue& x,const ExactValue& y,unsigned power);
}
