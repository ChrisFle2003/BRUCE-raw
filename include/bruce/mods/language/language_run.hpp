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

LanguageRunV1 execute_language_draft(const LanguageDraftV1& draft);
std::string render_language_run(const LanguageRunV1& run);
std::string pack_language_run(const LanguageRunV1& run);
LanguageRunV1 replay_language_run(std::string_view bytes);
}
