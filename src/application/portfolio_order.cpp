#include "bruce/application/portfolio_order.hpp"
namespace bruce {
PortfolioOrder plan_portfolio_order(const ExactResearchTask& task,const std::function<void(std::uint64_t)>& spend) {
    if(task.strategy=="portfolio-expression") {
        auto prefix_task=task;prefix_task.strategy="portfolio-quadratic";
        auto result=plan_portfolio_order(prefix_task,spend);
        result.strategies.push_back(4);return result;
    }
    PortfolioOrder result;
    const bool quadratic=task.strategy=="portfolio-quadratic";
    if(quadratic) result.strategies.push_back(3);
    if(task.training.size()<3) return result;
    const NumberDomain d("rational",task.max_bits);
    const auto& s=task.training;
    auto sub=[&](const ExactValue& a,const ExactValue& b) { spend(1); return d.subtract(a,b); };
    auto div=[&](const ExactValue& a,const ExactValue& b) { spend(1); return d.divide(a,b); };
    auto equal=[&](const ExactValue& a,const ExactValue& b) { spend(1); return a==b; };
    const auto dx01=sub(s[1].x,s[0].x),dx12=sub(s[2].x,s[1].x);
    const auto dy01=sub(s[1].y,s[0].y),dy12=sub(s[2].y,s[1].y);
    const auto slope01=div(dy01,dx01),slope12=div(dy12,dx12);
    if(equal(slope01,slope12)) { result.reason="affine_prefix"; return result; }
    bool nonzero=true;
    for(unsigned i=0;i<3;++i) if(equal(s[i].y,0)) nonzero=false;
    if(nonzero) {
        const auto y0=div(1,s[0].y),y1=div(1,s[1].y),y2=div(1,s[2].y);
        const auto dy0=sub(y1,y0),dy1=sub(y2,y1);
        const auto inv01=div(dy0,dx01),inv12=div(dy1,dx12);
        if(equal(inv01,inv12)) {
            result.strategies=quadratic ? std::vector<unsigned>{2,0,1,3} : std::vector<unsigned>{2,0,1}; result.reason="reciprocal_affine_prefix"; return result;
        }
    }
    // V2 tie rule: reciprocal-quadratic hints precede direct-quadratic hints.
    if(quadratic && nonzero && s.size()>=4 && !equal(s[3].y,0)) {
        std::array<ExactValue,4> inverse;
        for(unsigned i=0;i<4;++i) inverse[i]=div(1,s[i].y);
        const auto first=div(sub(inverse[1],inverse[0]),dx01);
        const auto second=div(sub(inverse[2],inverse[1]),dx12);
        const auto third=div(sub(inverse[3],inverse[2]),sub(s[3].x,s[2].x));
        const auto left=div(sub(second,first),sub(s[2].x,s[0].x));
        const auto right=div(sub(third,second),sub(s[3].x,s[1].x));
        if(equal(left,right)) {result.strategies={3,0,1,2}; result.reason="reciprocal_quadratic_prefix"; return result;}
    }
    if(s.size()>=4) {
        const auto dx23=sub(s[3].x,s[2].x),dy23=sub(s[3].y,s[2].y);
        const auto slope23=div(dy23,dx23);
        const auto delta0=sub(slope12,slope01),delta1=sub(slope23,slope12);
        const auto span0=sub(s[2].x,s[0].x),span1=sub(s[3].x,s[1].x);
        const auto second0=div(delta0,span0),second1=div(delta1,span1);
        if(equal(second0,second1)) { result.strategies=quadratic ? std::vector<unsigned>{1,0,2,3} : std::vector<unsigned>{1,0,2}; result.reason="quadratic_prefix"; return result; }
    }
    result.reason="no_shape_hint"; return result;
}
}
