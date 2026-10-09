#include "bruce/mods/language/language.hpp"
#include "bruce/evidence/bundle.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <optional>

namespace bruce {
namespace {
constexpr std::string_view header="BRUCE_LANGUAGE_DRAFT_V1\n";
constexpr std::size_t max_draft_bytes=128*1024*1024;

bool ascii_space(unsigned char ch) {
    return ch==' ' || ch=='\t' || ch=='\n' || ch=='\r' || ch=='\f' || ch=='\v';
}
bool valid_utf8(std::string_view text) {
    for (std::size_t i=0;i<text.size();) {
        const auto lead=static_cast<unsigned char>(text[i]);
        if (lead<0x80) { ++i; continue; }
        unsigned count=0;
        std::uint32_t code=0;
        if (lead>=0xc2 && lead<=0xdf) { count=2; code=lead&0x1f; }
        else if (lead>=0xe0 && lead<=0xef) { count=3; code=lead&0x0f; }
        else if (lead>=0xf0 && lead<=0xf4) { count=4; code=lead&0x07; }
        else return false;
        if (text.size()-i<count) return false;
        for (unsigned j=1;j<count;++j) {
            const auto ch=static_cast<unsigned char>(text[i+j]);
            if ((ch&0xc0)!=0x80) return false;
            code=(code<<6)|(ch&0x3f);
        }
        if ((count==2 && code<0x80) || (count==3 && code<0x800) ||
            (count==4 && code<0x10000) || (code>=0xd800 && code<=0xdfff) || code>0x10ffff) return false;
        i+=count;
    }
    return true;
}
std::string normalized(std::string_view text) {
    if (!valid_utf8(text)) throw Error("InvalidLanguageUtf8");
    std::string result;
    result.reserve(text.size());
    bool pending_space=false;
    for (unsigned char ch:text) {
        if (ascii_space(ch)) { pending_space=!result.empty(); continue; }
        if (pending_space) result.push_back(' ');
        pending_space=false;
        result.push_back(ch>='A' && ch<='Z' ? static_cast<char>(ch-'A'+'a') : static_cast<char>(ch));
    }
    return result;
}
bool valid_alias(std::string_view alias) {
    if (alias.empty()) return false;
    for (unsigned char ch:alias)
        if (!((ch>='a' && ch<='z') || (ch>='0' && ch<='9') || ch=='_')) return false;
    return true;
}
void validate_inputs(std::vector<LanguageInputV1>& inputs) {
    if (inputs.size()!=2) throw Error("InvalidLanguageInputCount");
    for (const auto& input:inputs) {
        if (!valid_alias(input.alias)) throw Error("InvalidLanguageAlias");
        if (input.digest!=sha256(input.bytes)) throw Error("LanguageInputDigestMismatch");
    }
    std::sort(inputs.begin(),inputs.end(),[](const auto& a,const auto& b){return a.alias<b.alias;});
    if (inputs[0].alias==inputs[1].alias) throw Error("DuplicateLanguageAlias");
}
bool has_aliases(const std::vector<LanguageInputV1>& inputs,std::string_view first,std::string_view second) {
    return (inputs[0].alias==first && inputs[1].alias==second) ||
           (inputs[0].alias==second && inputs[1].alias==first);
}
void put_field(std::string& out,std::string_view value) {
    out+=std::to_string(value.size());
    out+=':';
    out.append(value);
}
void put_number(std::string& out,std::uint64_t value) { put_field(out,std::to_string(value)); }
std::string_view read_field(std::string_view body,std::size_t& offset) {
    const auto start=offset;
    while (offset<body.size() && body[offset]>='0' && body[offset]<='9') ++offset;
    if (offset==start || offset>=body.size() || body[offset]!=':') throw Error("InvalidLanguageDraftField");
    const auto digits=body.substr(start,offset-start);
    if (digits.size()>1 && digits.front()=='0') throw Error("NoncanonicalLanguageDraft");
    const auto count=integer<std::size_t>(digits);
    ++offset;
    if (count>body.size()-offset) throw Error("InvalidLanguageDraftField");
    const auto value=body.substr(offset,count);
    offset+=count;
    return value;
}
std::uint64_t read_number(std::string_view body,std::size_t& offset) {
    const auto field=read_field(body,offset);
    if (field.empty() || (field.size()>1 && field.front()=='0')) throw Error("NoncanonicalLanguageDraft");
    return integer<std::uint64_t>(field);
}
unsigned read_unsigned(std::string_view body,std::size_t& offset) {
    const auto value=read_number(body,offset);
    if (value>std::numeric_limits<unsigned>::max()) throw Error("InvalidLanguageDraftField");
    return static_cast<unsigned>(value);
}
std::string status_text(LanguageInterpretationStatus status) {
    switch (status) {
        case LanguageInterpretationStatus::understood: return "understood";
        case LanguageInterpretationStatus::ambiguous: return "ambiguous";
        case LanguageInterpretationStatus::unsupported: return "unsupported";
    }
    throw Error("InvalidLanguageStatus");
}
LanguageInterpretationStatus parse_status(std::string_view value) {
    if (value=="understood") return LanguageInterpretationStatus::understood;
    if (value=="ambiguous") return LanguageInterpretationStatus::ambiguous;
    if (value=="unsupported") return LanguageInterpretationStatus::unsupported;
    throw Error("InvalidLanguageStatus");
}
bool same_content(const LanguageDraftV1& a,const LanguageDraftV1& b) {
    return a.raw_text==b.raw_text && a.normalized_text==b.normalized_text &&
           a.grammar_version==b.grammar_version && a.inputs==b.inputs &&
           a.budget.candidates==b.budget.candidates && a.budget.operations==b.budget.operations &&
           a.budget.trace_events==b.budget.trace_events && a.budget.results==b.budget.results &&
           a.max_bits==b.max_bits && a.max_denominator==b.max_denominator &&
           a.max_trace_bytes==b.max_trace_bytes && a.status==b.status &&
           a.alternatives==b.alternatives && a.selected_id==b.selected_id;
}
void validate_draft(const LanguageDraftV1& draft) {
    LanguageRequestV1 request;
    request.text=draft.raw_text;
    request.inputs=draft.inputs;
    request.budget=draft.budget;
    request.max_bits=draft.max_bits;
    request.max_denominator=draft.max_denominator;
    request.max_trace_bytes=draft.max_trace_bytes;
    request.grammar_version=draft.grammar_version;
    auto expected=interpret_language(std::move(request));
    if (!draft.selected_id.empty()) {
        if (expected.status!=LanguageInterpretationStatus::ambiguous) throw Error("InvalidLanguageSelection");
        const auto found=std::find_if(expected.alternatives.begin(),expected.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if (found==expected.alternatives.end()) throw Error("UnknownLanguageAlternative");
        expected.selected_id=draft.selected_id;
    }
    if (!same_content(draft,expected)) throw Error("NoncanonicalLanguageDraft");
}
}

LanguageDraftV1 interpret_language(LanguageRequestV1 request) {
    if (request.grammar_version!=language_grammar_version_v1) throw Error("UnsupportedLanguageGrammar");
    validate_inputs(request.inputs);
    LanguageDraftV1 draft;
    draft.raw_text=std::move(request.text);
    draft.normalized_text=normalized(draft.raw_text);
    draft.grammar_version=std::move(request.grammar_version);
    draft.inputs=std::move(request.inputs);
    draft.budget=request.budget;
    draft.max_bits=request.max_bits;
    draft.max_denominator=request.max_denominator;
    draft.max_trace_bytes=request.max_trace_bytes;
    if (draft.normalized_text=="untersuche @train mit @holdout auf rationale formeln.") {
        if (!has_aliases(draft.inputs,"train","holdout")) throw Error("MissingLanguageAlias");
        draft.status=LanguageInterpretationStatus::understood;
        draft.alternatives={{"rational-portfolio-v1","rational-portfolio-v1 train=train holdout=holdout"}};
    } else if (draft.normalized_text=="untersuche @left und @right auf rationale formeln.") {
        if (!has_aliases(draft.inputs,"left","right")) throw Error("MissingLanguageAlias");
        draft.status=LanguageInterpretationStatus::ambiguous;
        draft.alternatives={{"train=left;holdout=right","rational-portfolio-v1 train=left holdout=right"},
                            {"train=right;holdout=left","rational-portfolio-v1 train=right holdout=left"}};
    }
    return draft;
}

LanguageDraftV1 select_language_alternative(const LanguageDraftV1& draft,std::string_view id) {
    validate_draft(draft);
    if (draft.status!=LanguageInterpretationStatus::ambiguous || !draft.selected_id.empty())
        throw Error("InvalidLanguageSelection");
    const auto found=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
        [&](const auto& alternative){return alternative.id==id;});
    if (found==draft.alternatives.end()) throw Error("UnknownLanguageAlternative");
    auto selected=draft;
    selected.selected_id=std::string(id);
    return selected;
}

std::string pack_language_draft(const LanguageDraftV1& draft) {
    validate_draft(draft);
    std::string body;
    put_field(body,draft.grammar_version);
    put_field(body,draft.raw_text);
    put_field(body,draft.normalized_text);
    put_number(body,draft.inputs.size());
    for (const auto& input:draft.inputs) {
        put_field(body,input.alias);
        put_field(body,input.bytes);
        put_field(body,input.digest);
    }
    put_number(body,draft.budget.candidates);
    put_number(body,draft.budget.operations);
    put_number(body,draft.budget.trace_events);
    put_number(body,draft.budget.results);
    put_number(body,draft.max_bits);
    put_number(body,draft.max_denominator);
    put_number(body,draft.max_trace_bytes);
    put_field(body,status_text(draft.status));
    put_number(body,draft.alternatives.size());
    for (const auto& alternative:draft.alternatives) {
        put_field(body,alternative.id);
        put_field(body,alternative.rule);
    }
    put_field(body,draft.selected_id);
    if (body.size()>max_draft_bytes) throw Error("LanguageDraftByteLimit");
    return std::string(header)+sha256(body)+"\n"+body;
}

LanguageDraftV1 unpack_language_draft(std::string_view bytes) {
    if (bytes.size()>max_draft_bytes+header.size()+65) throw Error("LanguageDraftByteLimit");
    if (!bytes.starts_with(header) || bytes.size()<header.size()+65 || bytes[header.size()+64]!='\n')
        throw Error("InvalidLanguageDraftHeader");
    const auto digest=bytes.substr(header.size(),64);
    const auto body=bytes.substr(header.size()+65);
    if (digest!=sha256(body)) throw Error("LanguageDraftDigestMismatch");
    std::size_t offset=0;
    LanguageDraftV1 draft;
    draft.grammar_version=read_field(body,offset);
    draft.raw_text=read_field(body,offset);
    draft.normalized_text=read_field(body,offset);
    const auto input_count=read_number(body,offset);
    if (input_count!=2) throw Error("InvalidLanguageInputCount");
    for (std::uint64_t i=0;i<input_count;++i) {
        LanguageInputV1 input;
        input.alias=read_field(body,offset);
        input.bytes=read_field(body,offset);
        input.digest=read_field(body,offset);
        draft.inputs.push_back(std::move(input));
    }
    draft.budget.candidates=read_number(body,offset);
    draft.budget.operations=read_number(body,offset);
    draft.budget.trace_events=read_number(body,offset);
    draft.budget.results=read_number(body,offset);
    draft.max_bits=read_unsigned(body,offset);
    draft.max_denominator=read_unsigned(body,offset);
    draft.max_trace_bytes=read_number(body,offset);
    draft.status=parse_status(read_field(body,offset));
    const auto alternative_count=read_number(body,offset);
    if (alternative_count>2) throw Error("InvalidLanguageAlternativeCount");
    for (std::uint64_t i=0;i<alternative_count;++i) {
        LanguageAlternativeV1 alternative;
        alternative.id=read_field(body,offset);
        alternative.rule=read_field(body,offset);
        draft.alternatives.push_back(std::move(alternative));
    }
    draft.selected_id=read_field(body,offset);
    if (offset!=body.size() || pack_language_draft(draft)!=bytes) throw Error("NoncanonicalLanguageDraft");
    return draft;
}

namespace {
constexpr std::string_view header_v2="BRUCE_LANGUAGE_DRAFT_V2\n";

void validate_inputs_v2(std::vector<LanguageInputV2>& inputs) {
    if (inputs.size()!=2) throw Error("InvalidLanguageInputCount");
    for (const auto& input:inputs) {
        if (!valid_alias(input.alias)) throw Error("InvalidLanguageAlias");
        if (input.digest!=sha256(input.bytes)) throw Error("LanguageInputDigestMismatch");
    }
    std::sort(inputs.begin(),inputs.end(),[](const auto& a,const auto& b){return a.alias<b.alias;});
    if (inputs[0].alias==inputs[1].alias) throw Error("DuplicateLanguageAlias");
}

bool has_aliases_v2(const std::vector<LanguageInputV2>& inputs,std::string_view first,std::string_view second) {
    return (inputs[0].alias==first && inputs[1].alias==second) ||
           (inputs[0].alias==second && inputs[1].alias==first);
}

bool contains_alias_v2(const std::vector<LanguageInputV2>& inputs,std::string_view alias) {
    return std::any_of(inputs.begin(),inputs.end(),[&](const auto& input){return input.alias==alias;});
}

bool valid_reference_v2(std::string_view value) {
    return value.size()>1 && value.front()=='@' && valid_alias(value.substr(1));
}

std::vector<std::string_view> split_words_v2(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t offset=0;
    while (offset<text.size()) {
        while (offset<text.size() && text[offset]==' ') ++offset;
        if (offset==text.size()) break;
        const auto start=offset;
        while (offset<text.size() && text[offset]!=' ') ++offset;
        words.push_back(text.substr(start,offset-start));
    }
    return words;
}

bool target_v2(const std::vector<std::string_view>& words,std::size_t index) {
    return index+1<words.size() && words[index]=="rationale" &&
           (words[index+1]=="formeln." || words[index+1]=="beziehungen.");
}

bool verb_v2(std::string_view value) {
    return value=="untersuche" || value=="analysiere" || value=="bewerte" || value=="teste";
}

bool connector_v2(const std::vector<std::string_view>& words,std::size_t index,std::size_t& after) {
    if (index<words.size() && (words[index]=="mit" || words[index]=="anhand")) {
        after=index+1;
        return true;
    }
    if (index+2<words.size() && words[index]=="unter" && words[index+1]=="verwendung" && words[index+2]=="von") {
        after=index+3;
        return true;
    }
    return false;
}

struct ParsedV2 {
    std::string first;
    std::string second;
    bool role_ambiguous=false;
};

std::string canonical_text_v2(const ParsedV2& parsed) {
    if(parsed.role_ambiguous)
        return "untersuche @"+parsed.first+" und @"+parsed.second+
            " auf rationale formeln.";
    return "untersuche @"+parsed.first+" mit @"+parsed.second+
        " auf rationale formeln.";
}

std::optional<ParsedV2> parse_v2(std::string_view text) {
    const auto words=split_words_v2(text);
    if (words.size()<6 || !verb_v2(words[0]) || !valid_reference_v2(words[1])) return std::nullopt;
    if (words[2]=="und") {
        return std::nullopt;
    }
    std::size_t after=0;
    if (!connector_v2(words,2,after) || after>=words.size() || !valid_reference_v2(words[after]))
        return std::nullopt;
    if (after+4!=words.size() || words[after+1]!="auf" || !target_v2(words,after+2))
        return std::nullopt;
    return ParsedV2{std::string(words[1].substr(1)),std::string(words[after].substr(1)),false};
}

std::optional<ParsedV2> parse_ambiguous_v2(std::string_view text) {
    const auto words=split_words_v2(text);
    if (words.size()!=7 || !verb_v2(words[0]) || !valid_reference_v2(words[1]) ||
        words[2]!="und" || !valid_reference_v2(words[3]) || words[4]!="auf" || !target_v2(words,5))
        return std::nullopt;
    return ParsedV2{std::string(words[1].substr(1)),std::string(words[3].substr(1)),true};
}

bool same_content_v2(const LanguageDraftV2& a,const LanguageDraftV2& b) {
    return a.raw_text==b.raw_text && a.normalized_text==b.normalized_text &&
           a.grammar_version==b.grammar_version && a.inputs==b.inputs &&
           a.budget.candidates==b.budget.candidates && a.budget.operations==b.budget.operations &&
           a.budget.trace_events==b.budget.trace_events && a.budget.results==b.budget.results &&
           a.max_bits==b.max_bits && a.max_denominator==b.max_denominator &&
           a.max_trace_bytes==b.max_trace_bytes && a.status==b.status &&
           a.alternatives==b.alternatives && a.selected_id==b.selected_id;
}

void validate_draft_v2(const LanguageDraftV2& draft) {
    LanguageRequestV2 request;
    request.text=draft.raw_text;
    request.inputs=draft.inputs;
    request.budget=draft.budget;
    request.max_bits=draft.max_bits;
    request.max_denominator=draft.max_denominator;
    request.max_trace_bytes=draft.max_trace_bytes;
    request.grammar_version=draft.grammar_version;
    auto expected=interpret_language_v2(std::move(request));
    if (expected.status==LanguageInterpretationStatus::unsupported)
        throw Error("UnsupportedLanguageDraft");
    if (!draft.selected_id.empty()) {
        if (expected.status!=LanguageInterpretationStatus::ambiguous) throw Error("InvalidLanguageSelection");
        const auto found=std::find_if(expected.alternatives.begin(),expected.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if (found==expected.alternatives.end()) throw Error("UnknownLanguageAlternative");
        expected.selected_id=draft.selected_id;
    }
    if (!same_content_v2(draft,expected)) throw Error("NoncanonicalLanguageDraft");
}

void put_field_v2(std::string& out,std::string_view value) {
    out+=std::to_string(value.size());
    out+=':';
    out.append(value);
}
void put_number_v2(std::string& out,std::uint64_t value) { put_field_v2(out,std::to_string(value)); }
std::string_view read_field_v2(std::string_view body,std::size_t& offset) {
    const auto start=offset;
    while (offset<body.size() && body[offset]>='0' && body[offset]<='9') ++offset;
    if (offset==start || offset>=body.size() || body[offset]!=':') throw Error("InvalidLanguageDraftField");
    const auto digits=body.substr(start,offset-start);
    if (digits.size()>1 && digits.front()=='0') throw Error("NoncanonicalLanguageDraft");
    const auto count=integer<std::size_t>(digits);
    ++offset;
    if (count>body.size()-offset) throw Error("InvalidLanguageDraftField");
    const auto value=body.substr(offset,count);
    offset+=count;
    return value;
}
std::uint64_t read_number_v2(std::string_view body,std::size_t& offset) {
    const auto field=read_field_v2(body,offset);
    if (field.empty() || (field.size()>1 && field.front()=='0')) throw Error("NoncanonicalLanguageDraft");
    return integer<std::uint64_t>(field);
}
unsigned read_unsigned_v2(std::string_view body,std::size_t& offset) {
    const auto value=read_number_v2(body,offset);
    if (value>std::numeric_limits<unsigned>::max()) throw Error("InvalidLanguageDraftField");
    return static_cast<unsigned>(value);
}
}

LanguageDraftV2 interpret_language_v2(LanguageRequestV2 request) {
    if (request.grammar_version!=language_grammar_version_v2) throw Error("UnsupportedLanguageGrammar");
    validate_inputs_v2(request.inputs);
    LanguageDraftV2 draft;
    draft.raw_text=std::move(request.text);
    draft.normalized_text=normalized(draft.raw_text);
    draft.grammar_version=std::move(request.grammar_version);
    draft.inputs=std::move(request.inputs);
    draft.budget=request.budget;
    draft.max_bits=request.max_bits;
    draft.max_denominator=request.max_denominator;
    draft.max_trace_bytes=request.max_trace_bytes;

    const auto parsed=parse_v2(draft.normalized_text);
    const auto ambiguous=parse_ambiguous_v2(draft.normalized_text);
    const auto* shape=parsed ? &*parsed : (ambiguous ? &*ambiguous : nullptr);
    if (shape==nullptr) return draft;
    draft.normalized_text=canonical_text_v2(*shape);
    if (!contains_alias_v2(draft.inputs,shape->first) || !contains_alias_v2(draft.inputs,shape->second) ||
        shape->first==shape->second) throw Error("MissingLanguageAlias");
    if (has_aliases_v2(draft.inputs,"train","holdout")) {
        draft.status=LanguageInterpretationStatus::understood;
        draft.alternatives={{"rational-portfolio-v2", "rational-portfolio-v2 train=train holdout=holdout"}};
        return draft;
    }
    draft.status=LanguageInterpretationStatus::ambiguous;
    draft.alternatives={{"train="+shape->first+";holdout="+shape->second,
                         "rational-portfolio-v2 train="+shape->first+" holdout="+shape->second},
                        {"train="+shape->second+";holdout="+shape->first,
                         "rational-portfolio-v2 train="+shape->second+" holdout="+shape->first}};
    std::sort(draft.alternatives.begin(),draft.alternatives.end(),
        [](const auto& left,const auto& right){return left.id<right.id;});
    return draft;
}

LanguageDraftV2 select_language_alternative_v2(const LanguageDraftV2& draft,std::string_view id) {
    validate_draft_v2(draft);
    if (draft.status!=LanguageInterpretationStatus::ambiguous || !draft.selected_id.empty())
        throw Error("InvalidLanguageSelection");
    const auto found=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
        [&](const auto& alternative){return alternative.id==id;});
    if (found==draft.alternatives.end()) throw Error("UnknownLanguageAlternative");
    auto selected=draft;
    selected.selected_id=std::string(id);
    return selected;
}

std::string pack_language_draft_v2(const LanguageDraftV2& draft) {
    validate_draft_v2(draft);
    std::string body;
    put_field_v2(body,draft.grammar_version);
    put_field_v2(body,draft.raw_text);
    put_field_v2(body,draft.normalized_text);
    put_number_v2(body,draft.inputs.size());
    for (const auto& input:draft.inputs) {
        put_field_v2(body,input.alias);
        put_field_v2(body,input.bytes);
        put_field_v2(body,input.digest);
    }
    put_number_v2(body,draft.budget.candidates);
    put_number_v2(body,draft.budget.operations);
    put_number_v2(body,draft.budget.trace_events);
    put_number_v2(body,draft.budget.results);
    put_number_v2(body,draft.max_bits);
    put_number_v2(body,draft.max_denominator);
    put_number_v2(body,draft.max_trace_bytes);
    put_field_v2(body,status_text(draft.status));
    put_number_v2(body,draft.alternatives.size());
    for (const auto& alternative:draft.alternatives) {
        put_field_v2(body,alternative.id);
        put_field_v2(body,alternative.rule);
    }
    put_field_v2(body,draft.selected_id);
    if (body.size()>max_draft_bytes) throw Error("LanguageDraftByteLimit");
    return std::string(header_v2)+sha256(body)+"\n"+body;
}

LanguageDraftV2 unpack_language_draft_v2(std::string_view bytes) {
    if (bytes.size()>max_draft_bytes+header_v2.size()+65) throw Error("LanguageDraftByteLimit");
    if (!bytes.starts_with(header_v2) || bytes.size()<header_v2.size()+65 || bytes[header_v2.size()+64]!='\n')
        throw Error("InvalidLanguageDraftHeader");
    const auto digest=bytes.substr(header_v2.size(),64);
    const auto body=bytes.substr(header_v2.size()+65);
    if (digest!=sha256(body)) throw Error("LanguageDraftDigestMismatch");
    std::size_t offset=0;
    LanguageDraftV2 draft;
    draft.grammar_version=read_field_v2(body,offset);
    draft.raw_text=read_field_v2(body,offset);
    draft.normalized_text=read_field_v2(body,offset);
    const auto input_count=read_number_v2(body,offset);
    if (input_count!=2) throw Error("InvalidLanguageInputCount");
    for (std::uint64_t i=0;i<input_count;++i) {
        LanguageInputV2 input;
        input.alias=read_field_v2(body,offset);
        input.bytes=read_field_v2(body,offset);
        input.digest=read_field_v2(body,offset);
        draft.inputs.push_back(std::move(input));
    }
    draft.budget.candidates=read_number_v2(body,offset);
    draft.budget.operations=read_number_v2(body,offset);
    draft.budget.trace_events=read_number_v2(body,offset);
    draft.budget.results=read_number_v2(body,offset);
    draft.max_bits=read_unsigned_v2(body,offset);
    draft.max_denominator=read_unsigned_v2(body,offset);
    draft.max_trace_bytes=read_number_v2(body,offset);
    draft.status=parse_status(read_field_v2(body,offset));
    const auto alternative_count=read_number_v2(body,offset);
    if (alternative_count>2) throw Error("InvalidLanguageAlternativeCount");
    for (std::uint64_t i=0;i<alternative_count;++i) {
        LanguageAlternativeV2 alternative;
        alternative.id=read_field_v2(body,offset);
        alternative.rule=read_field_v2(body,offset);
        draft.alternatives.push_back(std::move(alternative));
    }
    draft.selected_id=read_field_v2(body,offset);
    if (offset!=body.size() || pack_language_draft_v2(draft)!=bytes) throw Error("NoncanonicalLanguageDraft");
    return draft;
}

namespace {
// V3's fixed additions are deliberately narrow. Existing V2 phrases are
// accepted through parse_v2/parse_ambiguous_v2 below.
std::optional<ParsedV2> parse_v3_addition(std::string_view text) {
    const auto words=split_words_v2(text);
    if (words.size()!=7 || !valid_reference_v2(words[1]) ||
        !valid_reference_v2(words[3]) || words[4]!="auf" || !target_v2(words,5))
        return std::nullopt;
    if (words[0]=="prüfe" && words[2]=="gegen")
        return ParsedV2{std::string(words[1].substr(1)),std::string(words[3].substr(1)),false};
    if (words[0]=="vergleiche" && words[2]=="und")
        return ParsedV2{std::string(words[1].substr(1)),std::string(words[3].substr(1)),true};
    return std::nullopt;
}

void validate_inputs_v3(std::vector<LanguageInputV3>& inputs) {
    if (inputs.size()!=2) throw Error("InvalidLanguageInputCount");
    for (const auto& input:inputs) {
        if (!valid_alias(input.alias)) throw Error("InvalidLanguageAlias");
        if (input.digest!=sha256(input.bytes)) throw Error("LanguageInputDigestMismatch");
    }
    std::sort(inputs.begin(),inputs.end(),[](const auto& a,const auto& b){return a.alias<b.alias;});
    if (inputs[0].alias==inputs[1].alias) throw Error("DuplicateLanguageAlias");
}

bool contains_alias_v3(const std::vector<LanguageInputV3>& inputs,std::string_view alias) {
    return std::any_of(inputs.begin(),inputs.end(),[&](const auto& input){return input.alias==alias;});
}

bool same_content_v3(const LanguageDraftV3& a,const LanguageDraftV3& b) {
    return a.raw_text==b.raw_text && a.normalized_text==b.normalized_text &&
           a.grammar_version==b.grammar_version && a.inputs==b.inputs &&
           a.budget.candidates==b.budget.candidates && a.budget.operations==b.budget.operations &&
           a.budget.trace_events==b.budget.trace_events && a.budget.results==b.budget.results &&
           a.max_bits==b.max_bits && a.max_denominator==b.max_denominator &&
           a.max_trace_bytes==b.max_trace_bytes && a.status==b.status &&
           a.alternatives==b.alternatives && a.selected_id==b.selected_id;
}

void validate_draft_v3_for_selection(const LanguageDraftV3& draft) {
    LanguageRequestV3 request;
    request.text=draft.raw_text;
    request.inputs=draft.inputs;
    request.budget=draft.budget;
    request.max_bits=draft.max_bits;
    request.max_denominator=draft.max_denominator;
    request.max_trace_bytes=draft.max_trace_bytes;
    request.grammar_version=draft.grammar_version;
    auto expected=interpret_language_v3(std::move(request));
    if (!draft.selected_id.empty()) {
        if (expected.status!=LanguageInterpretationStatus::ambiguous) throw Error("InvalidLanguageSelection");
        const auto found=std::find_if(expected.alternatives.begin(),expected.alternatives.end(),
            [&](const auto& alternative){return alternative.id==draft.selected_id;});
        if (found==expected.alternatives.end()) throw Error("UnknownLanguageAlternative");
        expected.selected_id=draft.selected_id;
    }
    if (!same_content_v3(draft,expected)) throw Error("NoncanonicalLanguageDraft");
}
}

LanguageDraftV3 interpret_language_v3(LanguageRequestV3 request) {
    if (request.grammar_version!=language_grammar_version_v3) throw Error("UnsupportedLanguageGrammar");
    validate_inputs_v3(request.inputs);
    LanguageDraftV3 draft;
    draft.raw_text=std::move(request.text);
    draft.normalized_text=normalized(draft.raw_text);
    draft.grammar_version=std::move(request.grammar_version);
    draft.inputs=std::move(request.inputs);
    draft.budget=request.budget;
    draft.max_bits=request.max_bits;
    draft.max_denominator=request.max_denominator;
    draft.max_trace_bytes=request.max_trace_bytes;

    auto parsed=parse_v2(draft.normalized_text);
    if (!parsed) parsed=parse_ambiguous_v2(draft.normalized_text);
    if (!parsed) parsed=parse_v3_addition(draft.normalized_text);
    if (!parsed) return draft;
    draft.normalized_text=canonical_text_v2(*parsed);
    if (!contains_alias_v3(draft.inputs,parsed->first) ||
        !contains_alias_v3(draft.inputs,parsed->second) || parsed->first==parsed->second)
        throw Error("MissingLanguageAlias");
    if (contains_alias_v3(draft.inputs,"train") && contains_alias_v3(draft.inputs,"holdout")) {
        draft.status=LanguageInterpretationStatus::understood;
        draft.alternatives={{"rational-portfolio-v2", "rational-portfolio-v2 train=train holdout=holdout"}};
        return draft;
    }
    draft.status=LanguageInterpretationStatus::ambiguous;
    draft.alternatives={{"train="+parsed->first+";holdout="+parsed->second,
                         "rational-portfolio-v2 train="+parsed->first+" holdout="+parsed->second},
                        {"train="+parsed->second+";holdout="+parsed->first,
                         "rational-portfolio-v2 train="+parsed->second+" holdout="+parsed->first}};
    std::sort(draft.alternatives.begin(),draft.alternatives.end(),
        [](const auto& left,const auto& right){return left.id<right.id;});
    return draft;
}

LanguageDraftV3 select_language_alternative_v3(const LanguageDraftV3& draft,std::string_view id) {
    validate_draft_v3_for_selection(draft);
    if (draft.status!=LanguageInterpretationStatus::ambiguous || !draft.selected_id.empty())
        throw Error("InvalidLanguageSelection");
    const auto found=std::find_if(draft.alternatives.begin(),draft.alternatives.end(),
        [&](const auto& alternative){return alternative.id==id;});
    if (found==draft.alternatives.end()) throw Error("UnknownLanguageAlternative");
    auto selected=draft;
    selected.selected_id=std::string(id);
    return selected;
}

std::string pack_language_draft_v3(const LanguageDraftV3& draft) {
    validate_draft_v3_for_selection(draft);
    if (draft.status==LanguageInterpretationStatus::unsupported) throw Error("UnsupportedLanguageDraft");
    if (draft.status==LanguageInterpretationStatus::ambiguous && draft.selected_id.empty())
        throw Error("UnselectedLanguageAlternative");
    std::string body;
    put_field_v2(body,draft.grammar_version);
    put_field_v2(body,draft.raw_text);
    put_field_v2(body,draft.normalized_text);
    put_number_v2(body,draft.inputs.size());
    for (const auto& input:draft.inputs) {
        put_field_v2(body,input.alias);
        put_field_v2(body,input.bytes);
        put_field_v2(body,input.digest);
    }
    put_number_v2(body,draft.budget.candidates);
    put_number_v2(body,draft.budget.operations);
    put_number_v2(body,draft.budget.trace_events);
    put_number_v2(body,draft.budget.results);
    put_number_v2(body,draft.max_bits);
    put_number_v2(body,draft.max_denominator);
    put_number_v2(body,draft.max_trace_bytes);
    put_field_v2(body,status_text(draft.status));
    put_number_v2(body,draft.alternatives.size());
    for (const auto& alternative:draft.alternatives) {
        put_field_v2(body,alternative.id);
        put_field_v2(body,alternative.rule);
    }
    put_field_v2(body,draft.selected_id);
    if (body.size()>max_draft_bytes) throw Error("LanguageDraftByteLimit");
    constexpr std::string_view header_v3="BRUCE_LANGUAGE_DRAFT_V3\n";
    return std::string(header_v3)+sha256(body)+"\n"+body;
}

LanguageDraftV3 unpack_language_draft_v3(std::string_view bytes) {
    constexpr std::string_view header_v3="BRUCE_LANGUAGE_DRAFT_V3\n";
    if (bytes.size()>max_draft_bytes+header_v3.size()+65) throw Error("LanguageDraftByteLimit");
    if (!bytes.starts_with(header_v3) || bytes.size()<header_v3.size()+65 ||
        bytes[header_v3.size()+64]!='\n') throw Error("InvalidLanguageDraftHeader");
    const auto body=bytes.substr(header_v3.size()+65);
    if (bytes.substr(header_v3.size(),64)!=sha256(body)) throw Error("LanguageDraftDigestMismatch");
    std::size_t offset=0;
    LanguageDraftV3 draft;
    draft.grammar_version=read_field_v2(body,offset);
    draft.raw_text=read_field_v2(body,offset);
    draft.normalized_text=read_field_v2(body,offset);
    const auto input_count=read_number_v2(body,offset);
    if (input_count!=2) throw Error("InvalidLanguageInputCount");
    for (std::uint64_t i=0;i<input_count;++i) {
        LanguageInputV3 input;
        input.alias=read_field_v2(body,offset);
        input.bytes=read_field_v2(body,offset);
        input.digest=read_field_v2(body,offset);
        draft.inputs.push_back(std::move(input));
    }
    draft.budget.candidates=read_number_v2(body,offset);
    draft.budget.operations=read_number_v2(body,offset);
    draft.budget.trace_events=read_number_v2(body,offset);
    draft.budget.results=read_number_v2(body,offset);
    draft.max_bits=read_unsigned_v2(body,offset);
    draft.max_denominator=read_unsigned_v2(body,offset);
    draft.max_trace_bytes=read_number_v2(body,offset);
    draft.status=parse_status(read_field_v2(body,offset));
    const auto alternative_count=read_number_v2(body,offset);
    if (alternative_count>2) throw Error("InvalidLanguageAlternativeCount");
    for (std::uint64_t i=0;i<alternative_count;++i) {
        LanguageAlternativeV3 alternative;
        alternative.id=read_field_v2(body,offset);
        alternative.rule=read_field_v2(body,offset);
        draft.alternatives.push_back(std::move(alternative));
    }
    draft.selected_id=read_field_v2(body,offset);
    if (offset!=body.size() || pack_language_draft_v3(draft)!=bytes)
        throw Error("NoncanonicalLanguageDraft");
    return draft;
}
}
