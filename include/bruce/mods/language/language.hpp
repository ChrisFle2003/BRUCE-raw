#pragma once
#include "bruce/core/types.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
inline constexpr std::string_view language_grammar_version_v1="language-grammar-v1";

enum class LanguageInterpretationStatus { understood, ambiguous, unsupported };

struct LanguageInputV1 {
    std::string alias,bytes,digest;
    bool operator==(const LanguageInputV1&) const = default;
};
struct LanguageAlternativeV1 {
    std::string id,rule;
    bool operator==(const LanguageAlternativeV1&) const = default;
};
struct LanguageRequestV1 {
    std::string text;
    std::vector<LanguageInputV1> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    std::string grammar_version=std::string(language_grammar_version_v1);
};
struct LanguageDraftV1 {
    std::string raw_text,normalized_text,grammar_version;
    std::vector<LanguageInputV1> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    LanguageInterpretationStatus status=LanguageInterpretationStatus::unsupported;
    std::vector<LanguageAlternativeV1> alternatives;
    std::string selected_id;
};

LanguageDraftV1 interpret_language(LanguageRequestV1 request);
LanguageDraftV1 select_language_alternative(const LanguageDraftV1& draft,std::string_view id);
std::string pack_language_draft(const LanguageDraftV1& draft);
LanguageDraftV1 unpack_language_draft(std::string_view bytes);
}
