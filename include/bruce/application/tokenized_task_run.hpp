#pragma once
#include "bruce/application/execution_session.hpp"
#include "bruce/token/task_spec.hpp"

namespace bruce {
struct TokenizedTaskRunV1 {
    std::string spec_bytes,spec_digest;
    ExactResearchTask compiled_task;
    ExecutionSession session;
    std::string native_bundle_digest;
    std::vector<std::string> phases;
    std::string explanation;
};

TokenizedTaskRunV1 execute_tokenized_task_spec(const TokenizedTaskSpecV1& spec);
std::string pack_tokenized_task_run(const TokenizedTaskRunV1& run);
TokenizedTaskRunV1 replay_tokenized_task_run(std::string_view bytes);
std::string render_tokenized_task_run(const TokenizedTaskRunV1& run);
}
