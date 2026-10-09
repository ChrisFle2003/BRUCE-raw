#include "bruce/mods/language/language.hpp"
#include "bruce/application/research_contract.hpp"
#include "bruce/evidence/bundle.hpp"
#include "bruce/math/numbers.hpp"
#include <algorithm>

namespace bruce {
namespace {
struct Roles {
    const LanguageInputV2* train;
    const LanguageInputV2* holdout;
    std::string interpretation;
};

const LanguageInputV2& input(const LanguageDraftV2& draft, std::string_view alias) {
    for (const auto& value : draft.inputs)
        if (value.alias == alias) return value;
    throw Error("MissingLanguageAlias");
}

Roles resolve(const LanguageDraftV2& draft) {
    if (draft.grammar_version != language_grammar_version_v2)
        throw Error("UnsupportedLanguageGrammar");
    if (draft.status == LanguageInterpretationStatus::unsupported)
        throw Error("UnsupportedLanguageDraft");
    if (draft.status == LanguageInterpretationStatus::understood) {
        if (!draft.selected_id.empty() || draft.alternatives.size() != 1 ||
            draft.alternatives.front().id != "rational-portfolio-v2")
            throw Error("InvalidLanguageSelection");
        return {&input(draft, "train"), &input(draft, "holdout"),
                "rational-portfolio-v2"};
    }
    if (draft.status != LanguageInterpretationStatus::ambiguous)
        throw Error("UnsupportedLanguageDraft");
    if (draft.selected_id.empty()) throw Error("UnselectedLanguageAlternative");
    const std::string_view prefix = "train=";
    const std::string_view separator = ";holdout=";
    if (!draft.selected_id.starts_with(prefix)) throw Error("UnknownLanguageAlternative");
    const auto split = draft.selected_id.find(separator, prefix.size());
    if (split == std::string::npos || split == prefix.size() ||
        split + separator.size() >= draft.selected_id.size())
        throw Error("UnknownLanguageAlternative");
    const auto train = std::string_view(draft.selected_id).substr(prefix.size(), split - prefix.size());
    const auto holdout = std::string_view(draft.selected_id).substr(split + separator.size());
    const auto found = std::find_if(draft.alternatives.begin(), draft.alternatives.end(),
        [&](const auto& alternative) { return alternative.id == draft.selected_id; });
    if (train == holdout || found == draft.alternatives.end())
        throw Error("UnknownLanguageAlternative");
    return {&input(draft, train), &input(draft, holdout), draft.selected_id};
}

ExactResearchTask core_task(const LanguageDraftV2& draft, const Roles& roles) {
    ExactResearchTask task;
    task.domain = "rational";
    task.strategy = "portfolio-rational";
    task.planner = std::string(research_strategy("portfolio-rational").default_planner);
    task.minimum = "-8";
    task.maximum = "8";
    task.max_power = 4;
    task.max_degree = 3;
    task.max_bits = draft.max_bits;
    task.max_denominator = draft.max_denominator;
    task.max_trace_bytes = draft.max_trace_bytes;
    task.budget = draft.budget;
    const NumberDomain domain(task.domain, task.max_bits);
    task.training = parse_exact_samples(roles.train->bytes, domain);
    task.holdout = parse_exact_samples(roles.holdout->bytes, domain);
    // Keep native Core validation and its error names/priorities visible to
    // the caller; the language layer does not wrap or translate them.
    validate_exact_task(task, domain);
    return task;
}
}

LanguageCoreBindingV1 bind_language_to_core_v1(const LanguageDraftV2& draft) {
    // Repack/reparse is the canonicality gate.  It also rechecks input bytes,
    // digests, grammar interpretation and explicit alternative membership.
    const auto canonical = unpack_language_draft_v2(pack_language_draft_v2(draft));
    const auto roles = resolve(canonical);
    auto task = core_task(canonical, roles);
    return {ApplicationTask{std::move(task)}, roles.interpretation,
            roles.train->alias, roles.holdout->alias,
            roles.train->digest, roles.holdout->digest};
}

namespace {
struct RolesV3 {
    const LanguageInputV3* train;
    const LanguageInputV3* holdout;
    std::string interpretation;
};

const LanguageInputV3& input_v3(const LanguageDraftV3& draft, std::string_view alias) {
    for (const auto& value : draft.inputs)
        if (value.alias == alias) return value;
    throw Error("MissingLanguageAlias");
}

RolesV3 resolve_v3(const LanguageDraftV3& draft) {
    if (draft.grammar_version != language_grammar_version_v3)
        throw Error("UnsupportedLanguageGrammar");
    if (draft.status == LanguageInterpretationStatus::unsupported)
        throw Error("UnsupportedLanguageDraft");
    if (draft.status == LanguageInterpretationStatus::understood) {
        if (!draft.selected_id.empty() || draft.alternatives.size() != 1 ||
            draft.alternatives.front().id != "rational-portfolio-v2")
            throw Error("InvalidLanguageSelection");
        return {&input_v3(draft, "train"), &input_v3(draft, "holdout"),
                "rational-portfolio-v2"};
    }
    if (draft.status != LanguageInterpretationStatus::ambiguous)
        throw Error("UnsupportedLanguageDraft");
    if (draft.selected_id.empty()) throw Error("UnselectedLanguageAlternative");
    constexpr std::string_view prefix = "train=";
    constexpr std::string_view separator = ";holdout=";
    if (!draft.selected_id.starts_with(prefix)) throw Error("UnknownLanguageAlternative");
    const auto split = draft.selected_id.find(separator, prefix.size());
    if (split == std::string::npos || split == prefix.size() ||
        split + separator.size() >= draft.selected_id.size())
        throw Error("UnknownLanguageAlternative");
    const auto train = std::string_view(draft.selected_id).substr(prefix.size(), split - prefix.size());
    const auto holdout = std::string_view(draft.selected_id).substr(split + separator.size());
    const auto found = std::find_if(draft.alternatives.begin(), draft.alternatives.end(),
        [&](const auto& alternative) { return alternative.id == draft.selected_id; });
    if (train == holdout || found == draft.alternatives.end())
        throw Error("UnknownLanguageAlternative");
    return {&input_v3(draft, train), &input_v3(draft, holdout), draft.selected_id};
}

ExactResearchTask core_task_v3(const LanguageDraftV3& draft, const RolesV3& roles) {
    ExactResearchTask task;
    task.domain = "rational";
    task.strategy = "portfolio-rational";
    task.planner = std::string(research_strategy("portfolio-rational").default_planner);
    task.minimum = "-8";
    task.maximum = "8";
    task.max_power = 4;
    task.max_degree = 3;
    task.max_bits = draft.max_bits;
    task.max_denominator = draft.max_denominator;
    task.max_trace_bytes = draft.max_trace_bytes;
    task.budget = draft.budget;
    const NumberDomain domain(task.domain, task.max_bits);
    task.training = parse_exact_samples(roles.train->bytes, domain);
    task.holdout = parse_exact_samples(roles.holdout->bytes, domain);
    validate_exact_task(task, domain);
    return task;
}
}

LanguageCoreBindingV1 bind_language_to_core_v3(const LanguageDraftV3& draft) {
    // Keep V3's canonical codec as the admission gate.  No V1/V2 fallback is
    // attempted; native Core validation and its error priority remain visible.
    const auto canonical = unpack_language_draft_v3(pack_language_draft_v3(draft));
    const auto roles = resolve_v3(canonical);
    auto task = core_task_v3(canonical, roles);
    return {ApplicationTask{std::move(task)}, roles.interpretation,
            roles.train->alias, roles.holdout->alias,
            roles.train->digest, roles.holdout->digest};
}
}
