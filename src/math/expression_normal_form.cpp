#include "bruce/math/expression_normal_form.hpp"
#include <algorithm>
#include <numeric>

namespace bruce {
namespace {
using Poly=std::vector<ExactValue>;
constexpr std::size_t coefficient_limit=65;
class Polynomials {
public:
    const NumberDomain& d;
    const ExpressionSpend& spend;
    void size(std::size_t n) const {if(!n || n>coefficient_limit) throw ExpressionNormalizationLimit{};}
    bool zero(const Poly& p) const {spend(1);return p.size()==1 && p[0]==0;}
    Poly trim(Poly p) const {
        size(p.size());
        while(p.size()>1) {spend(1);if(p.back()!=0) break;p.pop_back();}
        return p;
    }
    Poly add(const Poly& a,const Poly& b,bool subtract=false) const {
        Poly p(std::max(a.size(),b.size()),0);size(p.size());
        for(std::size_t i=0;i<p.size();++i) {
            const ExactValue x=i<a.size() ? a[i] : ExactValue(0),y=i<b.size() ? b[i] : ExactValue(0);
            spend(1);p[i]=subtract ? d.subtract(x,y) : d.add(x,y);
        }
        return trim(std::move(p));
    }
    Poly multiply(const Poly& a,const Poly& b) const {
        size(a.size()+b.size()-1); Poly p(a.size()+b.size()-1,0);
        for(std::size_t i=0;i<a.size();++i) for(std::size_t j=0;j<b.size();++j) {
            spend(1);const auto v=d.multiply(a[i],b[j]);
            spend(1);p[i+j]=d.add(p[i+j],v);
        }
        return trim(std::move(p));
    }
    std::pair<Poly,Poly> divide(Poly a,const Poly& b) const {
        if(zero(b)) throw Error("ZeroPolynomialDivisor");
        Poly q(a.size()>=b.size() ? a.size()-b.size()+1 : 1,0);
        while(!zero(a) && a.size()>=b.size()) {
            const auto shift=a.size()-b.size();
            spend(1);const auto factor=d.divide(a.back(),b.back());
            spend(1);q[shift]=d.add(q[shift],factor);
            for(std::size_t i=0;i<b.size();++i) {
                spend(1);const auto product=d.multiply(factor,b[i]);
                spend(1);a[shift+i]=d.subtract(a[shift+i],product);
            }
            a=trim(std::move(a));
        }
        return {trim(std::move(q)),std::move(a)};
    }
    Poly exact_divide(const Poly& a,const Poly& b) const {
        auto [q,r]=divide(a,b);
        if(!zero(r)) throw Error("NonExactPolynomialDivision");
        return q;
    }
    Poly monic(Poly p) const {
        if(zero(p)) return p;
        const auto lead=p.back();
        for(auto& v:p) {spend(1);v=d.divide(v,lead);}
        return p;
    }
    Poly gcd(Poly a,Poly b) const {
        while(!zero(b)) {auto remainder=divide(a,b).second;a=std::move(b);b=std::move(remainder);}
        return monic(std::move(a));
    }
    Poly radical(Poly p) const {
        if(p.size()==1) return zero(p) ? Poly{0} : Poly{1};
        Poly derivative(p.size()-1);
        for(std::size_t i=1;i<p.size();++i) {spend(1);derivative[i-1]=d.multiply(p[i],ExactValue(static_cast<unsigned long>(i)));}
        const auto common=gcd(p,trim(std::move(derivative)));
        return monic(exact_divide(p,common));
    }
    void reduce(RationalExpressionForm& f) const {
        if(zero(f.denominator)) throw Error("ZeroNormalFormDenominator");
        const auto common=gcd(f.numerator,f.denominator);
        f.numerator=exact_divide(f.numerator,common);
        f.denominator=exact_divide(f.denominator,common);
        const auto lead=f.denominator.back();
        for(auto& c:f.numerator) {spend(1);c=d.divide(c,lead);}
        for(auto& c:f.denominator) {spend(1);c=d.divide(c,lead);}
    }
    ExactValue evaluate(const Poly& p,const ExactValue& x) const {
        size(p.size());spend(1);auto value=d.check(p.back());
        for(std::size_t i=p.size()-1;i>0;--i) {
            spend(1);const auto v=d.multiply(value,x);
            spend(1);value=d.add(v,p[i-1]);
        }
        return value;
    }
};
}
RationalExpressionForm normalize_expression(const NumberDomain& d,const Expression& tree,const ExpressionSpend& spend) {
    if(d.name()!="rational" || !spend) throw Error("InvalidNormalizationConfiguration");
    validate_expression(tree);
    const Polynomials p{d,spend};
    std::vector<RationalExpressionForm> values;
    for(const auto& n:tree) {
        spend(1); RationalExpressionForm f;
        if(n.op=='x') f.numerator={0,1};
        else if(n.op=='c') f.numerator={d.check(n.value)};
        else {
            const auto& a=values[n.left];const auto& b=values[n.right];
            if(a.empty_domain || b.empty_domain || (n.op=='/' && p.zero(b.numerator))) {
                f.empty_domain=true;f.excluded_polynomial={0};values.push_back(std::move(f));continue;
            }
            f.excluded_polynomial=p.radical(p.multiply(a.excluded_polynomial,b.excluded_polynomial));
            if(n.op=='+' || n.op=='-') {
                const auto left=p.multiply(a.numerator,b.denominator);
                const auto right=p.multiply(b.numerator,a.denominator);
                f.numerator=p.add(left,right,n.op=='-');
                f.denominator=p.multiply(a.denominator,b.denominator);
            } else if(n.op=='*') {
                f.numerator=p.multiply(a.numerator,b.numerator);f.denominator=p.multiply(a.denominator,b.denominator);
            } else if(n.op=='^') {
                // Raising a defined rational function to a positive integer preserves
                // its original holes, including those removed by cancellation.
                f.numerator={1};f.denominator={1};
                const auto exponent=tree[n.right].value.get_num().get_ui();
                for(unsigned long i=0;i<exponent;++i) {
                    f.numerator=p.multiply(f.numerator,a.numerator);
                    f.denominator=p.multiply(f.denominator,a.denominator);
                }
            } else {
                f.excluded_polynomial=p.radical(p.multiply(f.excluded_polynomial,b.numerator));
                f.numerator=p.multiply(a.numerator,b.denominator);f.denominator=p.multiply(a.denominator,b.numerator);
            }
            p.reduce(f);
        }
        values.push_back(std::move(f));
    }
    return values.back();
}
RationalExpressionForm normalize_fraction(const NumberDomain& d,const std::vector<ExactValue>& numerator,
    const std::vector<ExactValue>& denominator,const ExpressionSpend& spend) {
    if(d.name()!="rational" || !spend) throw Error("InvalidNormalizationConfiguration");
    const Polynomials p{d,spend};
    p.size(numerator.size());p.size(denominator.size());
    RationalExpressionForm f;f.numerator=numerator;f.denominator=denominator;
    for(auto* coefficients:{&f.numerator,&f.denominator})
        for(auto& c:*coefficients) {spend(1);c=d.check(c);}
    f.numerator=p.trim(std::move(f.numerator));f.denominator=p.trim(std::move(f.denominator));
    if(p.zero(f.denominator)) return {{0},{1},{0},true};
    f.excluded_polynomial=p.radical(f.denominator);
    p.reduce(f);return f;
}
ExactValue evaluate_normal_form(const NumberDomain& d,const RationalExpressionForm& form,const ExactValue& x,const ExpressionSpend& spend) {
    if(d.name()!="rational" || !spend) throw Error("InvalidNormalizationConfiguration");
    const Polynomials p{d,spend};
    spend(1);if(form.empty_domain) throw ExpressionPole{};
    const auto input=d.check(x);
    const auto guard=p.evaluate(form.excluded_polynomial,input);
    spend(1);if(guard==0) throw ExpressionPole{};
    const auto numerator=p.evaluate(form.numerator,input),denominator=p.evaluate(form.denominator,input);
    spend(1);if(denominator==0) throw ExpressionPole{};
    spend(1);return d.divide(numerator,denominator);
}
std::string normal_form_key(const RationalExpressionForm& form) {
    std::string key=form.empty_domain ? "empty;" : "defined;";
    for(const auto* p:{&form.numerator,&form.denominator,&form.excluded_polynomial}) {
        key+='[';for(const auto& c:*p) key+=exact_text(c)+';';key+=']';
    }
    return key;
}
RationalDomainComparison compare_rational_domains(const NumberDomain& d,
    const std::vector<ExactValue>& left_exclusion,const std::vector<ExactValue>& right_exclusion,
    const ExpressionSpend& spend,bool modular,bool bounded_witness) {
    if(d.name()!="rational" || !spend || (bounded_witness && !modular)) throw Error("InvalidNormalizationConfiguration");
    const Polynomials p{d,spend};
    auto canonical=[&](Poly values) {
        p.size(values.size());
        for(auto& value:values) {spend(1);value=d.check(value);}
        return p.radical(p.trim(std::move(values)));
    };
    const auto left=canonical(left_exclusion),right=canonical(right_exclusion);
    RationalDomainComparison result;
    const bool left_empty=p.zero(left),right_empty=p.zero(right);
    if(left_empty || right_empty) {
        result.relation=left_empty==right_empty ? "EQUAL" : "DIFFERENT";
        result.reason=left_empty==right_empty ? "BOTH_EMPTY" : "ONE_EMPTY";
        return result;
    }
    const auto common=p.gcd(left,right);
    result.left_remainder=p.exact_divide(left,common);
    result.right_remainder=p.exact_divide(right,common);
    // For a rational discriminant, a rational square root exists exactly when
    // its canonical numerator AND denominator are integer squares.
    auto roots=[&](const Poly& coefficients,std::optional<ExactValue>& witness,
                   unsigned& certificate_prime,std::vector<unsigned>& certificate_values,
                   std::uint64_t& witness_candidates) -> std::string {
        spend(1);
        if(coefficients.size()==1) return "NONE";
        if(coefficients.size()==2) {
            spend(1);const auto negative=d.subtract(0,coefficients[0]);
            spend(1);witness=d.divide(negative,coefficients[1]);
            return "PRESENT";
        }
        if(coefficients.size()>3) {
            if(!modular) return "UNKNOWN";
            // The remainders are monic. A rational root's reduced denominator
            // divides the leading coefficient after clearing denominators.
            // Thus, if all coefficient denominators are units mod p, a rational
            // root would reduce to a root in F_p. Absence in F_p proves absence in Q.
            for(const unsigned prime:{2,3,5,7,11,13,17,19,23,29,31}) {
                spend(1);
                std::vector<unsigned> reduced;
                bool usable=true;
                for(const auto& coefficient:coefficients) {
                    spend(1);const auto den=mpz_fdiv_ui(coefficient.get_den_mpz_t(),prime);
                    if(!den) {usable=false;break;}
                    spend(1);const auto num=mpz_fdiv_ui(coefficient.get_num_mpz_t(),prime);
                    unsigned inverse=1;
                    for(;;++inverse) {spend(1);if((den*inverse)%prime==1) break;}
                    spend(1);reduced.push_back(static_cast<unsigned>((num*inverse)%prime));
                }
                if(!usable) continue;
                std::vector<unsigned> values;
                bool has_root=false;
                for(unsigned x=0;x<prime;++x) {
                    unsigned value=0;
                    for(auto it=reduced.rbegin();it!=reduced.rend();++it) {
                        spend(1);value=(value*x+*it)%prime;
                    }
                    spend(1);if(!value) {has_root=true;break;}
                    values.push_back(value);
                }
                if(!has_root) {
                    certificate_prime=prime;certificate_values=std::move(values);
                    return "NONE";
                }
            }
            if(bounded_witness) {
                // Fixed finite probe, not a rational-root absence theorem.
                // Enumerate reduced a/b: b=1..16, a=0,+1,-1,...,+16,-16.
                for(unsigned denominator=1;denominator<=16;++denominator) {
                    for(unsigned magnitude=0;magnitude<=16;++magnitude) {
                        spend(1);if(std::gcd(magnitude,denominator)!=1) continue;
                        for(const int sign:{1,-1}) {
                            if(!magnitude && sign<0) continue;
                            spend(1);const auto x=d.check(ExactValue(sign*static_cast<long>(magnitude),denominator));
                            const auto value=p.evaluate(coefficients,x);
                            spend(1);++witness_candidates;
                            if(value==0) {witness=x;return "PRESENT";}
                        }
                    }
                }
            }
            return "UNKNOWN";
        }
        spend(1);const auto b2=d.multiply(coefficients[1],coefficients[1]);
        spend(1);const auto ac=d.multiply(coefficients[2],coefficients[0]);
        spend(1);const auto twice=d.add(ac,ac);
        spend(1);const auto four=d.add(twice,twice);
        spend(1);const auto discriminant=d.subtract(b2,four);
        spend(1);if(discriminant<0) return "NONE";
        spend(1);const bool square_n=mpz_perfect_square_p(discriminant.get_num_mpz_t());
        spend(1);const bool square_d=mpz_perfect_square_p(discriminant.get_den_mpz_t());
        if(!square_n || !square_d) return "NONE";
        mpz_class n,den;
        spend(1);mpz_sqrt(n.get_mpz_t(),discriminant.get_num_mpz_t());
        spend(1);mpz_sqrt(den.get_mpz_t(),discriminant.get_den_mpz_t());
        spend(1);const auto top=d.subtract(ExactValue(n,den),coefficients[1]);
        spend(1);const auto bottom=d.add(coefficients[2],coefficients[2]);
        spend(1);witness=d.divide(top,bottom);
        return "PRESENT";
    };
    std::optional<ExactValue> left_witness,right_witness;
    result.left_roots=roots(result.left_remainder,left_witness,result.left_modular_prime,result.left_modular_values,result.left_witness_candidates);
    result.right_roots=roots(result.right_remainder,right_witness,result.right_modular_prime,result.right_modular_values,result.right_witness_candidates);
    result.reason="COPRIME_REMAINDERS";
    if(left_witness || right_witness) {
        result.relation="DIFFERENT";
        result.witness=left_witness ? left_witness : right_witness;
        result.excluded_side=left_witness ? "left" : "right";
        // Independently substitute into the original guards. A witness must
        // be excluded on exactly one side, including after common-factor removal.
        const auto l=p.evaluate(left,*result.witness),r=p.evaluate(right,*result.witness);
        spend(1);
        if((left_witness && (l!=0 || r==0)) || (!left_witness && (r!=0 || l==0)))
            throw Error("DomainWitnessCountercheckFailed");
    } else if(result.left_roots=="NONE" && result.right_roots=="NONE") result.relation="EQUAL";
    return result;
}
}
