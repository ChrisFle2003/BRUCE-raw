#pragma once
#include "bruce/application/rule_proposal.hpp"
#include "bruce/core/replay_control.hpp"
#include <vector>
#include <cstdint>
namespace bruce {
inline constexpr std::size_t rule_history_limit=8*1024*1024;
struct RuleStage {
    std::string target,base;
    std::uint64_t generation;
    RuleProposal proposal;
    std::string bytes() const;
    std::string confirmation() const;
    static RuleStage parse(const std::string& bytes,const ReplayCheckpoint& checkpoint={});
};
class RuleHistory {
public:
    RuleHistory(std::string target,const std::string& bytes,const ReplayCheckpoint& checkpoint={});
    RuleStage stage(std::string_view next) const;
    void check(const RuleStage&,const ReplayCheckpoint& checkpoint={}) const;
    // Caller supplies the explicit decision. This is not a human identity check.
    std::string append(const RuleStage&,std::string_view confirmation,std::uint64_t time) const;
    const std::string& active() const {return active_;}
    std::string report() const;
private:
    std::string target_,head_,active_="BRUCE_RULES_V1\nsnapshot uninitialized\n";
    std::vector<std::pair<std::string,std::uint64_t>> decisions_;
    std::string serialize(const ReplayCheckpoint& checkpoint={}) const;
};
}
