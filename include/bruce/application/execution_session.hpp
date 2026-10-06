#pragma once
#include "bruce/application/application_task.hpp"
#include <exception>
#include "bruce/application/session_budget.hpp"
namespace bruce {
enum class ExecutionPhase { created,executing,serializing,replaying,ready,failed };
std::string_view phase_name(ExecutionPhase);
class ExecutionPhases {
    ExecutionPhase current_=ExecutionPhase::created;
    std::vector<ExecutionPhase> history_{current_};
public:
    void advance(ExecutionPhase);
    ExecutionPhase current() const {return current_;}
    const std::vector<ExecutionPhase>& history() const {return history_;}
};
struct VerificationBudget {
    std::optional<std::uint64_t> max_replay_bytes;
};
struct SerializationUsage {
    std::optional<std::uint64_t> max_serialized_bytes;
    std::uint64_t serialized_bytes=0;
};
struct SerializationBudget {
    std::optional<std::uint64_t> max_serialized_bytes;
};
struct VerificationControl {
    std::function<bool()> cancelled;
    bool grid_replay_checkpoints=false;
    bool research_replay_checkpoints=false;
    bool comparison_replay_checkpoints=false;
    bool compatibility_replay_checkpoints=false;
    bool serialization_digest_checkpoints=false;
    bool serialization_grid_records=false;
};
struct ExecutionSession {
    ApplicationRun run;
    std::string bundle;
    ExecutionPhases phases;
    VerificationBudget verification_budget;
    bool verification_cancellation=false;
    bool grid_replay_checkpoints=false;
    bool research_replay_checkpoints=false;
    bool comparison_replay_checkpoints=false;
    bool compatibility_replay_checkpoints=false;
    bool serialization_digest_checkpoints=false;
    bool serialization_grid_records=false;
    bool execution_experience_cancellation=false;
    bool execution_grid_source_cancellation=false;
    bool execution_compatibility_source_cancellation=false;
    bool execution_resume_import_cancellation=false;
    SessionUsage session_usage={};
    SerializationUsage serialization_usage={};
};
class ExecutionSessionError final : public Error, public std::nested_exception {
    ExecutionPhase failed_at_;
    ExecutionPhases phases_;
    std::string cause_;
    SessionUsage session_usage_;
    SerializationUsage serialization_usage_;
    ExecutionSessionError(ExecutionPhases,std::string cause,SessionUsage,SerializationUsage);
    friend ExecutionSession execute_checked_application(ApplicationTask,const ExecutionControl&,VerificationBudget,VerificationControl,SessionBudget,SerializationBudget);
public:
    const SessionUsage& session_usage() const {return session_usage_;}
    const SerializationUsage& serialization_usage() const {return serialization_usage_;}
    ExecutionPhase failed_at() const {return failed_at_;}
    const ExecutionPhases& phases() const {return phases_;}
    const std::string& cause() const {return cause_;}
};
ExecutionSession execute_checked_application(ApplicationTask,const ExecutionControl& control={},VerificationBudget budget={},VerificationControl verification={},SessionBudget session_budget={},SerializationBudget serialization_budget={});
std::string render_execution_session(const ExecutionSession&);
std::string render_execution_failure(const ExecutionSessionError&);
}
