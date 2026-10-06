#include "bruce/math/expression.hpp"
#include <set>

namespace bruce {
namespace {
bool commutative(char op) {return op=='+' || op=='*';}
std::uint64_t pairs(std::uint64_t a,std::uint64_t b,bool triangular) {
    // At most 65 terminals and nine nodes: every product here is below 2^50.
    return triangular ? a*(a+1)/2 : a*b;
}
void validate(const Expression& tree) {
    if(tree.empty() || tree.size()>9 || tree.size()%2==0) throw Error("InvalidExpressionTree");
    std::vector<unsigned> references(tree.size());
    for(unsigned i=0;i<tree.size();++i) {
        const auto& n=tree[i];
        if(n.op=='x' || n.op=='c') continue;
        if(n.op!='+' && n.op!='-' && n.op!='*' && n.op!='/' && n.op!='^') throw Error("InvalidExpressionOperator");
        if(n.left>=i || n.right>=i) throw Error("InvalidExpressionChild");
        if(n.op=='^') {
            const auto& exponent=tree[n.right];
            if(exponent.op!='c' || exponent.value.get_den()!=1 || exponent.value<2 || exponent.value>8)
                throw Error("InvalidExpressionExponent");
        }
        ++references[n.left]; ++references[n.right];
    }
    for(unsigned i=0;i+1<tree.size();++i) if(references[i]!=1) throw Error("InvalidExpressionTree");
}
std::vector<std::string> formulas(const Expression& tree) {
    validate(tree);
    std::vector<std::string> result;
    for(const auto& n:tree) {
        if(n.op=='x') result.emplace_back("x");
        else if(n.op=='c') {
            const auto literal=exact_text(n.value);
            result.push_back(n.value.get_den()!=1 || n.value<0 ? "("+literal+")" : literal);
        }
        else result.push_back("("+result[n.left]+n.op+result[n.right]+")");
    }
    return result;
}
}
std::string canonical_expression_operators(std::string_view operators) {
    if(operators.empty() || operators.size()>5) throw Error("InvalidExpressionOperators");
    std::string result;
    for(char op:operators) {
        if(std::string_view("+-*/^").find(op)==std::string_view::npos || operators.find(op)!=operators.rfind(op))
            throw Error("InvalidExpressionOperators");
    }
    for(char op:std::string_view("+-*/^")) if(operators.find(op)!=std::string_view::npos) result+=op;
    return result;
}
void validate_expression(const Expression& tree) {validate(tree);}
ExpressionGrammar::ExpressionGrammar(std::vector<ExactValue> constants,unsigned max_nodes,const ExpressionSpend& spend,bool indexed,std::string operators,bool cached)
    :constants_(std::move(constants)),max_nodes_(max_nodes),indexed_(indexed),cached_(cached),operators_(canonical_expression_operators(operators)) {
    if(cached_ && !indexed_) throw Error("ExpressionCacheRequiresIndex");
    if(constants_.size()>64 || max_nodes_<1 || max_nodes_>9 || max_nodes_%2==0) throw Error("InvalidExpressionGrammar");
    std::set<std::string> seen;
    for(auto& c:constants_) {
        spend(1);
        if(c.get_den()==0) throw Error("ZeroDenominator");
        c.canonicalize();
        if(!seen.insert(exact_text(c)).second) throw Error("DuplicateExpressionConstant");
    }
    if(operators_.find('^')!=std::string::npos) for(std::size_t i=0;i<constants_.size();++i) {
        spend(1);const auto& c=constants_[i];
        if(c.get_den()==1 && c>=2 && c<=8) exponent_ranks_.push_back(i+1);
    }
    counts_[1]=constants_.size()+1; total_=counts_[1];
    for(unsigned size=3;size<=max_nodes_;size+=2) {
        for(char op:operators_) for(unsigned left=1;left<size-1;left+=2) {
            spend(1);
            const unsigned right=size-1-left;
            if(commutative(op) && left>right) continue;
            if(op=='^' && (right!=1 || exponent_ranks_.empty())) continue;
            const auto begin=counts_[size];
            counts_[size]+=op=='^' ? counts_[left]*exponent_ranks_.size() : pairs(counts_[left],counts_[right],commutative(op) && left==right);
            if(indexed_) {
                spend(1);blocks_[size].push_back({op,left,right,begin,counts_[size]});
            }
        }
        total_+=counts_[size];
    }
}
std::size_t ExpressionGrammar::index_entries() const {
    std::size_t result=0;for(const auto& blocks:blocks_) result+=blocks.size();return result;
}
unsigned ExpressionGrammar::append_indexed(Expression& tree,unsigned size,std::uint64_t rank,const ExpressionSpend& spend,bool cacheable) const {
    const bool use_cache=cached_ && cacheable && size==3;
    const auto original_rank=rank;
    const auto base=tree.size();
    const auto slot=(rank*5+size)%cache_.size();
    if(use_cache) {
        spend(1); // Direct slot probe, including full key comparison.
        const auto& entry=cache_[slot];
        if(entry.size==size && entry.rank==rank) {
            spend(size); // Each copied node and its index relocation.
            for(auto node:entry.tree) {
                if(node.op!='x' && node.op!='c') {node.left+=base;node.right+=base;}
                tree.push_back(std::move(node));
            }
            ++cache_stats_.hits;
            return tree.size()-1;
        }
        ++cache_stats_.misses;
    }
    spend(1);
    if(size==1) {
        tree.push_back(rank==0 ? ExpressionNode{} : ExpressionNode{'c',0,0,constants_.at(rank-1)});
        return tree.size()-1;
    }
    const auto& blocks=blocks_[size];
    std::size_t low=0,high=blocks.size();
    while(low<high) {
        spend(1);const auto middle=low+(high-low)/2;
        if(rank<blocks[middle].end) high=middle;else low=middle+1;
    }
    if(low==blocks.size()) throw Error("ExpressionRankOutOfRange");
    const auto& block=blocks[low];rank-=block.begin;
    std::uint64_t li,ri;
    if(block.op=='^') {li=rank/exponent_ranks_.size();ri=exponent_ranks_[rank%exponent_ranks_.size()];}
    else if(commutative(block.op) && block.left==block.right) {
        const auto n=counts_[block.left];
        auto offset=[&](std::uint64_t i){return i*(2*n-i+1)/2;};
        std::uint64_t first=0,last=n;
        while(first+1<last) {
            spend(1);const auto middle=first+(last-first)/2;
            if(offset(middle)<=rank) first=middle;else last=middle;
        }
        li=first;ri=li+rank-offset(li);
    } else {li=rank/counts_[block.right];ri=rank%counts_[block.right];}
    const auto l=append_indexed(tree,block.left,li,spend,true),r=append_indexed(tree,block.right,ri,spend,true);
    tree.push_back({block.op,l,r,0});
    if(use_cache) {
        auto& entry=cache_[slot];
        spend(size+1+(entry.size ? 1 : 0)); // Copy, insert and optional eviction, before mutation.
        Expression copy(tree.begin()+base,tree.end());
        for(auto& node:copy) if(node.op!='x' && node.op!='c') {node.left-=base;node.right-=base;}
        if(entry.size) ++cache_stats_.evictions;else ++cache_stats_.entries;
        entry={size,original_rank,std::move(copy)};
    }
    return tree.size()-1;
}
unsigned ExpressionGrammar::append(Expression& tree,unsigned size,std::uint64_t rank,const ExpressionSpend& spend) const {
    spend(1);
    if(size==1) {
        tree.push_back(rank==0 ? ExpressionNode{} : ExpressionNode{'c',0,0,constants_.at(rank-1)});
        return tree.size()-1;
    }
    for(char op:operators_) for(unsigned left=1;left<size-1;left+=2) {
        spend(1);
        const unsigned right=size-1-left;
        if(commutative(op) && left>right) continue;
        if(op=='^' && (right!=1 || exponent_ranks_.empty())) continue;
        const bool triangular=commutative(op) && left==right;
        const auto count=op=='^' ? counts_[left]*exponent_ranks_.size() : pairs(counts_[left],counts_[right],triangular);
        if(rank>=count) {rank-=count; continue;}
        std::uint64_t li,ri;
        if(op=='^') {li=rank/exponent_ranks_.size();ri=exponent_ranks_[rank%exponent_ranks_.size()];}
        else if(triangular) {
            const auto n=counts_[left];
            auto offset=[&](std::uint64_t i) {return i*(2*n-i+1)/2;};
            std::uint64_t low=0,high=n;
            while(low+1<high) {
                spend(1); const auto mid=low+(high-low)/2;
                if(offset(mid)<=rank) low=mid; else high=mid;
            }
            li=low; ri=li+rank-offset(li);
        } else {li=rank/counts_[right]; ri=rank%counts_[right];}
        const auto l=append(tree,left,li,spend),r=append(tree,right,ri,spend);
        tree.push_back({op,l,r,0}); return tree.size()-1;
    }
    throw Error("ExpressionRankOutOfRange");
}
Expression ExpressionGrammar::at(std::uint64_t id,const ExpressionSpend& spend) const {
    if(id>=total_) throw Error("ExpressionRankOutOfRange");
    for(unsigned size=1;size<=max_nodes_;size+=2) {
        spend(1);
        if(id>=counts_[size]) {id-=counts_[size]; continue;}
        Expression tree; tree.reserve(size);
        if(indexed_) append_indexed(tree,size,id,spend);else append(tree,size,id,spend);
        return tree;
    }
    throw Error("ExpressionRankOutOfRange");
}
ExactValue evaluate_expression(const NumberDomain& d,const Expression& tree,const ExactValue& x,const ExpressionSpend& spend) {
    validate(tree);
    std::function<ExactValue(unsigned)> visit=[&](unsigned i)->ExactValue {
        spend(1); const auto& n=tree[i];
        if(n.op=='x') return d.check(x);
        if(n.op=='c') return d.check(n.value);
        const auto a=visit(n.left),b=visit(n.right);
        spend(1);
        if(n.op=='+') return d.add(a,b);
        if(n.op=='-') return d.subtract(a,b);
        if(n.op=='*') return d.multiply(a,b);
        if(n.op=='^') {
            auto exponent=b.get_num().get_ui();
            ExactValue result=1,base=a;
            while(exponent) {
                if(exponent&1u) {spend(1);result=d.multiply(result,base);}
                exponent>>=1;
                if(exponent) {spend(1);base=d.multiply(base,base);}
            }
            return result;
        }
        if(b==0) throw ExpressionPole{};
        return d.divide(a,b);
    };
    return visit(tree.size()-1);
}
ExactValue counter_evaluate_expression(const NumberDomain& d,const Expression& tree,const ExactValue& x,const ExpressionSpend& spend) {
    validate(tree);
    // A separate bottom-up integer numerator/denominator path. Intermediate integer
    // products obey the same bit limit, even where rational cancellation could shrink them.
    struct Pair {mpz_class numerator,denominator;};
    auto checked=[&](mpz_class v) {if(integer_bits(v)>d.max_bits()) throw NumericLimit{}; return v;};
    auto mul=[&](const mpz_class& a,const mpz_class& b) {spend(1); return checked(a*b);};
    auto add=[&](const mpz_class& a,const mpz_class& b) {spend(1); return checked(a+b);};
    auto sub=[&](const mpz_class& a,const mpz_class& b) {spend(1); return checked(a-b);};
    std::vector<Pair> values;
    for(const auto& n:tree) {
        spend(1);
        if(n.op=='x' || n.op=='c') {const auto q=d.check(n.op=='x' ? x : n.value); values.push_back({q.get_num(),q.get_den()}); continue;}
        const auto& a=values[n.left]; const auto& b=values[n.right];
        Pair v;
        if(n.op=='+' || n.op=='-') {
            const auto lhs=mul(a.numerator,b.denominator),rhs=mul(b.numerator,a.denominator);
            v={n.op=='+' ? add(lhs,rhs) : sub(lhs,rhs),mul(a.denominator,b.denominator)};
        } else if(n.op=='*') v={mul(a.numerator,b.numerator),mul(a.denominator,b.denominator)};
        else if(n.op=='^') {
            // Independent linear integer-products countercheck, not repeated squaring.
            v={1,1};
            for(unsigned long i=0;i<b.numerator.get_ui();++i)
                v={mul(v.numerator,a.numerator),mul(v.denominator,a.denominator)};
        } else {
            spend(1); if(b.numerator==0) throw ExpressionPole{};
            v={mul(a.numerator,b.denominator),mul(a.denominator,b.numerator)};
        }
        spend(1);
        ExactValue q(v.numerator,v.denominator); q.canonicalize(); q=d.check(q);
        values.push_back({q.get_num(),q.get_den()});
    }
    return d.check(ExactValue(values.back().numerator,values.back().denominator));
}
std::string expression_formula(const Expression& tree) {return formulas(tree).back();}
std::vector<std::string> expression_conditions(const Expression& tree) {
    const auto text=formulas(tree); std::vector<std::string> result;
    for(const auto& n:tree) if(n.op=='/') result.push_back(text[n.right]+" != 0");
    return result;
}
}
