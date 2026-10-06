#include "bruce/token/task_spec.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <set>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_TOKENIZED_TASK_SPEC_V1\n";
constexpr std::size_t max_spec_bytes=128*1024*1024;

void put(std::string& out,std::string_view value) { out+=std::to_string(value.size()); out+=':'; out.append(value); }
void put_num(std::string& out,std::uint64_t value) { put(out,std::to_string(value)); }
std::string_view take(std::string_view body,std::size_t& pos) {
    const auto begin=pos;
    while(pos<body.size() && body[pos]>='0' && body[pos]<='9') ++pos;
    if(begin==pos || pos==body.size() || body[pos]!=':' ||
       (pos-begin>1 && body[begin]=='0')) throw Error("InvalidTaskSpecField");
    const auto size=integer<std::size_t>(body.substr(begin,pos-begin));
    ++pos;
    if(size>body.size()-pos) throw Error("InvalidTaskSpecField");
    const auto value=body.substr(pos,size); pos+=size; return value;
}
std::string read(std::string_view body,std::size_t& pos) { return std::string(take(body,pos)); }
std::uint64_t num(std::string_view body,std::size_t& pos) {
    const auto value=take(body,pos);
    if(value.empty() || (value.size()>1 && value.front()=='0')) throw Error("NoncanonicalTaskSpecNumber");
    return integer<std::uint64_t>(value);
}
unsigned small_num(std::string_view body,std::size_t& pos) {
    const auto value=num(body,pos);
    if(value>std::numeric_limits<unsigned>::max()) throw Error("InvalidTaskSpecNumber");
    return static_cast<unsigned>(value);
}
bool alias_ok(std::string_view alias) {
    if(alias.empty()) return false;
    for(unsigned char c:alias)
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return false;
    return true;
}
bool valid_utf8(std::string_view text) {
    for(std::size_t i=0;i<text.size();) {
        const auto lead=static_cast<unsigned char>(text[i]);
        if(lead<0x80) {++i;continue;}
        unsigned count=0; std::uint32_t code=0;
        if(lead>=0xc2 && lead<=0xdf) {count=2;code=lead&0x1f;}
        else if(lead>=0xe0 && lead<=0xef) {count=3;code=lead&0x0f;}
        else if(lead>=0xf0 && lead<=0xf4) {count=4;code=lead&0x07;}
        else return false;
        if(count>text.size()-i) return false;
        for(unsigned j=1;j<count;++j) {
            const auto c=static_cast<unsigned char>(text[i+j]);
            if((c&0xc0)!=0x80) return false;
            code=(code<<6)|(c&0x3f);
        }
        if((count==2 && code<0x80) || (count==3 && code<0x800) ||
           (count==4 && code<0x10000) || (code>=0xd800 && code<=0xdfff) || code>0x10ffff) return false;
        i+=count;
    }
    return true;
}
std::string normalize(std::string_view value) {
    if(!valid_utf8(value)) throw Error("InvalidTaskSpecUtf8");
    std::string out; bool space=false;
    for(unsigned char c:value) {
        if(c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\f' || c=='\v') {
            space=!out.empty(); continue;
        }
        if(space) out+=' ';
        space=false;
        out+=static_cast<char>(c>='A' && c<='Z' ? c-'A'+'a' : c);
    }
    return out;
}
std::string table_body(const TokenTableV1& table) {
    std::string body; put(body,table.version); put_num(body,table.entries.size());
    for(const auto& e:table.entries) {
        put(body,e.id); put(body,e.visible_text); put(body,e.normalized_text); put_num(body,e.ordinal);
    }
    return body;
}
constexpr std::array<std::string_view,3> canonical_v1_token_text={
    "Train","Holdout","Rational Portfolio"};
