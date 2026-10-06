#include "bruce/application/research_contract.hpp"
#include "bruce/application/research.hpp"
#include <algorithm>
#include <set>
namespace bruce {
namespace {
constexpr std::string_view enumerated[]={"enumerate"};
constexpr std::string_view power[]={"enumerate","adaptive"};
constexpr std::string_view polynomial[]={"interpolate"};
constexpr std::string_view rational[]={"solve"};
constexpr std::string_view expression[]={"grammar","indexed-grammar","cached-grammar"};
constexpr std::string_view fixed_portfolio[]={"portfolio"};
constexpr std::string_view portfolio[]={"portfolio","adaptive-order","experience-order"};
const ResearchStrategy strategies[]={
    {"affine","enumerate",ResearchExecutor::affine_power,enumerated},
    {"power-affine","enumerate",ResearchExecutor::affine_power,power},
    {"polynomial","interpolate",ResearchExecutor::polynomial,polynomial},
    {"rational-linear","solve",ResearchExecutor::rational,rational},
    {"rational-quadratic","solve",ResearchExecutor::rational,rational},
    {"expression","grammar",ResearchExecutor::expression,expression},
    {"portfolio","portfolio",ResearchExecutor::portfolio,fixed_portfolio},
    {"portfolio-rational","portfolio",ResearchExecutor::portfolio,portfolio},
    {"portfolio-quadratic","portfolio",ResearchExecutor::portfolio,portfolio},
    {"portfolio-expression","portfolio",ResearchExecutor::portfolio,portfolio}
};
}
std::span<const ResearchStrategy> research_strategies(){return strategies;}
const ResearchStrategy& research_strategy(std::string_view id){
    for(const auto& s:strategies) if(s.id==id) return s;
    throw Error("UnknownResearchStrategy");
}
void validate_research_route(std::string_view id,std::string_view planner){
    bool known=false;
    for(const auto& s:strategies) for(auto p:s.planners) known|=p==planner;
    if(!known) throw Error("UnknownSearchPlanner");
    const auto& strategy=research_strategy(id);
    if(std::find(strategy.planners.begin(),strategy.planners.end(),planner)!=strategy.planners.end()) return;
    if(planner=="adaptive-order" || planner=="experience-order") throw Error("AdaptiveOrderRequiresRationalPortfolio");
    if(strategy.executor==ResearchExecutor::expression || planner=="grammar" || planner=="indexed-grammar" || planner=="cached-grammar") throw Error("ExpressionRequiresGrammarPlanner");
    if(strategy.executor==ResearchExecutor::rational || planner=="solve") throw Error("RationalLinearRequiresSolvePlanner");
    if(strategy.executor==ResearchExecutor::portfolio || planner=="portfolio") throw Error("PortfolioRequiresPortfolioPlanner");
    if(strategy.executor==ResearchExecutor::polynomial || planner=="interpolate") throw Error("PolynomialRequiresInterpolationPlanner");
    throw Error("AdaptiveRequiresPowerStrategy");
}
void validate_exact_resource_limits(const ExactResearchTask& t) {
    if(!t.max_denominator || t.max_denominator>16 || (t.domain=="bigint" && t.max_denominator!=1))
        throw Error("InvalidDenominatorLimit");
    const auto& b=t.budget;
    if(!b.candidates || b.candidates>1000000 || !b.operations || b.operations>10000000 ||
       b.trace_events<2 || b.trace_events>100002 || !b.results || b.results>((t.strategy=="expression" || t.strategy=="portfolio-expression") && t.expression_equivalence=="rational-q-witness" ? 256U : 128U) ||
       t.max_trace_bytes<256 || t.max_trace_bytes>4194304) throw Error("InvalidExactBudget");
}
std::uint64_t validate_exact_task(ExactResearchTask& t,const NumberDomain& domain) {
    validate_research_route(t.strategy,t.planner);
    if((t.strategy=="portfolio-rational" || t.strategy=="portfolio-quadratic" || t.strategy=="portfolio-expression") && t.domain!="rational") throw Error("RationalPortfolioRequiresRationalDomain");
    if((t.strategy=="rational-linear" || t.strategy=="rational-quadratic") && (t.domain!="rational" || t.max_power!=1)) throw Error("InvalidRationalLinearDomainOrPower");
    if((t.strategy=="portfolio" || (t.strategy=="portfolio-rational" || t.strategy=="portfolio-quadratic" || t.strategy=="portfolio-expression")) && t.max_degree>8) throw Error("InvalidPolynomialDegree");
    if(t.strategy=="polynomial" && (t.max_degree>8 || t.max_power!=1)) throw Error("InvalidPolynomialDegree");
    if((t.planner=="experience-order")!=!t.experience_snapshot.empty() || t.experience_snapshot.size()>1024*1024) throw Error("InvalidExperienceConfiguration");
    if(t.domain_cache!="none" && t.domain_cache!="equal-64") throw Error("UnknownDomainCache");
    if(t.domain_cache!="none" && ((t.strategy!="expression" && t.strategy!="portfolio-expression") || t.expression_equivalence!="rational-q-modular")) throw Error("DomainCacheRequiresModularExpression");
    t.expression_operators=canonical_expression_operators(t.expression_operators);
    if(t.expression_operators!="+-*/" && t.strategy!="expression" && t.strategy!="portfolio-expression") throw Error("OperatorsRequireExpressionStrategy");
    if(t.expression_planner!="grammar" && t.expression_planner!="indexed-grammar" && t.expression_planner!="cached-grammar") throw Error("UnknownExpressionPlanner");
    if(t.expression_planner!="grammar" && t.strategy!="portfolio-expression") throw Error("ExpressionPlannerRequiresFiveFamilyPortfolio");
    if(t.expression_equivalence!="syntax" && t.expression_equivalence!="rational" && t.expression_equivalence!="rational-q" && t.expression_equivalence!="rational-q-modular" && t.expression_equivalence!="rational-q-witness") throw Error("UnknownExpressionEquivalence");
    if(t.strategy!="expression" && t.strategy!="portfolio-expression" && t.expression_equivalence!="syntax") throw Error("EquivalenceRequiresExpressionStrategy");
    if(t.strategy=="portfolio-expression" && ((t.planner!="portfolio" && t.planner!="adaptive-order" && t.planner!="experience-order") || (t.expression_equivalence!="rational" && t.expression_equivalence!="rational-q" && t.expression_equivalence!="rational-q-modular" && t.expression_equivalence!="rational-q-witness") || t.max_nodes<1 || t.max_nodes>9 || t.max_nodes%2==0)) throw Error("InvalidExpressionPortfolioConfiguration");
    if(t.strategy=="expression" && (t.domain!="rational" || t.max_nodes<1 || t.max_nodes>9 || t.max_nodes%2==0 || t.max_power!=1)) throw Error("InvalidExpressionConfiguration");
    if(t.planner=="enumerate" && t.cancel_after_checkpoints) throw Error("CheckpointRequiresAdaptivePlanner");
    if(t.max_power<1 || t.max_power>8 || (t.strategy=="affine" && t.max_power!=1))
        throw Error("InvalidPowerLimit");
    const auto low=domain.parse_integer(t.minimum), high=domain.parse_integer(t.maximum);
    const mpz_class width=high-low+1;
    if(width<1 || width>4096) throw Error("InvalidExactSearchRange: width must be 1..4096");
    t.minimum=low.get_str(); t.maximum=high.get_str();
    validate_exact_resource_limits(t);
    if(t.training.empty() || t.training.size()>4096 || t.holdout.size()>4096) throw Error("InvalidSampleCount");
    std::set<std::string> seen;
    std::size_t sample_bytes=0;
    for(auto* samples:{&t.training,&t.holdout}) {
        for(auto& s:*samples) {
            s.x=domain.check(s.x); s.y=domain.check(s.y);
            const auto x=exact_text(s.x),y=exact_text(s.y);
            sample_bytes+=x.size()+y.size()+2;
            if(sample_bytes>1024*1024) throw Error("SampleByteLimit");
            if(!seen.insert(x).second) throw Error("DuplicateOrOverlappingSampleInput");
        }
    }
    if((t.strategy=="rational-quadratic" || t.strategy=="portfolio-quadratic") && width.get_ui()*t.max_denominator>32768) throw Error("QuadraticParameterSlotLimit");
    if((t.strategy=="expression" || t.strategy=="portfolio-expression") && width.get_ui()*t.max_denominator>64) throw Error("ExpressionConstantSlotLimit");
    return width.get_ui()*t.max_denominator;
}
std::string render_research_contract(){
    std::string out="{\"contract\":\"exact-research-task-v1\",\"dynamic_plugins\":false,\"scope\":\"Built-in exact research; numeric validation and evidence gates; no universal phase runtime\",\"gates\":{\"sample_evidence_only\":true,\"learn_promotion\":false,\"publish\":false},\"strategies\":[";
    bool first=true;
    for(const auto& s:strategies){
        if(!first) out+=',';
        first=false;
        out+="{\"id\":"+json_string(s.id)+",\"default_planner\":"+json_string(s.default_planner)+",\"planners\":[";
        bool first_planner=true;
        for(auto p:s.planners){if(!first_planner) out+=',';first_planner=false;out+=json_string(p);}
        out+="]}";
    }
    return out+"]}";
}
}
