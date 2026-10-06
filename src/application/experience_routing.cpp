#include "bruce/application/experience_routing.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
namespace bruce {
std::string experience_training_key(const ExactResearchTask& t) {
    std::vector<std::string> pairs;
    for(const auto& s:t.training) pairs.push_back(exact_text(s.x)+" "+exact_text(s.y)+"\n");
    std::sort(pairs.begin(),pairs.end());
    std::string body;
    for(const auto& pair:pairs) body+=pair;
    return sha256(body);
}
std::string experience_context_key(const ExactResearchTask& t,const std::string& shape) {
    if(t.strategy=="portfolio-expression" && t.domain_cache=="equal-64") return sha256(std::string("routing-context-v10-five-q-cache\nequal-64-key4096-v1\nexpression-and-union-separate-v1\nrational-domain-gcd-modular-v1\nq-domain-union-v1\nshared-budget-v3\nliteral-2-to-8-v1\n")+
        t.expression_planner+"\n"+(t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")+"\n"+t.expression_operators+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\n"+shape);
    if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q-witness") return sha256(std::string("routing-context-v11-five-q-witness\nrational-domain-gcd-modular-witness16-v1\nq-domain-union-v1\nshared-budget-v4\nliteral-2-to-8-v1\n")+
        t.expression_planner+"\n"+(t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")+"\n"+t.expression_operators+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\n"+shape);
    if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q-modular") return sha256(std::string("routing-context-v9-five-q-modular\nrational-domain-gcd-modular-v1\nq-domain-union-v1\nshared-budget-v3\nliteral-2-to-8-v1\n")+
        t.expression_planner+"\n"+(t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")+"\n"+t.expression_operators+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\n"+shape);
    if(t.strategy=="portfolio-expression" && t.expression_equivalence=="rational-q") return sha256(std::string("routing-context-v8-five-q\nrational-domain-gcd-quadratic-v1\nq-domain-union-v1\nshared-budget-v3\nliteral-2-to-8-v1\n")+
        t.expression_planner+"\n"+(t.expression_planner=="cached-grammar" ? "direct-map-64-size3-v1" : "none")+"\n"+t.expression_operators+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\n"+shape);
    if(t.strategy=="portfolio-expression" && t.expression_planner=="cached-grammar") return sha256(std::string("routing-context-v7-five-cached\nsubtree-cache-v1\ndirect-map-64-size3-v1\nliteral-2-to-8-v1\n")+
        t.expression_operators+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+
        std::to_string(t.max_denominator)+"\n"+std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+
        std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\nnormal-form-union-v1\n"+shape);
    if(t.strategy=="portfolio-expression" && t.expression_operators!="+-*/") return sha256(std::string(t.expression_operators.find('^')!=std::string::npos ? "routing-context-v6-five-power\nliteral-2-to-8-v1\n" : "routing-context-v5-five-selected\n")+
        t.expression_operators+"\n"+(t.expression_planner=="indexed-grammar" ? "block-index-v1" : "size-ranked-v1")+"\n"+
        t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+std::to_string(t.max_bits)+"\n"+
        std::to_string(t.max_denominator)+"\n"+std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+
        std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\nnormal-form-union-v1\n"+shape);
    if(t.strategy=="portfolio-expression") return sha256(std::string(t.expression_planner=="indexed-grammar" ? "routing-context-v4-five-indexed\nblock-index-v1\n" : "routing-context-v3-five\n")+t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+
        std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+
        std::to_string(t.max_nodes)+"\n"+t.expression_equivalence+"\nnormal-form-union-v1\n"+shape);
    return sha256(std::string(t.strategy=="portfolio-quadratic" ? "routing-context-v2-four\n" : "routing-context-v1\n")+t.domain+"\n"+t.minimum+"\n"+t.maximum+"\n"+
        std::to_string(t.max_bits)+"\n"+std::to_string(t.max_denominator)+"\n"+
        std::to_string(t.max_power)+"\n"+std::to_string(t.max_degree)+"\n"+shape);
}
void rank_experience(const ExperienceStore& store,const ExactResearchTask& task,PortfolioOrder& plan,
    std::vector<std::uint64_t>& votes,std::uint64_t& used,const std::function<void(std::uint64_t)>& spend) {
    const std::size_t count=task.strategy=="portfolio-expression" ? 5 : task.strategy=="portfolio-quadratic" ? 4 : 3;
    if(votes.size()!=count || plan.strategies.size()!=count) throw Error("InvalidExperienceRankingShape");
    std::array<bool,5> seen{};
    for(auto index:plan.strategies) {
        if(index>=count || seen[index]) throw Error("InvalidExperienceRankingShape");
        seen[index]=true;
    }
    const auto key=experience_training_key(task),context=experience_context_key(task,plan.reason);
    for(const auto& record:store.records) {
        spend(1);
        if(!record.active || record.training_fingerprint==key || record.context!=context || record.votes.size()!=votes.size()) continue;
        ++used;
        for(std::size_t i=0;i<votes.size();++i) {spend(1); votes[i]+=record.votes[i];}
    }
    // Stable insertion sort: equal scores retain the current shape heuristic's order.
    for(unsigned i=1;i<plan.strategies.size();++i) {
        unsigned j=i;
        while(j) {
            spend(1);
            if(votes[plan.strategies[j]]<=votes[plan.strategies[j-1]]) break;
            std::swap(plan.strategies[j],plan.strategies[j-1]); --j;
        }
    }
    if(used) plan.reason="experience_votes:"+plan.reason;
}
}