void validate_canonical_v1_table(const TokenizedTaskSpecV1& spec) {
    if(spec.table.entries.size()!=canonical_v1_token_text.size()) throw Error("NoncanonicalTaskSpecV1Table");
    for(std::size_t i=0;i<canonical_v1_token_text.size();++i)
        if(spec.table.entries[i].visible_text!=canonical_v1_token_text[i])
            throw Error("NoncanonicalTaskSpecV1Table");
    if(spec.ordered_token_ids.empty()) return;
    if(spec.ordered_token_ids.size()!=spec.table.entries.size()) throw Error("NoncanonicalTaskSpecV1TokenOrder");
    for(std::size_t i=0;i<spec.ordered_token_ids.size();++i)
        if(spec.ordered_token_ids[i]!=spec.table.entries[i].id) throw Error("NoncanonicalTaskSpecV1TokenOrder");
}
void validate_roles(const std::vector<TokenizedRoleV1>& roles,const TokenizedTaskSpecV1& spec) {
    if(roles.size()!=3) throw Error("InvalidTaskSpecRoles");
    std::set<std::string> names;
    for(const auto& role:roles) {
        if(!names.insert(role.role).second ||
           (role.role!="train" && role.role!="holdout" && role.role!="route")) throw Error("InvalidTaskSpecRole");
        if(std::find(spec.ordered_token_ids.begin(),spec.ordered_token_ids.end(),role.token_id)==spec.ordered_token_ids.end())
            throw Error("UnknownTaskSpecToken");
        const auto expected_token_id=role.role=="train" ? spec.table.entries[0].id :
                                     role.role=="holdout" ? spec.table.entries[1].id : spec.table.entries[2].id;
        if(role.token_id!=expected_token_id) throw Error("NoncanonicalTaskSpecV1RoleBinding");
        if(role.role=="route") {
            if(!role.asset_alias.empty()) throw Error("InvalidTaskSpecRouteAsset");
        } else {
            if(role.asset_alias!=role.role) throw Error("NoncanonicalTaskSpecV1RoleAssetBinding");
            if(std::none_of(spec.assets.begin(),spec.assets.end(),[&](const auto& a){return a.alias==role.asset_alias;}))
                throw Error("MissingTaskSpecAsset");
        }
    }
    if(names.size()!=3 || roles[0].role!="holdout" || roles[1].role!="route" || roles[2].role!="train")
        throw Error("InvalidTaskSpecRoles");
    if(roles[0].asset_alias==roles[2].asset_alias) throw Error("DuplicateTaskSpecRoleAsset");
}
void sort_roles(std::vector<TokenizedRoleV1>& roles) {
    std::sort(roles.begin(),roles.end(),[](const auto& a,const auto& b){return a.role<b.role;});
}
ExactResearchTask native_configuration(const TokenizedTaskSpecV1& spec) {
    ExactResearchTask task;
    task.domain="rational";task.strategy="portfolio-rational";task.planner="portfolio";
    task.minimum=spec.minimum;task.maximum=spec.maximum;
    task.max_power=spec.max_power;task.max_degree=spec.max_degree;
    task.max_bits=spec.max_bits;task.max_denominator=spec.max_denominator;
    task.max_trace_bytes=spec.max_trace_bytes;task.budget=spec.budget;
    return task;
}
void validate(const TokenizedTaskSpecV1& spec,bool require_roles) {
    if(spec.task_spec_version!=task_spec_version_v1) throw Error("UnsupportedTaskSpecVersion");
    if(spec.normalization_version!=token_normalization_version_v1) throw Error("UnsupportedTaskSpecNormalization");
    if(spec.table.version!="v1") throw Error("UnsupportedTokenTableVersion");
    if(spec.route_id!="rational-portfolio-v1" || spec.domain!="rational") throw Error("UnsupportedTaskSpecRoute");
    validate_exact_resource_limits(native_configuration(spec));
    if(spec.max_power<1 || spec.max_power>8 || spec.max_degree<1 || spec.max_degree>8)
        throw Error("InvalidTaskSpecLimit");
    const NumberDomain domain(spec.domain,spec.max_bits);
    const auto low=domain.parse_integer(spec.minimum),high=domain.parse_integer(spec.maximum);
    const mpz_class width=high-low+1;
    if(spec.minimum!=low.get_str() || spec.maximum!=high.get_str() || width<1 || width>4096)
        throw Error("InvalidTaskSpecRange");
    if(spec.table.entries.empty()) throw Error("EmptyTokenTable");
    std::set<std::string> ids;
    for(std::size_t i=0;i<spec.table.entries.size();++i) {
        const auto& e=spec.table.entries[i];
        if(e.ordinal!=i || e.normalized_text.empty() || e.normalized_text!=normalize(e.visible_text))
            throw Error("InvalidTokenEntry");
        std::string seed; put(seed,spec.table.version); put(seed,e.normalized_text); put_num(seed,e.ordinal);
        if(e.id!=sha256(seed) || !ids.insert(e.id).second) throw Error("InvalidTokenIdentity");
    }
    if(spec.table_digest!=sha256(table_body(spec.table))) throw Error("TokenTableDigestMismatch");
    validate_canonical_v1_table(spec);
    for(const auto& id:spec.ordered_token_ids) if(!ids.contains(id)) throw Error("UnknownTaskSpecToken");
    if(spec.assets.size()!=2) throw Error("InvalidTaskSpecAssetCount");
    for(const auto& asset:spec.assets)
        if(!alias_ok(asset.alias) || asset.digest!=sha256(asset.bytes)) throw Error("InvalidTaskSpecAsset");
    if(spec.assets[0].alias>=spec.assets[1].alias) throw Error("DuplicateOrUnsortedTaskSpecAlias");
    if(require_roles && spec.roles.empty() && spec.alternatives.empty()) throw Error("MissingTaskSpecRoles");
    if(!spec.roles.empty()) validate_roles(spec.roles,spec);
    std::string previous;
    for(const auto& alternative:spec.alternatives) {
        if(alternative.id.empty() || (!previous.empty() && previous>=alternative.id)) throw Error("InvalidTaskSpecAlternative");
        validate_roles(alternative.roles,spec);
        previous=alternative.id;
    }
    if(!spec.alternatives.empty() && !spec.roles.empty()) throw Error("ConflictingTaskSpecRoles");
    if(require_roles && !spec.alternatives.empty() && spec.selected_id.empty())
        throw Error("UnselectedTaskSpecAlternative");
    if(spec.alternatives.empty() && !spec.selected_id.empty()) throw Error("InvalidTaskSpecSelection");
    if(!spec.selected_id.empty() && std::none_of(spec.alternatives.begin(),spec.alternatives.end(),
        [&](const auto& a){return a.id==spec.selected_id;})) throw Error("UnknownTaskSpecAlternative");
}
void put_roles(std::string& body,const std::vector<TokenizedRoleV1>& roles) {
    put_num(body,roles.size());
    for(const auto& role:roles) {put(body,role.role);put(body,role.token_id);put(body,role.asset_alias);}
}
std::vector<TokenizedRoleV1> read_roles(std::string_view body,std::size_t& pos) {
    const auto count=num(body,pos);
    if(count>3) throw Error("InvalidTaskSpecRoles");
    std::vector<TokenizedRoleV1> roles;
    for(std::uint64_t i=0;i<count;++i) roles.push_back({read(body,pos),read(body,pos),read(body,pos)});
    return roles;
}
}

