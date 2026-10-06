#pragma once
#include "bruce/core/types.hpp"
#include <gmpxx.h>

namespace bruce {
using ExactValue=mpq_class;
struct NumericLimit : Error { NumericLimit():Error("IntegerBitLimit") {} };
std::string exact_text(const ExactValue& value);
std::size_t integer_bits(const mpz_class& value);

class NumberDomain {
public:
    NumberDomain(std::string name,unsigned max_bits);
    ExactValue parse(std::string_view text) const;
    mpz_class parse_integer(std::string_view text) const;
    ExactValue check(ExactValue value) const;
    ExactValue add(const ExactValue& a,const ExactValue& b) const;
    ExactValue subtract(const ExactValue& a,const ExactValue& b) const;
    ExactValue multiply(const ExactValue& a,const ExactValue& b) const;
    ExactValue divide(const ExactValue& a,const ExactValue& b) const;
    ExactValue power(ExactValue base,std::uint64_t exponent) const;
    mpz_class modular_power(mpz_class base,std::uint64_t exponent,const mpz_class& modulus) const;
    std::pair<mpz_class,mpz_class> divmod(const mpz_class& a,const mpz_class& b) const;
    std::string name() const { return name_; }
    unsigned max_bits() const { return bits_; }
private:
    std::string name_;
    unsigned bits_;
};
ExactValue exact_affine(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& x);
bool exact_countercheck(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& x,const ExactValue& y);
}
