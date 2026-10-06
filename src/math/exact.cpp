#include "bruce/math/exact.hpp"
#include <limits>
namespace bruce {
std::uint64_t power(std::uint64_t base, std::uint64_t exponent, unsigned max_bits) {
    if (max_bits<1 || max_bits>64) throw Error("InvalidBitLimit: expected 1..64");
    const auto limit=max_bits==64 ? UINT64_MAX : (std::uint64_t{1}<<max_bits)-1;
    const auto multiply=[limit](std::uint64_t x,std::uint64_t y) {
        const __uint128_t value=__uint128_t(x)*y;
        if (value>limit) throw Error("IntegerBitLimit");
        return static_cast<std::uint64_t>(value);
    };
    std::uint64_t value=1;
    while(exponent) {
        if(exponent&1) value=multiply(value,base);
        exponent>>=1;
        if(exponent) base=multiply(base,base);
    }
    return value;
}
std::uint64_t modular_power(std::uint64_t base, std::uint64_t exponent, std::uint64_t modulus) {
    if (!modulus) throw Error("InvalidModulus");
    std::uint64_t value=1%modulus;
    base%=modulus;
    while(exponent) {
        if(exponent&1) value=static_cast<std::uint64_t>((__uint128_t(value)*base)%modulus);
        exponent>>=1;
        if(exponent) base=static_cast<std::uint64_t>((__uint128_t(base)*base)%modulus);
    }
    return value;
}
std::int64_t affine(std::int64_t a, std::int64_t b, std::int64_t x) {
    const __int128_t value=__int128_t(a)*x+b;
    if (value<INT64_MIN || value>INT64_MAX) throw Error("IntegerOverflow");
    return static_cast<std::int64_t>(value);
}
bool countercheck(std::int64_t a, std::int64_t b, Sample sample) {
    if(a==0) return sample.y==b;
    const __int128_t difference=__int128_t(sample.y)-b;
    return difference%a==0 && difference/a==sample.x;
}
}
