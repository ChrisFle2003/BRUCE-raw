#include "bruce/math/domain_cache.hpp"
namespace bruce {
RationalDomainComparison EqualDomainCache::compare(const std::vector<ExactValue>& left,
    const std::vector<ExactValue>& right,const ExpressionSpend& spend) {
    if(domain_.name()!="rational" || !spend) throw Error("InvalidNormalizationConfiguration");
    auto work=[&](std::uint64_t n) {spend(n);stats_.operations+=n;};
    std::string key;
    bool eligible=true;
    for(const auto* polynomial:{&left,&right}) {
        work(1);
        if(polynomial->empty() || polynomial->size()>65) {eligible=false;break;}
        key+='[';
        for(const auto& coefficient:*polynomial) {
            work(1);const auto value=domain_.check(coefficient);
            work(1);const auto text=exact_text(value);
            // One delimiter per coefficient and a closing bracket per polynomial.
            if(key.size()+text.size()+2>4096) {eligible=false;break;}
            key+=text;key+=';';
        }
        if(!eligible) break;
        key+=']';
    }
    if(!eligible) {
        ++stats_.bypasses;
        return compare_rational_domains(domain_,left,right,spend,true);
    }
    std::uint64_t hash=14695981039346656037ULL;
    for(const unsigned char byte:key) {work(1);hash=(hash^byte)*1099511628211ULL;}
    auto& stored=keys_[hash%keys_.size()];
    work(1);bool same=stored.size()==key.size();
    if(same) for(std::size_t i=0;i<key.size();++i) {
        work(1);if(stored[i]!=key[i]) {same=false;break;}
    }
    if(same) {
        ++stats_.hits;
        RationalDomainComparison result;result.relation="EQUAL";result.reason="CACHED_EQUAL";
        return result;
    }
    ++stats_.misses;
    auto result=compare_rational_domains(domain_,left,right,spend,true);
    if(result.relation=="EQUAL") {
        // A stop before this charge cannot publish a cache entry.
        work(1);
        if(stored.empty()) ++stats_.entries; else ++stats_.evictions;
        stats_.key_bytes-=stored.size();stats_.key_bytes+=key.size();++stats_.inserts;
        stored=std::move(key);
    }
    return result;
}
}
