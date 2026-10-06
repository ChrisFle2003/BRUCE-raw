#include "bruce/application/exact_planner.hpp"

namespace bruce {
CoefficientPlan plan_coefficients(const ExactResearchTask& task,unsigned power,
    const std::function<void(std::uint64_t)>& spend) {
    const NumberDomain rational("rational",task.max_bits);
    const auto& first=task.training.front();
    spend(2*power);
    const auto u0=rational.power(first.x,power);
    for(std::size_t i=1;i<task.training.size();++i) {
        spend(2*power);
        const auto ui=rational.power(task.training[i].x,power);
        spend(2);
        if(ui==u0) {
            if(task.training[i].y!=first.y) return {"inconsistent_projection",0,0,i};
            continue;
        }
        spend(1); const auto dy=rational.subtract(task.training[i].y,first.y);
        spend(1); const auto du=rational.subtract(ui,u0);
        spend(1); const auto a=rational.divide(dy,du);
        spend(1); const auto product=rational.multiply(a,u0);
        spend(1); const auto b=rational.subtract(first.y,product);
        const auto low=rational.parse_integer(task.minimum),high=rational.parse_integer(task.maximum);
        auto slot=[&](const ExactValue& value)->std::optional<std::uint64_t> {
            const auto numerator=value.get_num(),denominator=value.get_den();
            if(numerator<low || numerator>high || denominator>task.max_denominator) return std::nullopt;
            const mpz_class index=(numerator-low)*task.max_denominator+denominator-1;
            return index.get_ui();
        };
        const auto ai=slot(a),bi=slot(b);
        if(!ai || !bi || (power>1 && a==0)) return {"outside_family",0,0,i};
        return {"derived",*ai,*bi,i};
    }
    return {};
}
}
