#pragma once
#include "bruce/application/rule_history.hpp"
#include "bruce/core/types.hpp"
namespace bruce {
// Local Linux persistence adapter. No terminal UI and no mathematical planning.
// Confirmation text is content binding, not authentication of a human caller.
class LocalFiles {
public:
    static std::string read(const std::string& path,std::size_t limit=max_file_bytes,const ReplayCheckpoint& checkpoint={});
    static void publish_new(const std::string& path,std::string_view content,const ReplayCheckpoint& checkpoint={});
    // Durable snapshot replacement. Caller owns locking and validates the old state.
    static void replace_atomically(const std::string& path,std::string_view content);
    static std::string rule_target(const std::string& path);
    // Receives the canonical target used in the history's content binding.
    static RuleHistory load_rules(const std::string& target,bool allow_missing=false,const ReplayCheckpoint& checkpoint={});
    static std::string active_rules(const std::string& path,const ReplayCheckpoint& checkpoint={});
    // Receives the same canonical target that was shown for confirmation.
    // Caller obtains explicit confirmation through the human-facing UI first.
    // Acquires lock, reloads/rechecks current base and commits state + journal.
    static RuleHistory activate_rules(const std::string& path,const RuleStage& stage,const std::string& confirmation);
};
}
