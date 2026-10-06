#pragma once
#include <string>
#include "bruce/core/replay_control.hpp"
#include <string_view>
namespace bruce {
// Validated proposal content only. No file IO, terminal, approval or activation.
// Digest and confirmation strings are content bindings, not authorizations.
class RuleProposal {
public:
    explicit RuleProposal(std::string bytes,const ReplayCheckpoint& checkpoint={});
    static RuleProposal create(std::string_view previous,std::string_view proposed);
    const std::string& bytes() const {return bytes_;}
    const std::string& previous() const {return previous_;}
    const std::string& proposed() const {return proposed_;}
    const std::string& digest() const {return digest_;}
    void check_current(std::string_view current,const ReplayCheckpoint& checkpoint={}) const;
    std::string confirmation(std::string_view output_path) const;
private:
    std::string bytes_,previous_,proposed_,digest_;
};
}
