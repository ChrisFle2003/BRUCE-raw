#include "bruce/application/portfolio_version.hpp"
#include <array>

namespace bruce {
namespace {
constexpr std::array<std::string_view,9> fixed_planners={
    "five-family-v1","five-family-indexed-v1","five-family-selected-v1",
    "five-family-power-v1","five-family-cached-v1","five-family-q-v1",
    "five-family-q-modular-v1","five-family-q-cache-v1","five-family-q-witness-v1"};
PortfolioVersion make_version(unsigned generation,unsigned order) {
    const auto number=std::to_string(8+generation*3+order);
    const auto routing=std::to_string(3+generation);
    return {"BRUCE_PORTFOLIO_BUNDLE_V"+number+"\n","bruce-portfolio-v"+number,
        order==0 ? std::string(fixed_planners[generation]) :
        order==1 ? "sample-shape-v"+routing : "experience-order-v"+routing};
}
}
PortfolioVersion portfolio_version(const ExactResearchTask& task) {
    if(task.strategy!="portfolio-expression" ||
       (task.planner!="portfolio" && task.planner!="adaptive-order" && task.planner!="experience-order"))
        throw Error("InvalidFiveFamilyVersionRequest");
    const unsigned generation=task.expression_equivalence=="rational-q-witness" ? 8 :
        task.domain_cache=="equal-64" ? 7 : task.expression_equivalence=="rational-q-modular" ? 6 :
        task.expression_equivalence=="rational-q" ? 5 : task.expression_planner=="cached-grammar" ? 4 :
        task.expression_operators.find('^')!=std::string::npos ? 3 : task.expression_operators!="+-*/" ? 2 :
        task.expression_planner=="indexed-grammar" ? 1 : 0;
    return make_version(generation,task.planner=="experience-order" ? 2 : task.planner=="adaptive-order" ? 1 : 0);
}
std::optional<PortfolioVersion> portfolio_version_from_header(std::string_view bundle) {
    for(unsigned generation=0;generation<fixed_planners.size();++generation)
        for(unsigned order=0;order<3;++order) {
            auto version=make_version(generation,order);
            if(bundle.starts_with(version.header)) return version;
        }
    return {};
}
}
