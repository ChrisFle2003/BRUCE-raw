#include "bruce/math/power_affine.hpp"

namespace bruce {
namespace {
void check_power(unsigned power) {
    if(power<1 || power>8) throw Error("InvalidPowerLimit: expected 1..8");
}
}
ExactValue power_affine(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,
    const ExactValue& x,unsigned power) {
    check_power(power);
    const auto slope=domain.check(a),offset=domain.check(b),input=domain.check(x);
    if(slope==0) return offset;
    return exact_affine(domain,slope,offset,domain.power(input,power));
}
bool power_countercheck(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,
    const ExactValue& x,const ExactValue& y,unsigned power) {
    check_power(power);
    auto value=domain.check(a);
    const auto offset=domain.check(b),input=domain.check(x),expected=domain.check(y);
    // Deliberately does not reuse the exponentiation or its forward result.
    for(unsigned i=0;i<power;++i) value=domain.multiply(value,input);
    return domain.add(value,offset)==expected;
}
}
