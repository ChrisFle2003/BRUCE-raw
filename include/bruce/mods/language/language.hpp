#pragma once
#include "bruce/application/application_task.hpp"
#include "bruce/core/types.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace bruce {
inline constexpr std::string_view language_grammar_version_v1="language-grammar-v1";
inline constexpr std::string_view language_grammar_version_v2="language-grammar-v2";
inline constexpr std::string_view language_grammar_version_v3="language-grammar-v3";

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

// V2 is a separate wire contract for the deterministic free-language grammar.
// Keep the V1 types above unchanged so existing drafts and replays retain
// their original meaning.
struct LanguageInputV2 {
    std::string alias,bytes,digest;
    bool operator==(const LanguageInputV2&) const = default;
};
struct LanguageAlternativeV2 {
    std::string id,rule;
    bool operator==(const LanguageAlternativeV2&) const = default;
};
struct LanguageRequestV2 {
    std::string text;
    std::vector<LanguageInputV2> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    std::string grammar_version=std::string(language_grammar_version_v2);
};
struct LanguageDraftV2 {
    std::string raw_text,normalized_text,grammar_version;
    std::vector<LanguageInputV2> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    LanguageInterpretationStatus status=LanguageInterpretationStatus::unsupported;
    std::vector<LanguageAlternativeV2> alternatives;
    std::string selected_id;
};

// V3 extends the fixed German phrase catalog without changing V1/V2 drafts.
struct LanguageInputV3 {
    std::string alias,bytes,digest;
    bool operator==(const LanguageInputV3&) const = default;
};
struct LanguageAlternativeV3 {
    std::string id,rule;
    bool operator==(const LanguageAlternativeV3&) const = default;
};
struct LanguageRequestV3 {
    std::string text;
    std::vector<LanguageInputV3> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    std::string grammar_version=std::string(language_grammar_version_v3);
};
struct LanguageDraftV3 {
    std::string raw_text,normalized_text,grammar_version;
    std::vector<LanguageInputV3> inputs;
    Budget budget;
    unsigned max_bits=4096,max_denominator=8;
    std::uint64_t max_trace_bytes=1048576;
    LanguageInterpretationStatus status=LanguageInterpretationStatus::unsupported;
    std::vector<LanguageAlternativeV3> alternatives;
    std::string selected_id;
};

// The language module produces only a Core task.  The caller that owns the
// dispatch package may wrap this task in CoreDispatchRequestV1 and provide
// its execution control.  Keeping that wrapper out of this header prevents a
// Language -> NanoCell/dispatcher dependency.
struct LanguageCoreBindingV1 {
    ApplicationTask task;
    std::string alternative_id;
    std::string train_alias, holdout_alias;
    std::string train_digest, holdout_digest;
};

LanguageDraftV1 interpret_language(LanguageRequestV1 request);
LanguageDraftV1 select_language_alternative(const LanguageDraftV1& draft,std::string_view id);
std::string pack_language_draft(const LanguageDraftV1& draft);
LanguageDraftV1 unpack_language_draft(std::string_view bytes);

LanguageDraftV2 interpret_language_v2(LanguageRequestV2 request);
LanguageDraftV2 select_language_alternative_v2(const LanguageDraftV2& draft,std::string_view id);
std::string pack_language_draft_v2(const LanguageDraftV2& draft);
LanguageDraftV2 unpack_language_draft_v2(std::string_view bytes);
LanguageCoreBindingV1 bind_language_to_core_v1(const LanguageDraftV2& draft);

LanguageDraftV3 interpret_language_v3(LanguageRequestV3 request);
LanguageDraftV3 select_language_alternative_v3(const LanguageDraftV3& draft,std::string_view id);
std::string pack_language_draft_v3(const LanguageDraftV3& draft);
LanguageDraftV3 unpack_language_draft_v3(std::string_view bytes);
LanguageCoreBindingV1 bind_language_to_core_v3(const LanguageDraftV3& draft);
}
