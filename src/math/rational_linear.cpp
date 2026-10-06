#include "bruce/math/rational_linear.hpp"

namespace bruce {
ExactValue rational_linear(const NumberDomain& d,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& x) {
    const auto denominator=d.add(x,c);
    if(denominator==0) throw RationalPole{};
    return d.divide(d.add(d.multiply(a,x),b),denominator);
}
bool rational_linear_countercheck(const NumberDomain& d,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& x,const ExactValue& y) {
    if(d.add(x,c)==0) throw RationalPole{};
    return d.add(d.multiply(y,x),d.multiply(y,c))==d.add(d.multiply(a,x),b);
}
ExactValue rational_quadratic(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& d,const ExactValue& x) {
    const auto denominator=domain.add(domain.multiply(domain.add(x,c),x),d);
    if(denominator==0) throw RationalPole{};
    return domain.divide(domain.add(domain.multiply(a,x),b),denominator);
}
bool rational_quadratic_countercheck(const NumberDomain& domain,const ExactValue& a,const ExactValue& b,const ExactValue& c,const ExactValue& d,const ExactValue& x,const ExactValue& y) {
    const auto denominator=domain.add(domain.add(domain.multiply(x,x),domain.multiply(c,x)),d);
    if(denominator==0) throw RationalPole{};
    const auto quadratic=domain.multiply(domain.multiply(y,x),x);
    const auto linear=domain.multiply(domain.multiply(y,c),x);
    return domain.add(domain.add(quadratic,linear),domain.multiply(y,d))==domain.add(domain.multiply(a,x),b);
}
namespace {
template<std::size_t N>
RationalFractionPlan<N> solve_fraction_system(const NumberDomain& d,const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend) {
    if(d.name()!="rational" || xs.empty() || xs.size()!=ys.size()) throw Error("InvalidRationalLinearSystem");
    using Row=std::array<ExactValue,N+1>;
    std::array<std::optional<Row>,N> basis;
    RationalFractionPlan<N> plan;
    for(std::size_t i=0;i<xs.size();++i) {
        const auto x=d.check(xs[i]),y=d.check(ys[i]);
        spend(1); const auto minus_y=d.subtract(0,y);
        Row row{};
        if constexpr(N==3) {
            spend(1); row={x,1,minus_y,d.multiply(x,y)};
        } else {
            spend(1); const auto minus_yx=d.multiply(minus_y,x);
            spend(1); const auto x2=d.multiply(x,x);
            spend(1); row={x,1,minus_yx,minus_y,d.multiply(x2,y)};
        }
        ++plan.rows;
        bool inserted=false;
        for(unsigned col=0;col<N;++col) {
            spend(1); if(row[col]==0) continue;
            const ExactValue factor=row[col];
            if(basis[col]) {
                for(unsigned j=col;j<=N;++j) {
                    spend(1); const auto product=d.multiply(factor,(*basis[col])[j]);
                    spend(1); row[j]=d.subtract(row[j],product);
                }
            } else {
                for(unsigned j=col;j<=N;++j) { spend(1); row[j]=d.divide(row[j],factor); }
                basis[col]=row; ++plan.rank; inserted=true; break;
            }
        }
        if(!inserted) { spend(1); if(row[N]!=0) { plan.decision="inconsistent"; return plan; } }
    }
    if(plan.rank<N) return plan;
    for(int col=N-1;col>=0;--col) {
        auto value=(*basis[col])[N];
        for(unsigned j=col+1;j<N;++j) {
            spend(1); const auto product=d.multiply((*basis[col])[j],plan.coefficients[j]);
            spend(1); value=d.subtract(value,product);
        }
        plan.coefficients[col]=value;
    }
    plan.decision="derived"; return plan;
}
}
RationalLinearPlan solve_rational_linear(const NumberDomain& d,const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend) { return solve_fraction_system<3>(d,xs,ys,spend); }
RationalQuadraticPlan solve_rational_quadratic(const NumberDomain& d,const std::vector<ExactValue>& xs,const std::vector<ExactValue>& ys,
    const std::function<void(std::uint64_t)>& spend) { return solve_fraction_system<4>(d,xs,ys,spend); }
}
