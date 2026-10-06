#include "bruce/math/numbers.hpp"
#include <algorithm>

namespace bruce {
std::size_t integer_bits(const mpz_class& value) { return value==0 ? 0 : mpz_sizeinbase(value.get_mpz_t(),2); }
std::string exact_text(const ExactValue& value) { return value.get_str(); }
NumberDomain::NumberDomain(std::string name,unsigned max_bits):name_(std::move(name)),bits_(max_bits) {
    if(name_!="bigint" && name_!="rational") throw Error("UnknownNumberDomain");
    if(bits_<1 || bits_>16384) throw Error("InvalidBitLimit: expected 1..16384");
}
mpz_class NumberDomain::parse_integer(std::string_view text) const {
    const auto digits=text.starts_with('-') ? text.substr(1) : text;
    if(digits.empty() || !std::all_of(digits.begin(),digits.end(),[](unsigned char c) { return c>='0' && c<='9'; }))
        throw Error("InvalidExactInteger");
    // Reject oversized textual components before GMP allocates a large integer.
    if(digits.size()>(bits_*30103ULL)/100000+2) throw NumericLimit();
    mpz_class value;
    if(value.set_str(std::string(text),10)!=0) throw Error("InvalidExactInteger");
    if(integer_bits(value)>bits_) throw NumericLimit();
    return value;
}
ExactValue NumberDomain::parse(std::string_view text) const {
    const auto slash=text.find('/');
    if(slash==std::string_view::npos) return ExactValue(parse_integer(text));
    if(name_!="rational") throw Error("FractionRequiresRationalDomain");
    const auto numerator=parse_integer(text.substr(0,slash));
    const auto denominator=parse_integer(text.substr(slash+1));
    if(denominator==0) throw Error("ZeroDenominator");
    ExactValue value(numerator,denominator);
    value.canonicalize();
    return check(value);
}
ExactValue NumberDomain::check(ExactValue value) const {
    if(value.get_den()==0) throw Error("ZeroDenominator");
    value.canonicalize();
    if(integer_bits(value.get_num())>bits_ || integer_bits(value.get_den())>bits_) throw NumericLimit();
    if(name_=="bigint" && value.get_den()!=1) throw Error("NonIntegralValue");
    return value;
}
ExactValue NumberDomain::add(const ExactValue& a,const ExactValue& b) const { return check(check(a)+check(b)); }
ExactValue NumberDomain::subtract(const ExactValue& a,const ExactValue& b) const { return check(check(a)-check(b)); }
ExactValue NumberDomain::multiply(const ExactValue& a,const ExactValue& b) const { return check(check(a)*check(b)); }
std::pair<mpz_class,mpz_class> NumberDomain::divmod(const mpz_class& a,const mpz_class& b) const {
    (void)check(ExactValue(a)); (void)check(ExactValue(b));
    if(b==0) throw Error("DivisionByZero");
    const mpz_class modulus=abs(b);
    mpz_class r;
    mpz_fdiv_r(r.get_mpz_t(),a.get_mpz_t(),modulus.get_mpz_t());
    const mpz_class q=(a-r)/b;
    (void)check(ExactValue(q)); (void)check(ExactValue(r));
    return {q,r};
}
ExactValue NumberDomain::divide(const ExactValue& a,const ExactValue& b) const {
    const auto left=check(a),right=check(b);
    if(right==0) throw Error("DivisionByZero");
    if(name_=="bigint") {
        const auto [q,r]=divmod(left.get_num(),right.get_num());
        if(r!=0) throw Error("NonIntegralDivision: quotient="+q.get_str()+" remainder="+r.get_str());
        return check(ExactValue(q));
    }
    return check(left/right);
}
ExactValue NumberDomain::power(ExactValue base,std::uint64_t exponent) const {
    base=check(base);
    ExactValue result=1;
    while(exponent) {
        if(exponent&1) result=multiply(result,base);
        exponent>>=1;
        if(exponent) base=multiply(base,base);
    }
    return result;
}
mpz_class NumberDomain::modular_power(mpz_class base,std::uint64_t exponent,const mpz_class& modulus) const {
    (void)check(ExactValue(base)); (void)check(ExactValue(modulus));
    if(modulus<=0) throw Error("InvalidModulus");
    base%=modulus;
    if(base<0) base+=modulus;
    mpz_class result=1%modulus;
    while(exponent) {
        if(exponent&1) result=(result*base)%modulus;
        exponent>>=1;
        if(exponent) base=(base*base)%modulus;
    }
    return result;
}
ExactValue exact_affine(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& x) {
    return domain.add(domain.multiply(a,x),b);
}
bool exact_countercheck(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& x,const ExactValue& y) {
    const auto slope=domain.check(a),offset=domain.check(b),input=domain.check(x),output=domain.check(y);
    if(slope==0) return output==offset;
    const auto difference=domain.subtract(output,offset);
    if(domain.name()=="bigint") {
        const auto [q,r]=domain.divmod(difference.get_num(),slope.get_num());
        return r==0 && ExactValue(q)==input;
    }
    return domain.divide(difference,slope)==input;
}
}
