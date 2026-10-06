#pragma once
#include "bruce/core/replay_control.hpp"
#include <string>
#include <variant>
namespace bruce {
struct ExperienceAddTask {std::string previous,source;};
struct ExperienceRevokeTask {std::string previous,target,reason;};
struct ExperienceReplaceTask {std::string previous,target,reason,source;};
struct ExperienceInspectTask {std::string snapshot;};
using ExperienceTask=std::variant<ExperienceAddTask,ExperienceRevokeTask,ExperienceReplaceTask,ExperienceInspectTask>;
struct ExperienceResult {std::string snapshot,report;};
// Explicit snapshot action only: no file IO, rule activation or human approval assertion.
ExperienceResult execute_experience(const ExperienceTask&,const ReplayCheckpoint& checkpoint={});
}
