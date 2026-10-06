#include "bruce/math/polynomial.hpp"
#include <set>

namespace bruce {
namespace {
void check_size(std::size_t size) {
    if(size<1 || size>9) throw Error("InvalidPolynomialSize");
}
}
std::vector<ExactValue> interpolate_polynomial(const NumberDomain& rational,
    const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend) {
    check_size(xs.size());
    if(xs.size()!=ys.size() || rational.name()!="rational" || !spend) throw Error("InvalidInterpolationInput");
    std::vector<ExactValue> x,work;
    std::set<std::string> seen;
    for(std::size_t i=0;i<xs.size();++i) {
        x.push_back(rational.check(xs[i])); work.push_back(rational.check(ys[i]));
        if(!seen.insert(exact_text(x.back())).second) throw Error("DuplicateInterpolationInput");
    }
    for(std::size_t order=1;order<x.size();++order) {
        for(std::size_t i=x.size()-1;i>=order;--i) {
            spend(1); const auto numerator=rational.subtract(work[i],work[i-1]);
            spend(1); const auto denominator=rational.subtract(x[i],x[i-order]);
            spend(1); work[i]=rational.divide(numerator,denominator);
        }
    }
    std::vector<ExactValue> coefficients{work.back()};
    for(std::size_t k=x.size()-1;k>0;--k) {
        std::vector<ExactValue> next(coefficients.size()+1,0);
        for(std::size_t j=0;j<coefficients.size();++j) {
            spend(1); const auto product=rational.multiply(coefficients[j],x[k-1]);
            spend(1); next[j]=rational.subtract(next[j],product);
            spend(1); next[j+1]=rational.add(next[j+1],coefficients[j]);
        }
        spend(1); next[0]=rational.add(next[0],work[k-1]);
        coefficients=std::move(next);
    }
    return coefficients;
}
ExactValue polynomial_horner(const NumberDomain& domain,const std::vector<ExactValue>& coefficients,const ExactValue& x) {
    check_size(coefficients.size());
    const auto input=domain.check(x);
    auto value=domain.check(coefficients.back());
    for(std::size_t i=coefficients.size()-1;i>0;--i)
        value=domain.add(domain.multiply(value,input),coefficients[i-1]);
    return value;
}
ExactValue polynomial_sum(const NumberDomain& domain,const std::vector<ExactValue>& coefficients,const ExactValue& x) {
    check_size(coefficients.size());
    const auto input=domain.check(x);
    ExactValue value=0,power=1;
    for(std::size_t i=0;i<coefficients.size();++i) {
        value=domain.add(value,domain.multiply(coefficients[i],power));
        if(i+1<coefficients.size()) power=domain.multiply(power,input);
    }
    return value;
}
std::string polynomial_formula(const std::vector<ExactValue>& coefficients) {
    check_size(coefficients.size());
    std::string result;
    for(std::size_t i=0;i<coefficients.size();++i) {
        if(i) result+='+';
        result+='('+exact_text(coefficients[i])+')';
        if(i) result+="*x^"+std::to_string(i);
    }
    return result;
}
}
