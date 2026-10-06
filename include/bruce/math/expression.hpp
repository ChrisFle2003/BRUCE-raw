#pragma once
#include "bruce/math/numbers.hpp"
#include <array>
#include <functional>

namespace bruce {
using ExpressionSpend=std::function<void(std::uint64_t)>;
struct ExpressionNode {
    char op='x'; // x, c, +, -, *, /, ^; ^ requires a literal exponent 2..8.
    unsigned left=0,right=0;
    ExactValue value=0;
};
using Expression=std::vector<ExpressionNode>;
struct ExpressionPole : Error { ExpressionPole():Error("ExpressionDivisionByZero") {} };
std::string canonical_expression_operators(std::string_view);
struct ExpressionCacheStats {std::uint64_t hits=0,misses=0,evictions=0,entries=0;};
class ExpressionGrammar {
public:
    ExpressionGrammar(std::vector<ExactValue> constants,unsigned max_nodes,const ExpressionSpend&,bool indexed=false,std::string operators="+-*/",bool cached=false);
    std::uint64_t size() const { return total_; }
    std::size_t index_entries() const;
    ExpressionCacheStats cache_stats() const {return cache_stats_;}
    Expression at(std::uint64_t id,const ExpressionSpend&) const;
private:
    std::vector<ExactValue> constants_;
    unsigned max_nodes_;
    bool indexed_,cached_;
    struct CacheEntry {unsigned size=0;std::uint64_t rank=0;Expression tree;};
    mutable std::array<CacheEntry,64> cache_;
    mutable ExpressionCacheStats cache_stats_;

    std::string operators_;
    std::vector<std::uint64_t> exponent_ranks_;
    struct Block {char op;unsigned left,right;std::uint64_t begin,end;};
    std::array<std::vector<Block>,10> blocks_;
    unsigned append_indexed(Expression&,unsigned size,std::uint64_t rank,const ExpressionSpend&,bool cacheable=false) const;
    std::array<std::uint64_t,10> counts_{};
    std::uint64_t total_=0;
    unsigned append(Expression&,unsigned size,std::uint64_t rank,const ExpressionSpend&) const;
};
void validate_expression(const Expression&);
ExactValue evaluate_expression(const NumberDomain&,const Expression&,const ExactValue& x,const ExpressionSpend&);
ExactValue counter_evaluate_expression(const NumberDomain&,const Expression&,const ExactValue& x,const ExpressionSpend&);
std::string expression_formula(const Expression&);
std::vector<std::string> expression_conditions(const Expression&);
}