TokenizedTaskSpecV1 make_tokenized_task_spec(TokenizedTaskSpecV1 spec) {
    if(spec.task_spec_version!=task_spec_version_v1 || spec.normalization_version!=token_normalization_version_v1)
        throw Error("UnsupportedTaskSpecVersion");
    if(spec.table.version!="v1") throw Error("UnsupportedTokenTableVersion");
    for(std::size_t i=0;i<spec.table.entries.size();++i) {
        auto& e=spec.table.entries[i];
        if(e.ordinal!=i) throw Error("InvalidTokenOrdinal");
        const auto normalized=normalize(e.visible_text);
        if(!e.normalized_text.empty() && e.normalized_text!=normalized) throw Error("InvalidTokenNormalization");
        e.normalized_text=normalized;
        std::string seed; put(seed,spec.table.version);put(seed,e.normalized_text);put_num(seed,e.ordinal);
        const auto id=sha256(seed);
        if(!e.id.empty() && e.id!=id) throw Error("InvalidTokenIdentity");
        e.id=id;
    }
    const auto digest=sha256(table_body(spec.table));
    if(!spec.table_digest.empty() && spec.table_digest!=digest) throw Error("TokenTableDigestMismatch");
    spec.table_digest=digest;
    std::sort(spec.assets.begin(),spec.assets.end(),[](const auto& a,const auto& b){return a.alias<b.alias;});
    sort_roles(spec.roles);
    for(auto& alternative:spec.alternatives) sort_roles(alternative.roles);
    std::sort(spec.alternatives.begin(),spec.alternatives.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    validate(spec,false);
    return spec;
}

std::string pack_tokenized_task_spec(const TokenizedTaskSpecV1& input) {
    auto spec=make_tokenized_task_spec(input);
    validate(spec,true);
    std::string body;
    put(body,spec.task_spec_version);put(body,spec.normalization_version);
    put(body,spec.table.version);put(body,spec.table_digest);put_num(body,spec.table.entries.size());
    for(const auto& e:spec.table.entries) {put(body,e.id);put(body,e.visible_text);put(body,e.normalized_text);put_num(body,e.ordinal);}
    put_num(body,spec.ordered_token_ids.size());for(const auto& id:spec.ordered_token_ids) put(body,id);
    put_num(body,spec.assets.size());for(const auto& a:spec.assets) {put(body,a.alias);put(body,a.bytes);put(body,a.digest);}
    put_roles(body,spec.roles);
    put_num(body,spec.alternatives.size());for(const auto& a:spec.alternatives) {put(body,a.id);put_roles(body,a.roles);}
    put(body,spec.selected_id);put(body,spec.route_id);put(body,spec.domain);
    put(body,spec.minimum);put(body,spec.maximum);
    put_num(body,spec.budget.candidates);put_num(body,spec.budget.operations);
    put_num(body,spec.budget.trace_events);put_num(body,spec.budget.results);
    put_num(body,spec.max_bits);put_num(body,spec.max_denominator);
    put_num(body,spec.max_power);put_num(body,spec.max_degree);put_num(body,spec.max_trace_bytes);
    if(body.size()>max_spec_bytes) throw Error("TaskSpecByteLimit");
    return std::string(header)+sha256(body)+"\n"+body;
}

TokenizedTaskSpecV1 unpack_tokenized_task_spec(std::string_view bytes) {
    if(!bytes.starts_with(header) || bytes.size()>max_spec_bytes+header.size()+65 ||
       bytes.size()<header.size()+65 || bytes[header.size()+64]!='\n') throw Error("InvalidTaskSpecHeader");
    const auto body=bytes.substr(header.size()+65);
    if(bytes.substr(header.size(),64)!=sha256(body)) throw Error("TaskSpecDigestMismatch");
    TokenizedTaskSpecV1 spec;std::size_t pos=0;
    spec.task_spec_version=read(body,pos);spec.normalization_version=read(body,pos);
    spec.table.version=read(body,pos);spec.table_digest=read(body,pos);
    const auto entries=num(body,pos);if(entries>body.size()) throw Error("InvalidTokenTableSize");
    for(std::uint64_t i=0;i<entries;++i) spec.table.entries.push_back({read(body,pos),read(body,pos),read(body,pos),num(body,pos)});
    const auto tokens=num(body,pos);if(tokens>body.size()) throw Error("InvalidTokenSequenceSize");
    for(std::uint64_t i=0;i<tokens;++i) spec.ordered_token_ids.push_back(read(body,pos));
    const auto assets=num(body,pos);if(assets>2) throw Error("InvalidTaskSpecAssetCount");
    for(std::uint64_t i=0;i<assets;++i) spec.assets.push_back({read(body,pos),read(body,pos),read(body,pos)});
    spec.roles=read_roles(body,pos);
    const auto alternatives=num(body,pos);if(alternatives>body.size()) throw Error("InvalidTaskSpecAlternativeCount");
    for(std::uint64_t i=0;i<alternatives;++i) {
        TokenizedAlternativeV1 a;a.id=read(body,pos);a.roles=read_roles(body,pos);spec.alternatives.push_back(std::move(a));
    }
    spec.selected_id=read(body,pos);spec.route_id=read(body,pos);spec.domain=read(body,pos);
    spec.minimum=read(body,pos);spec.maximum=read(body,pos);
    spec.budget.candidates=num(body,pos);spec.budget.operations=num(body,pos);
    spec.budget.trace_events=num(body,pos);spec.budget.results=num(body,pos);
    spec.max_bits=small_num(body,pos);spec.max_denominator=small_num(body,pos);
    spec.max_power=small_num(body,pos);spec.max_degree=small_num(body,pos);spec.max_trace_bytes=num(body,pos);
    if(pos!=body.size() || pack_tokenized_task_spec(spec)!=bytes) throw Error("NoncanonicalTaskSpec");
    return spec;
}

ExactResearchTask compile_tokenized_task_spec(const TokenizedTaskSpecV1& input) {
    const auto spec=make_tokenized_task_spec(input);
    validate(spec,true);
    const std::vector<TokenizedRoleV1>* roles=&spec.roles;
    if(!spec.alternatives.empty()) {
        if(spec.selected_id.empty()) throw Error("UnselectedTaskSpecAlternative");
        const auto it=std::find_if(spec.alternatives.begin(),spec.alternatives.end(),[&](const auto& a){return a.id==spec.selected_id;});
        roles=&it->roles;
    }
    const auto asset=[&](std::string_view role)->const TokenizedAssetV1& {
        const auto binding=std::find_if(roles->begin(),roles->end(),[&](const auto& r){return r.role==role;});
        return *std::find_if(spec.assets.begin(),spec.assets.end(),[&](const auto& a){return a.alias==binding->asset_alias;});
    };
    auto task=native_configuration(spec);
    const NumberDomain domain(task.domain,task.max_bits);
    task.training=parse_exact_samples(asset("train").bytes,domain);
    task.holdout=parse_exact_samples(asset("holdout").bytes,domain);
    return task;
}
}
