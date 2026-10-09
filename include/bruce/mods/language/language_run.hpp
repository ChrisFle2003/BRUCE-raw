#pragma once
#include "bruce/application/execution_session.hpp"
#include "bruce/mods/language/language.hpp"

namespace bruce {
struct LanguageRunV1 {
    LanguageDraftV1 draft;
    ExecutionSession session;
    std::vector<std::string> phases;
    std::string explanation;
};

struct LanguageRunV2 {
    LanguageDraftV2 draft;
    ExecutionSession session;
    std::vector<std::string> phases;
    std::string explanation;
};

struct LanguageRunV3 {
    LanguageDraftV3 draft;
    ExecutionSession session;
    std::vector<std::string> phases;
    std::string explanation;
};

LanguageRunV1 execute_language_draft(const LanguageDraftV1& draft);
std::string render_language_run(const LanguageRunV1& run);
std::string pack_language_run(const LanguageRunV1& run);
LanguageRunV1 replay_language_run(std::string_view bytes);

LanguageRunV2 execute_language_draft_v2(const LanguageDraftV2& draft);
std::string render_language_run_v2(const LanguageRunV2& run);
std::string pack_language_run_v2(const LanguageRunV2& run);
LanguageRunV2 replay_language_run_v2(std::string_view bytes);

LanguageRunV3 execute_language_draft_v3(const LanguageDraftV3& draft);
std::string render_language_run_v3(const LanguageRunV3& run);
std::string pack_language_run_v3(const LanguageRunV3& run);
LanguageRunV3 replay_language_run_v3(std::string_view bytes);
}
